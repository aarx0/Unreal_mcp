# Bridge TODO

A working list of concrete bridge improvements found while *using* the MCP bridge on
this project. Unlike `docs/Roadmap.md` (the large aspirational feature-phase plan),
this is a short, actionable backlog of real gaps/bugs hit in practice. Check items off
as they land.

> Origin: surfaced during the 2026-06 audio/options-menu work (verifying the asset
> read/write actions on `manage_asset`).

---

## Open

### [ ] Deep export/import of struct & object-ref array properties
**Symptom.** Reading an asset whose property is a `TArray<FStruct>` (or a struct that
*contains* one) does not return structured JSON. Reading `UInputMappingContext` (UE 5.7)
via `manage_asset get_asset_properties` returned `"Mappings": []` for the deprecated
array and dumped the real bindings as an opaque `ExportText` blob under the nested
`DefaultKeyMappings` struct — not queryable JSON. (Worth surfacing the deprecated/empty
property too: `UInputMappingContext::Mappings` is `UE_DEPRECATED(5.7)`, the live data now
lives in `DefaultKeyMappings.Mappings` — a struct wrapping `TArray<FEnhancedActionKeyMapping>`,
each element holding an object-ref `Action` + a `FKey` struct. So this single case needs
nested-struct recursion **and** array-of-struct recursion **and** object-ref-to-path.)

**Root cause.** `ExportArrayToJson` (`Private/McpPropertyReflection.cpp`, ~L551) only
deep-exports primitive inner types (str/name/bool/float/double/int/int64/byte-enum).
Any other inner — `FStructProperty`, `FObjectProperty`, nested `FArrayProperty` — hits
the `else` branch (~L605) and is flattened to a single `ExportText` string. The single
`FStructProperty` branch (~L150) also text-falls-back. The write side
(`ImportJsonToArray`, ~L616) has the same primitive-only limitation.

**Fix.**
- In `ExportArrayToJson`, when `Inner` is an `FStructProperty`, recurse over the struct's
  child properties and emit a nested JSON object per element (reuse the per-property
  export path). When `Inner` is an `FObjectProperty`, emit the object path string.
- Mirror it in `ImportJsonToArray` so `set_asset_property` can target struct-array
  elements (e.g. add/edit an input mapping).
- Same gap exists for struct *fields* generally — the single-struct branch (~L150) also
  text-falls-back; a shared "export struct → JSON object" helper would cover both.

**Why it matters.** Lots of config assets are arrays of structs (input mappings, data
tables, curves, montage sections, material layer stacks). Without this they're opaque.

**Effort.** Medium. Recursion + object-ref-to-path; watch for cycles / deep nesting (cap depth).

---

### [ ] Advertise the new `manage_asset` actions in the TS schema
`get_referencers`, `get_asset_properties`, and `set_asset_property` are routed and work,
but they aren't listed in `consolidated-tool-definitions.ts`, so they only function via
passthrough and aren't discoverable to clients. Add them to the `manage_asset` action
enum + param docs.

**Effort.** Small.

---

### [ ] Scoped "expected error" suppression for the error-capture device
`replace_widget_class` currently calls `ClearCapturedErrors()` after re-compiling the
widget BP, because `FWidgetBlueprintEditorUtils::ReplaceWidgets` logs a transient
"There can only be one event node bound to this component" error during its internal
recompiles that the global error-scrape would otherwise promote to a false
`ENGINE_ERROR`. Two altitude problems with the current fix: (1) the blanket clear wipes
*all* captured errors/warnings, so a genuinely different error logged during the replace
would be masked too; (2) it pays a second full `CompileBlueprint` purely to launder the
log. Deeper fix: give the capture device a scoped expected-error mechanism (e.g.
`PushExpectedError(Pattern)` / RAII scope, or an ignore-pattern list) so the scrape never
promotes a known-transient log in the first place — usable by any handler that drives
internal recompiles, without a blanket wipe. Then `replace_widget_class` can drop the
extra compile + `ClearCapturedErrors` and just scope the known message.

**Effort.** Medium. Touches the FMcpRequestErrorDevice / capture path in
`McpAutomationBridgeSubsystem`; keep `ClearCapturedErrors` as the blunt fallback.

---

## Done

_(move completed items here with the commit/PR that closed them)_
