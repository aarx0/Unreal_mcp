# Cross-tool consolidation plan (2026-07-06)

Evidence base for the consolidation/confusing-names pass. Built from a **static handler map**:
every action name that appears on more than one tool, resolved to the C++ handler each tool
dispatches to. No editor, no runtime — pure source analysis of `Private/MCP/Calls/*.cpp`.

The key distinction: **same handler** across tools = a benign convenience alias; **different
handlers** = either legitimate domain polymorphism (same verb, different noun) or real divergent
duplication (same operation implemented twice). Only the last category is a problem.

---

## A. Benign aliases (same handler, two names) — low priority

These are one implementation exposed under two tool surfaces. Harmless, but redundant.

| Action | Tools | Shared handler |
|---|---|---|
| `add_tag` | Inspect, ControlActor | `HandleControlActorAddTag` |
| `create_snapshot` | Inspect, ControlActor | `HandleControlActorCreateSnapshot` |
| `get_components` | Inspect, ControlActor | `HandleControlActorGetComponents` |
| `get_component_property` | Inspect, ControlActor | `HandleControlActorGetComponentProperty` |
| `set_component_property` | Inspect, ControlActor | `HandleControlActorSetComponentProperties` |
| `save_all` | ManageAsset, ControlEditor | `HandleControlEditorSaveAll` |

**Pattern:** `inspect` is a thin alias surface over `control_actor`'s actor family (also
`restore_snapshot`, `export`, `delete_object`, `get_bounding_box`, `get_metadata` per the
2026-07-06 inspect find). Decide once: keep the dual surface by design (inspect = read-oriented
convenience) or make `control_actor` canonical and drop the inspect copies. Not urgent — no
divergence risk since it's literally the same function.

## B. Real divergent duplicates (same operation, different handlers) — the consolidation targets

Each of these is the *same* operation implemented twice (or thrice), so behavior can silently
drift between tools. Recommended canonical home in **bold**; verify equivalence before dropping.

| Action | Tools → handlers | Canonical? |
|---|---|---|
| `generate_lods` | BuildEnvironment→`HandleEnvironmentGenerateLODs`, ManageAsset→`HandleGenerateLODs`, **ManageGeometry→`HandleGeometryGenerateLODs`** | ManageGeometry (LODs are a geometry op); alias/drop the other two |
| `create_widget` / `add_widget_child` | SystemControl→`HandleUiCreateWidget`/`HandleUiAddWidgetChild`, **ManageBlueprint→`HandleManageWidgetAuthoringAction`** | ManageBlueprint owns widgets (dozens of typed actions); **remove** from system_control |
| `screenshot` | SystemControl→`HandleUiScreenshot`, **ControlEditor→`HandleControlEditorScreenshot`** | ControlEditor (viewport control); drop/alias system_control's |
| `show_stats` | SystemControl→`HandlePerfShowStats`, **ControlEditor→`HandleControlEditorShowStats`** | ControlEditor (it also has `hide_stats`; system_control does **not** — see asymmetry below) |
| `get_project_settings` | SystemControl→`HandleUiGetProjectSettings`, Inspect→`HandleInspectGetProjectSettings` | pick one (Inspect fits "introspection"); alias the other |
| `find_by_class` | ControlActor→`HandleControlActorFindByClass`, Inspect→`HandleInspectFindByClass` | + param split: control_actor takes `class`/`className`, inspect only `className`/`classPath`. Pick one home + one param name |
| `find_by_tag` | ControlActor→`HandleControlActorFindByTag`, Inspect→`HandleInspectFindByTag` (both find **actors**); ManageAsset→`HandleFindByTag` (finds **assets** — genuinely separate) | unify the actor pair |
| `build_lighting` | BuildEnvironment→`HandleLightingBuildLighting`, ManageLevel→`HandleLevelBuildLighting` | two bakers for the same op — **Aaron previously said don't merge**; flagged for a conscious call, not silent merge |

### Structural picture — `system_control` is shedding responsibilities to better homes
Three of the Category-B rows point the same way: `system_control` accreted actions that already
have proper owners — **widgets → manage_blueprint**, **screenshot → control_editor**,
**show_stats → control_editor**. Combined with the 2026-07-06 "system_control overload" entry,
the clean move is to **slim system_control to its coherent core** (execute_command/python,
set_cvar, get/set_project_setting, logging, profiling, build/UBT) and drop/relocate the rest.

### Asymmetry bug (not just taste)
`system_control` has `show_stats` but **no `hide_stats`** (only `control_editor` has hide). So via
system_control you can turn the stat overlay on and not off. If system_control keeps stats at all,
it needs `hide_stats`; better, drop `show_stats` and let control_editor own the pair.

## C. Legitimate domain polymorphism (same verb, different noun) — leave alone

Different handlers because they operate on genuinely different object types. This is normal and
should **not** be "consolidated" — merging them would be worse. Listed so the pass doesn't waste
time relitigating them:

`create`, `delete`, `duplicate`, `list`, `rename`, `save`, `get_metadata`, `set_metadata`,
`get_info` (×10 gameplay tools, each its own domain), `add_component` (actor vs BP), `add_node`
(behavior-tree vs BP graph), `pause`/`play`/`stop` (PIE vs sequence), `connect_nodes` (BT vs
material), `delete_node` (material vs BP graph), `arrange_graph` (material vs BP graph), `cleanup`
(anim artifacts vs effect filter), `set_visibility` (actor vs widget), `get_animation_info`
(skeletal montage vs UMG widget animation).

## Suggested execution order (tonight — each needs a rebuild, so batch)

1. **Zero-risk drops first** — remove `system_control` `create_widget`/`add_widget_child`
   (dup of manage_blueprint), `screenshot`, `show_stats` (dups of control_editor). Schema change
   → golden regen. Biggest surface-area cleanup, lowest behavioral risk.
2. **generate_lods** — collapse to ManageGeometry after confirming the three handlers agree.
3. **find_by_class / find_by_tag** — pick canonical home for the actor pair + unify `class`
   param name (this closes the 2026-07-06 inspect friction find).
4. **inspect alias family (Category A)** — decide keep-or-drop as one batch (same-handler, safe).
5. **build_lighting, get_project_settings** — Aaron's explicit call each (divergent, some by design).

All of the above are schema/registration edits + golden re-pin behind the normal gate
(editor closed → Build.bat -NoUBA → relaunch → smoke → golden). None started.
