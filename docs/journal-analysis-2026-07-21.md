# Call-journal analysis — 2026-07-21

First analysis pass over the per-call journal (`Saved/McpJournal/calls-*.jsonl`,
2026-07-11 → 2026-07-21). Aggregates via `scripts/journal-stats.ps1`; the alias
cross-reference joins declared params whose description contains "alias" against
actual client-sent arg names. Run context: essentially two users (Claude sessions +
Aaron's client) on one project — treat as strong evidence for the exercised tools,
no evidence at all for the idle ones.

## Snapshot

1,463 calls, 1,292 ok / 171 failed (11.7%). Volume by tool:
manage_blueprint 443, system_control 316, control_actor 286, control_editor 174,
inspect 127, manage_asset 83, manage_level 17, manage_effect 6, manage_sequence 5,
manage_ai 3, manage_input 2, manage_audio 1. **Every other tool: zero calls**
(animation_physics, build_environment, manage_gas, manage_geometry, manage_character,
manage_combat, manage_interaction, manage_inventory, manage_networking,
manage_level_structure, manage_sequence partially...). Zero-call tools contribute no
alias evidence either way.

## Alias-kill evidence

~130 declared alias params; only 9 were genuinely sent in 10 days.

**Aliases with real usage (keep):**
- `control_actor.name` (21 — primary spelling for find_by_name), `classPath` (7)
- `manage_blueprint.x`/`y` (19 each — now honored on add_node too), `path` (5)
- `manage_blueprint.saveAfterCompile` (16)
- `manage_asset.paths` (3 — delete)
- `manage_sequence.path` (1), `system_control.category` (1)
- (`nodeType` 95 / `memberName` 41 are grep false-positives — the word "alias"
  appears in their descriptions but they're primary params.)

**Evidenced kill candidates — tool heavily exercised, alias never sent once:**
- `control_actor` (286 calls): `actor_name`, `component_name`, `collision_enabled`
  (snake_case tier), `class`, `actorClass`
- `manage_blueprint` (443): whole snake_case tier (`blueprint_path`,
  `component_class`, `component_name`, `material_path`, `mesh_path`, `new_parent`,
  `parent_component`, `property_name`), `candidates` (legacy), `assetPath`
  (graph-action alias of blueprintPath — everyone uses blueprintPath), `oldName`,
  `targetFunction`
- `inspect` (127): `class`, `propertyPath`
- `control_editor` (174): `inputAction`, `inputType`
- `manage_asset` (83): `assets`, `blendTexture`, `content`, `csv`, `json`,
  `dataTablePath`, `numLODs`, `overlayTexture`, `recursivePaths`, `rowStructPath`,
  `tablePath`
- `system_control` (316): `additionalArgs`/`arguments` pair (keep one),
  `packageName`, `test`

**No-data tier (do NOT kill on this evidence):** the big alias piles in manage_gas,
manage_audio, manage_ai, build_environment, manage_geometry, manage_networking,
manage_inventory, manage_interaction, animation_physics — their tools saw ≤3 calls
total. Killing there is a taste call, not a data call.

## Failure hotspots (≥5 calls, ≥20% fail)

| Action | Fails/Calls | Disposition |
|---|---|---|
| control_editor.open_level | 4/5 | FIXED 2026-07-19 (undeclared deferral) — all 4 predate it |
| control_editor.possess | 3/5 | INVALID_ARGUMENT ×3 — worth a look at what it demands |
| inspect.set_component_property | 10/21 | mostly pre-dates the 2026-07-19 batch-mode fix |
| manage_blueprint.arrange_graph | 8/19 | 3× stale-client nodes-as-string (client cache, known), 2× COMMENT_BOXES_PRESENT (deliberate refusals, working as intended) |
| system_control.execute_command | 33/98 | EXEC_FAILED = console command returned false; dominated by deliberate probes (PIE-only commands outside PIE etc.) — not a bridge defect |
| control_actor.get_actor_bounds | 3/9 | all three: client sent `name`, action only reads actorName — ergonomic gap (the `name` alias is declared tool-wide but this handler ignores it) |
| manage_asset.set_node_value | 2/7, add_custom_expression 2/5 | material authoring friction, small n |
| manage_blueprint.connect_pins | 18/85 | 17× ENGINE_ERROR — real link refusals (schema rejects the connection), not param friction; the perennial pin-name/type iteration loop |
| manage_blueprint.set_default | 4/20 | includes foreign `save`/`compile`/`value` — see gaps below |

## Ergonomic gaps (foreign-param rejects — what clients keep trying to send)

- `manage_blueprint.create_node :: location` ×6 — models naturally reach for a
  `location` object instead of x/y. Cheap: accept `location:{x,y}` or hint at x/y in
  the reject.
- `control_actor.get_actor_bounds :: name` ×3 — honor the tool-wide `name` alias
  here (or stop declaring it tool-wide).
- `manage_blueprint.set_default :: save`/`compile` ×2 each — callers expect the
  standard save/compile riders on set_default (and `create` got one of each too).
- `system_control.tail_log :: lines` ×2, `control_editor.simulate_input ::
  eventType` ×2 — small; note simulate_input also got called on manage_input where
  it doesn't exist (schema-enum reject, "did you mean" fired correctly).

## Recommendation

1. Kill the evidenced-zero aliases in the six exercised tools (one commit, schema +
   handler reads together, golden re-pin). ~30 params.
2. Leave the no-data tiers alone until their tools get real use (or fold into the
   Aaron-taste confusing-names pass).
3. Fix the two cheap ergonomic gaps with data behind them: get_actor_bounds `name`,
   create_node `location` (or a better reject hint).
4. connect_pins/possess/material-authoring friction: watch, no action — small n or
   already-explained.
