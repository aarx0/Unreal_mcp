# Bridge TODO

A working list of concrete bridge improvements found while *using* the MCP bridge on
this project. Unlike `docs/Roadmap.md` (the large aspirational feature-phase plan),
this is a short, actionable backlog of real gaps/bugs hit in practice. Check items off
as they land.

> Origin: surfaced during the 2026-06 audio/options-menu work (verifying the asset
> read/write actions on `manage_asset`).

---

## Open

### [ ] Reflection read completeness: `FText` export + generic R3 widening

Two small read gaps, both **fixture-blocked in this project** (no asset here has a
top-level `FText` the asset-handler reflection twin can reach, so they can't be
runtime-verified yet — do them when such an asset exists, or attended):
- `FTextProperty` is not handled in `ExportPropertyToJsonValue` → `FText` properties are
  silently skipped on read. Add a branch returning `.ToString()` (lossy of namespace/key,
  fine for inspection), plus a generic `ExportText` fallback so no property is silently
  dropped.
- **R3:** widen the *generic* reflection in `McpAutomationBridgeHelpers.h`
  (`ApplyJsonValueToProperty` + `ExportPropertyToJsonValue`) with `FText` and
  `FClassProperty` branches (benefits `set_object_property` + ~13 callers). Header → 64-TU
  rebuild; the plan recommends an automation test for the branch order.

### [ ] Runtime-verify `TSet` import

`TMap`/`TSet` import landed (`da29c05`); `TMap` is runtime-verified but `TSet` had no
set-property fixture in this project. Verify on a real `TSet` property when one is
available (the code mirrors the verified map path).

### [ ] `set_common_button_input_action` (Common UI) — ready to build, no consumer yet

The last deferred Common UI handler: set a button's `FDataTableRowHandle`
(DataTable + RowName) and call `SetTriggeringInputAction`, validating
`RowStruct->IsChildOf(CommonInputActionDataBase)`. Needs an entry in
`McpConsolidatedActions::CommonUi()` (header → rebuild) + a handler in
`McpAutomationBridge_CommonUIHandlers.cpp`. **Blocked on a fixture:** this project has no
`CommonInputActionDataBase` DataTable (and no DataTables at all as of 2026-06-04), so it
can't be live-tested and has no current consumer — build when the menu work actually adds
a CommonInput action table (the bridge also has no `create_data_table` action yet, so the
fixture would need fabricating).

---

## Done

_(completed items, newest first)_

### [x] `TMap` / `TSet` property import
`da29c05` (2026-06-04). Maps import from a JSON object `{ "<key>": <value> }` (keys from
field names, keys+values routed through `ApplyJsonValueToProperty` so struct/object/
instanced values work), sets from a JSON array; both tolerate a stringified form.
`EmptyValues`/`EmptyElements` + per-entry `AddDefaultValue_Invalid_NeedsRehash` + `Rehash`.
*Verified live* on `IMC_GameCommands.MappingProfileOverrides` (`{"ProfileA":{},
"ProfileB":{}}` round-tripped). `TSet` mirrors the same path — compile-verified, no fixture.

### [x] Instanced subobject round-trip — export + import (all shapes)
EXPORT `d72f8c3`: `CPF_InstancedReference` object values export as nested
`{ "__class", ...props... }` instead of a subobject path (depth-capped, transient/
deprecated skipped). IMPORT `7589789` (top-level arrays — owner threaded through
`ApplyJsonValueToProperty`/`ImportJsonToArray` → `NewObject(Owner, Class)` Outered to the
asset; string-tolerant array values; helpers-twin export mirror via public
`ExportInstancedObjectToJson`). IMPORT `685282f` (struct-nested — strip instanced fields
before `JsonObjectToUStruct`, re-instance after). *Verified live* on `IA_Mute.Triggers`
(Pressed→Hold), `AM_AttackMontage1.Notifies` (AnimNotify_PlaySound), and read on both
montage + IA fixtures.

### [x] Deep export/import of struct & object-ref array properties
`ExportArrayToJson` / `ExportPropertyToJsonValue` recurse into `FStructProperty` inners
(nested object per element, via shared `ExportStructToJson`), `FObjectProperty` inners
(path string), and other non-primitive inners — depth-capped (6), `ExportText` fallback,
transient/deprecated skipped. Import (`ApplyJsonValueToProperty` + `ImportJsonToArray`)
covers object-by-path, byte/enums, soft refs, and structs (via `JsonObjectToUStruct`).
Mirrored into the `McpAutomationBridgeHelpers.h` twin. *Verified live* on
`IMC_GameCommands.DefaultKeyMappings.Mappings`.

### [x] Advertise the new `manage_asset` actions (native schema)
`get_referencers`, `get_asset_properties`, `set_asset_property` were already in the native
tool schema's `action` enum (auto-sourced from `McpConsolidatedActions::ManageAsset()`),
so over the native `/mcp` transport they were discoverable and dispatchable from the start.
Added `propertyName`/`includeTransient` param docs to `McpTool_ManageAsset.cpp`.

> The originally-planned TypeScript-schema edits were a no-op for this native-only fork and
> were removed with the TypeScript bridge — the C++ `McpTool_*.cpp` schemas are the single
> source of truth.
