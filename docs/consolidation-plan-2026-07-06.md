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
| `generate_lods` ⚠️**CORRECTED** | BuildEnvironment→`HandleEnvironmentGenerateLODs` (thin wrapper) → **ManageAsset→`HandleGenerateLODs`** (real mesh-reduction impl: FMeshReductionSettings, static-mesh + landscape); ManageGeometry→`HandleGeometryGenerateLODs` = a **DIFFERENT technique** (GeometryScript remesh, active on 5.7) | **NOT a 3-way dup** (original "collapse to Geometry" was WRONG). Env≡Asset are the SAME handler — Env just delegates to `HandleGenerateLODs`; so drop Env's redundant alias (or keep for discoverability), **Asset canonical**. Geometry is a separate GeometryScript LOD op — **do NOT merge** (would swap mesh-reduction for remeshing); leave it, or rename to disambiguate. |
| `create_widget` / `add_widget_child` | SystemControl→`HandleUiCreateWidget`/`HandleUiAddWidgetChild`, **ManageBlueprint→`HandleManageWidgetAuthoringAction`** | ✅ **DONE (ee6e317d)**: removed from system_control (+ dead handlers deleted). ManageBlueprint owns widgets. |
| `screenshot` | SystemControl→`HandleUiScreenshot`, **ControlEditor→`HandleControlEditorScreenshot`** | ✅ **DONE (ee6e317d)**: dropped from system_control; ControlEditor owns it. |
| `show_stats` | SystemControl→`HandlePerfShowStats`, **ControlEditor→`HandleControlEditorShowStats`** | ✅ **DONE (ee6e317d)**: dropped from system_control (which lacked `hide_stats` anyway); ControlEditor owns the show/hide pair. |
| `get_project_settings` | SystemControl→`HandleUiGetProjectSettings`, Inspect→`HandleInspectGetProjectSettings` | Genuine divergent dup (VERIFIED: two different impls — Ui walks GEngine, Inspect its own set — likely return **different data**). Pick one home (Inspect fits "introspection"); reconcile the output sets first, don't blind-alias. |
| `find_by_class` | ControlActor→`HandleControlActorFindByClass`, Inspect→`HandleInspectFindByClass` | + param split: control_actor takes `class`/`className`, inspect only `className`/`classPath`. Pick one home + one param name |
| `find_by_tag` | ControlActor→`HandleControlActorFindByTag`, Inspect→`HandleInspectFindByTag` (both find **actors**); ManageAsset→`HandleFindByTag` (finds **assets** — genuinely separate) | unify the actor pair |
| `build_lighting` | BuildEnvironment→`HandleLightingBuildLighting`, ManageLevel→`HandleLevelBuildLighting` | two bakers for the same op — **Aaron previously said don't merge**; flagged for a conscious call, not silent merge |

### Handler-equivalence verification (2026-07-06, static — read the actual handler bodies)
Before acting on the "canonical home" recommendations, I read each divergent pair's source to
confirm they're actually the same operation. Results:
- **`generate_lods` — recommendation was WRONG** (see ⚠️ row). Env just delegates to Asset's
  `HandleGenerateLODs` (aliases); Geometry is a *different* GeometryScript technique. Not a
  3-way collapse. This is why "verify equivalence before dropping" is in the header — caught one.
- **`get_project_settings`** — two genuinely different impls (Ui via GEngine vs Inspect's own);
  confirm the returned data sets before picking one, or you'll silently drop fields.
- **`find_by_class` / `find_by_tag`** — genuine divergent dups: separate real impls of the same
  actor search (ControlActor's vs Inspect's), so the "unify + pick one param name" plan holds.
- **`create_widget`/`add_widget_child`/`screenshot`/`show_stats`** — shipped (ee6e317d).
Lesson: a shared *action name* with *different handlers* is only a real dup if the handlers do
the same thing — some are thin delegations (alias), some are different techniques (leave alone).

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

1. ✅ **DONE (ee6e317d)** — dropped `system_control` `create_widget`/`add_widget_child`
   (dup of manage_blueprint), `screenshot`, `show_stats` (dups of control_editor), plus the
   now-dead handlers. Golden re-pinned, statics/live all green.
2. **generate_lods** — ⚠️ NOT a collapse (see corrected row + verification note). At most: drop
   `build_environment`'s thin alias of Asset's handler (keep Asset canonical); leave/disambiguate
   `manage_geometry`'s separate GeometryScript op. Small, optional.
3. **find_by_class / find_by_tag** — pick canonical home for the actor pair + unify `class`
   param name (this closes the 2026-07-06 inspect friction find). Genuine dups (verified).
4. **inspect alias family (Category A)** — decide keep-or-drop as one batch (same-handler, safe).
5. **get_project_settings** — reconcile the two output sets first (verified divergent), then pick a
   home. **build_lighting** — Aaron's explicit call (divergent, likely by design).

Remaining items are schema/registration edits + golden re-pin behind the normal gate
(editor closed → Build.bat -NoUBA → relaunch → smoke → golden).
