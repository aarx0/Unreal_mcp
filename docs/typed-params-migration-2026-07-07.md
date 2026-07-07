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
- **`d129520d`** — `control_actor.add_component` drops its `properties` init-map
  (create + register only; keeps meshPath). Verified: creates a PointLight that
  attaches/registers. **✅ control_actor fully migrated.**

## Next steps (in order)
1. `manage_asset.set_asset_property` — polymorphic `value` → discriminated union
   via the shared helper (direct analog of set_component_property, no special
   cases since it's a plain asset object). Then the rest of manage_asset's ~20
   free-form params (many are bounded shapes: reductionSettings, size, …).
2. `manage_blueprint` (16 params) — overlaps with the ③ legacy dispatcher
   (recursive HandleBlueprintAction); careful, smaller batches.
3. Bounded-shapes pass across the remaining tools (colors/sizes/bounds).

## Pending re-publish
add_component's decl change is Live-Coding-patched but not yet re-published to the
running editor's schema (still advertises `properties` until the next restart) —
harmless (handler ignores it); the next rebuild picks it up.

## Decisions parked for Aaron
- **add_component init-props**: drop vs single-value (leaning drop).
- Any handler whose `value` has bespoke (non-reflection) semantics gets noted
  here rather than guessed.
