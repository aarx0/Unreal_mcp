# Common UI Integration Plan — McpAutomationBridge (aarx0 fork)

> **Status:** SHIPPED. Phases 0–2 (build wiring, helper extraction, typed `add_common_*`/`set_common_*` handlers) landed 2026-05-29 (121c0f25, d755e459) — see `McpAutomationBridge_CommonUIHandlers.cpp`. Phase 3 (runtime activatable-stack push/pop) deliberately unbuilt (PIE test-tooling only; TODO.md). Kept for the design rationale/citations, not as an open proposal. The TS-sync sections below are moot — the TypeScript bridge was deleted outright 2026-06-03 (b1f52dba).
> **Author/driver:** aarx0 fork; solo. This is a *bridge* tool until Epic ships an official UE 5.8 MCP, so the design optimizes for **clean, well-factored, deletable-later** over breadth.
> **Method:** Produced by an 8-agent codebase/domain investigation + a 3-lens adversarial review. Load-bearing claims were re-verified against engine + plugin source (citations below). The raw synthesis had **one compile-blocker and one dual-sync blocker**; both are corrected here.

---

## 0. TL;DR — what this plan actually commits to

1. **Link CommonUI as an optional, delay-loaded module** (never a hard dependency) behind an `MCP_HAS_COMMON_UI` compile guard. CommonUI is `EnabledByDefault:false` in the engine, so a hard dep would break the bridge's load on any project without it.
2. **Don't grow the 7,256-line `WidgetAuthoringHandlers.cpp`.** Extract its reusable helpers into a shared header/.cpp, and put all Common-UI-specific code in a new, deletable `McpAutomationBridge_CommonUIHandlers.cpp` routed at the **dispatch-lambda level** (so it never enters the mega-function).
3. **~80% of Common UI is free** once a generic `string→UClass` widget resolver exists and the existing reflection `set_style` path is used. Only **a small set of typed handlers** are needed for shapes reflection can't express.
4. **The #1 correction from review:** Common UI buttons/styles are **abstract classes**. You author a button by constructing your *own concrete Blueprint subclass* (`WBP_MenuButton`), not `UCommonButtonBase`. The handlers must take a `TSubclassOf` and use `ConstructWidget(UClass*)`, not the `ConstructWidget<T>()` template.
5. **First PR is tiny and behavior-preserving:** build wiring + helper extraction + class resolver, proving the whole pipeline (create a `UCommonActivatableWidget` BP, compile) with **zero new Common UI actions**.

---

## 1. Verified facts (the load-bearing ones)

