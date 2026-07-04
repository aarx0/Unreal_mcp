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

## Bootstrap state (2026-07-04)

1171 declarations registered at boot. 1035 fleet-verified; 136 `UnverifiedDecl`
(validation skips them). The biggest unverified class: actions whose handler
dispatches on an INTERNAL name that collides with another tool's advertised
action (`manage_asset.delete_asset` → internal `delete`, which `build_environment`
also advertises) — the registration-gate translation maps are the fix (follow-up
in TODO). The lint's bootstrap allowlist pins 151 parser brace-scope
mis-attributions. Pre-ship false-positive sweep: material battery 18/18 +
full ui-nav suite + assorted calls = zero per-action rejections on valid traffic.

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
