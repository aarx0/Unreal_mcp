# Docs reference test

Offline check that living docs don't reference things that no longer exist — the mechanical
half of doc-rot prevention (born from the 2026-07-03 staleness audit: 60 findings, mostly
references to the deleted WebSocket/TypeScript era and one pruned-config trap).

`docs-reference-test.ps1` scans every tracked `.md` except point-in-time records
(CHANGELOGs, TODO.md, dated audit reports) for:

- **link** — relative markdown links that don't resolve
- **path** — backticked references to our files (`Mcp*`, `.ts`, `docs/…`) that aren't in the tree
- **symbol** — backticked `Mcp*`/`Handle*` identifiers absent from the source
- **tripwire** — known-rot phrases (`ws://` endpoints, `npx` install steps, `src/tools/`,
  "TypeScript and C++", `Saved/Config` as a place to put settings, …)

A line (±1 for wrapping) that says its referent is gone (deleted/removed/moot/pruned/…) is
exempt — history is allowed to be history. Deliberate leftovers are pinned in
`docs-reference-allowlist.txt`.

```powershell
pwsh -File tests/docs/docs-reference-test.ps1                  # check
pwsh -File tests/docs/docs-reference-test.ps1 -UpdateAllowlist # pin current findings
```

What this can't catch: a status line that's syntactically valid but semantically stale
("Status: not yet built" on a shipped design). That's covered by convention instead —
see AGENTS.md: docs flip in the same commit as the change that outdates them.