Engine source: `C:\Epic\UE_5.7\Engine\Plugins\Runtime\CommonUI\`

| Fact | Evidence | Consequence |
|---|---|---|
| `UCommonButtonBase` is **abstract** | `UCLASS(Abstract, Blueprintable, MinimalAPI ...)` — `CommonButtonBase.h:68` | Cannot be `ConstructWidget`'d. Author via a concrete BP subclass. |
| **No concrete `UCommonButton` ships** | search for `class .*UCommonButton\b` → empty | There is nothing to instantiate directly; the project's `WBP_MenuButton` *is* the concrete class. |
| `UCommonButtonStyle`/`UCommonTextStyle` are `: public UObject`, **abstract style classes** | `CommonButtonBase.h:69`, `CommonTextBlock.h:22` | Styles are assigned **by class** (`TSubclassOf`), never instantiated. |
| Button style property | `TSubclassOf<UCommonButtonStyle> Style;` `CommonButtonBase.h:814`; setter `SetStyle(TSubclassOf<UCommonButtonStyle>)` `:423` | Use the typed `SetStyle` so the slate style rebuild fires. A raw property write won't rebuild. |
| `UCommonTextBlock : public UTextBlock` (**concrete**) | `CommonTextBlock.h:129`; `SetStyle(TSubclassOf<UCommonTextStyle>)` `:151` | Directly constructible. Text content uses inherited `UTextBlock::SetText(FText)`. |
| CommonUI plugin is **off by default**, **Beta** | `CommonUI.uplugin`: `"EnabledByDefault": false`, `"IsBetaVersion": true` | Must be optional + delay-loaded. Beta ⇒ acknowledge possible 5.8 API churn (fine for a throwaway bridge). |
| `CommonInput` is a **public dep of** `CommonUI` | `CommonUI.Build.cs` `PublicDependencyModuleNames` includes `"CommonInput"` | Linking `CommonUI` transitively links `CommonInput`. A separate dep entry is redundant for linking. |
| `CommonUIEditor` is **Editor-type tooling only** | `CommonUI.uplugin` module `"Type":"Editor"`; contains `CommonUIEditorSettings` etc. | **Drop it** — none of the four needed APIs live there. |
| TS routing falls through, doesn't reject | `consolidated-tool-handlers.ts:196` routes only if `widgetAuthoringActionSet.has(action)`, else `:198` → `handleBlueprintTools(action)` | A new action missing from the TS const **silently mis-routes to the blueprint handler** — it does *not* hit a clean "unknown widget action". Adding to the TS const is **mandatory routing**, not a convenience. |

Plugin-internal line numbers below (e.g. `WidgetAuthoringHandlers.cpp:154-829`) come from the investigation and were cross-checked by two review agents, but **verify each before editing** — anchor edits on the named symbol/branch, not the absolute line.

---

## 2. What needs code vs. what's free

Authoring a widget is two phases: **construct** it (make the object, add it to the widget tree) and **configure** it (set its properties). "Reflection is free" refers only to *configure*: the existing `set_style` path (`ImportText_Direct` write / `MCP_PROPERTY_EXPORT_TEXT` read) sets any property by name on any `UWidget` — `FText`, `TSubclassOf`, enums, structs included. So once a Common UI widget exists, ordinary property editing is already free.

**Free — existing code, no new handler** (configure phase, via `set_style`/`set_object_property`):
- button/label text, text color, font size, justification
- padding, alignment, visibility, `bIsFocusable`, `bIsInteractionEnabled`
- any plain bool/enum/int/struct property on any Common UI widget

**Needs a typed handler** — four cases, each for a *different* reason (not because they're "big"):

| # | What | Why reflection can't do it end-to-end |
|---|---|---|
| 1 | **Construct a Common UI button** | The base is `UCLASS(Abstract)` and ships no concrete class — you must pass *your* concrete subclass (`WBP_MenuButton_C`) and build it via the runtime `ConstructWidget(UClass*)`, not the compile-time `ConstructWidget<T>()`. A **construction** limit, not a property limit. |
| 2 | **Make a style apply** | Reflection can *set* the `Style` property, but only the typed `SetStyle(TSubclassOf<...>)` fires `UpdateFromStyle`/the slate rebuild — a raw write leaves the button unstyled until reopened. A **setter side-effect**. |
| 3 | **Button input action** | The `FDataTableRowHandle` is reflection-settable, but validating the row is a real `CommonInputActionDataBase` row needs typed code. A **validation** gap. |
| 4 | **Push onto an activatable stack** | `BP_AddWidget(TSubclassOf<...>)` is a runtime function call against a *live* widget, not a property — and it's runtime presentation, not asset authoring (§8). **Not a property at all.** |

So §2 is effectively the scope of hand-written work: a handful of typed handlers for these four, plus lean on reflection for everything else.

**Construction example (case 1).** The existing helper bakes the type in at compile time ([`WidgetAuthoringHandlers.cpp:589`](plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_WidgetAuthoringHandlers.cpp#L589)):
```cpp
T* Widget = WidgetTree->ConstructWidget<T>(T::StaticClass(), Name);  // T fixed at COMPILE time
```
That works for concrete UMG types (`UButton`). For Common UI you resolve a concrete subclass from a *runtime asset path*, guard against abstract, and use the non-template overload:
```cpp
UClass* ButtonClass = LoadClass<UWidget>(nullptr, *ButtonClassPath);  // "/Game/UI/WBP_MenuButton.WBP_MenuButton_C"
if (!ButtonClass || ButtonClass->HasAnyClassFlags(CLASS_Abstract)) return /* error */;
UWidget* Button = WidgetTree->ConstructWidget(ButtonClass, WidgetName);  // runtime UClass*, no <T>
```
The plugin never names your asset; the path is a parameter, so the same handler builds any future button subclass with no recompile.

**Beyond editor authoring — three mechanisms, so little is truly "impossible"; it's a question of which path:**
- **A — MCP tool → reflection edit** (asset/editor state, no rebuild): the free/cheap path above.
- **B — MCP tool → drive PIE** (`control_editor play` + `simulate_input`, already in the plugin): can manipulate *live* widget instances — e.g. push onto an activatable stack during play. Transient (not persisted to an asset), testing-shaped, needs PIE running + a handler that finds the live instance.
- **C — write C++ source** (plugin or game module) + rebuild: anything needing new compiled symbols (custom widget bases, action bindings, the handlers themselves). Normal dev work — done by editing source, not by an MCP call. The only hard line: a single MCP *tool call* can't synthesize compiled C++ at runtime; that always needs mechanism C + a rebuild.

> **On the optional reflection widening (§6):** the generic `ApplyJsonValueToProperty` helper (used by `set_object_property`, **not** `set_style`) lacks an `FText` branch and treats `TSubclassOf` via the generic `FObjectProperty` path. Widening it helps the whole plugin but is **not** required for Common UI text/styles — `set_style` already covers those. So §6 is optional hardening, not a fix.

---

## 3. Architecture decisions

### 3.1 New deletable TU, not growth of the mega-function
`WidgetAuthoringHandlers.cpp` is **one ~6,400-line function** (≈line 838→EOF) under `#pragma warning(disable:4883)`. The module deliberately runs `PCHUsage=NoPCHs` + `bUseUnity=true` + `NumIncludedBytesPerUnityCPPOverride=256KB` (`Build.cs:135-138`) to dodge C1060/C3859 heap/PCH exhaustion across 50+ handlers. Adding Common UI inline worsens the worst translation unit.

