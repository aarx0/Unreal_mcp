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

---

# Survey of the remaining ~40 free-form params — and the decision I need from you

I surveyed every remaining `FreeformObject` param in `manage_asset` (20) and
`manage_blueprint` (16) before continuing, and stopped, because the rest is **not**
a uniform "apply the same pattern" — it splits into clean work, forks, and one
genuine kind-set decision that's yours. `control_actor` was the clean core (all
its setters are scalar/vector); the bigger tools are more varied.

## The categories

**A. Type-specific value setters — CLEAN, but need a kind-set extension (~8)**
These are declared `value: "any type"` but each action's type is actually *fixed*:
- `set_scalar_parameter_value`, `add_scalar_parameter` defaultValue → **float**
- `set_vector_parameter_value`, `add_vector_parameter` defaultValue → **color/vector**
- `set_static_switch_parameter_value`, `add_static_switch_parameter` default → **bool**

They already call natively-typed APIs (`SetScalarParameterValue(float)` /
`SetVectorParameterValue(FLinearColor)`). Converting them is clean **once the union
has a `colorValue{r,g,b,a}`** for the vector/color ones (floats/bools already fit).

**B. Colors — CLEAN, need `colorValue` (~5):** `startColor`, `endColor`,
`primaryColor`, `secondaryColor`, `luminanceFactors{r,g,b}` (texture create/desat).

**C. Bounded shapes — CLEAN typed objects (~4):** `pagination{offset,limit}`,
`reductionSettings{percentTriangles,…}`. Plus `size` which is *polymorphic*
(a number for icon size OR `{x,y}` for a canvas slot) — needs a tiny split call.

**D. General-purpose value setters — FORK (~12):** `set_asset_property`,
`set_default`, `set_scs_property`, `set_node_property`, `set_pin_default_value`,
`set_style`, `add_animation_keyframe`, `add_slider`/`add_spinbox` value, `rowData`
(DataTable). These apply to the **full reflected type range** — object references,
arrays, custom structs, and **instanced subobjects** (`set_asset_property` explicitly
round-trips `{"__class",…}` for IMC input triggers/modifiers). A 5-kind union would
**regress** all of that. These can't be a fixed typed field.

**E. Open maps (~4):** `metadata` (asset + variable + blueprint) — genuinely
arbitrary key→string. Keep as an open map, or kill.

## The decision I need (drives everything left)

**The value KIND-SET.** `control_actor` used bool/int/float/string/vector. To do
categories A+B I need to add at least **`colorValue{r,g,b,a}`** to the union. That's
a natural extension (like `vectorValue`) — but it's the first *design* call beyond
the proven core, so I'm not making it unilaterally.

And category **D** is the real question: general setters need object/array/struct/
instanced values, which a simple typed field can't express. Options:
1. **Add `colorValue` (+ maybe `objectValue` = asset path string), and leave D's
   long tail (arrays/structs/instanced) on a single *typed-object* `structValue`
   escape** — one declared object field, transmits correctly, handler feeds it to
   the existing reflection importer. Covers everything, one "structured" door.
2. **Split by known type** where the action implies one (A/B become typed), and
   leave the truly-general D setters as-is (documented power-user path) for now.
3. Something else you have in mind.

My lean: **option 1** — add `colorValue` + `objectValue` for the common typed cases,
and a single `structValue` (typed object) escape for the genuinely-structured long
tail, so nothing regresses and there's still no free-form blob. But this is your
call, and it shapes ~30 of the remaining conversions, so I stopped here rather than
guess it at 1am.

## Overnight outcome
`control_actor` fully migrated + verified (3 handlers, shared helper). Everything
else is gated on the kind-set decision above. Pick a direction and the rest is
mostly mechanical — I can crank through A/B/C fast and handle D per your choice.

## Decisions parked for Aaron
- **add_component init-props**: drop vs single-value (leaning drop).
- Any handler whose `value` has bespoke (non-reflection) semantics gets noted
  here rather than guessed.
