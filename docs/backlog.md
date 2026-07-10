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

## ✅ Handler-side sequence (the three you set) — all shipped
- **① dead code** — done (above).
- **② silent-success — CONVENTION: fail-in-place, NO rollback.** Aaron rejected transactional
  rollback (`Cancel()`) as unverifiable per-setter — a reported "rolled back" can leave live
  side-effect state (SetMobility/SetSimulatePhysics). House rule: writes that land **stay**; on
  any failure, error with `applied`+`failed` (`PROPERTY_WRITE_FAILED`) and the caller re-reads
  fresh state; `NO_CHANGES_REQUESTED` on an empty request; pre-validate-before-write is fine (it
  never starts, so nothing to revert); prefer atomic actions so partial can't arise. **SHIPPED
  2026-07-09:** every bag setter is converted-or-justified — the forcing-function lint
  (`tests/schema/silent-success-lint.ps1`) is green with 10 justified allowlist entries, the
  `FMcpWriteReport` + `SendWriteReportResponse` helper landed, and the
  `silent-success-convention-2026-07-06.md` guard-model section is reconciled.
- **③ delete legacy dispatchers — SHIPPED 2026-07-09/10.** All monolithic string dispatchers
  are gone (manage_blueprint's `HandleBlueprintAction`/`HandleBlueprintGraphAction`/
  `HandleSCSAction`/widget/CommonUi + the niagara/material/audio authoring subsystems), extracted
  to per-action `FMcpCall` members; the legacy registration-map dispatch path was deleted
  end-to-end (`df9a2fcc`) and boot validation now certifies one `FMcpCall` class per published
  action (`3981f117`). `FMcpCallRegistry` is the sole dispatch.

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
- ~~**Phase 3 per-action schemas (`oneOf`)**~~ **ANSWERED 2026-07-09 — client does NOT honor top-level
  oneOf; pilot REVERTED.** Fresh-session evidence: the Claude Code MCP client flattened `control_editor`'s
  33 branches into a merged property bag with an auto-generated "exactly one of" description, an EMPTY
  `required`, and — fatally — `action` reduced to the FIRST branch's `const: "play"`, hiding the other 32
  actions from the model entirely. `control_editor` is back on the flat fold (enum + folded fragments,
  same facade as every other tool). PAUSED, not deleted: the server-side branch-resolved validation in
  `McpNativeTransport::CollectSchemaViolations` remains (it no-ops on flat schemas) so a future
  faithful-rendering client only needs the one facade method restored — see
  [`phase3-per-action-schema-design-2026-07-06.md`](phase3-per-action-schema-design-2026-07-06.md).
  Discoverability of per-action params stays with the per-action decl rejection messages (they name the
  accepted params on any miss).

## 🔵 Smaller / older
- ~~**manage_blueprint `create` ignores `path`** — sprayed to /Game root~~ ✅ FIXED 2026-07-08 (`638a67a7`):
  resolve path→savePath→folder→/Game + declared path/folder on the `create` fragment. **Lead:** the same
  gap likely affects the OTHER create_* actions — their schemas advertise `path`/`folder` with a
  "falls back to savePath, then folder" description, but no handler reads `folder` at all and the
  widget/menu creators read only `savePath`. Worth a consistency sweep (a shared ResolveCreateFolder helper).
- ~~**animation_physics `setup_ragdoll`/`activate_ragdoll`** — full impls exist unwired; wire or retire~~ ✅ WIRED:
  both actions now run the full implementations (`RequiresEditor | Mutating`); setup_ragdoll published with
  its own fragment (it previously had no class at all — only activate_ragdoll had a NOT_IMPLEMENTED stub).
- **`execute_python` IMC/BP-default authoring traps** — documented gotchas (TODO).
- **Clone sync** — `c:/GitHub/Unreal_mcp` behind the fork (reset declined once).
