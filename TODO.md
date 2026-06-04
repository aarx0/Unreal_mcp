# Bridge TODO

A working list of concrete bridge improvements found while *using* the MCP bridge on
this project. Unlike `docs/Roadmap.md` (the large aspirational feature-phase plan),
this is a short, actionable backlog of real gaps/bugs hit in practice. Check items off
as they land.

> Origin: surfaced during the 2026-06 audio/options-menu work (verifying the asset
> read/write actions on `manage_asset`).

---

## Open

### [ ] Round-trip Instanced subobject arrays (input Triggers/Modifiers, etc.)
**Symptom.** `get_asset_properties` exports an *Instanced* `TArray<TObjectPtr<…>>`
(e.g. `FEnhancedActionKeyMapping.Triggers` / `.Modifiers`, which are
`UPROPERTY(EditAnywhere, Instanced)`) by emitting each element's `GetPathName()` as a
plain path string. For an instanced subobject that drops all of the subobject's own
config (e.g. a Hold trigger's `HoldTimeThreshold`). On `set_asset_property` the matching
import resolves the path with *reference* semantics (no `PPF_InstanceSubobjects`), so it
either fails to resolve the stale path or points the element at the old shared subobject
instead of creating a fresh instance.

**Root cause.** `ExportPropertyToJsonValue`'s `FObjectProperty` branch always emits a
path string; it never checks `CPF_InstancedReference` / instanced-subobject semantics.
`IMC_GameCommands` is unaffected today (its mappings have empty Triggers/Modifiers), so
this was out of scope for the deep struct/object-array work — but any asset that
*populates* an instanced object array (input triggers/modifiers, montage notifies,
instanced components, …) silently loses that config on read and can't round-trip on write.

**Fix.** On export, detect instanced object properties (`CPF_InstancedReference`, or the
property class defaulting to instanced) and deep-export the subobject via
`ExportObjectToJson` into a nested `{…}` object including its class name, so the engine
importer (`FJsonObjectConverter`) re-instances it. Mirror on the import side.

**Effort.** Medium. Needs the instanced-vs-asset distinction + a class-name field + import
re-instancing; reuse the existing depth cap. Surfaced by the adversarial review of the
deep-array work (2026-06).

---

## Done

_(completed items, newest first)_

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
