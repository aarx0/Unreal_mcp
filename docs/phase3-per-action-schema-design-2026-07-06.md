# Phase 3 design — per-action schemas (`oneOf` discriminated union)

Status: **design proposal, Aaron-gated.** No code written. Prepared as no-editor prep so the
build/rebuild work can be decided and executed in one sitting.

## The problem Phase 3 solves

Phase 2 (schema-from-decls) gave every action its own authored schema *fragment* (`S_*`), but the
**published** tool schema still folds them into one flat object. From the live fold
(`Tools/McpTool_ControlEditor.cpp`):

```cpp
FMcpSchemaBuilder B;
B.StringEnum(TEXT("action"), McpConsolidatedActions::ControlEditor(), ...);
for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("control_editor")))
    Call->AppendSchema(B);                       // every fragment dumps into the SAME builder
return B.ClearRequired().Required({TEXT("action")}).Build();
```

Result: one object whose properties are `action` + the **union of every action's params**. So an
LLM reading `control_editor`'s schema sees `location, rotation, actorName, viewMode, speed,
button, key, filename …` all as flat siblings, with nothing tying `location/rotation` to
`set_camera` or `actorName` to `possess`. Per-action **required** params aren't expressed at all
(only `action` is required at the tool level). The server already enforces per-action params
correctly (transport validator + derived decls — that's where the 2026-07-06 foreign-param
rejects come from), but the *client* is flying blind: the schema under-specifies, so the model
guesses params and learns only from rejects.

## Proposed shape — `oneOf` keyed on `action`

Publish the tool's `inputSchema` as a discriminated union, one branch per action:

```json
{
  "type": "object",
  "oneOf": [
    { "properties": { "action": {"const": "set_camera"},
                      "location": {"type":"array"}, "rotation": {"type":"array"} },
      "required": ["action"] },
    { "properties": { "action": {"const": "possess"},
                      "actorName": {"type":"string"}, "objectPath": {"type":"string"} },
      "required": ["action", "actorName"] },
    { "properties": { "action": {"const": "pause"} }, "required": ["action"] }
  ]
}
```

Each branch advertises **exactly** its action's params, and can carry that action's **real
required set** (currently handler-only) — a direct fail-fast win: the client now knows
`possess` needs `actorName` before calling.

## Why this is a small change (Phase 2 already did the hard part)

The per-action data already exists. `CallsForTool(tool)` returns the action list; each `FMcpCall`
has `AppendSchema()` (its fragment) and `GetDecl().Action`/required. So `BuildInputSchema`
changes from "fold all into one B" to "build one sub-schema per Call, collect into `oneOf`":

```cpp
TArray<TSharedPtr<FJsonValue>> Branches;
for (FMcpCall* Call : Registry.CallsForTool(Tool))
{
    FMcpSchemaBuilder Bi;
    Bi.StringConst(TEXT("action"), Call->GetDecl().Action);   // discriminator (new tiny builder verb)
    Call->AppendSchema(Bi);                                   // the action's own params + its Required()
    Branches.Add(MakeShared<FJsonValueObject>(Bi.BuildWithActionRequired()));
}
// inputSchema = { type:object, oneOf: Branches }
```

New infra needed: a `StringConst` builder method (emit `{const: "..."}`) and a branch-builder that
keeps each fragment's `Required()` (Phase 2 deliberately cleared it in the flat fold — here we
*want* it, per branch). Everything else reuses existing pieces. Derived decls stay the validation
source of truth; the schema and decls still come from the same fragment, so they can't drift.

## Risks / open questions (verify in the pilot)

1. **Client support for top-level `oneOf` in `inputSchema` — THE risk.** JSON-Schema `oneOf` with a
   `const` discriminator is standard, but not every tool-use client honors a top-level `oneOf`
   (some only read a flat `properties`). If Claude's tool-use ignores it, the model gets *worse*
   info, not better. **Must** pilot one tool and confirm the model picks the right branch before
   rolling out. If unsupported: fallback is keep-flat but enrich the `action` enum description with
   per-action param hints (weaker, but client-agnostic).
2. **Schema size** — a 40-action tool yields 40 branches. Larger payload, but structured; and it's
   the same information the flat union already carries, just partitioned.
3. **Stricter than today (good, but a behavior change)** — `oneOf` rejects cross-action params at
   the client layer. Server already does this, so it aligns; but any client-side validators become
   stricter. Matches the fail-fast direction.
4. **Golden snapshot + schema tests** — `tests/schema/tools-schema.golden.json` and the
   snapshot test regenerate; the param-reconciliation/action-decl-lint tooling is unaffected
   (still fragment-derived).

## Pilot plan (one rebuild)

1. Add `StringConst` + branch-builder to `FMcpSchemaBuilder`.
2. Convert **`control_editor`** only (small; good mix — empty-decl `pause`/`play`, param'd
   `set_camera`/`possess`). Leave the other 21 on the flat fold.
3. Rebuild (editor closed → Build.bat -NoUBA → relaunch), re-pin golden for control_editor.
4. **Verification that decides the whole phase:** via MCP, does the model call `control_editor`
   with correct per-action params *without* a foreign-param reject round-trip? Compare against a
   flat-fold tool. If the branch selection works → roll out to all 22 (mechanical). If not →
   abandon `oneOf`, do the enum-description fallback.

## Relationship to Phase 4 (per-action tools / defer-loading)

`oneOf` keeps **22 tools**, each self-describing per action — the incremental win. The larger
scaling play (the tool-search / `defer_loading` pattern) is to **explode each action into its own
tool** (`manage_effect__debug_shape`, …) so clients lazy-load only the actions they need (~85%
token cut at thousands-of-tools scale). That's a bigger, client-visible change (tool count goes
from 22 to ~400) and should be its own phase *after* `oneOf` proves the per-action schemas are
correct — `oneOf` branches convert almost directly into standalone per-action tool schemas.

## Recommendation

Do the **`control_editor` `oneOf` pilot** tonight (one rebuild, ~1 verification round-trip). It's
low-cost and the answer (does the client honor top-level `oneOf`?) gates everything downstream —
including whether Phase 4 is even worth attempting. Everything before that verification is cheap;
everything after is mechanical rollout.
