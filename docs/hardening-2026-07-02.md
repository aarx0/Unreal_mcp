# Bridge hardening — 2026-07-02

One-day sweep: a live dogfood audit of all 22 tools, three fix waves, two new
regression tests, and an editor-exit crash fix. This document is the
consolidated record; the raw material lives in:

- [`dogfood-audit-2026-07-02.md`](dogfood-audit-2026-07-02.md) — the audit's 112 findings with verbatim repros
- [`architecture-review-2026-07-02.md`](architecture-review-2026-07-02.md) — the same-day structural review (separate effort, same date)
- `TODO.md` (repo root) — the live backlog of what remains

## Method

16 agents acted as schema-only first-time MCP users against a live editor —
471 tool calls across all 22 tools, every claim reproduced live. Fixes were
then applied by domain-partitioned agents with disjoint file ownership, and
**every fix was re-verified by re-running the audit's exact failing repro**
against the rebuilt editor (75/75 green), with disk-level evidence (full-precision
mtimes, CDO readbacks, bounds readbacks) rather than trusting responses.

## The three systemic defects

1. **Silent-success writes.** Dozens of actions accepted a param, echoed it in a
   success response, and persisted nothing. Root causes varied — the interaction
   `configure_*` family wrote values only to the live CDO, never to
   `FBPVariableDescription::DefaultValue`, so every full compile re-seeded the CDO
   from empty; `set_anchor` never wrote the slot; GAS tag setters returned
   `tagsAdded:[]` for unregistered tags; brush volumes were spawned with a null
   `UModel` so they had *no geometry at all*.
2. **Schema↔handler param drift.** The published inputSchema and the handlers
   disagreed on ~20 param names at audit time; a static reconciliation test later
   found ~40 more declared-but-never-read params. Every one either wastes a call
   (loud) or silently drops caller intent (worse).
3. **Ignored destination paths.** Multiple create actions dropped the caller's
   `path` in favor of hardcoded folders (`/Game/Audio/Cues`, `/Game/Animations`,
   `/Game/GeneratedMeshes`, `/Game/Interactables`, `/Game` root) — content
   pollution in a shared project, reported as success.

## What shipped (aarx0/dev)

| Commit | What |
|---|---|
| `9f2e604e` | Audit report (112 findings: 17 high / 51 med / 44 low) |
| `02242512` | 80-finding fix wave: silent-success class, destination paths, param aliases, dead actions implemented (GAS `add_ability`, BT task/decorator/service wrappers, `set_niagara_parameter` reconnected), `find_by_tag` round-trip, SoundCue root wiring, inspect `saved:true` honesty, diff_asset fixes, sanitizer no longer redacts error guidance, brush-volume geometry via `UActorFactory::CreateBrushForVolumeActor`, `Encumberance`→`Encumbrance` |
| `ec40710f` | TODO: UNKNOWN_ACTION echoes tool name; no save/save_all action |
| `24f24704` | `tests/schema/param-reconciliation-test.ps1` + allowlist |
| `f74348e9` | Dead-code sweep (Interaction §6/§7, AudioAuthoring dead creates, MiscHandlers PPV chain) + interaction destination aliases |
| `91a8f79d` | **Editor-exit crash fix** (see post-mortem below) |
| `856ce8b4` | Dead-param triage wave 1: `reductionSettings` implemented on `generate_lods`; 24 params removed (incl. `timeoutMs`×4, response-keys declared as inputs) |
| `25892437` | `configure_sight_config` perception params completed (top-level + nested, applied to `UAISenseConfig_Sight`) |
| `dfbc4e99` | Dead-param triage wave 2: 14 implemented (geometry tuning knobs, GAS `period`/`setByCallerTag`, EQS `searchCenter`), 4 removed (incl. deprecated `loSHearingRange`) |

Notable root-causes beyond the headline classes:

- **"TOOL_DISABLED race" was not a race.** `manage_tools` enablement is global,
  shared across sessions; the audit's own tools agent created real disabled
  windows under its 15 parallel siblings. The error now names the last mutation
  and its age, and states that availability is shared. Per-session enablement is
  a deliberate design decision left open (TODO).
- **`manage_ai create` silently bound new BTs to the first `BlackboardData` found**
  — in this project, the production `BB_Memory`. It now honors `blackboardPath`
  and never auto-picks silently.
- **`set_modifier_magnitude` with `magnitudeType:'SetByCaller'` silently wrote a
  ScalableFloat** — now requires and applies `setByCallerTag`.

## Editor-exit crash post-mortem (`91a8f79d`)

Shutdown crashed in `FMcpNativeTransport::Shutdown()` (use-after-free,
timing-dependent). `ListenSocket` had two owners: the accept thread's exit path
destroyed it, while `Thread->Kill(true)` re-invoked `Stop()` from the game
thread, whose `ListenSocket->Close()` could then touch freed memory.
Fix: single ownership — `Stop()` only closes the socket to unblock the accept
wait; `Shutdown()` destroys it exactly once after the thread is joined.
Verified: repeated launch→quit cycles exit 0 with an orderly shutdown log.

Live Coding note: the crash executed from `patch_0.exe` — LC patch modules can
absorb TUs beyond the ones you edited. After multi-file changes, prefer a full
editor-closed rebuild; LC is for single-TU iteration.

## Tests (all offline-runnable except the material suite)

```
pwsh -File tests/schema/schema-snapshot-test.ps1          # tools/list contract vs golden (-UpdateGolden to re-pin)
pwsh -File tests/schema/param-reconciliation-test.ps1     # declared vs actually-read params, both directions (-UpdateAllowlist)
pwsh -File tests/material/material-authoring-test.ps1     # 18 live assertions (needs the editor + bridge up)
```

The reconciliation test is the structural guard against defect class #2:
direction 1 fails on schema params no handler reads; direction 2 fails on
payload reads no schema declares (allowlisted backlog: 552 undeclared reads —
pre-existing, tracked; dead pins are down to 6 documented regex-invisible
indirect reads).

## Process discoveries (team-relevant)

- **The editor's git source-control plugin auto-stages every newly created asset
  into the game repo index.** Anything created in-editor (by human or bridge)
  lands staged, silently. After bridge sessions, check `git status` and unstage
  scratch before committing.
- A fresh editor boots into HubWorld (the production menu level); level-mutating
  bridge calls before loading a scratch level will edit it. Load
  `/Game/ZZ_McpAudit/L_McpAudit` first.

## Current trust state

The audit's "do not trust the success response" list is now empty. Remaining
weak spots are readback depth (several `get_*_info` actions are too thin to
verify what a write just did — the next planned wave) and the items in
TODO.md's dogfood section (per-session tool enablement, repo-wide
`appliedProperties` convention, `HandleMiscAction` deletion, thin-readback pass).
