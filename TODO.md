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

## Roadmap — missing / incomplete capabilities

> Grouped by theme; reliability items cross-reference the **Bugs** section below. Rough
> priority: 🔴 near-term / unblocks current work · 🟡 valuable soon · 🟢 nice-to-have / deferred.
> The bridge is broad and mostly real (~36 tools, 600+ actions, most gameplay domains genuinely
> implemented) — this lists the genuine holes, not the large covered surface.

### A. Reliability & correctness — harden what already ships
Flakiness in shipped surface erodes trust during real authoring.
- ✅ **Transport mid-call drop — RESOLVED** (2026-06-06; see `docs/pull-architecture.md`).
  Solved **pull-only** rather than via the keepalive/result-cache idea: de-streamed `tools/call`
  to plain HTTP request/response, removed all server push + keepalive, deleted the WebSocket
  transport, and made mutating ops idempotent (fail-fast on name conflict) so a dropped response
  recovers by retry / state re-query. Verified live.
- 🔴 **Widget-authoring ensures + save persistence** — see Bugs §"remove_widget … stale
  variable→GUID" and §"add_common_* … handled ensure on a dirty WBP". Strip the stale
  variable→GUID on `remove_widget`; treat the self-healing `SeenVariableNames` ensure as
  benign in the post-op error guard; make `compile {save:true}` actually persist structural
  deletes. Directly affects the in-progress Common UI menu migration.
- 🟡 **`ReadHttpRequest` busy-poll** — see Bugs §"ReadHttpRequest recv loop busy-polls".
  Replace the 1 ms sleep-spin with `Socket->Wait(WaitForRead)` + chunked header reads.
- 🟡 **Synchronous results / streaming** — `system_control run_tests` is fire-and-forget (no
  result capture); `docs/Roadmap.md` Phase 5 ("real-time streaming for logs/test results",
  remote Insights profiling) is unstarted. Add wait/poll-by-request-id (pairs with the
  transport result cache) so automation/test outcomes are retrievable, not just logged.

### B. Authoring capability gaps — new features that unblock real workflows
- 🔴 **DataTable row CRUD** — `create_data_table` makes an *empty* table; there is **no**
  `add_row`/`edit_row`/`remove_row`/`get_rows` and no CSV/JSON import. This **blocks the
  `set_common_button_input_action` workflow** (Done, below) — it needs a populated
  `CommonInputActionDataBase` table, which today must be filled by hand. Highest-value new
  capability. Implement by reflection-populating a `RowStruct` instance via the existing
  `McpPropertyReflection::ApplyJsonValueToProperty` path, then `UDataTable::AddRow`; add bulk
  CSV/JSON import.
- 🟡 **Enhanced Input authoring depth** — IMC/IA creation + key mapping exist (Input group);
  verify round-trips and fill the sparse trigger/modifier authoring (modifier/trigger
  factories are thin).
- 🟡 **Common UI completeness** — have: add button/text/border, assign style, bind input
  action. Missing / runtime-only: activatable-widget stack push/pop + focus/nav, style-**asset
  creation** (only assignment today), input-action→click wiring. Some is inherently runtime
  (see `COMMONUI_INTEGRATION_PLAN.md`).
- 🟡 **Asset import pipeline** — `import` works (Interchange) but there's no post-import
  configuration: texture compression/sRGB/streaming, audio codec/sample-rate, mesh
  LOD/Nanite/collision. Add a post-import `configure_import` pass (or params on `import`).
- 🟢 **GAS ability-graph depth** — GAS scaffolding is broad (create ability/effect/attribute/
  cue BPs), but multi-step ability *logic* (wait-for-input → montage → apply-effect) is manual
  graph wiring. Templated ability-task chains would speed the boss/combat work.

### C. Runtime / verify loop — mostly HAVE; close the narrow gaps
The bridge already enters/exits PIE (`control_editor play`/`stop`/`eject` via
`RequestPlaySession`/`RequestEndPlayMap`), frame-steps (`single_frame_step`,
`set_fixed_delta_time`), injects `simulate_input`, and reads live state (`inspect
runtime_report`/`pie_report`/`find_by_class`). Remaining gaps:
- 🟡 **Input-injection fidelity** — `simulate_input` routes through `FSlateApplication` key/
  mouse events: right for **menu/UI** testing (works today), but may not exercise **Enhanced
  Input gameplay** mappings (movement/abilities). Verify; if needed, add player-input /
  Enhanced-Input injection for gameplay tests.
- 🟡 **Synchronous test/assert** — see A "synchronous results." Closing it makes the
  author→play→assert loop scriptable end-to-end.
- 🟢 **Gameplay-aware assertion helpers** — optional sugar over `runtime_report`/`inspect`
  (read an attribute, confirm an ability tag) for combat verification.

### D. Long tail — official `docs/Roadmap.md` phases 27+ (note & defer)
`Roadmap.md` lists phases 27+ as unstarted: PCG, sky/weather/water, advanced rendering
(ray tracing/post-process), cinematics & Movie Render Queue, **data/persistence & SaveGame**,
build/deploy, editor utilities, and plugin integrations (MetaHuman, Quixel/Megascans,
Wwise/FMOD, EOS/Steam, Chaos, accessibility, modding). Mostly not relevant to
RhyaTowerOfWishes' near-term needs — tracked there, not re-listed here. The one to watch for
a shipping game: **SaveGame / persistence authoring** (Phase 31) — promote if the game needs it.

---

