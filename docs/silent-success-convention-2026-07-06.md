# Silent-success convention — analysis & design (2026-07-06)

Task ② of the handler-side cleanup ("silent success"). Status: **analysis + design, needs
Aaron's decisions before rollout.** Autonomous scoping stopped here on purpose (see "Why not
just blast it").

## The reality (measured, not assumed)
- **911 of 1124 registered actions are `Mutating`.**
- `appliedProperties`/`ignoredParams` exists **inline in only 4 files** (Blueprint, Combat,
  Control, Interaction handlers) — hand-rolled per handler, **no shared helper**.
- The `~19` "scaffold" success responses are **honest** — e.g.
  `"Damage variables scaffolded; no damage was applied at runtime."` They *tell* the caller
  nothing ran at runtime. These are **not** the silent-success bug.
- The genuinely-silent no-ops (accept params → apply nothing → plain `success`) were mostly
  **already fixed** in the 2026-07-02 audit (interaction `configure_*`, `set_anchor`, etc.).

So Task ② is **not** a pile of current bugs to fix — it's a **preventative convention** so future
(and the long tail of un-audited) mutating handlers can't silently succeed. That's design +
behavioral change, not a mechanical sweep.

## Proposed convention
A small shared helper so the pattern is uniform instead of hand-rolled:
```cpp
struct FMcpApplyReport {
    void Applied(const FString& name);              // a real change landed
    void Ignored(const FString& name, const FString& why); // param accepted but not used
    bool AnyApplied() const;
    void EmitInto(TSharedPtr<FJsonObject>& Result) const;  // appliedProperties[] + ignoredParams[]
};
```
Plus the guard at each mutating handler's tail:
> if the caller requested changes but `AnyApplied()` is false → `SendAutomationError(...,
> "no requested changes were applied", "NO_CHANGES_APPLIED")` instead of a hollow success.

## The subtlety that needs your call
**Scaffolds break the simple guard.** The combat/interaction scaffolds *intentionally* apply
nothing at runtime (they author BP variables for game code to consume). A blanket
"error when nothing applied at runtime" would wrongly fail them. Options:
1. **Two response kinds** — `applied` (real mutation, guarded) vs `scaffolded` (explicit, allowed
   to apply nothing, must carry the honest message). Cleanest, but every handler must self-classify.
2. **Guard only on the param-setter subset** — handlers that take property/value params get the
   error-on-zero; scaffolds/creators are exempt by not adopting it. Less uniform, lower risk.
3. **Report-only, no error** — everyone emits `appliedProperties`/`ignoredParams`, but zero-applied
   is a warning in the payload, not an error. Zero behavioral-break risk; weaker fail-fast.

## Why not just blast it autonomously
- 911 handlers, each with **handler-specific** "did I apply anything" logic — no mechanical rule.
- error-on-zero is a **behavior change**: calls that pass today could start failing. That needs
  your risk acceptance + a decision on the scaffold carve-out above.
- The helper's shape + where it lives is a design choice you should own (you maintain this).
Task ① (dead code) was safe to auto-run because the compiler/linker *proved* correctness. Nothing
proves a silent-success rollout correct except judgment — so it waits for yours.

## Decision points for you
1. Which guard model — (1) two-kinds, (2) setter-subset, or (3) report-only?
2. error-on-zero as a hard `NO_CHANGES_APPLIED` error, or a soft warning?
3. Rollout order — highest-risk domains first (property/config setters), or per-tool as touched?

Once you pick, the helper + a pilot on one domain is ~30 min, then it's incremental per-handler.
I did **not** build the helper or touch handlers yet — no design committed to react against.