➡ **New `McpAutomationBridge_CommonUIHandlers.cpp` (~400-600 lines)** holding a single `HandleCommonUiAction` and all `#include "CommonButtonBase.h"`-style includes behind `#if MCP_HAS_COMMON_UI`. Deleting the feature = remove one `.cpp` + one `Build.cs` block + one schema block + one routing branch.

### 3.2 Route at the dispatch lambda, *before* the widget mega-function
> **Review correction (major).** Actions placed in `McpConsolidatedActions::WidgetAuthoring()` are routed by `IsWidgetAuthoringAction` (Subsystem.cpp ~1389) straight **into** `HandleManageWidgetAuthoringAction` (the mega-function). To delegate to our new TU without editing that function, add an `IsCommonUiAction(SubAction)` check in the `manage_blueprint` lambda (Subsystem.cpp ~1384) that calls `HandleCommonUiAction` **before** the `IsWidgetAuthoringAction` branch, and keep the Common UI actions **out of** `WidgetAuthoring()` (use a dedicated `CommonUi()` action list). This preserves the deletable-TU goal and never touches the 6,400-line function.

### 3.3 Extract shared widget helpers (the enabling refactor)
The `WidgetAuthoringHelpers` namespace (`WidgetAuthoringHandlers.cpp:154-705`: `LoadWidgetBlueprint`, `SafeAddWidgetToTree`, `RegisterWidgetGuid`, `CreateAndRegisterWidget<T>`, etc.) is the cleanest seam. Extract it into `McpAutomationBridge_WidgetAuthoringHelpers.{h,cpp}` so the new TU reuses it instead of forking a third widget-create path.

> **Review corrections:**
> - These helpers are **already external linkage** (named namespace, no `static`). The refactor is a *move*, not a static→extern conversion. The real hazard is **LNK2005 duplicate symbol** if definitions are left behind — move them out entirely (declarations in header, definitions only in the new .cpp).
> - `WidgetAuthoringHandlers.cpp:707` does `using namespace WidgetAuthoringHelpers;` — the new header must preserve the namespace name or every in-file call site breaks.
> - **Split the extraction:** the validation helpers at `:713-829` call into `UMcpAutomationBridgeSubsystem` (error capture). Keep those subsystem-coupled helpers separate so the shared "dependency-light" header stays dependency-light.
> - **Honest scope:** moving helpers shrinks the *file* by ~675 lines but the oversized *function* (838→EOF) is unchanged. The 4883/mega-function problem is **not** improved by this refactor — it's just isolation so we don't make it worse.

