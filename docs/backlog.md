# MCP bridge — open work index

Single source for what's open, so we stop re-deriving it. Grouped by what each item needs.
Detailed analysis lives in the linked docs; the fork `TODO.md` holds the granular dogfood finds.

Legend: ✅ shipped · 🟢 decided, ready to run · 🟡 needs an Aaron decision · 🔵 smaller/older

## ✅ Shipped (this session)
- **schema-from-decls Phase 2** — all 22 tools single-source schema+validation (`b1c73a20`).
- **Dead-code removal** — 31 unreachable handlers, −4,638 lines (`f395330b`).
- **system_control duplicate drops** — create_widget/add_widget_child/screenshot/show_stats (`ee6e317d`).
- **Scaffold + no-op rip** — 55 actions removed (~5k lines), `7c6947df`. Kept the two real
  component-builders. Closed interaction's 5 scaffolds + networking's 11 no-ops.
- **Silent-success convention set (no rollback)** — `set_transform` + `set_component_property`
  reworked; `set_actor_collision`/`set_visibility` already align (`844cd076` + earlier). See ②.

## 🟡 Handler-side sequence (the three you set)
- **① dead code** — done (above).
- **② silent-success — CONVENTION: fail-in-place, NO rollback.** Aaron rejected transactional
  rollback (`Cancel()`) as unverifiable per-setter — a reported "rolled back" can leave live
  side-effect state (SetMobility/SetSimulatePhysics). House rule: writes that land **stay**; on
  any failure, error with `applied`+`failed` (`PROPERTY_WRITE_FAILED`) and the caller re-reads
  fresh state; `NO_CHANGES_REQUESTED` on an empty request; pre-validate-before-write is fine (it
  never starts, so nothing to revert); prefer atomic actions so partial can't arise. Done on
  set_transform + set_component_property; **~24 real-write handlers remain.** Error names + the
  `silent-success-convention-2026-07-06.md` doc's guard-model section need reconciling to this.
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
- **Phase 3 per-action schemas (`oneOf`)** — [`phase3-per-action-schema-design-2026-07-06.md`](phase3-per-action-schema-design-2026-07-06.md).
  **Pilot SHIPPED 2026-07-08 (`1d0700c2`):** `control_editor` publishes 33 per-action oneOf branches;
  server side fully verified (per-action params + required advertised, branch-resolved type-check,
  clean boot after fixing a validation-before-registration ordering bug the oneOf exposed). **STILL
  BLOCKED on the headline question — does the client honor a top-level oneOf?** Needs a FRESH harness
  session (the MCP client caches the tool schema at connect and won't re-fetch mid-session). Next
  session: open a new Claude Code session, inspect `control_editor`'s advertised schema (oneOf vs
  flattened), and see if the model calls per-action params without a foreign-param reject vs a
  flat-fold tool. If honored → roll out to the other 21 (mechanical: same BuildInputSchema loop). If
  not → revert control_editor to the flat fold (one facade method) and do the enum-description fallback.

## 🔵 Smaller / older
- **manage_blueprint `create_blueprint` ignores `path`** — sprays to /Game root (TODO 2026-07-03).
- **animation_physics `setup_ragdoll`/`activate_ragdoll`** — full impls exist unwired; wire or retire.
- **`execute_python` IMC/BP-default authoring traps** — documented gotchas (TODO).
- **Clone sync** — `c:/GitHub/Unreal_mcp` behind the fork (reset declined once).
