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

---

# Deep dive (2026-06-07) — grounded design + recommendation

This section supersedes the "Open design questions" above with concrete answers,
grounded in the project's actual menus (inspected live via the bridge).

## Grounding — the real menus
Both target menus derive **directly from the engine `UCommonActivatableWidget`** (no
project subclass), with CommonUI-aware interactive children:
- **`WBP_PauseScreen`** (parent `CommonActivatableWidget`): root `CanvasPanel` → BG image,
  title text, 3× `WBP_VolumeSlider`.
- **`WBP_OptionsScreen`** (parent `CommonActivatableWidget`): root `CanvasPanel` → `VerticalBox`
  of rows; interactive children are `WBP_VolumeSlider`, `AnalogSlider`, and `WBP_MenuButton`
  (a CommonButton) for Back.

Two implications:
1. Children are already CommonUI buttons/sliders, so CommonUI's action router handles most
   D-pad movement **once the screen activates and focus lands on a starting widget**. The
   missing authoring piece is almost entirely the **desired-focus target**.
2. There is **no project `UCommonActivatableWidget` subclass** to hang a `DesiredFocus`
   property on — the pivotal fact for the approach choice below.

## The decision (desired-focus target)
- **B1 — project base class (recommended; needs the team).** Add a tiny
  `UGOSActivatableWidget : UCommonActivatableWidget` with
  `UPROPERTY(EditAnywhere) FName DesiredFocusWidget;` and override
  `NativeGetDesiredFocusTarget()` → `GetWidgetFromName(DesiredFocusWidget)`. Then (a) reparent
  each menu to it — the bridge already has `set_widget_parent_class`; (b) the bridge sets the
  property. Turns the hard case into "reparent + set a property," reused across all screens.
  Cost: the base class lives in the **game module** (RhyaTowerOfWishes) — the team adds it; the
  bridge/fork can't (and shouldn't) touch game source.
- **B2 — bridge generates the `BP_GetDesiredFocusTarget` override (bridge-only).** A new action
  authors the BP function override inside the WidgetBlueprint (function graph: `GetWidgetFromName`
  → return node). No game code; works on the existing engine-based widgets as-is. Cost: BP
  function-graph generation is the fiddliest bridge work here, and it's one override per widget.

**Recommendation:** B1 if the team will add the one base class (cleanest, shared by all screens);
otherwise B2 as the bridge-owned fallback. The bridge's own work is small either way:
B1 = `set_widget_parent_class` + set-property (both exist/trivial); B2 = one graph-authoring action.

## Activation flags (the easy half) — already doable today
`bAutoActivate` / `bIsModal` / `bSupportsActivationFocus` / `bAutoRestoreFocus` are UPROPERTYs →
settable now via `inspect set_property` on the widget CDO. Action: verify the exact set_property
call sticks on a WidgetBlueprint CDO and document it; a `set_activatable_flags` convenience is
optional sugar, not required.

## Nav rules (D-pad order) — follow-up only if needed
Per-widget `Navigation` (`FWidgetNavigation`: Up/Down/Left/Right = Escape/Wrap/Explicit) sets
movement order. CommonUI auto-nav covers simple layouts, so this is only for custom order. Open:
can `inspect set_property` write the nested `Navigation` struct, or is a dedicated
`set_widget_navigation` action needed? Test before building.

## First implementation slice (when we build)
1. **Check first:** do the menus already override `BP_GetDesiredFocusTarget`? If so, focus may
   already work and the gap is elsewhere — don't build anything until this is confirmed.
2. Decide B1 vs B2 (the team-base-class question).
3. Add `set_desired_focus {widgetPath, focusWidgetName}` — sets the base-class property (B1) or
   authors the BP override (B2).
4. Verify in PIE with a gamepad: activate Pause → focus lands on the first slider/button → D-pad
   moves between them.

---

# RESOLVED (2026-06-07) — no base class, no new bridge action needed

The B1/B2 analysis above is **obsolete**: the engine already provides exactly this, and the
bridge can already author it. (Discovered while compiling a draft `UGOSActivatableWidget` C++ base
— UHT errored that `DesiredFocusWidget` "is already defined in scope `UUserWidget`". It is.)

- Every `UUserWidget` has `UPROPERTY(EditDefaultsOnly) FWidgetChild DesiredFocusWidget;` plus
  `SetDesiredFocusWidget(FName)` / `GetDesiredFocusWidget()`.
- `UCommonActivatableWidget::NativeGetDesiredFocusTarget()` already **falls back to
  `GetDesiredFocusWidget()`** when no BP override is present (see
  `CommonActivatableWidget.cpp` ~L95-103). The project's menus derive from `CommonActivatableWidget`
  and do **not** override `BP_GetDesiredFocusTarget` (confirmed via `get_graph_details` on
  `WBP_PauseScreen`'s EventGraph), so the fallback path applies.

**So the whole feature is one existing bridge call per menu — no game C++, no reparent, no new action:**

```
manage_blueprint set_default {
  blueprintPath: "/Game/UI/WBP_PauseScreen",
  propertyName:  "DesiredFocusWidget.WidgetName",   // nested path into the FWidgetChild
  value:         "Master_Volume_Slider"             // the child to focus on activate
}
```

`blueprint_set_default` resolves the nested struct path, sets it on the generated-class CDO,
recompiles, and saves. **Verified live 2026-06-07** on a throwaway `CommonActivatableWidget`-based
WBP: `set_default DesiredFocusWidget.WidgetName=TestBtn` → response echoes `value:"TestBtn"`,
persisted to the asset.

Remaining to actually ship on the real menus:
1. Run the `set_default` call on each real menu (`WBP_PauseScreen`, `WBP_OptionsScreen`) with the
   desired starting widget. (Touches Aaron's uncommitted menu assets — do on his go.)
2. PIE gamepad check: open the menu, confirm focus lands on the chosen widget and the D-pad moves
   between the CommonButtons/sliders (CommonUI's action router handles the movement).

Nav-rules (custom D-pad order) remains the only possible follow-up, and only if auto-nav order
isn't good enough.
