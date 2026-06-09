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

## Status: deterministic (focus-stabilized) — usable as a gate

1. **Observe assertions** (`inPie`, `stackContains`/`stackDepth`) are deterministic and
   catch real state — e.g. `pause_menu.json` reliably confirms `WBP_PauseScreen_C` becomes
   the active screen.

2. **Drive assertions** (`inputType`, `focusedWidget`) were initially **non-deterministic**
   (back-to-back runs flipped `Gamepad`/`SViewport` ↔ `MouseAndKeyboard`/`SWindow`), because
   the faithful `ProcessKeyDownEvent` nav only routes through CommonUI when the PIE game
   viewport owns Slate focus — which an unattended run doesn't guarantee.

   **Fixed (2026-06-08):** `simulate_nav` now **focus-stabilizes** — before delivering the
   key it focuses the PIE game viewport (`FSlateApplication::SetUserFocusToGameViewport`)
   *iff* focus isn't already inside it, so the faithful pad path routes through CommonUI on
   every run, without stealing focus from an already-correctly-focused in-game widget. Opt
   out with `stabilizeFocus:false`; the nav response reports `focusStabilized`.

   **Fixed (2026-06-09):** root-caused the *other* half of the flakiness — CommonInput
   refuses to reclassify the input method while the editor isn't the OS-foreground app
   (`FCommonInputPreprocessor::IsRelevantInput` gates on `FSlateApplication::IsActive()`),
   so the `inputType: Gamepad` flip silently depended on window foreground state.
   `simulate_nav` now brackets the key delivery with
   `SetHandleDeviceInputWhenApplicationNotActive(true)` (+ restore) and reports
   `slateAppActive` for diagnostics. Both determinism gates (focus path + app-active) are
   now satisfied unconditionally. Verified: 3 back-to-back runs of `pause_menu.json` are
   byte-identical (`3 pass, 1 fail` — the fail is a confirmed product bug, see below).
   Usable as a CI gate now. Full RCA: `COMMONUI_FOCUS_INPUT.md` (2026-06-09 section).

(The faithful drive remains the default and is also used for interactive verification — see
`COMMONUI_FOCUS_INPUT.md`. A deterministic `mode:"slate"` (`NavigateFromWidget`) alternative
was considered and not needed once focus-stabilize made the faithful path repeatable.)

## Specs
- `pause_menu.json` — opens the pause menu via `TogglePause`, confirms it's the active
  screen, drives one gamepad nav, and asserts focus sits on a volume row's inner
  AnalogSlider (`focusedName VolumeSlider`). **All-green since 2026-06-09** (3 byte-identical
  `4 pass, 0 fail` runs): the confirmed product bug (DesiredFocusWidget targeting a
  non-focusable `WBP_VolumeSlider` wrapper → silent `SetFocus` no-op) was fixed asset-only by
  making the wrapper focusable with its own `DesiredFocusWidget = VolumeSlider`, so the
  engine's `UUserWidget` focus relay (`NativeOnFocusReceived`, UserWidget.cpp:2414-2424)
  forwards the router's desired-target focus to the real slider. Verified beyond the spec:
  DPad Down moves focus row→row through the wrappers, and DPad Left/Right adjusts only the
  focused row's value. Full RCA + fix history: `COMMONUI_FOCUS_INPUT.md` (2026-06-09 section).
