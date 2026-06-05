# Bridge TODO

A working list of concrete bridge improvements found while *using* the MCP bridge on
this project. Unlike `docs/Roadmap.md` (the large aspirational feature-phase plan),
this is a short, actionable backlog of real gaps/bugs hit in practice. Check items off
as they land.

> Origin: surfaced during the 2026-06 audio/options-menu work (verifying the asset
> read/write actions on `manage_asset`).

> **Fixture tip (2026-06-04):** a created Blueprint's variables are fully reachable by
> `get`/`set_asset_property` via the **CDO path** `<bp-path>.Default__<Class>_C`. Create a
> BP, `add_variable` the type you need (incl. `Set<>`/`Array<>`/`Map<>`), read/write it on
> the CDO path. This is how the reflection round-trips were verified.

---

## Open

_(no planned feature items right now — see Bugs below for tracked defects)_

---

## Bugs (found while using the bridge — track, fix when convenient)

### [ ] `create_widget_blueprint` ignores `savePath`
`manage_blueprint create_widget_blueprint` always creates the asset under `/Game/UI`,
ignoring the `savePath` param. Observed 2026-06-04 fabricating a fixture: asked for
`/Game/McpTmp`, got `/Game/UI/WBP_BtnTest`. The handler should honor `savePath` (falling back
to `/Game/UI` only when empty), matching `manage_blueprint create` / `create_data_table`.

### [ ] Common UI `add_common_button` / `add_common_text` fire a handled ensure on a dirty WBP
The FIRST `add_common_*` on a freshly created (clean) Widget Blueprint succeeds silently, but
a SECOND add — or an add after another structural edit — logs a `Handled ensure` (the native
handler returns success but the bridge's "Unreal logged errors" guard surfaces it as
`ENGINE_ERROR`). Repro 2026-06-04: create WBP → `add_common_button` (ok) → `add_common_text`
(ensure) → `add_common_button` (ensure). Likely the construct / `SafeAddWidgetToTree` path on
an already-modified, unsaved CommonUI WidgetTree. Related: runtime-added (unsaved) widgets can
vanish from the tree across later ops — a `set_common_button_input_action` couldn't find a
button an earlier call had set, because nothing saved the WBP in between. Worth deciding
whether `add_common_*` should compile+save (or at least be ensure-clean) so multi-widget
authoring in one session is reliable.

### [ ] Native MCP transport drops mid-call while the editor GameThread is busy
When the editor is busy (asset-registry scan / GC / still initializing right after launch, or a
long synchronous op), a `tools/call` can fail with "MCP server transport dropped mid-call;
response was lost" even though the operation **completed**. Repro 2026-06-04: two back-to-back
`manage_asset` calls (`delete_asset`, then `exists`) dropped right after a relaunch; the editor
stayed alive and the delete had in fact succeeded (a later `exists` showed the asset gone) — only
the responses were lost. So the client can't tell success from failure for a mutating op.

Root cause (read `McpNativeTransport.cpp`): `HandleToolsCall` (~L1271) sends the SSE response
headers on the socket thread, parks the connection in `SSEConnections`, then
`QueueAutomationRequest`s the work onto the subsystem queue — which is *"drained by the core ticker
after world ticking"* (~L1419), i.e. on the **GameThread, once per tick**. So nothing runs and **no
bytes flow on the per-request SSE stream** until the GameThread ticks. Crucially the per-request
stream (`FSSEConnection`) has **no keepalive** — the `:keepalive` frames
(`WriteNotificationKeepalive`) only cover the long-lived `GET /mcp` notification streams
(`FNotificationStream`). So while the GameThread is saturated the response stream is silent and the
connection is torn down (client read timeout, well under the server's 300s `RequestTimeoutSeconds`,
or a reset during init). The GameThread later drains the queue and runs the op — mutation lands —
but `CompletePendingRequest` then writes the result to a dead socket (write fails under the 5s
`WriteTimeoutSeconds`), so it's logged as lost and is unrecoverable.

Fix directions: (a) send periodic `:keepalive`/heartbeat frames on the **per-request** SSE stream
too, driven from the socket thread (which is NOT blocked by the GameThread), so a slow op doesn't
look dead; (b) emit an immediate "accepted" SSE event when the request is queued; (c) make results
re-queryable by request id (short-lived result cache + a `get_result` probe) so a dropped response
is recoverable instead of forcing a blind retry or an independent state re-read; (d) optionally
return a fast "warming up" status until the editor finishes its initial load.

### [ ] `manage_asset delete` / `delete_asset` ignores `assetPath`, requires `paths`
`delete_asset { assetPath: "/Game/Foo" }` returns `INVALID_ARGUMENT: No paths provided`; only
`paths: ["/Game/Foo"]` works (observed 2026-06-04). Other `manage_asset` actions accept the
singular `assetPath`, so this is an inconsistency that surprises callers. The delete handler
should also accept `assetPath` (and `path`/`name`) for parity, or the schema should stop
advertising `assetPath` for the delete actions.

---

## Done

_(completed items, newest first)_

