# Schema regression tests

Two self-contained checks on the bridge's published tool contract.

## schema-snapshot-test.ps1

Pins `tools/list` against the checked-in golden (`tools-schema.golden.json`). Needs a running
editor with the bridge live.

```powershell
pwsh -File tests/schema/schema-snapshot-test.ps1               # compare
pwsh -File tests/schema/schema-snapshot-test.ps1 -UpdateGolden # re-pin after a deliberate change
```

**Live-Coding trap:** tool schemas are cached at registration, so after a schema edit
applied via `live_coding_compile` the live `tools/list` still serves the OLD schema —
`-UpdateGolden` then silently re-pins the stale one (caught 2026-07-03: a param removal
"passed" until the next cold boot rebuilt the cache and the golden mismatched). Re-pin
only against a cold-booted editor running a full rebuild of the schema change.

## param-reconciliation-test.ps1

Offline (no editor) static check of schemas vs. handler source, both directions: schema params no
handler reads (dead params) and payload keys a handler reads that no schema declares
(undiscoverable params). Deliberate exemptions live in `param-reconciliation-allowlist.txt` —
each pin documents why (e.g. regex-invisible indirect reads through helpers).

```powershell
pwsh -File tests/schema/param-reconciliation-test.ps1                  # check
pwsh -File tests/schema/param-reconciliation-test.ps1 -UpdateAllowlist # re-pin
```

## action-decl-lint.ps1

Handler source vs the authored action declarations (`Private/MCP/Decls/McpDecl_*.h`
— the server's per-action contract; see `docs/action-declarations.md`). Statically
re-derives branch-local payload reads (brace-scope attribution — the machinery that
used to *generate* the deleted parsed table, demoted to a drift detector) and fails
when a handler branch reads a param its action's declaration doesn't list. One
directional by design: the fleet-authored declarations legitimately contain reads
this parser can't see. `action-decl-lint-allowlist.txt` pins the bootstrap's known
parser mis-attributions.

```powershell
pwsh -File tests/schema/action-decl-lint.ps1                  # check
pwsh -File tests/schema/action-decl-lint.ps1 -UpdateAllowlist # re-pin after review
```
