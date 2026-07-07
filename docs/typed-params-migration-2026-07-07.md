# Typed-params migration — progress log (started 2026-07-07, overnight)

Aaron approved the proof-of-concept and asked me to crank overnight. This tracks
what's done, what's next, and any decisions that need his call in the morning.

## The design (agreed with Aaron)

Kill free-form JSON blobs on the wire; everything is a scalar or a **declared
shape**. No magic string-parsing. Three piles (see `backlog.md` + memory
`feedback_partial_handling_no_rollback`):

1. **Open maps** (`properties`, `variables`, …) → **gone**. Atomic single sets.
2. **Bounded shapes** (colors, sizes, bounds, …) → **declare the real typed
   object** (like `location`), so it transmits correctly.
3. **Polymorphic `value`** → **discriminated union** of typed fields
   (`boolValue`/`intValue`/`floatValue`/`stringValue`/`vectorValue`…). The
   populated field is the type tag *and* a real schema type (transmits as a real
   object). Server cross-checks the field against the property's reflected type
   and fails loud (`VALUE_TYPE_MISMATCH`); exactly one field required
   (`NO_CHANGES_REQUESTED` / `AMBIGUOUS_VALUE`).

Also in force: **fail-in-place, no rollback** (rollback is unverifiable
per-setter). Writes that land stay; failures report applied+failed.

## Key constraint learned
Schema changes need the **client** on the fresh schema. A long-running session
caches tool schemas at startup, so `vectorValue`-as-object can't be directly
verified mid-session (it stringifies off the stale cache). Fresh sessions /
`/mcp` reconnect get it. Handler logic is still verifiable via
`bypassParamCheck:true`. Each schema batch needs a rebuild+restart to re-publish.

## Progress log

- **`b0f2fda0`** — `control_actor.set_component_property` → discriminated union
  (pile 1 + 3 on one handler). Killed `properties` map + polymorphic `value`.
  Verified live: floatValue applies; boolValue→float fails loud; two fields →
  AMBIGUOUS; Mobility(string)/SimulatePhysics(bool) special-cases land.
- **`65e0141f`** — shared helper `ReadDiscriminatedValue` +
  `PropertyMatchesValueKind` in McpPropertyReflection (DRY for the rollout). And
  `control_actor.set_blueprint_variables` → single-variable discriminated union
  (killed `variables` map). Verified live: boolValue on bCanBeDamaged applies
  (=False); floatValue on it (uint8) → VALUE_TYPE_MISMATCH.

## Next steps (in order)
1. `control_actor.add_component` — its `properties` init-map. **Doing: drop it**
   (create + register + keep meshPath convenience; set props afterward via
   set_component_property — atomic, and the map is broken/transmission anyway).
   Aaron: confirm/revert in the morning. → finishes control_actor.
2. Refactor `set_component_property` to use the shared helper too (low-priority
   DRY; it works inline today).
3. Bounded-shapes pass across tools (colors/sizes/bounds) → typed objects.
4. `manage_asset` (20 free-form params) and `manage_blueprint` (16) — the big two.

## Decisions parked for Aaron
- **add_component init-props**: drop vs single-value (leaning drop).
- Any handler whose `value` has bespoke (non-reflection) semantics gets noted
  here rather than guessed.
