# Common UI — Gamepad Focus / Navigation (design stub)

**Status:** parked for a future deep-dive design pass. Not yet built. Captures the
2026-06-07 scoping so we don't re-derive it. Pairs with `COMMONUI_INTEGRATION_PLAN.md`.

## Why
The active UI goal is migrating menus (Pause, Options) to Common UI **with controller
support**. The gamepad blocker is focus/navigation: when a menu screen activates, *some*
widget must receive focus, and the D-pad/stick must move focus predictably between widgets.
Today the bridge can build the widgets and styles but does nothing for focus/nav.

## What the engine exposes (UE 5.7 `UCommonActivatableWidget`)
Two very different halves — this split is the whole point of the design.

### 1. Activation flags — plain `UPROPERTY`s (already reachable today)
`CommonActivatableWidget.h`:
- `bAutoActivate` (default false) — activate on construct
- `bSupportsActivationFocus` (default true) — activation manages focus at all
- `bIsModal` (default false, EditCondition bSupportsActivationFocus) — trap focus within
- `bAutoRestoreFocus` (EditCondition bSupportsActivationFocus) — restore prior focus on reactivate

These are editable properties, so they are **already settable via the generic
`inspect set_property`** on the widget CDO. A dedicated bridge action would only be sugar.
→ Verify the exact `set_property` invocation that sticks these on a WidgetBlueprint CDO and
document it; probably no new action needed.

### 2. The desired-focus *target* — NOT a property (the real gap)
Which widget actually gets focus on activate comes from:
- `GetDesiredFocusTarget()` (public) → `NativeGetDesiredFocusTarget()` (C++ virtual)
  → `BP_GetDesiredFocusTarget()` (BlueprintImplementableEvent).

There is **no `DesiredFocusWidget` UPROPERTY** to set. Authoring this means generating a
Blueprint function override (`BP_GetDesiredFocusTarget`) whose graph returns a specific
child widget. That's real graph authoring, not a property poke — the heavy part.

## Open design questions (for the deep dive)
1. **Desired-focus authoring approach** — pick one:
   - (a) Generate the `BP_GetDesiredFocusTarget` override graph (return a named child widget).
     Most faithful; needs reliable BP-function-graph authoring (return node + variable get).
   - (b) Project-side base class: a `UCommonActivatableWidget` subclass in C++/BP with a
     `UPROPERTY` `DesiredFocusWidgetName`/`TObjectPtr<UWidget> DesiredFocus` that the base's
     `NativeGetDesiredFocusTarget()` resolves. Then the bridge just sets a property (trivial).
     **Likely the better answer** — turns the hard case into the easy case. Decide who owns
     adding that base class (combat/UI team) vs. the bridge.
3. **Per-widget navigation rules** — `UWidget` nav (`SetNavigationRuleExplicit`/`Escape`/`Wrap`,
   `Navigation` object) for D-pad movement order. Are these reachable via `set_property`, or do
   they need a dedicated `set_widget_navigation` action? (FNavigationData is nested/structy.)
4. **Focus groups / analog cursor** — `UCommonUIActionRouterBase`, activation root scoping.
   Mostly runtime; probably out of scope for authoring.
5. **Back / cancel handling** — `bIsBackHandler`, back action binding (CommonUI input actions).
   Adjacent to `set_common_button_input_action`; could fold in.

## Candidate bridge actions (if we build it)
- `set_activatable_flags` — sugar over set_property for the 4 activation flags (optional).
- `set_desired_focus` — depends on Q1: either author the BP override (a) or set the base-class
  property (b).
- `set_widget_navigation` — explicit nav rules per widget (Q3), if set_property can't.

## What's already covered (don't rebuild)
- Activation flags → `inspect set_property` (verify + document).
- Reparenting a WBP to `CommonActivatableWidget` → `set_widget_parent_class` (exists).
- Button input-action binding → `set_common_button_input_action` (exists).
- Style assets → `create_common_button_style` / `create_common_text_style` (done 2026-06-07).

## References
- `Engine/Plugins/Runtime/CommonUI/Source/CommonUI/Public/CommonActivatableWidget.h`
- `COMMONUI_INTEGRATION_PLAN.md` (broader Common UI integration notes)
