# Compound-action triage — the 140 `configure_*`/`setup_*` actions (2026-07-06)

Input for the "should we atomize actions?" decision (Task ② context). Method: 6 read-only agents
classified every `configure_*`/`setup_*` handler by **what it actually writes** (rubric below);
I spot-checked the borderline calls. No edits made.

## Rubric
- **A** atomic — writes ≤2 real values / one focused setting.
- **C** cohesive-compound — writes 3+ sub-fields of ONE logical thing (a struct, one component's
  config, one constraint). Naturally set together; partial state is meaningless. → *keep, make transactional.*
- **G** grab-bag — writes 3+ INDEPENDENT knobs a user might set separately. → *split candidate.*
- **N** no-op/scaffold — creates Blueprint vars for game code (honest scaffold) OR echoes inputs and
  writes nothing / only a marker tag.

## Totals (140 actions)
| bucket | count | % |
|---|---|---|
| A atomic | 42 | 30% |
| C cohesive (keep + transactional) | 27 | 19% |
| G grab-bag (split candidate) | 16 | 11% |
| N no-op / scaffold | 55 | 39% |

Per tool: networking A12/C1/G1/N12 · combat A4/C3/G0/N18 · character A2/C5/G2/N9 ·
ai+interaction A6/C4/G8/N7 · inv+gas+audio A7/C7/G2/N4 · anim/env/system-tail A11/C7/G3/N5.

## The reframe (this is the actual finding)
Splitting grab-bags is **not** where the value is — two things matter more:

**1. The N bucket (39%) is two different things, and only one is a bug.**
- **Honest scaffolds (~40)** — combat weapon/ammo/recoil/combo, character `setup_mantling`/etc.,
  create BP member vars and return `variablesAdded`. Creating the vars *is* the success (your
  point). **Not bugs.** But: are ~40 thin "author these vars for you" actions worth carrying as
  MCP tools, vs documenting "define these vars yourself"? That's a **product/scope call**, and it's
  the single biggest bucket.
- **Silent no-ops (~15)** — accept params, write **nothing** (or only a marker tag), return
  success. The networking Sessions family (`configure_voice_settings`, `lan_play`,
  `session_interface`, `local_session_settings`, `push_to_talk`, `net_serialization`), the
  interaction destruction family (`setup_destructible_mesh`, `configure_destruction_*` — marker
  tag only), `configure_level_bounds` (computes + echoes), `setup_ragdoll` (returns
  NOT_IMPLEMENTED but shaped like success). **These are the real silent-success bugs.**

**2. Partial application is pervasive across ALL buckets — not just grab-bags.**
Many "atomic" actions accept params they silently drop: `configure_player_start` ignores
location/rotation, `configure_spectating` ignores viewMode, `configure_hearing_config` ignores
affiliation/age, `configure_virtual_texture` ignores tileSize, `configure_projectile_movement`
reads `projectileLifespan` and never writes it. **This is exactly the thing your all-or-nothing
rule catches**, and it's everywhere, not confined to compound actions.

**Grab-bags (G=16) are mostly scaffold-shaped or borderline-cohesive.** Spot-check: interaction's
`configure_door_properties` reads independent knobs *and* creates BP vars — a grab-bag-shaped
scaffold, not a real multi-property write. The genuinely-hard grab-bags that write real,
independent *engine* state are only ~6–10: `configure_movement_speeds` (9 CMC tunables),
`configure_jump`, `configure_nav_mesh_settings`, `setup_perception`, `configure_smart_link_behavior`,
`configure_occlusion_culling`/`world_partition` (if you count CVar bundles).

## Recommendation
1. **All-or-nothing / transactional is the high-leverage fix** (confirms your instinct). Apply it to
   the real-mutation actions — the **27 C** + the real-write A/G — so partial application becomes a
   loud failure with rollback. This is the bulk of the value and needs no splitting.
2. **Fix the ~15 silent no-ops** — either implement them or make them `NO_CHANGES_APPLIED` errors.
   Small, concrete, high-value bug list (mostly networking Sessions + interaction destruction).
3. **Split only the ~6–10 hard grab-bags** — opportunistically, not as a program. Low ROI relative to (1)/(2).
4. **Separately decide the ~40 honest scaffolds** — keep as thin authoring actions, or retire in
   favor of "define these vars yourself"? Biggest bucket, but it's a scope question, not a bug.

Net: the atomize-everything instinct is right in spirit but the data says **the lever is
all-or-nothing on the real mutations + killing the no-ops, not mass-splitting.** Splitting is a
rounding error next to those two.

## Appendix — full per-action tables
The six agent tables (every action, bucket, what it writes, one-line reason) are preserved in the
run transcripts; the borderline calls each agent self-flagged are the C/G-vs-N scaffold overlaps
noted above. Regenerate by re-running the triage if a per-action spec is needed for execution.
