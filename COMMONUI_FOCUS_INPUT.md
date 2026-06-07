# CommonUI Focus / Input Introspection + Drive

**Status:** ✅ BUILT 2026-06-07 (faithful drive, per Aaron's call). Pairs with
`COMMONUI_FOCUS_NAV.md` (authoring side). Implementation:
`McpAutomationBridge_FocusInputHandlers.cpp`. The design discussion below is kept for
context; the **As-built** section records what actually shipped + the API corrections.

## As-built (2026-06-07)
Two actions, both in one self-contained TU (`McpAutomationBridge_FocusInputHandlers.cpp`):
- **`inspect ui_focus`** (observe) — `HandleInspectUiFocus`. Returns a snapshot:
  `inPie`, `focusedWidget` {slateType, name?, class?}, `focusPath` (root→leaf), and
  (in PIE w/ CommonUI) `inputType`, `currentGamepad`, `activeActivatable`,
  `desiredFocusTarget`, `activatableStack`, `boundActions`. Wired as an early delegate in
  `HandleInspectAction` (global, no objectPath).
- **`control_editor simulate_nav`** (drive, faithful) — `HandleControlEditorSimulateNav`.
  `direction` (Up/Down/Left/Right/Accept/Back/Next/Previous) + `device` (gamepad default /
  keyboard) or explicit `key`; delivers the nav key via `FSlateApplication::ProcessKeyDownEvent`
  (faithful — runs CommonUI's input preprocessor + nav config like a real pad), then returns
  the post-nav snapshot + `focusChanged`. Wired next to `simulate_input` in `HandleControlEditorAction`.

### API corrections vs the design below (verified against UE 5.7 headers)
- ❌ `UCommonUIActionRouterBase::DebugDumpRootList` is **NOT public** — it lives on a private
  nested struct (`FActionDomainSortedRootList`). Replaced with public APIs:
  **`FindActivatable(SWidget, LocalPlayer)`** (activatable owning focus) + **`GatherActiveBindings()`**
  (bound actions) + a `TObjectIterator<UCommonActivatableWidget>` scan filtered to the PIE world
  (active stack). Zero private coupling.
- ⚠️ **Build fix:** `UCommonInputSubsystem` is in the **CommonInput** module, a *sibling* of
  CommonUI. CommonUI's public dependency on it does NOT re-export its symbols, so the link failed
  with unresolved `COMMONINPUT_API` externals until `CommonInput` was added to `Build.cs`
  (delay-load, gated on `bHasCommonUI`).

### Verified live (2026-06-07, rebuild + PIE)
- Compiles + links clean. Both actions reachable over MCP, return clean JSON, **no crashes**.
- `ui_focus` outside PIE: `inPie:false` + correct Slate focus path (SWindow→…→SLevelViewport).
- `ui_focus` in PIE: `inPie:true`, **`inputType`/`currentGamepad` populated** (proves the
  CommonInput runtime linkage), **`activatableStack` shows the live `WBP_MainMenu_CUI_C`**
  (active-screen detection works).
- `simulate_nav` outside PIE → clean `NOT_IN_PIE`; in PIE → resolves direction→key, delivers it
  (`handled:true`), returns the post-nav snapshot.
- **OPEN (the design's Decision-C spike, for Aaron's gamepad test):** focus *movement* in a real
  **menu** is not yet demonstrated. In M&K input mode CommonUI drives selection by mouse hover (no
  Slate keyboard focus → `focusedWidget` is the viewport, not a button). And this project's PIE
  starts in **gameplay** (main menu blends to gameplay on Play), so a synthesized gamepad DPad is
  consumed by gameplay, not menu nav — `inputType` did not flip to Gamepad from a synthesized event.
  → To confirm the drive in a real menu (pause/options, gamepad UI-input mode), Aaron's gamepad
  feel-test is the check. Possible refinement if synthesized events don't register as a real pad:
  inject via a valid `FInputDeviceId`, or read the game viewport's local-player focus rather than
  Slate user 0. **Bridge-ops lesson learned:** never force-kill the editor (next launch hangs on the
  unclean-shutdown modal); quit via `control_editor console_command QUIT_EDITOR` for a clean restart.

## The problem
CommonUI's hardest-to-eyeball state is **not** the widget tree (a screenshot or
`get_widget_info` covers that). It's the **runtime focus + input-routing state**:
- which widget currently owns focus, and the focus path up to the root,
- the current input type (Gamepad / Mouse&Keyboard / Touch),
- what the **activatable widget stack** actually contains (which screens are active),
- whether gamepad nav actually routes through each focusable element.

That state is invisible over MCP. A screenshot shows what's *drawn*, not who owns focus
or why the stick does nothing. This is where CommonUI eats hours, and it's exactly what
blocked auto-testing the `DesiredFocusWidget` we just set on the menus. The fix is the
CommonUI-specific **observe/verify loop**: an introspection call + a nav-drive call, so an
agent can "push screen → ask who has focus → press Down → ask again" instead of guessing.
No generic Unreal MCP has this.

## Two tools

### 1. `inspect ui_focus` — observe (read-only, PIE-only)
Returns one structured snapshot:
```
{
  inPie: true,
  inputType: "Gamepad" | "MouseAndKeyboard" | "Touch",
  focusedWidget: { name, class, path: [ancestor names root→leaf] },
  desiredFocusTarget: { name, class } | null,   // of the active activatable widget
  activatableStack: [ { name, class, isActive, bIsModal } ... ],  // top→bottom
  boundActions: [ ... ]                          // input actions bound at the active root
}
```

### 2. `simulate_nav` — drive (PIE-only)
`simulate_nav { direction: Up|Down|Left|Right|Next|Previous|Accept|Back }` → performs one
navigation step, then returns the **new** focus (same shape as `ui_focus.focusedWidget`) so
the caller can diff. Loopable: the agent presses a direction and sees where focus landed.

## Architecture (UE 5.7 APIs — grounded)

### Observe
- **Focused widget:** `FSlateApplication::Get().GetUserFocusedWidget(0)` → `SWidget`.
  Walk `GetParentWidget()` for the path.
  - ⚠️ **Decision A — SWidget→UMG name mapping.** Slate widgets don't natively carry the
    UMG widget name. Options: (a) `FReflectionMetaData::GetWidgetReflectionMetaData` (debug
    metadata; present in non-shipping builds — fine for an editor tool); (b) cast to
    `SObjectWidget` and read the owning `UWidget`; (c) match the SWidget against the active
    `UWidgetTree`'s cached widgets. Lean (a)/(b); pick the robust one in the spike.
- **Active activatable stack + bound actions:** `UCommonUIActionRouterBase::Get(ContextWidget)`
  then `DebugDumpRootList(OutStr, /*actions*/true, /*children*/true, /*inactive*/false)`.
  This is a **public** method that dumps the activation root list (widgets + bound actions) —
  so we avoid coupling to the private `FActivatableTreeRoot` type. We can either return the
  dump text or parse it into the structured `activatableStack`/`boundActions`.
  - ⚠️ **Decision B — getting the router.** `Get()` needs a `UWidget&` context. Use the
    focused UMG widget if we have one, else any widget in the active PIE world (or resolve the
    local player's router). Settle the cleanest acquisition in the spike.
- **Desired-focus-target:** from the active `UCommonActivatableWidget` (top of stack / focus
  walk), call `GetDesiredFocusTarget()` (public).
- **Input type:** `UCommonInputSubsystem::GetCurrentInputType()` → `ECommonInputType`
  (confirm exact symbol/header in the spike).
- **OS input mode (Game/Menu/All):** no clean getter — omit or best-effort; not worth forcing.

### Drive
- ⚠️ **Decision C — fidelity vs determinism (the important one).**
  - **Faithful:** `FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(navKey, ...))` with
    the actual gamepad nav key (DPad/virtual-nav) + user index 0. Routes through CommonUI's
    action router + nav config **exactly like a real pad** — which is the whole point ("does
    the gamepad actually route?"). Cost: construct a correct `FKeyEvent`/`FAnalogInputEvent`.
  - **Deterministic:** `FSlateApplication::Get().NavigateFromWidget(0, focusedWidget, {EUINavigation::Down})`
    — direct Slate navigation. Simpler/predictable but **bypasses** CommonUI's routing, so it
    won't catch action-router/analog-cursor issues.
  - **Lean: `ProcessKeyDownEvent` (faithful) as the primary**, since fidelity is the value;
    keep `NavigateFromWidget` as an optional `mode:"slate"` for deterministic debugging.
- After the event, re-query `GetUserFocusedWidget(0)` and return the new focus.

## Clean integration (the part to get right)
- **Taxonomy:** read → `inspect ui_focus` (it's introspection); drive → `simulate_nav` on
  `control_actor`/`control_editor` (next to `simulate_input`). Keeps observe/drive split,
  consistent with existing tools.
- **PIE-only:** both error clearly (`NOT_IN_PIE`) when not playing — runtime-only state.
- **One snapshot primitive:** `ui_focus` returns the whole picture in one call (focus + stack
  + desired + input type), so the agent observes atomically.
- **No private coupling:** prefer `DebugDumpRootList` (public) over reaching into
  `FActivatableTreeRoot` (private header). If structured output is wanted, parse the dump in
  the handler rather than linking private types.
- **Module deps:** bridge already pulls CommonUI (delay-load) for the CommonUI authoring
  handlers, and Slate/SlateCore/UMG for widget authoring — so no new hard deps expected
  (confirm CommonInput for the input-type call).

## MVP slicing
1. **`inspect ui_focus` (observe only).** Focus path + `DebugDumpRootList` + desired target +
   input type. Cheap, immediately useful — turns "eyeball + confusion" into "ask who owns
   focus," and would let the agent verify the menu focus we set today.
2. **`simulate_nav` + focus read-back.** The full drive loop. Spike Decision C first.

## Open questions for discussion
- A: which SWidget→UMG name mapping is most robust in an `-unattended` editor PIE?
- B: cleanest way to acquire the action router (focused widget vs local player)?
- C: confirm `ProcessKeyDownEvent` actually routes through CommonUI in PIE (vs needing the
  viewport to have focus / a specific user index) — this is the spike's main risk.
- Output: return the raw `DebugDumpRootList` text, structured JSON, or both?
- Scope: is observe-only (slice 1) enough to unblock your menu work, or do we want the drive
  loop now?
