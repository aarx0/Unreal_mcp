# CommonUI focus/nav regression specs

Declarative golden-path tests for CommonUI focus + gamepad navigation, run over the
MCP bridge by [`scripts/ui-nav-test.ps1`](../../scripts/ui-nav-test.ps1) — no engine
rebuild (they compose the existing `inspect ui_focus`, `control_editor simulate_nav`,
and `control_actor call_actor_function` actions). Background + design:
[`COMMONUI_FOCUS_INPUT.md`](../../COMMONUI_FOCUS_INPUT.md).

Run (editor + bridge must be up):
```
pwsh -File scripts/ui-nav-test.ps1 -Spec tests/ui-nav/pause_menu.json
```
Exits non-zero if any assertion fails. Spec format is documented in the runner's header.

## Status: SPIKE — read before trusting results

This is a working proof of concept, **not yet a reliable CI gate.** It established two
things:

1. **Observe assertions are stable & useful.** `inPie`, `stackContains`/`stackDepth`
   (the active activatable stack) are deterministic and already catch real state — e.g.
   `pause_menu.json` reliably confirms `WBP_PauseScreen_C` becomes the active screen.

2. **Drive-dependent assertions are currently NON-DETERMINISTIC.** Across back-to-back
   runs of the *same* spec, `inputType` (M&K↔Gamepad) and `focusedWidget` flip:
   - run A: `inputType=Gamepad`, `focusedWidget=SViewport`
   - run B: `inputType=MouseAndKeyboard`, `focusedWidget=SWindow`

   Root cause: a *faithful* synthesized nav (`FSlateApplication::ProcessKeyDownEvent`,
   the Decision-C choice) only flips CommonUI's input mode + moves focus when the **PIE
   game viewport actually owns Slate/OS focus** — which an automated run doesn't
   guarantee (run B's `SWindow` focus = the editor window had focus, not the game
   viewport). So focus/input assertions flake.

### What this means for the regression layer
The drive needs to be made **deterministic for tests** before focus/nav assertions can
gate CI. Options (a decision, not yet made):
- **Focus-stabilize first:** before each nav, force the game viewport to own focus
  (engine has `SetUserFocusToGameViewport` / `SlateOperations.SetUserFocus(viewport)` —
  would need a small bridge step). Keeps the faithful drive.
- **Deterministic drive mode:** add `simulate_nav { mode: "slate" }` using
  `FSlateApplication::NavigateFromWidget` (the Decision-C alternative) for tests — stable,
  but bypasses CommonUI's action-router routing, so it tests Slate nav, not the full pad path.
- Keep faithful `simulate_nav` for interactive verification (where it works well — see
  `COMMONUI_FOCUS_INPUT.md`), and use one of the above for recorded regression.

Until that lands, treat the drive assertions here as informational, not a gate.

## Specs
- `pause_menu.json` — opens the pause menu via `TogglePause`, confirms it's the active
  screen, drives one gamepad nav, and asserts focus was grabbed. The
  `focusedWidgetNot SViewport` assertion is **expected to fail today**: `WBP_PauseScreen`'s
  CommonUI conversion is in progress and its `DesiredFocusWidget` doesn't resolve, so
  CommonUI falls back to focusing the viewport (`UIActionRouterTypes.cpp:1684`). Flip it to
  a `focusedName` assertion once the menu grabs focus.
