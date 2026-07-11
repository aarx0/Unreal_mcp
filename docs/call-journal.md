# Call journal

Every `tools/call` outcome writes one JSONL line to
`<Project>/Saved/McpJournal/calls-<yyyyMMdd>.jsonl` (day-keyed by local date;
`ts` inside each line is UTC ISO-8601). UE's own logs only record rejections
and rotate away after ~10 sessions; the journal keeps both successes and
failures, so failure *rates*, alias usage, and latency are queryable across
sessions. Written by `McpCallJournal::Record` in `McpNativeTransport.cpp`.

## Line shape

```json
{"ts":"2026-07-11T02:31:07.412Z","tool":"manage_blueprint","action":"add_variable",
 "ok":false,"code":"MISSING_REQUIRED","why":"missing_required",
 "args":["action","blueprintPath"],"offending":["variableName","variableType"],
 "ms":0,"session":"28D142B8..."}
```

- `why` — which gate produced the outcome: `schema`, `foreign_params`,
  `missing_required`, `required_group`, `nested_kind`, `nested_undeclared`,
  `nested_required`, `action_subaction`, `unknown_tool`, `overloaded` (transport
  rejects, `ms` 0), or `handler` (the call ran; `ok`/`code` are the handler's,
  `ms` is park-to-completion). Timeouts arrive as `why:"handler"` +
  `code:"TIMEOUT"` via the stale-request sweep.
- `args` — the top-level key NAMES the client sent (captured before the
  action/subAction mirror), values never recorded. This is the evidence base
  for alias-retirement decisions.
- `offending` — the param names (or violation sentences, for `schema`/nested
  gates) that caused a reject.

## Querying

```powershell
pwsh -File scripts/journal-stats.ps1                 # all days: rates, codes, slowest actions
pwsh -File scripts/journal-stats.ps1 -Day 20260711   # one day
pwsh -File scripts/journal-stats.ps1 -ArgUsage       # + arg-name frequency per tool
```

## Caveats

- Protocol-junk requests that die before a tool name is parsed are not
  journaled (nothing to attribute them to).
- A journal write failure never fails the call: it logs one Warning per
  session and drops the line.
- Files are never pruned; delete old days by hand if they bother you
  (hundreds of calls ≈ tens of KB).
