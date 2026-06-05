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

### [ ] `set_common_button_input_action` (Common UI)
Set a button's `TriggeringInputAction` `FDataTableRowHandle` (DataTable + RowName) on a
Common UI button widget inside a Widget Blueprint. Now unblocked: `create_data_table`
exists (below), so a `CommonInputActionDataBase` table can be fabricated. Plan, refined by
reading the engine: the row struct is `FCommonInputActionDataBase`
(`/Script/CommonUI.CommonInputActionDataBase`, derives from `FTableRowBase`); validate the
DataTable's `RowStruct->IsChildOf` it. Write the property **directly via reflection** rather
than calling `UCommonButtonBase::SetTriggeringInputAction()` — that runtime setter calls
`BindTriggeringInputActionToClick()` when `!IsDesignTime()` (which is the case for a
programmatically-loaded WidgetTree template widget), i.e. runtime input binding on an
archetype with no owning player. The details-panel-equivalent direct write persists the
authoring default with no runtime side effects. Add an entry in
`McpConsolidatedActions::CommonUi()` (header → rebuild) + a block in `HandleCommonUiAction`
(`McpAutomationBridge_CommonUIHandlers.cpp`). Report `rowFound` (non-blocking) so a handle
can be bound before its row is imported.

---

## Done

_(completed items, newest first)_

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