### 3.4 Generic class resolver
Add `WidgetAuthoringHelpers::ResolveWidgetClass(FString) → UClass*`: try short-name map (back-compat) → raw path `LoadClass`/`FindObject` → `/Script/CommonUI.<Name>` → `/Script/UMG.<Name>` → validate `IsChildOf(UWidget)`. Model on the correct `UiHandlers.cpp:252-267` pattern.

> **Review correction:** prefer `LoadClass`/`StaticLoadClass` (forces module load) over `FindObject` (find-only). Resolution of CommonUI types still requires the plugin enabled in the target project — only UMG-type resolution is truly dependency-free.

### 3.5 Attach to `manage_blueprint` (canonical), not a new tool
`FMcpToolRegistry::IsCanonicalMcpToolName` accepts only the 22 canonical names and **silently drops** others (`manage_widget_authoring` is already dropped). Common UI actions ride `manage_blueprint`, same as existing widget actions. No 23rd tool.

---

## 4. Build / link changes (`Build.cs` + `.uplugin`)

```csharp
// Build.cs — inside if (Target.bBuildEditor), in the optional-modules section
// (after the EnhancedInput block ~line 242):
AddOptionalDynamicModule(Target, EngineDir, "CommonUI", "CommonUI");
// CommonInput is transitively linked by CommonUI; the explicit delay-load entry
// is OPTIONAL (only adds an import surface). Include it only if a CommonInput
// symbol is called directly (set_common_button_input_action validation).
// AddOptionalDynamicModule(Target, EngineDir, "CommonInput", "CommonUI");

// Compile guard, gated on engine having the plugin source:
bool bHasCommonUI = FindOptionalModule(EngineDir, "CommonUI");
PublicDefinitions.Add(bHasCommonUI ? "MCP_HAS_COMMON_UI=1" : "MCP_HAS_COMMON_UI=0");
// ...and "MCP_HAS_COMMON_UI=0" in the non-editor else branch (lines ~319-326).
```

```jsonc
// McpAutomationBridge.uplugin — ONE entry in "Plugins" (one entry per plugin):
{ "Name": "CommonUI", "Enabled": true, "Optional": true }
```

**Do NOT:**
- ❌ Add `CommonUIEditor` (Editor-only tooling; none of the needed APIs live there). *(Review major — corrected from the raw synthesis.)*
- ❌ Add `CommonUI`/`CommonInput` as **hard** `Public`/`Private` deps (breaks DLL load where the plugin is disabled).
- ❌ Use `AddOptionalConditionalModule` (that variant is for data-symbol modules like `MetasoundFrontend`; CommonUI exports functions → delay-load is correct).
- ❌ Touch `PCHUsage=NoPCHs` / `bUseUnity` / `256KB` unity cap — those are the heap/PCH mitigations.
- ❌ Re-add `GameplayTags` (already public dep, `:145`), `UMG`/`UMGEditor` (already private editor deps, `:176`), `Slate`/`SlateCore` (`:161`), or `EnhancedInput` (already optional dynamic) — all present.

**Runtime guard for source-engine edge case (review minor):** even with `MCP_HAS_COMMON_UI=1`, a source engine that never compiled CommonUI lacks the delay-load DLL. Typed handlers should defensively check `LoadClass(...) != nullptr` and return `COMMON_UI_DISABLED` on failure rather than crashing on a missing import.

> ⚠ **Operational:** any `Build.cs`/`.uplugin` change forces a full UBT rebuild, **blocked while Unreal Live Coding is active for the engine install**. Close **all** UE 5.7 editors before rebuilding. You drive the build.

---

## 5. Refactor plan

