# Bridge TODO

A working list of concrete bridge improvements found while *using* the MCP bridge on
this project. Unlike `docs/Roadmap.md` (the large aspirational feature-phase plan),
this is a short, actionable backlog of real gaps/bugs hit in practice. Check items off
as they land.

> Origin: surfaced during the 2026-06 audio/options-menu work (verifying the asset
> read/write actions on `manage_asset`).

> **Fixture tip (2026-06-04):** a created Blueprint's variables are fully reachable by
> `get`/`set_asset_property` via the **CDO path** `<bp-path>.Default__<Class>_C` (e.g.
> `/Game/X/BP_Foo.Default__BP_Foo_C`). So you can *fabricate* reflection fixtures —
> create a BP, `add_variable` the type you need, read/write it on the CDO path. (Limited
> by `add_variable` not supporting container vars yet — see below.)

---

## Open

### [ ] `add_variable` container variables (Set / Map / typed Array)
`add_variable` resolves `variableType` as a single class/type and rejects container forms
(`Set<Name>` → `CLASS_NOT_FOUND`). Add container support (set/map/array + inner type[s]).
Useful for BP authoring generally, and it would let us *fabricate* `TSet`/`TMap` reflection
fixtures (see fixture tip) to runtime-verify the `TSet` import below.

### [ ] Runtime-verify `TSet` import
`TMap`/`TSet` import landed (`da29c05`); `TMap` is runtime-verified, `TSet` is
compile-verified only (no set-property fixture, and `add_variable` can't make one yet).
Verify once a `TSet` fixture is possible (the code mirrors the verified map path).

### [ ] Generic reflection widening (R3) + helpers-twin `FText`
`FText` export landed in the asset twin (`McpPropertyReflection`, `6b98c69`). The
`McpAutomationBridgeHelpers.h` twin (used by `inspect` / `set_object_property`) still lacks
the `FText` branch, and the generic importer there lacks `FText`/`FClassProperty` handling
(R3). Header → 64-TU rebuild. **Now testable** via a fabricated BP CDO fixture
(`inspect_cdo` / `inspect set_property`).

### [ ] `set_common_button_input_action` (Common UI) — needs a DataTable + a way to make one
Set a button's `FDataTableRowHandle` + call `SetTriggeringInputAction`, validating
`RowStruct->IsChildOf(CommonInputActionDataBase)`. Needs an entry in
`McpConsolidatedActions::CommonUi()` (header → rebuild) + a handler. Still blocked on a
`CommonInputActionDataBase` DataTable: none in the project, and the bridge has no
`create_data_table` action — add one (also generally useful), then fabricate the fixture.

---

## Done

_(completed items, newest first)_

### [x] `FText` property export
`6b98c69` (2026-06-04). `ExportPropertyToJsonValue` now emits an `FText`'s display string;
text properties are no longer dropped on read. Verified by round-tripping an `FText` BP
variable through its CDO path (the fixture trick above). Import already worked via the
generic `ImportText` string fallback.

### [x] `TMap` / `TSet` property import
`da29c05`. Maps import from a JSON object (keys from field names, values through the
importer), sets from a JSON array; both tolerate a stringified form. Verified on
`IMC_GameCommands.MappingProfileOverrides`. `TSet` compile-verified (no fixture).

### [x] Instanced subobject round-trip — export + import (all shapes)
EXPORT `d72f8c3` (`{ "__class", ... }` instead of a subobject path). IMPORT `7589789`
(top-level arrays — owner threaded → `NewObject(Owner, Class)` Outered to the asset;
string-tolerant array values; helpers-twin export mirror) and `685282f` (struct-nested —
strip instanced fields before `JsonObjectToUStruct`, re-instance after). Verified on
`IA_Mute.Triggers` (Pressed→Hold) and `AM_AttackMontage1.Notifies` (AnimNotify_PlaySound).

### [x] Deep export/import of struct & object-ref array properties
Recurses `FStructProperty`/`FObjectProperty`/other inners (depth-capped, `ExportText`
fallback); import covers object-by-path, byte/enums, soft refs, structs. Mirrored into the
helpers twin. Verified on `IMC_GameCommands.DefaultKeyMappings.Mappings`.

### [x] Advertise the new `manage_asset` actions (native schema)
`get_referencers`/`get_asset_properties`/`set_asset_property` were already routed; added
`propertyName`/`includeTransient` param docs. (The planned TypeScript-schema edits were a
no-op for this native-only fork and were removed with the TypeScript bridge.)
