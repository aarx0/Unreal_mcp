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
  `Execute()` is the shared pipeline (payload-null check, `RequiresEditor`
  gate → `Run()`); `ProcessAutomationRequest` consults `FindCall(tool,
  action)` before the legacy handler map, so classed actions win per-action
  while the rest of a family stays shimmed.
- **Classed families** (each deleted its string-dispatch chain, shim decl
  header, and handler-map registration in its landing commit; classed decls
  are lint-visible via the `// LINT-TOOL:` marker convention in `MCP/Calls/`):
  - **`control_actor` (2026-07-04, the pilot,
    `Private/MCP/Calls/McpCalls_ControlActor.cpp`).** 27 classes; each carries
    its true per-action contract (the bootstrap's 33-param mega-bag unions died
    here — TODO 04e resolved for this family); `Run()` delegates to the
    subsystem's dedicated handlers until the module split de-members those
    bodies. `inspect`'s actor-action forwarding converted from payload
    re-dispatch to typed direct calls (migration rule 5 in practice).
  - **`manage_sequence` (2026-07-04,
    `Private/MCP/Calls/McpCalls_ManageSequence.cpp`).** 32 classes; the
    dispatcher's 32 internally-manufactured `sequence_*` names died with the
    chain (the last of that prefix family), and its four inline-only bodies
    (`list_track_types`, `add_track`, `list_tracks`, `set_work_range`) were
    extracted to dedicated members. Flags are authored per action here —
    `RequiresEditor` everywhere except pure-reflection `list_track_types`,
    `Mutating` on writers — where the shim decls carried `None`.
  - **`control_editor` (2026-07-04,
    `Private/MCP/Calls/McpCalls_ControlEditor.cpp`).** 35 classes (34 handlers —
    `step_frame`/`single_frame_step` share one); this closed TODO 04e for good:
    the shim decls carried two identical 30-param mega-bag unions
    (`execute_command`, whose true contract is `command*` — a param the bag
    didn't even contain — and `single_frame_step`, whose true contract is
    empty) plus four contaminated rows (`possess`, `screenshot`,
    `simulate_input`, `simulate_nav`). All 35 contracts re-authored from the
    handler bodies' actual reads. `RequiresEditor` on all; `Mutating` on
    `undo`/`redo`/`save_all`/`execute_command`. PIE-only preconditions
    (pause/possess/simulate_nav/…) stay handler-enforced — the flag only
    covers GEditor existing.
  - **`manage_level` (2026-07-04,
    `Private/MCP/Calls/McpCalls_ManageLevel.cpp`).** 19 classes; this family
    had no dedicated handler functions at all — all 18 live implementations
    were inline branches in a 2,960-line dispatcher monolith, extracted to
    `HandleLevel*` members (stream/unload share `HandleLevelStreamInternal`,
    replicating unload's force-override semantics). The `set_metadata`
    declaration's `metadata`-object omission — a live false-reject that
    blocked the action's own payload — is fixed, and `create_light` is
    deduped onto the full LightingHandlers implementation via a typed call
    (migration rule 5; the SPAWN_ACTOR_AT_LOCATION payload rewrite that
    dropped `properties`/`name`/SkyLight support died with the chain). 13
    transport-dead hidden branches (`set_level_visibility`,
    `get_level_bounds`, `build_level_navigation`, …) were deleted and
    ledgered in `docs/dead-name-sweep-2026-07-04.md`.
  - **`system_control` (2026-07-04,
    `Private/MCP/Calls/McpCalls_SystemControl.cpp`).** 44 classes spanning six
    implementation TUs (Performance/SystemControl/Ui/Log/Debug/Render
    handlers) — the first classed family whose routing-table entry keeps a
    non-empty routed list: `Performance()` and `SystemControlCore()` survive
    in `GetToolRoutingTable()` solely so boot validation can prove the schema
    union still matches the published enum (no live lambda remains). Flags
    are authored per action and deliberately mixed: `RequiresEditor` on the
    33 actions whose implementing TUs are editor-gated; NOT on the five log
    actions (LogHandlers.cpp is not editor-gated — flagging them would
    regress headless use), `screenshot`, `execute_command`, `set_cvar`,
    `get_project_settings`, `set_project_setting`, `spawn_category`
    (handler-enforced preconditions). Two decls drop mega-bag residue: a
    bogus required `functionName` on `configure_texture_streaming` and
    `generate_memory_report` that neither handler reads. `set_cvar`'s inline
    lambda synthesis became `HandleSysSetCvar`; it and `execute_command`
    call `HandleConsoleCommandAction` directly — the `console_command`
    internal literal SURVIVES this classing (the console handler has other
    owners: ControlHandlers.cpp, EditorFunctionHandlers.cpp). Hidden bodies
    deleted and ledgered: `export_asset` (~180 lines, advertise-candidate),
    nine unreachable UiHandlers branches (`play_in_editor`, `stop_play`,
    `save_all`, `simulate_input`, `create_hud`, `set_widget_text`,
    `set_widget_image`, `set_widget_visibility`,
    `remove_widget_from_viewport`), and three unreachable RenderHandlers
    subactions (`create_render_target`, `attach_render_target_to_volume`,
    `nanite_rebuild_mesh` — the first and last are duplicates of live
    manage_asset implementations). The vestigial `test_progress`/`test_stale`
    gate names (no branches ever existed) died with the gate.

## Bootstrap state (2026-07-04, complete)

**1169 declarations registered at boot; all 1169 verified and enforced (100%).**
Reached in three same-day passes:

1. **Fleet bootstrap** — 57 file-scoped agents + three-witness cross-check:
   1171 decls, 962 verified (82%).
2. **Attribution burn-down** — alias-group synthesis (handlers comparing
   several names in one condition — `delete || delete_asset || delete_assets`),
   any-high-confidence-contributor merge, filename→tool affinity seeding,
   `manage_tools` hand-authored (it lived in the transport, which the fleet
   never read; the whole tool was removed 2026-07-04): 1064 verified (91%). A live argless probe of the remaining 107
   found exactly 2 dead and 105 alive.
3. **Deep attribution** — 13 *action-scoped* agents (the inverse of pass 1:
   given the action list plus the live probe responses as ground truth, trace
   dispatch to the implementing function across files): all 105 verified. The
   2 dead actions (`control_editor.set_viewport_resolution`,
   `manage_sequence.set_metadata` — advertised, no code path) were **deleted**
   from the routing enums and declarations. The decl lint acted as second
   witness and caught one agent trace pointed at a *shadowed duplicate
   implementation* of `bind_parameter_to_source` (TODO bug 2026-07-04b) plus
   two alt-group required-flag slips.

The lint allowlist pins 187 non-gap findings (parser brace-scope
mis-attributions, shared-dispatcher prologue reads, dead-branch reads).
False-positive sweeps after each pass: material battery + full ui-nav suite +
assorted valid traffic = zero per-action rejections.

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
5. **Classes never re-enter the dispatcher** (agreed 2026-07-04). Cross-action
   reuse is a typed shared function both classes call — never a cloned payload
   with a rewritten `subAction` handed back to string matching. When a family
   is classed, its payload-rewriting delegation sites convert to direct calls
   and the internal action names they targeted die with the chain.
6. **One name per action — no advertised aliases** (agreed 2026-07-04, shipped
   same day). The de-alias pass deleted 45 alias spellings (34 fleet-verified
   groups; `delete`/`delete_asset`/`delete_assets` → `delete`, bare verb wins
   when the tool supplies the noun). Internal canonical literals that rewrites
   still target (`play_anim_montage`, `save_level_as`, `console_command`,
   `spawn_light`, `stream_level`) survive inside handlers only and die at
   classing. `bake_lightmap` vs `build_lighting` turned out to be two different
   implementations, not aliases — both kept, decls corrected.
   Declarations do not attempt runtime-outcome truth (that is the apply-receipt
   work — see the dogfood TODO).