| ID | File | Action | Risk |
|---|---|---|---|
| **R1** | `WidgetAuthoringHelpers.{h,cpp}` (NEW) | Move the dependency-light helper namespace (`WidgetAuthoringHandlers.cpp:154-705`) out **entirely** (decls in header, defs in new .cpp; keep `CreateAndRegisterWidget<T>` as a header template). Preserve the `WidgetAuthoringHelpers` namespace name. **Verify by successful link (no LNK2005)**, not just "compiles". | Med — duplicate-symbol if move incomplete |
| **R1b** | (separate) | Keep the subsystem-coupled validation helpers (`:713-829`) **out** of the shared header (they pull in the bridge subsystem). Leave in place or a small validation TU. | Low |
| **R2** | `WidgetAuthoringHelpers.cpp` | Add `ResolveWidgetClass` (`/Script/CommonUI.*` + `/Script/UMG.*` + raw path, `LoadClass` not `FindObject`, validate `IsChildOf(UWidget)`). | Low — additive |
| **R3** | `McpAutomationBridgeHelpers.h` | *(Optional, see §6)* widen the **generic** reflection (`ApplyJsonValueToProperty` ~1505 + `ExportPropertyToJsonValue` ~1129, after the `FName` branch ~1140) with an `FText` branch and an `FClassProperty` validation branch **before** `FObjectProperty` (~1700). | Med — header included by 64 TUs; own PR |
| **R4** | `WidgetAuthoringHandlers.cpp` | Route the `add_widget_component` class fallback (~4861-4866) through `ResolveWidgetClass` so the new resolver **replaces** the brittle `FindObject("U"+name)` one rather than adding a 3rd. Add a generic `add_widget` sub-action. | Low |
| **R5** | `UiHandlers.cpp` | *(Deferred, separate cleanup)* converge `create_widget` onto `ResolveWidgetClass` + `RegisterWidgetGuid` + `SafeAddWidgetToTree` (it currently skips GUID registration). Orthogonal to Common UI. | Low-Med |

---

## 6. Generic reflection widening — *optional* (was "Phase 2")

> **Review correction:** `set_style` already round-trips `FText` and `TSubclassOf` via `ImportText_Direct`/`ExportText`. So this is **not** needed to set Common UI labels/styles. It only benefits `set_object_property` and the ~13 other `ApplyJsonValueToProperty` callers.

If pursued (R3): add `FTextProperty` (read+write) and a validating `FClassProperty` branch (with `IsChildOf` metaclass check; the existing `FObjectProperty` path already *serializes* a `_C` class path, so this is validation, not missing support). **Must be its own PR** — `McpAutomationBridgeHelpers.h` is `static inline` and included by **64 TUs** in a `NoPCH`/256KB-unity module, so a change here triggers the exact wide recompile the build config is tuned to avoid, and branch order matters (`FClassProperty` derives from `FObjectProperty`). Gate with an executable round-trip test (§9).

---

## 7. Dual-component sync — the real model

> **Moot as of 2026-06-03 (b1f52dba):** the TypeScript/Node bridge was deleted outright, so the
> bridge is native-HTTP-only and there is no second component to keep in sync. Kept for historical
> context on why the C++-side routing was designed the way it was.

> **Review correction (blocker).** Neither transport hard-rejects an unknown action via an allow-list. The TS `StringEnum`/native `StringEnum` are **advisory schema**, not runtime gates. The failure modes for an unsynced action are *silent mis-routes*, so syncing is mandatory:

