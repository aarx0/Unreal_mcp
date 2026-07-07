# MCP bridge — open work index

Single source for what's open, so we stop re-deriving it. Grouped by what each item needs.
Detailed analysis lives in the linked docs; the fork `TODO.md` holds the granular dogfood finds.

Legend: ✅ shipped · 🟢 decided, ready to run · 🟡 needs an Aaron decision · 🔵 smaller/older

## ✅ Shipped (this session)
- **schema-from-decls Phase 2** — all 22 tools single-source schema+validation (`b1c73a20`).
- **Dead-code removal** — 31 unreachable handlers, −4,638 lines (`f395330b`).
- **system_control duplicate drops** — create_widget/add_widget_child/screenshot/show_stats (`ee6e317d`).

## 🟢 Ready to run (decided)
- **Rip scaffolds + no-ops** — ~40 var-scaffolds + ~15 silent no-ops. Keep the two real
  component-builders (`setup_hitbox_component`, `setup_attachment_system`). Closes the older
  TODO items for interaction's 5 scaffolds + networking's 11 no-ops. Evidence:
  [`compound-action-triage-2026-07-06.md`](compound-action-triage-2026-07-06.md).
  **Blocking Q:** scope = both (~55) or scaffolds-only (~40)?

## 🟡 Handler-side sequence (the three you set)
- **① dead code** — done (above).
- **② silent-success → all-or-nothing/transactional** — pick guard model (hard error vs soft
  warning) + rollout order; apply to the ~27 cohesive + real-write actions.
  [`silent-success-convention-2026-07-06.md`](silent-success-convention-2026-07-06.md).
- **③ delete legacy dispatchers** — manage_blueprint's recursive `HandleBlueprintAction` +
  niagara/material/sequencer authoring subsystems. Biggest refactor; awaiting appetite.

## 🟡 Consolidation (cross-tool duplicates)
Plan: [`consolidation-plan-2026-07-06.md`](consolidation-plan-2026-07-06.md).
- **find_by_class / find_by_tag** (control_actor ↔ inspect) — pick home + unify `class`/`className`.
- **inspect alias family** (get_components/get_component_property/add_tag/…) — keep dual surface or drop.
- **get_project_settings** (inspect ↔ system_control) — reconcile output sets, pick one home.
- **generate_lods** — drop build_environment's alias of Asset; leave geometry (different technique).
- **build_lighting** (BuildEnv ↔ ManageLevel) — conscious keep-or-merge call.
- **system_control still overloaded** — testing/rendering-config/build buckets could move to real homes.

## 🟡 Naming / Phase 3
- **Confusing-names pass** — `class` vs `className` split + siblings (taste renames).
- **Phase 3 per-action schemas (`oneOf`)** — [`phase3-per-action-schema-design-2026-07-06.md`](phase3-per-action-schema-design-2026-07-06.md);
  `control_editor` pilot needs a fresh harness session to see if the client honors top-level oneOf.

## 🔵 Smaller / older
- **manage_blueprint `create_blueprint` ignores `path`** — sprays to /Game root (TODO 2026-07-03).
- **animation_physics `setup_ragdoll`/`activate_ragdoll`** — full impls exist unwired; wire or retire.
- **`execute_python` IMC/BP-default authoring traps** — documented gotchas (TODO).
- **Clone sync** — `c:/GitHub/Unreal_mcp` behind the fork (reset declined once).
