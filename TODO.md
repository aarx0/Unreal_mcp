# Bridge TODO

A working list of concrete bridge improvements found while *using* the MCP bridge on
this project. Unlike `docs/Roadmap.md` (the large aspirational feature-phase plan),
this is a short, actionable backlog of real gaps/bugs hit in practice. Check items off
as they land.

> Origin: surfaced during the 2026-06 audio/options-menu work (verifying the asset
> read/write actions on `manage_asset`).

> **Fixture tip (2026-06-04):** a created Blueprint's variables are fully reachable by
> `get`/`set_asset_property` via the **CDO path** `<bp-path>.Default__<Class>_C` (e.g.
> `/Game/X/BP_Foo.Default__BP_Foo_C`). Create a BP, `add_variable` the type you need
> (now incl. `Set<>`/`Array<>`/`Map<>`), read/write it on the CDO path. This is how the
> reflection round-trips were verified.

---

## Open

### [ ] Float/double **container** terminals collapse to int
`add_variable` container support (below, Done) creates `Set`/`Map`/`Array` vars, but a
float/double element/value terminal (`Map<Name,Float>`, `Set<Float>`, …) comes out
**int-typed** — the legacy `PC_Float` terminal isn't honored as a container terminal.
Fix: resolve `float`/`double` terminals to `PC_Real` + `PinSubCategory` (`PC_Float`/
`PC_Double`) and set `PinValueType.TerminalSubCategory` for maps. Scalar/int/name/object/
struct terminals are fine.

### [ ] Generic reflection widening (R3) + helpers-twin `FText`
`FText` export landed in the asset twin (`6b98c69`). The `McpAutomationBridgeHelpers.h`
twin (used by `inspect` / `set_object_property`) still lacks the `FText` branch, and its
generic importer lacks `FText`/`FClassProperty` handling (R3). Header → 64-TU rebuild.
**Now testable** via a fabricated BP CDO fixture (`inspect_cdo` / `inspect set_property`).

### [ ] `set_common_button_input_action` (Common UI) — needs a DataTable + a way to make one
Set a button's `FDataTableRowHandle` + call `SetTriggeringInputAction`, validating
`RowStruct->IsChildOf(CommonInputActionDataBase)`. Needs an entry in
`McpConsolidatedActions::CommonUi()` (header → rebuild) + a handler. Blocked on a
`CommonInputActionDataBase` DataTable: none in the project, and the bridge has no
`create_data_table` action — add one (also generally useful), then fabricate the fixture.

---

## Done

_(completed items, newest first)_

### [x] `add_variable` container variables (Set / Array / Map)
`3728d75` (2026-06-04). `add_variable` accepts `Set<Inner>`/`Array<Inner>`/`Map<Key,Value>`
(T-prefixes ok) → `PinType.ContainerType` (+ `PinValueType`). Verified by creating + value
round-tripping `Set<Name>`, `Map<Name,Int>`, `Array<Int>`. (Float container terminals →
int, see Open.)

### [x] Fix `TMap` value import + runtime-verify `TSet`
`6bea6a0` (2026-06-04). `TMap` import (`da29c05`) double-offset the value (passed
`GetValuePtr` to the `*_InContainer` accessors) → all primitive values dropped to default;
the empty-struct IMC fixture had masked it. Now passes `GetPairPtr` for both key and value.
Verified `Map<Name,Int>` round-trips `{"apple":5,"banana":7}`. `TSet` import also
runtime-verified (`Set<Name>` → `["Alpha","Beta","Gamma"]`) once container vars existed.

### [x] `FText` property export
`6b98c69`. `ExportPropertyToJsonValue` emits an `FText`'s display string; text properties
are no longer dropped on read. Verified by round-tripping an `FText` BP variable via its
CDO path. Import already worked via the generic `ImportText` string fallback.

### [x] `TMap` / `TSet` property import (initial)
`da29c05`. Maps import from a JSON object, sets from a JSON array; keys/values route
through the importer; both tolerate a stringified form. (Value-offset bug fixed later in
`6bea6a0`.)

### [x] Instanced subobject round-trip — export + import (all shapes)
EXPORT `d72f8c3`. IMPORT `7589789` (top-level arrays — owner threaded → `NewObject` Outered
to the asset; string-tolerant array values; helpers-twin export mirror) and `685282f`
(struct-nested — strip instanced fields before `JsonObjectToUStruct`, re-instance after).
Verified on `IA_Mute.Triggers` and `AM_AttackMontage1.Notifies`.

### [x] Deep export/import of struct & object-ref array properties
Recurses struct/object/other inners (depth-capped, `ExportText` fallback); import covers
object-by-path, byte/enums, soft refs, structs. Mirrored into the helpers twin. Verified on
`IMC_GameCommands.DefaultKeyMappings.Mappings`.

### [x] Advertise the new `manage_asset` actions (native schema)
`get_referencers`/`get_asset_properties`/`set_asset_property` were already routed; added
`propertyName`/`includeTransient` param docs. (The planned TypeScript-schema edits were a
no-op for this native-only fork and were removed with the TypeScript bridge.)