**To make a new action reach `HandleCommonUiAction`, edit all of:**
1. **TS** `WIDGET_AUTHORING_ACTIONS`-equivalent routing in `consolidated-tool-definitions.ts` → builds `widgetAuthoringActionSet`. *(Without this, stdio mis-routes to `handleBlueprintTools` → confusing blueprint `UNKNOWN_ACTION`.)* Better: add an explicit `case` in `widget-authoring-handlers.ts` per action.
2. **TS** `manage_blueprint` `inputSchema.properties` for new params (`buttonClass`, `styleClass`, `dataTable`, `rowName`, ...). *(Params drift independently of actions.)*
3. **C++** a dedicated `CommonUi()` action list + `IsCommonUiAction` check in the dispatch lambda (§3.2). *(Without this, even on native HTTP it falls to `HandleBlueprintAction` → blueprint `UNKNOWN_ACTION`; and `MCP_HAS_COMMON_UI=0` can't return `COMMON_UI_DISABLED` because the action never reaches the stub.)*
4. **C++** the live advertised enum is `ManageBlueprint()` = `ManageBlueprintCore()` + `WidgetAuthoring()`, surfaced via `McpTool_ManageBlueprint.cpp`.

> **Dead-file warning:** `McpTool_ManageWidgetAuthoring.cpp`'s `StringEnum` belongs to the **dropped** `manage_widget_authoring` tool and is never advertised. **Do not** edit it or point a parity test at it (the raw synthesis did — corrected here).

**Transport question (open, §11):** if you only use the **native HTTP** transport, the C++ side is authoritative and the TS edits are cosmetic. If you use the **TypeScript stdio bridge** (Claude Code's typical path), TS edits are **mandatory**.

---

## 8. Phased implementation

### Phase 0 — Prove the pipeline (first PR, part 1)
**Goal:** link CommonUI (optional+delay-load) and create a `UCommonActivatableWidget` Widget Blueprint via the existing `create_widget_blueprint` `parentClass` path, then compile. No new handler code.

- **C++:** **mandatory** `/Script/CommonUI.CommonActivatableWidget` `LoadClass` fallback in `create_widget_blueprint`'s parent-class block (~939-948). *(Review correction: the existing `FindFirstObject` only finds already-loaded classes; with delay-load the class isn't loaded, so it silently falls back to `UUserWidget` — a wrong result, not an error. `LoadClass` forces the module to load.)* Also: if a non-default `parentClass` was requested but unresolved, **return an error** instead of defaulting to `UUserWidget`.
- **Verify:** rebuild logs "added optional module CommonUI (delay-load)"; create `WBP_Phase0_CUI parentClass=CommonActivatableWidget` → compile → assert generated class `IsChildOf UCommonActivatableWidget`, compile `BS_UpToDate`, and `FModuleManager::IsModuleLoaded("CommonUI")` is true after resolution.

### Phase 1 — Helper extraction (first PR, part 2)
**Goal:** land R1/R1b/R2 as a behavior-preserving refactor (build wiring already locked in Phase 0, so any failure here is unambiguously the symbol move).
- **Verify:** module links green (no LNK2005), `NoPCH`/unity untouched, re-run Phase 0 test as a regression gate.

### Phase 2 — Typed Common UI handlers (the real feature)
**Goal:** `McpAutomationBridge_CommonUIHandlers.cpp` + `HandleCommonUiAction`, dispatched per §3.2, guarded by `#if MCP_HAS_COMMON_UI` (else returns `COMMON_UI_DISABLED`).

Actions (developer's actual `WBP_MainMenu_CUI` need):
- `add_common_button` — **takes a concrete `buttonClass` `TSubclassOf` (e.g. `/Game/UI/WBP_MenuButton.WBP_MenuButton_C`)**, validates `!HasAnyClassFlags(CLASS_Abstract)`, constructs via `ConstructWidget(UClass*)` (not the `<T>` template), `RegisterWidgetGuid`, `SafeAddWidgetToTree`. *(Review blocker fix.)*
- `add_common_text` — `UCommonTextBlock` is concrete; construct directly; text via inherited `SetText(FText)`.
- `add_common_border` — `UCommonBorder` concrete; construct directly.
- `set_common_button_style` / `set_common_text_style` — resolve style **class** (`/Game/...Style_C` or `/Script/...`) and call the typed `SetStyle(TSubclassOf<...>)` so the slate rebuild fires. Style classes are abstract `UObject`s referenced by class — never instantiate.
- `set_common_button_input_action` — set `FDataTableRowHandle` (DataTable+RowName), validate `RowStruct->IsChildOf(CommonInputActionDataBase)`, call `SetTriggeringInputAction` (note: `SetTriggeredInputAction` also exists — `CommonButtonBase.h:457` vs `:461`; confirm intent).

- **Verify (against a _copy_ of your menu):** first duplicate `WBP_MainMenu_CUI` → `/Game/UI/_McpTest/WBP_MainMenu_CUI_Test` (never mutate the real asset behind your back). Then add a button (class = `WBP_MenuButton`) to the copy, `set_common_button_style=/Game/UI/Styles/BS_MenuButton`, set label `FText`, compile `BS_UpToDate`, `get_widget_info` shows the button + assigned style class. Negative: with `MCP_HAS_COMMON_UI=0`, action returns `COMMON_UI_DISABLED` (still consumed).

### Phase 3 — Activatable presentation (deferred / optional)
`create_activatable_widget` (authoring a `UCommonActivatableWidgetStack` into a tree = normal `ConstructWidget`, fine) and `push_activatable_widget` (**runtime** `BP_AddWidget` — cannot run in the asset-authoring context the bridge operates in). **Defer** — no stack/root-layout asset exists in `Content/UI` yet (§11).

### Phase 4 — Schema parity + convergence (optional)
Parity test (§9), `handler-mapping.md` rows, R4 non-optional, R5 deferred cleanup.

---

## 9. Testing strategy

There is **no native automation-spec framework** in this plugin (`run_tests` is fire-and-forget, returns `started:true`, captures no result). So:

- **Vitest (`npm run test:unit`, CI-runnable, `MOCK_UNREAL_CONNECTION`):** proves **wiring only** — that each new TS case validates params and forwards the right `subAction`/payload. *Not* behavioral validation.
- **Live MCP integration (`tests/mcp-tools/core/manage-commonui.test.mjs`, NEW):** the **sole behavioral gate**. Requires a running editor + bridge (8090/8091) with CommonUI enabled. Clone the widget block in `manage-blueprint.test.mjs:188-323`, `toolName:'manage_blueprint'`. **Must be run with pasted pass output before each PR merges**, not merely authored.
- **For branch-order-sensitive C++ (R3 `FText`/`FClassProperty`):** add a real executable check (temporary `IMPLEMENT_SIMPLE_AUTOMATION_TEST` run from the editor Automation window) — a live round-trip won't localize a branch-order bug.
- Assert on in-memory signals (`existsAfter`, `assetClass`, `exportedValue`, compile `Status`) — `create_widget_blueprint` marks dirty without saving, and `McpSafeAssetSave` returns `saveSucceeded:false` (not error) headless.
- Unique-name creates use **strict `success`** + `assetClass`/`parentClass` assertions; reserve the idempotent `success|already exists` form for cleanup steps only.

### 9.1 End-to-end verification loop
For milestone verification (not tight TDD — editor boot is minutes), the loop is:
1. **Build** — run UBT via the shell with **all UE editors closed** (Live Coding blocks it).
2. **Launch** — start the editor; poll port 3123 for the bridge.
3. **Drive + verify** — the `unreal-mcp` tools reconnect into the session; exercise the new handlers against **disposable assets only** (`/Game/UI/_McpTest/...`, unique names, cleaned up) — never the real `WBP_MainMenu_CUI` unless explicitly authorized per run — then read back via `inspect`/`get_widget_info` and assert.
4. **Report** — pass/fail with the response payloads.

Optional independent verifier: a separate `claude` CLI (the MCP is registered at user scope) can drive the tools without having seen the implementation, reducing "grading my own homework" bias. Overkill for routine work; useful for a final acceptance pass.

---

## 10. Risks

| Risk | Mitigation |
|---|---|
| **Abstract button** — `ConstructWidget<UCommonButtonBase>` returns null | Take concrete `buttonClass TSubclassOf`, validate `!CLASS_Abstract`, use `ConstructWidget(UClass*)`. |
| **Class resolution** of unloaded delay-loaded types | `LoadClass`/`StaticLoadClass` (forces load); error (not silent UUserWidget) on unresolved non-default parent. |
| **Hard dep breaks portability** | Optional + delay-load + `Optional:true` + `MCP_HAS_COMMON_UI` guard. |
| **Style set via raw property doesn't apply** | Typed `SetStyle(TSubclassOf<...>)`. |
| **`helpers.h` widening recompiles 64 TUs** in NoPCH/unity | R3 is its own PR; consider moving reflection fns to a .cpp; gate with full rebuild. |
| **Routing into mega-function** | `IsCommonUiAction` at the dispatch lambda *before* `IsWidgetAuthoringAction`; keep actions out of `WidgetAuthoring()`. |
| **Dual-transport drift / dead StringEnum** | Sync all live mirrors (§7); target parity test at `ManageBlueprint()`/`McpTool_ManageBlueprint.cpp`, never the dead `McpTool_ManageWidgetAuthoring.cpp`. |
| **Live Coding blocks rebuild** | Close all UE 5.7 editors before rebuild; you drive it. |
| **Beta API churn in 5.8** | Acceptable for a deletable bridge; isolate behind one TU. |

---

## 11. Open questions (need your call)

1. ~~**Transport:** Claude Code stdio bridge, or native HTTP `/mcp`?~~ **Moot:** the TypeScript bridge was deleted 2026-06-03 (b1f52dba); native HTTP `/mcp` is the only transport.
2. **Scope:** just leaf authoring (button/text/border + styles on `WBP_MainMenu_CUI`), or also runtime activatable-stack push? The former is fully covered by Phases 0-2; the latter (Phase 3) is runtime-only and has no consuming asset yet.
3. **Style contract:** should `set_common_button_style` require you to pass the style class path (`/Game/UI/Styles/BS_MenuButton`), auto-create a default, or leave null (CommonUI project-settings templates)? A button with no style is effectively invisible.
4. ~~Does `RhyaTowerOfWishes.uproject` enable CommonUI?~~ **Resolved:** yes — `{"Name":"CommonUI","Enabled":true}` is in the committed `.uproject` ([RhyaTowerOfWishes.uproject:38](../../RhyaTowerOfWishes.uproject#L38)). The delay-load DLL will be present at runtime, so Phase 0 is verifiable in this project.
5. **R5 (`UiHandlers` convergence):** in scope for this effort or a separate cleanup?
6. **Action manifest:** adopt a single C++ source-of-truth that generates the TS consts (kills the multi-mirror maintenance), or accept per-phase lockstep + a parity test for this deletable feature?

---

## 12. First PR scope (Phase 0 + Phase 1 only)

Build wiring + behavior-preserving helper extraction, proving the pipeline end-to-end with **zero new Common UI behavior**:

1. `Build.cs`: `AddOptionalDynamicModule(CommonUI)` + `MCP_HAS_COMMON_UI` define (=1 in editor when found, =0 otherwise). **No `CommonUIEditor`.**
2. `McpAutomationBridge.uplugin`: single `{"Name":"CommonUI","Enabled":true,"Optional":true}`.
3. Extract `WidgetAuthoringHelpers` (`:154-705`) → new `.{h,cpp}` (leave subsystem-coupled validation `:713-829` in place); add `ResolveWidgetClass`.
4. **Mandatory** `/Script/CommonUI.*` `LoadClass` fallback + error-on-unresolved in `create_widget_blueprint`'s parent block.
5. NEW `tests/mcp-tools/core/manage-commonui.test.mjs` Phase 0 case + a Vitest forwarding case.

**Acceptance:** module links green (no LNK2005), `NoPCH`/256KB-unity untouched, console logs the delay-loaded CommonUI module, Phase 0 test passes (`assetClass`=`WidgetBlueprintGeneratedClass`, generated class `IsChildOf UCommonActivatableWidget`, compile `BS_UpToDate`, CommonUI module loaded), `WidgetAuthoringHandlers.cpp` shrinks by ~675 lines with no behavior change.

**Explicitly excluded from PR 1:** all `add_common_*`/`set_common_*` actions (Phase 2), the `helpers.h` reflection widening (R3), Phase 3 activatable, and R5. Each lands in its own PR. No commit attribution footers.