## Bugs (found while using the bridge — track, fix when convenient)

### [ ] `remove_widget` leaves a stale variable→GUID entry → handled ensure on next compile (+ gated save)
Removing a *named* widget (e.g. a `BindWidget`-style member like a `TextBlock` label) deletes it
from the `WidgetTree` but does NOT remove its entry from the WidgetBlueprint's variable→GUID map.
The next compile then fires a handled ensure — `WidgetBlueprintCompiler.cpp:828`
`Ensure condition failed: SeenVariableNames.Contains(It.Key())` /
`Variable [<Name>] was deleted but still has a GUID referenced by WidgetBlueprint [<WBP>]` — which
the bridge's "Unreal logged errors" guard surfaces as `ENGINE_ERROR` even though the delete and
compile actually succeeded. Two follow-on problems: (a) the ensure-gated `compile {save:true}`
returns `saved:false` and skips the package write, so the deletions sit **unpersisted on disk**
until something re-dirties + saves (e.g. a later `set_position`, whose `saveSucceeded:true` path
is NOT gated and does write); (b) the ensure clears itself after one more compile pass (the
compiler prunes the stale GUID), so a second compile is clean — meaning the asset is fine, the
noise is just the bridge mis-reporting a handled ensure as failure.
Repro 2026-06-05 (Pause-menu cleanup): `remove_widget TextBlock` → next `compile` logs the ensure
+ returns `saved:false`; `remove_widget` ×3 then `compile` (ensure, no save) → `compile` again
(clean, but `saved:false` because no longer dirty) → disk stale until a `set_position` saved it.
Fix directions: (a) on `remove_widget`, also strip the deleted widget's name from the WBP's
variable/GUID map (mirror `FWidgetBlueprintEditorUtils::DeleteWidgets`, which does this cleanup) so
no stale GUID survives; (b) treat the `SeenVariableNames` ensure as benign in the post-op
"engine errors" guard (don't fail the call on a self-healing handled ensure); (c) make the
compile-path save not gate on the dirty flag when `save:true` was explicitly requested (or fall
back to a direct package save) so structural deletes always persist.

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

### [x] Native MCP transport drops mid-call while the editor GameThread is busy — RESOLVED (pull architecture, 2026-06-06)
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

**RESOLVED 2026-06-06** (commits 9d96cc6 / 5361a5f / bce9344) — took a simpler path than the
keepalive/result-cache directions originally sketched here: went **pull-only**. De-streamed
`tools/call` to a plain HTTP request/response (no SSE → the parked-silent-stream failure mode is
gone), removed all server→client push + keepalive, disabled and deleted the WebSocket transport,
and made mutating ops idempotent (fail-fast on name conflict) so a dropped response is recovered by
retry / re-querying editor state — no result cache needed. Full design: `docs/pull-architecture.md`
(the old brief `docs/transport-mid-call-drop-problem.md` is marked superseded).

### [ ] `ReadHttpRequest` recv loop busy-polls (1ms sleep-spin, byte-at-a-time headers) instead of a readiness wait
The HTTP receive path in `McpNativeTransport.cpp` (`ReadHttpRequest`, ~L681) reads request
**headers one byte at a time** — `HasPendingData()` → on empty `Sleep(0.001)` and spin →
`Recv(&Byte, 1, …)` (~L699-711) — and the **body** loop does the same 1ms-sleep-on-empty spin
(~L819-843), each bounded by a 5s deadline. It's a mini busy-wait, not a real readiness wait:
puts a ~1ms latency floor on every read and burns a little idle CPU per in-flight connection.
It's also inconsistent with the **send** side (`SendAllBytes`, ~L283), which correctly uses
`Socket->Wait(ESocketWaitConditions::WaitForWrite, timeout)` to block until the socket is ready.
Found by code-read 2026-06-05; harmless at one agent on localhost, would only bite if connection
volume grew. Pure cleanup, not a correctness bug.
Fix: replace the `HasPendingData()` + `Sleep(0.001)` spins with
`Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan)` (mirroring `SendAllBytes`), and read
headers in chunks into a buffer (scan for `\r\n\r\n`) instead of one byte per `Recv`. Keep the 5s
deadline; removes the latency floor + idle-CPU spin and makes recv/send symmetric.

---

## Done

_(completed items, newest first)_

### [x] Bug fix: `create_widget_blueprint` ignored `savePath`
`manage_blueprint create_widget_blueprint` read `path`/`folder` but not `savePath` (the
`manage_blueprint` schema's param), so callers passing `savePath` silently got the `/Game/UI`
default. Now accepts `savePath` as an alias (precedence: `path` → `savePath` → `folder` →
`/Game/UI`). `.cpp`-only change in `McpAutomationBridge_WidgetAuthoringHandlers.cpp`. Verified
live: `savePath:/Game/McpTmp` created the WBP at `/Game/McpTmp/WBP_SaveTest`.

### [x] Bug fix: `delete` / `delete_asset` ignored `assetPath`
`HandleDeleteAssets` read `path`/`paths` but not `assetPath`/`assetPaths` (which the
`manage_asset` schema advertises), so `delete_asset { assetPath: … }` failed with "No paths
provided". Now accepts `assetPath` (singular) and `assetPaths` (array) alongside `path`/`paths`.
`.cpp`-only change in `McpAutomationBridge_AssetWorkflowHandlers.cpp`. Verified live:
`delete_asset { assetPath: "/Game/McpTmp/WBP_SaveTest" }` deleted it (`deletedCount:1`).

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
