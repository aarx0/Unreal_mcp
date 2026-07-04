# Action declarations — the server's contract

**Status: SHIPPED 2026-07-04 (Stage 1).**

The server owns a single statement of "here's what I know how to do": one
declaration per `(tool, action)` pair, listing the params that action reads and
which are required. Everything else derives from it. This replaced the
parsed-table system (a regex parser extracting truth from handler source) after
the architecture discussion of 2026-07-04: declarations are *authored*; the
parser survives only as a lint.

```
        McpDecl_*.h  (Private/MCP/Decls/ — one per tool family)
        /        |          \
   transport   published    action-decl lint
   validation  schema       (handler source vs declarations,
   (rejects    (Stage 3:    both directions)
   undeclared  derive and
   params)     delete the hand-built bags)
```

## The pieces

- **`McpCallRegistry.h`** — vocabulary (`FMcpParamDecl`, `FMcpCallDecl`,
  `EMcpCallFlags`) plus the registry. `McpRegisterAllActionDecls()` runs before
  the transport starts; duplicates are boot errors.
- **`Private/MCP/Decls/McpDecl_<Tool>.h`** — the declarations, one header per
  tool family. Bootstrapped by a 57-agent fleet reading the handler source
  (three-witness cross-check against the old parsed table and the published
  schema), **hand-maintained since**: adding an action means adding its
  declaration here.
- **Transport check** (`McpNativeTransport.cpp`) — params not in the called
  action's declaration reject with `INVALID_PARAMS`, the accepted list, and
  both fix paths. `bypassParamCheck:true` downgrades to response-visible
  `paramWarnings` for the wrong-declaration case (fixing a declaration needs a
  rebuild; a false rejection must not strand a task). `UnverifiedDecl` entries
  are skipped by validation — never enforce unattributed truth.
- **`FMcpCall`** — base class for fully-migrated actions (declaration and
  implementation co-located, Chromium-ExtensionFunction-style). New actions
  MUST be classes; legacy actions are shims (declaration + legacy family
  dispatch) that convert per-family, opportunistically.

## Bootstrap state (2026-07-04, after the attribution burn-down)

1171 declarations registered at boot; **1064 verified (91%)**, 107 `UnverifiedDecl`
(validation skips them — never enforce unattributed truth). The burn-down pass
resolved the alias-group class (handlers comparing several names in one
condition — `delete || delete_asset || delete_assets` — synthesized from
same-brace-range marker groups), the cross-file-delegation class (any
high-confidence contributor verifies a merged entry), and the
internal-name-only files (file→tool affinity seeded from filenames;
`manage_tools` hand-authored — it lives in the transport, which the fleet
never read). A live probe of all remaining unverified actions found exactly
**2 dead** (`control_editor.set_viewport_resolution`,
`manage_sequence.set_metadata` — advertised, no code path; implement-or-delete
in TODO); the other 105 are implemented but dispatched through mechanisms
neither the fleet's file-scoped prompts nor the parser attribute yet (mixed
utility files, function-per-action dispatch). The lint's bootstrap allowlist
pins 145 parser brace-scope mis-attributions. False-positive sweep: material
battery 18/18 + full ui-nav suite + assorted calls = zero per-action
rejections on valid traffic.

## Migration rules (agreed 2026-07-04)

1. **New actions are `FMcpCall` classes.** No new string branches in family
   handlers.
2. **Deletion is completion proof.** A shim dies the commit its class lands; a
   family's if/else dispatch chain dies the commit its last action is classed;
   the parsed-table system died the commit declarations landed.
3. **Required-param enforcement is staged.** Declarations carry `bRequired`,
   but the transport does not reject missing-required yet — that flip gets its
   own evidence pass (the fleet's required flags are the least-verified part
   of the bootstrap).
4. **Client never defines the contract**; it receives it via `tools/list`.
   Declarations do not attempt runtime-outcome truth (that is the apply-receipt
   work — see the dogfood TODO).
