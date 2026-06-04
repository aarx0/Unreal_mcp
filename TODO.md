# Bridge TODO

A working list of concrete bridge improvements found while *using* the MCP bridge on
this project. Unlike `docs/Roadmap.md` (the large aspirational feature-phase plan),
this is a short, actionable backlog of real gaps/bugs hit in practice. Check items off
as they land.

> Origin: surfaced during the 2026-06 audio/options-menu work (verifying the asset
> read/write actions on `manage_asset`).

---

## Open

### [ ] Import (re-instancing) of Instanced subobject arrays

The **export** half of the original "round-trip Instanced subobject arrays" item landed
2026-06-03 (commit `d72f8c3`, see Done). The **import** (re-instancing) half is still open
and is the hard part. Three findings from the export work pin down why it needs more than
a mirror of the export logic:

1. **No owner in `ApplyJsonValueToProperty`.** It gets a raw `void*` container, but
   re-instancing a subobject needs `NewObject<...>(Owner, Class)` — the owning `UObject`
   (the asset/package) as Outer so the subobject serializes with the asset. The owner must
   be *threaded* down `ApplyJsonValuesToObject -> ApplyJsonValueToProperty ->
   ImportJsonToArray` (signature change -> header -> full rebuild).
2. **Arrays destroy then null.** `ImportJsonToArray` does `EmptyValues()` + `Resize()`,
   which default-constructs object inners to **null** — so every element is a
   construct-from-scratch, not a reuse. (Confirmed against `IA_Mute.Triggers`, a top-level
   instanced array reachable by our import branch.)
3. **Struct-nested instanced objects bypass our importer.** The montage case
   (`FAnimNotifyEvent.Notify`) imports via the engine's
   `FJsonObjectConverter::JsonObjectToUStruct`, which applies its *own* instanced rules and
   never calls `ApplyJsonValueToProperty`. A coherent round-trip there must coordinate with
   (or pre-empt) `JsonObjectToUStruct`, honoring the exported `__class`.

**Fix.** Add a defaulted `UObject* OwnerForInstancing` threaded through the import path; in
the array/object branches, when the JSON value is an object with `__class`,
`LoadClass`/`FindObject` the class -> `NewObject(Owner, Class, RF_Transactional)` -> recurse
to apply nested props -> assign. For struct-nested cases, pre-create the subobjects from
`__class` before `JsonObjectToUStruct`, or replace that call with a field-by-field apply
that routes object fields through the new path. Reuse the depth cap. **Test attended** (a
round-trip mutates the asset — duplicate + `git checkout`): e.g. flip
`IA_Mute.Triggers[0]` Pressed->Hold and read back the `HoldTimeThreshold`.

**Effort.** Medium-High (was "Medium"; the `JsonObjectToUStruct` coupling raises it).

### [ ] Mirror instanced deep-export into the `McpAutomationBridgeHelpers.h` twin

`d72f8c3` landed instanced deep-export in the asset-handler reflection twin
(`McpPropertyReflection.cpp`) only. The header twin `McpAutomationBridgeHelpers.h` (used by
the ~13 other reflection callers, e.g. `inspect get_property`) still emits a path string
for instanced subobjects. Mirror the verified logic (it's a header -> full rebuild; pair it
with the import work above so the rebuild does double duty).

### [ ] `set_common_button_input_action` (Common UI) — ready to build, no consumer yet

The last deferred Common UI handler: set a button's `FDataTableRowHandle`
(DataTable + RowName) and call `SetTriggeringInputAction`, validating
`RowStruct->IsChildOf(CommonInputActionDataBase)`. Needs an entry in
`McpConsolidatedActions::CommonUi()` (header -> rebuild) + a handler in
`McpAutomationBridge_CommonUIHandlers.cpp`. **Blocked on a fixture:** this project has no
`CommonInputActionDataBase` DataTable (and no DataTables at all as of 2026-06-03), so it
can't be live-tested and has no current consumer — build when the menu work actually adds
a CommonInput action table.

---

## Done

_(completed items, newest first)_

### [x] Deep-export of Instanced subobjects (reflection read)
`ExportPropertyToJsonValue` (`McpPropertyReflection.cpp`) now detects
`CPF_InstancedReference` object values and deep-exports the subobject as a nested
`{ "__class": "<class path>", ...props... }` object — depth-capped, transient/deprecated
children skipped — instead of a bare subobject path that dropped all of its config. Plain
asset references still export as a path. Commit `d72f8c3` (2026-06-03), via Live Coding.

*Verified live* on two distinct shapes: `AM_AttackMontage1.Notifies` (struct-nested
`AnimNotify_PlaySound` now reads back `{__class, Sound: MS_SwordSlash, VolumeMultiplier,
...}`) and `IA_Mute.Triggers` (top-level instanced array — the literal Enhanced Input case
— now reads back `InputTriggerPressed` with `ActuationThreshold`). Import re-instancing and
the helpers-twin mirror remain open (see Open).

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
