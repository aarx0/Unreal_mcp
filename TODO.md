# Bridge TODO

A working list of concrete bridge improvements found while *using* the MCP bridge on
this project. Unlike `docs/Roadmap.md` (the large aspirational feature-phase plan),
this is a short, actionable backlog of real gaps/bugs hit in practice. Check items off
as they land.

> Origin: surfaced during the 2026-06 audio/options-menu work (verifying the asset
> read/write actions on `manage_asset`).

---

## Open

### [ ] Round-trip struct-nested Instanced subobjects (`JsonObjectToUStruct` bypass)

Top-level instanced arrays now round-trip (see Done — `7589789`). The remaining gap is
**instanced subobjects nested inside a struct** — the canonical case is a montage
`AnimNotify` (`FAnimNotifyEvent.Notify`). Those import through the engine's
`FJsonObjectConverter::JsonObjectToUStruct` (used by the struct import branch), which
applies its own instanced rules and never calls our `ApplyJsonValueToProperty`, so the
`__class` re-instancing path is bypassed.

**Fix.** In the struct import branch, before handing off to `JsonObjectToUStruct`, walk
the struct's `CPF_InstancedReference` object properties whose JSON value is an object with
`__class`, pre-create those subobjects (`NewObject(Owner, Class)` — the owner is already
threaded as of `7589789`) and apply them via our path; or replace the `JsonObjectToUStruct`
call for such structs with a field-by-field apply that routes object fields through
`ApplyJsonValueToProperty`. **Test attended** (mutates an asset — duplicate + delete):
flip a notify's `Notify` subobject on a montage copy and read it back.

**Effort.** Medium. The owner plumbing already exists; this is about intercepting the
struct path.

### [ ] `set_common_button_input_action` (Common UI) — ready to build, no consumer yet

The last deferred Common UI handler: set a button's `FDataTableRowHandle`
(DataTable + RowName) and call `SetTriggeringInputAction`, validating
`RowStruct->IsChildOf(CommonInputActionDataBase)`. Needs an entry in
`McpConsolidatedActions::CommonUi()` (header → rebuild) + a handler in
`McpAutomationBridge_CommonUIHandlers.cpp`. **Blocked on a fixture:** this project has no
`CommonInputActionDataBase` DataTable (and no DataTables at all as of 2026-06-04), so it
can't be live-tested and has no current consumer — build when the menu work actually adds
a CommonInput action table.

---

## Done

_(completed items, newest first)_

### [x] Round-trip (import) of top-level Instanced subobject arrays
`7589789` (2026-06-04). `set_asset_property` now writes an instanced
`TArray<TObjectPtr<...>>` from the `{ "__class", ... }` export form. An owning `UObject`
is threaded through `ApplyJsonValueToProperty`/`ImportJsonToArray` (defaulted — other
callers unaffected) and used as the `NewObject` Outer, so re-instanced subobjects
serialize into the asset; the new subobject is validated `IsChildOf` the property class and
its fields applied best-effort. Array import also tolerates a JSON string encoding the
array (the typeless `value` param leads some clients to stringify it). *Verified live*
(full rebuild + relaunch): on a duplicate of `IA_Mute`, writing
`Triggers = [{__class: InputTriggerHold, HoldTimeThreshold: 0.75, ...}]` re-instanced
Pressed→Hold and a fresh `get_asset_properties` read back the persisted `InputTriggerHold`.
Struct-nested instanced objects remain open (see Open).

### [x] Deep-export of Instanced subobjects (reflection read) + helpers-twin mirror
`d72f8c3` (export, `McpPropertyReflection.cpp`) and `7589789` (helpers-twin mirror via a
now-public `ExportInstancedObjectToJson`). `CPF_InstancedReference` object values export as
a nested `{ "__class", ...props... }` object — depth-capped, transient/deprecated children
skipped — instead of a bare subobject path. Verified live on `AM_AttackMontage1.Notifies`
(struct-nested `AnimNotify_PlaySound` → `{__class, Sound: MS_SwordSlash, ...}`) and
`IA_Mute.Triggers` (top-level instanced array → `InputTriggerPressed` with
`ActuationThreshold`). The helpers twin already deep-exported array/struct-nested cases via
the `McpPropertyReflection` delegation; the mirror closes the direct-top-level case.

### [x] Deep export/import of struct & object-ref array properties
`ExportArrayToJson` / `ExportPropertyToJsonValue` now recurse into `FStructProperty`
inners (a nested JSON object per element, via the shared `ExportStructToJson`),
`FObjectProperty` inners (object path string), and other non-primitive inners — with a
depth cap (6) and `ExportText` fallback. The single-struct branch recurses the same way,
and transient/deprecated child fields are skipped to match `ExportObjectToJson`. The
import side (`ApplyJsonValueToProperty` + `ImportJsonToArray`) was fleshed out from a
primitive-only stub to cover object-by-path, byte/enums, soft refs, and structs (via
`FJsonObjectConverter::JsonObjectToUStruct`), so struct/object arrays round-trip. Mirrored
into the `McpAutomationBridgeHelpers.h` twin (the ~13 other reflection callers).

*Verified live:* `get_asset_properties` on
`/Game/Blueprints/Characters/InputAction/IMC_GameCommands` now returns
`DefaultKeyMappings.Mappings` as structured JSON — an array of
`{Action: "<path>", Key: {KeyName: "M"}, SettingBehavior: "<enum>", …}` objects — instead
of an opaque `ExportText` blob.

### [x] Advertise the new `manage_asset` actions (native schema)
`get_referencers`, `get_asset_properties`, `set_asset_property` were already in the native
tool schema's `action` enum (auto-sourced from `McpConsolidatedActions::ManageAsset()` in
`McpConsolidatedActionRouting.h`), so over the native `/mcp` transport they were discoverable
**and** dispatchable from the start. Added `propertyName`/`includeTransient` param docs to
`McpTool_ManageAsset.cpp`'s `BuildInputSchema` for client-facing discoverability.

> The originally-planned TypeScript-schema edits (`consolidated-tool-definitions.ts` +
> `VALID_ASSET_ACTIONS`) were a no-op for this native-only fork and were removed with the
> TypeScript bridge — the C++ `McpTool_*.cpp` schemas are now the single source of truth.