### [x] `set_common_button_input_action` (Common UI)
Binds a `UCommonButtonBase`'s `TriggeringInputAction` (`FDataTableRowHandle`) on a button
widget inside a Widget Blueprint. Params: `widgetPath`, `widgetName`, `dataTable`, `rowName`.
Validates the widget is a `UCommonButtonBase` and the DataTable's `RowStruct->IsChildOf(
FCommonInputActionDataBase)` (`/Script/CommonUI.CommonInputActionDataBase`). Writes the handle
**directly via reflection** (`FindFProperty` → `ContainerPtrToValuePtr`) rather than calling
`UCommonButtonBase::SetTriggeringInputAction()`, whose runtime input binding
(`BindTriggeringInputActionToClick`) fires when `!IsDesignTime()` — true for a
programmatically-loaded WidgetTree template, i.e. it would do runtime binding on an archetype
with no owning player. The direct write is what the details panel persists. Reports `rowFound`
(non-blocking). Routed via `CommonUi()` (also feeds the `manage_blueprint` schema enum).
Verified live on a fabricated `WBP_MenuButton` instance + a `CommonInputActionDataBase` table:
set → independent read-back of `TriggeringInputAction` returned the exact `{DataTable, RowName}`
(twice), and the `WRONG`/`INVALID_ROW_STRUCT`/`NOT_FOUND`/`WIDGET_NOT_FOUND` guards all reject.

### [x] `manage_asset` action `create_data_table`
Create a `UDataTable` with a given row struct. Params: `name`, `path`, `rowStruct` (full
object path or bare struct name → `LoadObject` then `TryFindTypeSlow` fallback), optional
`save` (default true). Validates the struct derives from `FTableRowBase`; saves via
`McpDirectPackageSave` + registers with the asset registry. Routed in `HandleAssetAction`
+ declared in the subsystem header + listed in `ManageAssetCore()` (which also feeds the
tool schema enum, so it is advertised automatically). Verified live: full-path + bare-name
creation, `RowStruct` read-back, and rejection of a non-row struct (`Vector`) and an unknown
name. Generally useful, and unblocks `set_common_button_input_action`.

### [x] Generic reflection widening (R3): helpers-twin `FText` parity
The `McpAutomationBridgeHelpers.h` twin (`inspect get/set_property`, `set_object_property`)
lagged the asset twin on `FText`: export had no `FTextProperty` branch (text dropped to
`nullptr` on reads) and import ended in a hard "unsupported" error (no string fallback, so
`FText`/text-importable types couldn't be written). Added the `FTextProperty` export branch
(display string) + the same generic `ImportText_Direct` string fallback the asset twin had,
bringing the two twins to parity. Verified on a fabricated BP CDO fixture: an `FText` var
round-trips through **both** twins to the identical value incl. embedded quotes/commas
(`Say "hi" to all, friend — don't stop`). `FClassProperty` was already covered (derives from
`FObjectProperty`) and re-verified round-tripping `/Script/Engine.PointLight`.

### [x] Numeric reflection correctness: float/double variables + map/set export
Two distinct bugs found by thoroughly probing every numeric type in every position
(scalar / Array / Set / Map key / Map value) on a fabricated BP CDO fixture:
- `c0bff6a` — `add_variable` mapped `float`/`double` to the legacy `PC_Float` *category*,
  which UE5 doesn't honor → the BP compiler produced an **int** property → fractional values
  truncated (`1.5 -> 1`) for scalar AND container float/double vars (pre-existing for
  scalars). Fixed: `PC_Real` + float/double `PinSubCategory` at every position.
- `40c3bf9` — the map-value and set-element **export** (in BOTH reflection twins) only
  handled str/int/float/bool, dropping double/int64/byte/enum/object/struct to an
  `ExportText` string; non-str/name/int map keys became `"key_%d"`. Fixed: delegate to
  `ExportPropertyToJsonValue` (full coverage), keys preserved via `ExportText`.
Verified (rebuild + relaunch): scalar float/double + `Array<Float>`/`Set<Float>`/`Set<Double>`/
`Map<Name,Float>`/`Map<Name,Double>`/`Map<Name,Int64>` all round-trip correct JSON numbers.

### [x] `add_variable` container variables (Set / Array / Map)
`3728d75`. Accepts `Set<Inner>`/`Array<Inner>`/`Map<Key,Value>` (T-prefixes ok) →
`PinType.ContainerType` (+ `PinValueType`). Verified by creating + value round-tripping.

### [x] Fix `TMap` value import + runtime-verify `TSet`
`6bea6a0`. `TMap` import (`da29c05`) double-offset the value (`GetValuePtr` vs the
`*_InContainer` accessors) → all primitive values dropped to default (empty-struct IMC
fixture masked it). Now `GetPairPtr`. Verified `Map<Name,Int>` round-trips; `TSet` import
runtime-verified via a fabricated `Set<Name>`.

### [x] `FText` property export
`6b98c69`. `ExportPropertyToJsonValue` emits an `FText`'s display string. Verified via a BP
CDO fixture. Import already worked via the generic `ImportText` fallback.

### [x] `TMap` / `TSet` property import (initial)
`da29c05`. Maps from a JSON object, sets from a JSON array; keys/values route through the
importer; both tolerate a stringified form. (Value-offset bug fixed in `6bea6a0`.)

### [x] Instanced subobject round-trip — export + import (all shapes)
EXPORT `d72f8c3`. IMPORT `7589789` (top-level arrays — owner threaded → `NewObject` Outered
to the asset) and `685282f` (struct-nested — strip instanced fields before
`JsonObjectToUStruct`, re-instance after). Verified on `IA_Mute.Triggers` and
`AM_AttackMontage1.Notifies`.

### [x] Deep export/import of struct & object-ref array properties
Recurses struct/object/other inners (depth-capped, `ExportText` fallback). Mirrored into the
helpers twin. Verified on `IMC_GameCommands.DefaultKeyMappings.Mappings`.

### [x] Advertise the new `manage_asset` actions (native schema)
`get_referencers`/`get_asset_properties`/`set_asset_property` were already routed; added
`propertyName`/`includeTransient` param docs. (The planned TypeScript-schema edits were a
no-op for this native-only fork and were removed with the TypeScript bridge.)
