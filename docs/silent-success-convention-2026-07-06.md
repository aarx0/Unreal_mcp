# Silent-success convention — analysis & design (2026-07-06)

Task ② of the handler-side cleanup ("silent success"). Status: **SHIPPED 2026-07-09** —
convention set (fail-in-place, no rollback), helper + forcing-function lint landed, all bag
setters converted-or-justified. The analysis below is preserved; the resolved decisions are
marked inline.

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

## The convention (as shipped)
A shared helper (`McpHandlerUtils::FMcpWriteReport`) so the pattern is uniform instead of
hand-rolled:
```cpp
struct FMcpWriteReport {
    void MarkApplied(const FString& field);                       // a real change landed
    void MarkFailed(const FString& field, const FString& reason); // requested but rejected
    bool AnyApplied() const;
    bool AnyFailed() const;
    void WriteInto(const TSharedPtr<FJsonObject>& Data) const;    // appliedProperties[] + failed[]
};
```
Every bag setter routes each requested field through `MarkApplied`/`MarkFailed`, then finalizes
with `SendWriteReportResponse` (`McpResponseHelpers.h`), which owns the success rule:
> nothing requested → `NO_CHANGES_REQUESTED`; any field failed → `success=false`,
> `PROPERTY_WRITE_FAILED`, data carries `appliedProperties[]` + `failed[]` (no rollback — applied
> fields stay); every field applied → success. Enforced by
> `tests/schema/silent-success-lint.ps1`.

## The scaffold subtlety — resolved
**Scaffolds broke the simple guard.** The combat/interaction scaffolds *intentionally* applied
nothing at runtime (they authored BP variables for game code to consume), so a blanket
"error when nothing applied at runtime" would wrongly fail them. The options considered:
1. ~~**Two response kinds**~~ — `applied` (real mutation, guarded) vs `scaffolded` (explicit,
   allowed to apply nothing). REJECTED: every handler would have to self-classify.
2. **Guard only on the param-setter subset** — SHIPPED: the lint targets multi-optional-field
   "bag" setters (≥2 `HasField` guards); atomic setters/creators are exempt via the allowlist,
   and the scaffolds were removed outright rather than carved out.
3. ~~**Report-only, no error**~~ — zero-applied as a payload warning, not an error. REJECTED:
   `NO_CHANGES_REQUESTED` is a hard error, not a soft warning.

## Why not just blast it autonomously
- 911 handlers, each with **handler-specific** "did I apply anything" logic — no mechanical rule.
- error-on-zero is a **behavior change**: calls that pass today could start failing. That needs
  your risk acceptance + a decision on the scaffold carve-out above.
- The helper's shape + where it lives is a design choice you should own (you maintain this).
Task ① (dead code) was safe to auto-run because the compiler/linker *proved* correctness. Nothing
proves a silent-success rollout correct except judgment — so it waits for yours.

## Decisions (resolved 2026-07-09)
1. Guard model — **(2) setter-subset**, enforced by the bag-setter lint + allowlist.
2. error-on-zero — **hard error**: `NO_CHANGES_REQUESTED` on empty, `PROPERTY_WRITE_FAILED` on
   partial. No soft-warning path.
3. Rollout — per-tool, converting each family's bag setters until the lint went green.

The helper (`FMcpWriteReport`) + `SendWriteReportResponse` landed and every bag setter is
converted-or-justified (the allowlist holds only atomic-setter/creator false positives).
