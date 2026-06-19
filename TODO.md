# Bridge TODO

A working list of concrete bridge improvements found while *using* the MCP bridge on
this project. Unlike `docs/Roadmap.md` (the large aspirational feature-phase plan),
this is a short, actionable backlog of real gaps/bugs hit in practice. Check items off
as they land.

> Origin: surfaced during the 2026-06 audio/options-menu work (verifying the asset
> read/write actions on `manage_asset`).

> **Fixture tip (2026-06-04):** a created Blueprint's variables are fully reachable by
> `get`/`set_asset_property` via the **CDO path** `<bp-path>.Default__<Class>_C`. Create a
> BP, `add_variable` the type you need (incl. `Set<>`/`Array<>`/`Map<>`), read/write it on
> the CDO path. This is how the reflection round-trips were verified.

---

## Roadmap — missing / incomplete capabilities

> Grouped by theme; reliability items cross-reference the **Bugs** section below. Rough
> priority: 🔴 near-term / unblocks current work · 🟡 valuable soon · 🟢 nice-to-have / deferred.
> The bridge is broad and mostly real (~36 tools, 600+ actions, most gameplay domains genuinely
> implemented) — this lists the genuine holes, not the large covered surface.

### A. Reliability & correctness — harden what already ships
Flakiness in shipped surface erodes trust during real authoring.
- ✅ **Transport mid-call drop — RESOLVED** (2026-06-06; see `docs/pull-architecture.md`).
  Solved **pull-only** rather than via the keepalive/result-cache idea: de-streamed `tools/call`
  to plain HTTP request/response, removed all server push + keepalive, deleted the WebSocket
  transport, and made mutating ops idempotent (fail-fast on name conflict) so a dropped response
  recovers by retry / state re-query. Verified live.
- ✅ **Widget-authoring ensures + save persistence** — RESOLVED 2026-06-06. (a) `remove_widget`
  now deletes via the engine's canonical `FWidgetBlueprintEditorUtils::DeleteWidgets`
  (`DeleteSilently`), which strips the widget+children variable→GUID entries (`OnVariableRemoved`)
  exactly as the UMG designer does — so the `SeenVariableNames` ensure no longer fires at all,
  rather than firing-and-self-healing. (b) The post-op error guard now recognises a *handled
  ensure* dump as a block (opens on the `=== Handled ensure ===` marker, closes when normal
  logging resumes) and downgrades the whole block to a warning — defense-in-depth for any other
  path (incl. the Common UI ensure below). (c) explicit `saveAfterCompile:true` bypasses the
  save *time-throttle* so structural deletes always persist. Verified live: create WBP → add
  button → `remove_widget` → `compile {saveAfterCompile:true}` → `compiled:true, saved:true`,
  **no** ensure in the log, fresh `.uasset` written. The §"add_common_* … handled ensure on a
  dirty WBP" bug (below) is now covered by (b) but its own root cause is still open.
- ✅ **`ReadHttpRequest` busy-poll** — DONE 2026-06-06. Both the header and body read loops now
  drain the socket in chunks (2KB) and block on `Socket->Wait(ESocketWaitConditions::WaitForRead,
  slice)` when idle instead of `FPlatformProcess::Sleep(0.001f)` spinning + byte-at-a-time `Recv`.
  Headers are scanned for the CRLFCRLF terminator with a 3-byte cross-chunk overlap; bytes read
  past the terminator carry over as the start of the body. Readable-with-no-pending-data is treated
  as a peer close. Mirrors the existing send-path `Wait(WaitForWrite)` idiom. Verified live: normal
  calls + a 9 KB multi-chunk-body CSV import (500 rows) round-tripped intact.
- ✅ **Automation test results (poll-by-runId)** — DONE 2026-06-07. `system_control run_tests`
  is no longer fire-and-forget: it now enumerates matching tests via
  `FAutomationTestFramework::GetValidTestNames` (substring `filter` over displayName / fullPath /
  testName, `maxTests` cap, default 50), builds an `ActiveAutomationRun` queue, and returns
  `{runId, status:"running", total, tests[]}` immediately. The subsystem `Tick()` drives the run
  one test per frame via `TickAutomationTestRun()`; poll `system_control get_test_results`
  (`runId` optional — defaults to the current/last run) for
  `{status, total, completed, passed, failed, results:[{test, command, success, errorCount,
  warningCount, errors[]}]}`. Added `system_control list_tests` (read-only enumeration).
  Seeded `McpBridge.SelfTest.{Pass,Latent,Math}` in `McpAutomationBridge_SelfTests.cpp` (simple,
  latent multi-frame, and parameterized/complex) so the loop has deterministic project-local
  tests. **Crash-and-fix worth remembering:** the editor loads `FAutomationWorkerModule`, whose
  own per-frame `Tick()` calls `ExecuteLatentCommands()` and *concludes* (`StopTest`) **any**
  active test the instant `GIsAutomationTesting` is set — even one we started directly. A v1 that
  unguarded-`ExecuteLatentCommands()`'d on the next frame hit `check(GIsAutomationTesting)` and
  hard-crashed the editor (`AutomationTest.cpp:676`). Fix: bind `OnTestEndEvent` to capture each
  result from whichever side calls `StopTest`, and guard *every* framework call with an
  immediately-preceding `GIsAutomationTesting` check (both ticks are game-thread-sequential, so
  the flag can't flip mid-function). Verified live 2026-06-07: 4/4 self-tests pass; a
  live-coding-patched failing assertion reports `failed:1` + the exact message; editor survives.
  Minor known interaction: starting tests directly leaves the worker's `bExecuteNextNetworkCommand`
  false, so for tests 2+ in a run *we* (not the worker) call the concluding `StopTest` (handled by
  the guarded fallback). Helper: `scripts/mcp-call.ps1` (one-shot MCP initialize + tools/call).
- 🟡 **Log/profiling streaming** — `docs/Roadmap.md` Phase 5 real-time streaming for logs +
  remote Insights profiling is still unstarted (test results above are now covered).

### B. Authoring capability gaps — new features that unblock real workflows
- ✅ **`bind_event_to_delegate` — functional widget data-binding DONE 2026-06-18** (the plan below, built + verified
  end-to-end). New `manage_blueprint` widget-authoring action: subscribes a widget event graph to a multicast
  delegate on an EXTERNAL object reached via an event output pin — authors `UK2Node_AddDelegate` + a
  signature-matched `UK2Node_CustomEvent` + an optional handler-body call (member-widget getter → function),
  appended to the END of the source event's exec run so repeated binds stack. Params: `widgetPath`, `eventName`,
  `ownerPin`, `delegateName`, opt `handlerEventName`/`handlerTargetWidget`/`handlerFunction`. Built clean per the
  audit findings: `FScopedTransaction`, `K2Schema->TryCreateConnection` (not raw MakeLinkTo), compile + `McpSafeAssetSave`,
  idempotent by handler-event name, no post-`Finalize` `AllocateDefaultPins` (dodges the duplicate-pin trap).
  Routing in `WidgetAuthoring()` (header → full rebuild done); handler in `McpAutomationBridge_WidgetAuthoringHandlers.cpp`.
  **VERIFIED end-to-end:** authored WBP_HUD.InitializeHud → AddDelegate(`AttributeComponent.OnHealthPercentUpdateDelegate`)
  → custom event → ProgressBar.SetPercent (6/6 links, deadNodeCount 0); **survived an editor reload** (byte-identical
  graph — the real test); **drove the bar at runtime** (PIE in L_CombatGym: `ReceiveDamage(5)` → health 20 → live
  bound ProgressBar `Percent=0.20`). Game side: new C++ `UCombatHudWidget : UUserWidget` (BlueprintImplementableEvent
  `InitializeHud(UAttributeComponent*)`), player BeginPlay creates WBP_HUD + AddToViewport + InitializeHud (push model).
  **Uncommitted** (bridge local on `dev`; game C++ on `main`) — Aaron's call. **Remaining:** (a) ✅ DONE — now drives
  Aaron's styled `WBP_Player_HealthBar` (its `SetPercent`→`ProgressBar_19.SetPercent` body authored via **`create_node`
  CallFunction** — the friction was `add_node` only; `create_node` resolves `memberName`+`memberClass` fine; marked
  `ProgressBar_19` `bIsVariable` to clear the "not blueprint visible" compile warning; PlayerHealthBar set visible;
  standalone test bar removed). Re-verified in PIE: `ReceiveDamage(40)`→health 60→live `ProgressBar_19.Percent=0.60`.
  (b) wire the magic bar (same action, `OnMagicPercentUpdateDelegate` → its own SetPercent/bar); (c) HUD-creation in
  player BeginPlay works in the gym but should guard the not-yet-possessed case (level-placed pawns) — e.g.
  `NotifyControllerChanged`/`IsLocallyControlled`; (d) `MakePinType` fix is Live-Coding-only this session (source
  edited) — bake in on the next full rebuild.
- 🟡 **(superseded by the above — kept for the design rationale) Functional data-binding for widgets — original PLAN (2026-06-17).**
  Surfaced planning the game's heal/health HUD. **Finding after auditing the widget-authoring system:**
  the *visual* scaffolding is rich and works (panels, `add_progress_bar`, `add_image`, `set_style`,
  layout, animations, `create_hud_widget`, `add_health_bar`), and **widget-self delegate binding is
  real** — `bind_on_clicked` (WidgetAuthoringHandlers.cpp ~L3967) authors a genuine
  `UK2Node_ComponentBoundEvent` for any `BlueprintAssignable` multicast delegate on a *child widget*
  (works for UButton, CommonUI, custom), optionally wiring the event to a self function. That is the
  proven K2-authoring pattern to build on (`FGraphNodeCreator` + compile + `McpSafeAssetSave` + self-layout).
  **But the actual binding the HUD needs is missing.** The "binding" actions that would wire a widget's
  *value* to live data are stubs that return success + an instruction string and wire **nothing**:
  - `create_property_binding` (~L4435): returns `instruction` "Create function … use Property Binding dropdown".
  - `bind_text` / `bind_visibility` / `bind_color` / `bind_enabled` (~L3784+): same — advisory only.
  - `set_widget_binding` (~L5474): only *verifies* the property is bindable, then saves; no binding made.
  - `add_health_bar` (~L4973): builds HBox+label+ProgressBar but calls `SetPercent(1.0)` **statically** —
    a full bar wired to nothing.

  **Gap:** no action connects a widget value to a runtime data source. For this project the target rail is
  the event-driven one (see `UAttributeComponent::OnHealthPercentUpdateDelegate` /
  `OnMagicPercentUpdateDelegate`, broadcast from `ReceiveDamage`/`AddHealth`/`AddMagic`): subscribe in the
  widget's Event Construct, update `ProgressBar.Percent` only on change. This is harder than
  `bind_on_clicked` because the delegate lives on an **external** object (the player's AttributeComponent),
  resolved at runtime — not a child-widget property — so it's `UK2Node_AddDelegate` + a `UK2Node_CustomEvent`
  handler + a get-source chain, NOT a `ComponentBoundEvent`.

  **Proposed work (staged; each stage independently testable, build live before moving on):**
  1. **`bind_event_to_delegate`** (generic, the core). Params: `widgetPath`, `delegateOwnerExpr` (how to
     reach the object carrying the delegate — see design decision below), `delegateName`,
     `handlerName` (custom event to create/reuse), optional `targetWidget`+`setterFunction`+`argName`
     so the generated custom event calls e.g. `ProgressBar::SetPercent(Percentage)` directly. Builds
     `UK2Node_AddDelegate` (off Event Construct) bound to the source's delegate, a `UK2Node_CustomEvent`
     matching the delegate signature, and the setter call. Reuse `bind_on_clicked`'s creator/compile/save/
     layout helpers. **Heed the `add_event` duplicate-default-pin trap (Bugs §, L623)** — create the custom
     event via the same canonical path that fix landed on, or chains die on the next editor load.
  2. **Source-resolution** for the HUD: author the Event-Construct chain to get the delegate owner. Cleanest
     and most testable is to NOT author a fragile `GetPlayerPawn→Cast→GetAttributes` chain generically, but
     to support a **provided self/library getter function** whose return feeds `AddDelegate`'s target.
  3. **Convenience HUD macro:** extend `add_health_bar` (and add `add_attribute_bar` / magic) with an opt-in
     `bindToAttribute` that composes stages 1–2 for the player AttributeComponent → bar `Percent`, and sets
     the bar's initial `Percent` from the live value instead of a hardcoded `1.0`.
  4. **Honesty fix (cheap, do alongside):** the advisory stubs (`create_property_binding`, `bind_*`,
     `set_widget_binding`) currently report `success:true` while wiring nothing — either make them real
     (UMG property binding = `FDelegateRuntimeBinding` + a generated getter) or have them return a clear
     "advisory only / use bind_event_to_delegate" status so callers aren't misled.

  **Design decision for Aaron (shapes the API — his call, not a guess):** HUD↔player link — *pull* (HUD
  resolves the player in Construct) vs *push* (player's BeginPlay calls a BlueprintCallable
  `InitializeHud(UAttributeComponent*)` on the widget, which then binds). Push is more robust (no
  Construct-timing/race on who exists first) and makes source-resolution trivial; pull needs no C++ change
  to the character. This choice decides stage-2's shape.

  **Build/test:** new action names go in `WidgetAuthoring()` in `McpConsolidatedActionRouting.h` (header →
  **full rebuild**, close editor first, `-NoUBA`); handler bodies are `.cpp`-only (`WidgetAuthoringHandlers.cpp`)
  so iteration after the first rebuild can use `system_control live_coding_compile`. Verify live on a scratch
  WBP: bind → compile → **reload editor** (the duplicate-pin bug only bites on reload) → PIE, take damage,
  watch the bar move. Do NOT land any of this without the live reload+PIE check.
- ✅ **`get_graph_details` 1-call graph-hygiene pass** — DONE 2026-06-10. Per-node `x`/`y`
  (`NodePosX/Y`), `enabledState` (`GetDesiredEnabledState()` → Enabled/Disabled/DevelopmentOnly),
  `linkCount` (sum of pin `LinkedTo`), `pinCount`, and `isOrphan` (pin-bearing node with zero
  links; pinless comment nodes are NOT flagged — they're connected by placement, not pins).
  Orphans, disabled clutter, and smooshed layout are now all visible in one round-trip.
  Verified live: fresh-BP ghost events report `Disabled`+orphan, an unconnected `K2Node_Self`
  reports `isOrphan:true` at its authored x/y, and `WBP_MenuLayer` EventGraph (27 nodes) shows
  correct link counts with the comment node unflagged. Original motivation: Aaron caught an
  orphaned `float * float` node and an overlapping-node mess that a names-only review missed;
  the scan previously needed N `get_node_details` calls.
- ✅ **DataTable row CRUD** — DONE 2026-06-06. Added 5 `manage_asset` actions:
  `add_data_table_row`, `edit_data_table_row`, `remove_data_table_row`, `get_data_table_rows`,
  `import_data_table` (+ short aliases `add_row`/`edit_row`/`remove_row`/`get_rows`/`import_rows`),
  routed in `HandleAssetAction` → `HandleDataTableRowOp`. Rows are populated by
  `FJsonObjectConverter::JsonObjectToUStruct` onto the `RowStruct` memory; CRUD goes through the
  engine's `FDataTableEditorUtils::{AddRow,RemoveRow}` + `BroadcastPostChange`; import uses
  `UDataTable::CreateTableFrom{CSV,JSON}String` (format auto-detected from the text if `format`
  omitted). `rowData` accepts a JSON **object OR a JSON-encoded string** (MCP clients stringify
  freeform objects — verified the string path is what the Claude client actually sends). add
  fail-fasts `ROW_ALREADY_EXISTS` (idempotent retry); add rolls back its row if `rowData` is
  bad (atomic); edit is a partial merge; remove is idempotent (`alreadyAbsent`); import is
  best-effort (success + surfaced `problems`, never fails on a benign note). Verified live end
  to end against `/Script/Engine.MirrorTableRow`. This unblocks the
  `set_common_button_input_action` workflow (populating a `CommonInputActionDataBase` table).
  **Verified end-to-end 2026-06-06**: create DT (`rowStruct=CommonInputActionDataBase`) →
  `add_data_table_row` → `add_common_button` (project's `WBP_MenuButton`) →
  `set_common_button_input_action` → the button's `TriggeringInputAction` handle persists to the
  saved asset pointing at the right DataTable + row, with a clean compile.
- ✅ **Material authoring — swept & verified working** 2026-06-07. Full graph-authoring path is
  solid end-to-end via the canonical **`manage_asset`** tool (which re-dispatches material
  sub-actions to `HandleManageMaterialAuthoringAction` — see the surface note below):
  `create_material` → `add_scalar_parameter` / `add_vector_parameter` / `add_texture_sample`
  (each returns a `nodeId`) → `connect_nodes` (targetNodeId `"Main"` + `inputName=BaseColor/
  Roughness/...`) → `get_material_info` (reads back nodeCount, parameters, expressions w/ pos,
  **and the connections array**) → `compile_material` (`compiled:true, saved:true`). No bugs in
  the core path. (Minor non-blocking robustness note: `add_vector_parameter` reads `defaultValue`
  only via `TryGetObjectField`, so an MCP client that *stringifies* the rgba object would get the
  default white — same stringify gotcha the DataTable fix handles both ways; only matters for the
  vector default, not connectivity.)
- ✅ **`create_folder` false `existsAfter:false`** — FIXED 2026-06-07. `manage_asset create_folder`
  created the folder (`MakeDirectory` → `success:true`) but reported `existsAfter:false` because it
  verified via `VerifyAssetExists` (→ `DoesAssetExist`), and a folder is never an asset. Now
  verifies the directory on disk via `DoesAssetDirectoryExistOnDisk`. Verified live (live-coding
  patch): fresh folder + `/Game` root both report `existsAfter:true`. `.cpp`-only change in
  `McpAutomationBridge_AssetWorkflowHandlers.cpp::HandleCreateFolder`.
- ✅ **Canonical tool surface — dead tool defs removed** 2026-06-07. `FMcpToolRegistry::Register`
  drops any tool whose name fails `IsCanonicalMcpToolName` (a hard-coded 22-name allowlist in
  `McpToolRegistry.cpp`), so 14 `McpTool_*.cpp` files were compiled but **never registered** —
  confusing because they look registerable but aren't. Deleted them (Material­Authoring/Skeleton/
  Texture/Navigation/Lighting/Volumes/Splines/Input/GameFramework/BehaviorTree/Performance/Pipeline/
  Sessions/WidgetAuthoring). No functional change: `tools/list` is still 22, and those families'
  actions remain reachable because they're folded into canonical tools via re-dispatch (e.g.
  `manage_asset` → `IsMaterialAuthoringAction`/`IsTextureAction`; `manage_blueprint` →
  `IsWidgetAuthoringAction`/`IsCommonUiAction`; `manage_ai` covers BT/nav). The action lists live in
  `McpConsolidatedActionRouting.h`, not the deleted tool-def files. The handlers (`Handle*Action`)
  are untouched.
- ✅ **Tool-family sweep #2 + batch fixes** 2026-06-07. Swept asset-creating actions across families
  (render_target, blueprint, state_tree, eqs_query, blackboard, smart_object, mass, inventory_ui,
  niagara, sequence, anim) for crashes / false-success / read-back gaps. **No crashes, no
  false-success** (the three suspects — smart_object/mass/inventory_ui — create real assets, just
  omitted a verification field). Findings were papercuts; fixed (verified live):
  - `create_animation_blueprint` / `create_animation_bp` advertised + documented but only
    `create_anim_blueprint` was handled → added the aliases.
  - `create_blend_space_1d`/`2d` returned a generic `CREATE_FAILED` when no skeleton was passed (a
    blend space requires a target skeleton) → now `SKELETON_REQUIRED` with guidance.
  - `manage_sequence` unknown-action error said "not implemented by plugin" → now `UNKNOWN_ACTION`
    echoing the attempted sub-action and pointing at `create`/`sequence_create`. (Sequence creation
    itself works via `create` — `create_level_sequence` was never a real action.)
  - `create_smart_object_definition` / `create_mass_entity_config` now return `success`/`existsAfter`
    (AddVerification) like other creates.
  - ✅ **`manage_effect create_niagara_system`/`create_niagara_emitter` UNKNOWN_ACTION — FIXED
    2026-06-07.** Both were advertised in the `manage_effect` schema but returned `UNKNOWN_ACTION`
    via the tool. Mechanism (traced in `McpAutomationBridge_EffectHandlers.cpp`): `manage_effect`
    dispatches with `Action == "manage_effect"` (Pattern A). The re-dispatch (~L235): when
    `Lower=="manage_effect"`, it routes the payload sub-action to one of 4 specials
    (`list_debug_shapes`/`clear_debug_shapes`/`spawn_niagara`/`set_niagara_parameter`) **or else to
    `create_effect`**; the `create_effect` branch has no case for system/emitter → catch-all.
    **Fix applied:** added `create_niagara_system` + `create_niagara_emitter` to the
    `NiagaraAuthoringActions` set (checked at L219, *before* the manage_effect re-dispatch), so they
    route to `HandleManageNiagaraAuthoringAction` (which already handles both via `subAction`, stamped
    into the payload by `NormalizeNativeManageEffectSubAction`). One-point change, no `Lower` rewrite.
    `create_niagara_ribbon` intentionally left on its existing `create_effect`-path handling
    (`bCreateRibbon`, L1684/1880) — works there, no need to move it. **TRAP avoided (the harmful
    revert from earlier):** did NOT make `Lower` resolve to the payload sub-action globally — that
    makes `Lower!="manage_effect"`, disabling the re-dispatch and breaking `debug_shape`/
    `create_effect`. Verified live (live-coding patch): `manage_effect create_niagara_system` and
    `create_niagara_emitter` both create real assets; `list_debug_shapes` still works (re-dispatch
    gate intact). `.cpp`-only.
- ✅ **Introspection gaps CLOSED 2026-06-10** (found 2026-06-07 during focus/nav work):
  (1) **`manage_blueprint get_default`** `{blueprintPath, propertyName}` — read-counterpart to
  `set_default`; same resolution rules (flat name, or one `Component.Property` hop through an
  object property on the CDO); returns `value` + `propertyType`. Verified:
  `attributeComponent.maxHealth` on BP_CPP_Character → `Int 100`; `DesiredFocusWidget` on
  WBP_PauseScreen → `{WidgetName: Master_Volume_Slider}`.
  (2) **`manage_blueprint list_functions`** `{blueprintPath}` — one call returns
  `functionGraphs` (+`isOverride` when the parent declares a same-named BlueprintEvent),
  ubergraph `events` (`kind`: Event/Override/CustomEvent, `graphName`, `isEnabled` so ghost
  placeholders are filterable), `macroGraphs`, `interfaces`. Verified on WBP_PauseScreen:
  `BP_OnHandleBackAction` flagged `isOverride:true`; ghost PreConstruct/Tick report
  `isEnabled:false`. Routing added to `ManageBlueprintCore()` (header → full rebuild done).
  NOTE for a future cleanup pass: `HandleBlueprintAction` contains a DUPLICATE dead
  `blueprint_set_default` block (~L4300 after this change; the live one is ~L2925 — first
  match wins in the if-chain). Didn't touch it mid-feature.
- ✅ **Dead legacy BT branches removed** 2026-06-07. The simplistic `add_composite_node`/
  `add_task_node`/`add_decorator`/`add_service` branch bodies in `McpAutomationBridge_AIHandlers.cpp`
  were unreachable behind the `USE_GRAPH_AUTHORING` guard (added with the BT fix); deleted the 194
  dead lines, build verified clean. Guard now flows straight into `configure_bt_node`.
- ✅ **BT authoring fixed — graph path roots trees, simplistic actions deprecated** 2026-06-07.
  Two-part fix (both verified live):
  - **Part A (the real gap):** `connect_nodes` now accepts `parentNodeId:'root'` — it finds the
    `UBehaviorTreeGraphNode_Root` and connects it to the child, so the child becomes the tree entry.
    Previously there was no way to attach the top node to the root (`'root'` → `NODE_NOT_FOUND`),
    leaving `hasRootNode:false` (non-runnable). Now a full graph flow works:
    `create` → `add_node{nodeType:Selector}` → `connect_nodes{parentNodeId:'root', childNodeId}` →
    `add_node` + `connect_nodes` for children → `get_ai_info` shows `hasRootNode:true, btNodeCount:2`.
    (`McpAutomationBridge_BehaviorTreeHandlers.cpp` connect_nodes, mirrors add_subnode's root lookup.)
  - **Part B (chose option a):** the broken simplistic `add_composite_node`/`add_task_node`/
    `add_decorator`/`add_service` (which `NewObject`'d an orphaned node and falsely reported success)
    now return `Error [USE_GRAPH_AUTHORING]` with a message pointing at `add_node`/`connect_nodes`/
    `add_subnode`. One guard in `HandleManageAIAction`; the legacy branches are now unreachable
    (trivial TODO: delete the dead branches).
  Original report below for context.
- 🔴 **(RESOLVED above) BUG: `manage_ai` simple BT node actions report false success (orphaned nodes)** — found
  2026-06-07. Repro: `create_behavior_tree {name:BT_MCPSweepTest, path:/Game}` →
  `add_composite_node {compositeType:Selector}` → `add_task_node {taskType:Wait}` →
  `add_decorator {decoratorType:Loop}` → `get_ai_info`. All three "add" calls return success
  (`"Added Wait task"` etc.), but `get_ai_info` reports `btNodeCount:1, childDecorators:[],
  services:[]` — i.e. only the root composite exists; the task and decorator were `NewObject`'d,
  `MarkPackageDirty`'d, and **never attached** to any composite or graph (see
  `McpAutomationBridge_AIHandlers.cpp` `add_task_node`/`add_decorator`/`add_service` — they create
  the node then immediately respond OK without wiring it in). `add_composite_node` only sets
  `BT->RootNode = NewNode` (no children, no editor graph). Architectural root cause: a UE
  BehaviorTree is authored via its **editor graph** (`UBehaviorTreeGraph` + `UBehaviorTreeGraphNode_*`),
  which compiles down to `RootNode`; setting `RootNode`/orphaned nodes directly is fragile — opening
  the asset in the BT editor recompiles from the (empty) graph and **wipes** the hand-set `RootNode`.
  The correct path is the graph-based `manage_ai` actions `create`/`add_node`/`connect_nodes` →
  `HandleBehaviorTreeAction` (verified `create` works; `add_node`/`connect_nodes` need the
  **BehaviorTreeEditor** editor plugin enabled). **Fix is a decision for Aaron** (not done
  autonomously — it's a contract change): either (a) make `add_composite_node`/`add_task_node`/
  `add_decorator`/`add_service` error-with-guidance redirecting to the graph path, or (b) delete
  them, or (c) reimplement them on top of the graph builder. What DOES work today: BT/blackboard
  **asset** creation (`create_behavior_tree`, graph `create`, blackboard assets) and `get_ai_info`.
- 🟡 **Enhanced Input authoring depth** — IMC/IA creation + key mapping exist (Input group);
  verify round-trips and fill the sparse trigger/modifier authoring (modifier/trigger
  factories are thin).
  - **Specifics found authoring IA_Heal end-to-end 2026-06-17** (worked fully via the bridge —
    `create_input_action` → `set_input_trigger Down` → `add_mapping` ×2 → `set_default` HealAction
    on `BP_CPP_Character` → `compile`; all readback-verified). The rough edges:
    1. `create_input_action` takes no `valueType` — to set `ValueType` you must leave the input
       group for `set_asset_property propertyName=ValueType value="Boolean"`. (No-op for bool IAs
       since the `UInputAction` default is already Boolean, but undiscoverable.) → add an optional
       `valueType` param to `create_input_action`.
    2. `get_input_info` on a `UInputAction` reports `valueType`/`consumeInput` but NOT its own
       `Triggers[]` — only IMC mappings carry a `triggerCount`. So after `set_input_trigger` you
       can't confirm the trigger *class* via the input group. → emit a `triggers[]` (class names)
       for IAs in `get_input_info`.
    3. `set_input_trigger` is append-only (`InAction->Triggers.Add`, no dedup) → re-running an
       otherwise-idempotent authoring script stacks duplicate triggers. → add `replace:true`
       (Empty before Add) or dedup by class.
- 🟡 **Common UI completeness** — have: add button/text/border, assign style, bind input action,
  and (NEW 2026-06-07) **style-asset creation**. Missing / runtime-only: activatable-widget stack
  push/pop + focus/nav, input-action→click wiring. Some is inherently runtime
  (see `COMMONUI_INTEGRATION_PLAN.md`).
  - 📝 **Gamepad focus/nav (authoring)** — design stub parked in `COMMONUI_FOCUS_NAV.md`. Key
    finding: activation flags (`bAutoActivate`/`bIsModal`/`bSupportsActivationFocus`/
    `bAutoRestoreFocus`) are plain UPROPERTYs settable via `inspect set_property`, and the
    desired-focus *target* is the **engine-native `DesiredFocusWidget`** UPROPERTY (settable via
    `manage_blueprint set_default {propertyName:"DesiredFocusWidget.WidgetName"}`) — no BP override or
    C++ base class needed. Applied to the Pause/Options menus (Aaron's uncommitted asset changes).
  - ✅ **Focus/input introspection + drive (runtime)** BUILT 2026-06-07 — the observe/verify loop
    CommonUI was missing over MCP (design + as-built + corrections in `COMMONUI_FOCUS_INPUT.md`):
    `inspect ui_focus` (focused widget + path, active activatable + its desired-focus target, active
    activatable stack, current input type/gamepad, active-root bound actions) and
    `control_editor simulate_nav` (faithful gamepad/keyboard nav via `ProcessKeyDownEvent` + post-nav
    focus read-back). New TU `McpAutomationBridge_FocusInputHandlers.cpp`; uses only public CommonUI
    APIs (`FindActivatable`/`GatherActiveBindings`/`GetDesiredFocusTarget`) — NOT the private
    `DebugDumpRootList` the design assumed. **Build fix:** had to add the `CommonInput` module to
    `Build.cs` (sibling of CommonUI, not transitively linked) for `UCommonInputSubsystem`. Verified
    live: actions reachable, no crashes, `inputType`/`activatableStack` populated in PIE. **Ops
    lesson:** never force-kill the editor (next launch hangs on the unclean-shutdown modal) — quit
    via `control_editor console_command QUIT_EDITOR`.
  - ✅ **`simulate_nav` determinism — BOTH gates fixed (2026-06-08 + 2026-06-09).** The faithful
    drive was flaky for two stacked reasons, each an engine gate on synthesized input:
    (1) *focus path* — CommonUI's PIE check (`RefreshCurrentInputMethod`,
    `CommonInputPreprocessor.cpp:214-221`) requires the game viewport in the slate user's focus
    path → fixed 06-08 with focus-stabilize (`SetUserFocusToGameViewport(0)` iff outside, opt-out
    `stabilizeFocus:false`). (2) *OS foreground* — `FCommonInputPreprocessor::IsRelevantInput`
    refuses to reclassify the input method while `FSlateApplication::IsActive()` is false
    (`:176-178/192`), the normal state for a bridge-driven editor → fixed 06-09 by bracketing the
    key delivery with `SetHandleDeviceInputWhenApplicationNotActive(true)` (+ restore); response
    reports `slateAppActive`. Repro pre-fix: same spec flipped `Gamepad` ↔ `MouseAndKeyboard`
    depending on window foreground state. Post-fix: 3 byte-identical runs, `inputType=Gamepad`
    every time. Full RCA (adversarially verified against engine source):
    `COMMONUI_FOCUS_INPUT.md` 2026-06-09 section. The earlier `FInputDeviceId` suspicion was a red
    herring (`IsRelevantInput` reads only `GetUserIndex()`; the uint32 ctor already sets device 0).
  - ✅ **(FIXED 2026-06-09, Aaron-authorized)** — asset-only: `WBP_VolumeSlider` got
    `bIsFocusable=true` + its own `DesiredFocusWidget="VolumeSlider"`, so the engine's
    `UUserWidget::NativeOnFocusReceived` relay (`UserWidget.cpp:2414-2424`) forwards the router's
    desired-target focus from the wrapper row to the inner AnalogSlider. Fixes pause AND options
    screens (options' DesiredFocusWidget resolves to its own slider row — same bug). Verified
    live: activation focus lands on Master's slider, DPad Down moves row→row, DPad Left/Right
    adjusts only the focused row's value; `pause_menu.json` now 3× byte-identical
    `4 pass, 0 fail`. Gotcha worth remembering: CDO `set_default` doesn't reach already-loaded
    sub-widget template instances (reinstancing preserves old values) — had to
    `inspect set_property` the 8 loaded tree-template objects (subobject paths under
    `WBP_PauseScreen{,_C}:WidgetTree.*` / `WBP_OptionsScreen{,_C}:WidgetTree.*`); disk needed
    nothing (no serialized delta). Main-repo asset changes left uncommitted for Aaron.
    Original report below for context.
  - 📝 **Product bug surfaced by the layer (Aaron's side, recommendation only, 2026-06-09):**
    `WBP_PauseScreen.DesiredFocusWidget` targets `Master_Volume_Slider` — a non-focusable
    `WBP_VolumeSlider` wrapper (`URhyaVolumeSlider : UUserWidget`, `bIsFocusable=false`, no focus
    forwarding) — so the router's `SetFocus` silently no-ops (no success check,
    `UIActionRouterTypes.cpp:1657-1661`; the viewport fallback is unreachable when the target is
    non-null) and the pause menu never grabs focus in ANY input mode. Caught live:
    `Focused desired target Master_Volume_Slider` + `PIE: Warning:` (does-not-support-focus) in
    adjacent log lines. Recommended fix: expose the inner `VolumeSlider` (e.g.
    `URhyaVolumeSlider::GetFocusWidget()`) and override `BP_GetDesiredFocusTarget` on
    `WBP_PauseScreen` to return it — NOT just `bIsFocusable=true` on the wrapper (focus would land
    on a dead container that doesn't relay Left/Right to the slider). The
    `tests/ui-nav/pause_menu.json` `focusedWidgetNot` assertion stays red until fixed.
  - ✅ **Nav-regression layer (Aaron's #1) — BUILT 2026-06-08, deterministic 2026-06-09.**
    `scripts/ui-nav-test.ps1` (declarative spec runner over MCP HTTP: play/resolvePC/call/nav steps
    + inPie/inputType/stackContains/stackDepth/focusedWidget/focusedName asserts, exits non-zero on
    fail) + `tests/ui-nav/pause_menu.json` golden path + `tests/ui-nav/README.md`. Usable as a CI
    gate now.
  - ✅ **Spec suite expanded (2026-06-09, Aaron's #1 follow-through):** 4 specs, 12/12 runs green
    deterministic — `main_menu.json` (flip→desired-focus→nav), `options_roundtrip.json` (full pad
    push/pop/focus-restore journey), `pause_menu_kbm.json` (keyboard variant via the
    stabilize-before-activate trick — no flip exists for keyboard, so activation-time desired focus
    carries the assert). Runner gained `focusPathContains` (CommonButton leaves are unnamed internal
    `SCommonButton`s, so button asserts match any UMG name on the focus path).
  - ✅ **PRODUCT BUG FIXED (2026-06-09, main repo): gamepad A never clicked focused menu buttons.**
    Found by `options_roundtrip.json`: keyboard Enter clicked `Button_Options`, gamepad
    FaceButton_Bottom did nothing. Root cause (engine source): CommonUI's analog cursor
    preprocessor intercepts the virtual-accept key and, with the default
    `CommonUI.ShouldVirtualAcceptSimulateMouseButton=true` (`CommonAnalogCursor.cpp:25-28`,
    handling at `:339-372`), converts it into a **simulated mouse click at the cursor position** —
    the focused button never receives the key, defeating the project's
    `CommonButtonAcceptKeyHandling=TriggerClick` (DefaultGame.ini). Affects real pads identically.
    Fix: `CommonUI.ShouldVirtualAcceptSimulateMouseButton=0` under `[SystemSettings]` in
    `Config/DefaultEngine.ini` (+ set live via console for the running session). Verified on the
    pad end-to-end: A pushes options, A on Button_Back pops, focus auto-restores.
  - ✅ **(FIXED 2026-06-09, Aaron-authorized) CommonInput `InputData` authored over the bridge.**
    Built the full chain headlessly: `create_data_table` →
    `/Game/UI/Input/DT_UIInputActions` (rowStruct `CommonInputActionDataBase`) +
    `add_data_table_row` ×2 (`DefaultClick`: LMB / Gamepad_FaceButton_Bottom; `Back`: Escape /
    Gamepad_FaceButton_Right — note the row field is `DefaultGamepadInputTypeInfo`, NOT
    "GamepadInputTypeInfo") → `create_blueprint` `/Game/UI/Input/BP_UIInputData` (parent
    `CommonUIInputData`) → `set_default` the two `FDataTableRowHandle`s (JSON object form works) →
    `[/Script/CommonInput.CommonInputSettings] InputData=...BP_UIInputData_C` in DefaultGame.ini.
    **Gotcha:** the live settings CDO write does NOT take effect (`bInputDataLoaded` caches null;
    bridge `set_property` doesn't fire `PostEditChangeProperty`) — needs the editor restart to load
    from config. **Second gotcha (ops):** live-coding patches are session-scoped — after the editor
    restart the on-disk bridge DLL was stale (no focus-stabilize/inactive-input/rename fixes) until
    one `live_coding_compile` re-patched everything from source. Verified on the pad: options
    activation binds `Legacy_DT_UIInputActions_Back` (first non-empty `boundActions` ever),
    **gamepad B pops the screen**, focus restores, zero `no action provided` in the fresh session
    log. `options_roundtrip.json` extended with the B-pop leg (`11 pass, 0 fail` ×3; full suite
    12/12 runs green on the fresh editor). Original report below for context.
  - 📝 **(resolved above — original report) OPEN (Aaron): CommonInput `InputData` not configured → back-handler binding fails.**
    `WBP_OptionsScreen` has `bIsBackHandler=true`, but the project has no
    `[/Script/CommonInput.CommonInputSettings] InputData` (a `UCommonUIInputData` BP naming
    `DefaultClickAction`/`DefaultBackAction` rows in a `CommonInputActionDataBase` table). On every
    options activation CommonUI logs `Cannot create action binding for widget
    [WBP_OptionsScreen_C_0] - no action provided`, and **gamepad B cannot pop the screen** (the
    back handler never binds). Proper fix = author the input-data asset + table (the
    `create_data_table`/`add_data_table_row`/`set_common_button_input_action` actions cover the
    table side) and set it in Project Settings → Common Input. Doable over the bridge on request.
  - ✅ **Generic `add_widget` + `wrap_root` (2026-06-09)** — two widget-authoring gaps hit while
    building the CommonUI action bar: (1) **`add_widget`** `{widgetPath, widgetClass, name,
    parentSlot?}` adds ANY `UWidget` subclass (loaded short name, `/Script/Module.Class`, or
    `/Game/....Name_C`; probes CommonUI/CommonInput/UMG script modules; rejects abstract) via the
    same `SafeAddWidgetToTree` path as the typed adds — no more "no typed action for this class"
    dead-ends (used for `CommonActionWidget` + `CommonBoundActionBar`). (2) **`wrap_root`**
    `{widgetPath, panelType?=Overlay, name?}` constructs a panel, makes it the root, and re-slots
    the old root as its first child (Fill/Fill) — unblocks adding overlay chrome to a WBP whose
    root is a non-panel (e.g. `WBP_MenuLayer`'s `CommonActivatableWidgetStack` root), where the
    add_* root-replacement path correctly refuses. Both verified live (action-bar build below);
    suite stayed 12/12 green after the MenuLayer root restructure. **Built with them (game side,
    uncommitted):** `WBP_BoundActionButton` (CommonBoundActionButton subclass; HBox +
    `InputActionWidget` CommonActionWidget + `Text_ActionName` CommonTextBlock — exact BindWidget
    names required) + `CommonBoundActionBar` "ActionBar" in `WBP_MenuLayer` under a new
    `RootOverlay` (bottom-right, `ActionButtonClass` set via subobject-path `set_property`) +
    `bIsBackActionDisplayedInActionBar=true` on `WBP_OptionsScreen`. Verified in PIE: options open
    → bar `entries=1`, entry label "Back"; B pop → `entries=0`. NOTE reconfirmed: `screenshot`
    captures the scene render only (`FViewport::ReadPixels`) — **UMG/Slate UI is not in the
    capture**; live-tree probes (`get_num_entries`, entry text) are the verification path until an
    offscreen-composite capture exists.
  - ✅ **`add_function override:true` + `remove_function` (2026-06-09)** — BP function-OVERRIDE
    authoring (the designer's "Override" dropdown): `add_function {blueprintPath, functionName,
    override:true}` finds the parent `UFunction`, validates `FUNC_BlueprintEvent`, and builds the
    override graph with the parent signature, returning `entryNodeId`/`resultNodeId` for wiring.
    **Engine gotcha that cost a broken graph:** the signature source must be the OWNING CLASS —
    `AddFunctionGraph<UClass>(BP, Graph, false, OwnerClass)` exactly as `SMyBlueprint.cpp:2878`
    does; passing the `UFunction` (`<UFunction>`) creates a same-named LOCAL function and the
    compiler rejects the duplicate ("function name is already used"). `remove_function` (RemoveGraph
    by name, idempotent) added as the inverse + repair tool. First real use: overrode
    `BP_OnHandleBackAction` on `WBP_PauseScreen` (GetOwningPlayer → cast → `TogglePause`, return
    true so the default deactivate is skipped) — gamepad B now unpauses, with the pause screen's
    own `CommonBoundActionBar` showing "Back" while paused. Node-wiring notes: pure functions
    (e.g. `GetOwningPlayer`) have no exec pins; an override's auto-created entry comes pre-wired to
    the result node (break it first); the result pin is `ReturnValue`; the cast output pin name has
    a space (`AsGOSPlayer Controller`). This is the primitive the #2 "activatable boilerplate with
    GetDesiredFocusTarget wired" scaffolding needs.
  - ✅ **Introspection gaps flagged by Aaron (2026-06-09) — CLOSED same night.** Every
    `execute_python` probe from the gamepad-menu sessions now has a typed action:
    (a)+(b) **`inspect find_objects`** `{className, pathContains?, propertyNames[]?, exactClass?,
    includeCdo?, limit?}` — `TObjectIterator` discovery with reflected property readback via
    `McpPropertyReflection::ExportPropertyToJsonValue`. Covers the template-staleness hunts
    (`bIsFocusable` on `WBP_*:WidgetTree.*` archetypes — archetype subobjects ARE returned, only
    `RF_ClassDefaultObject` is filtered by default), live PIE widget state (`AllEntries` of a
    `CommonBoundActionBar`, slider `Value`, `bIsActive` of audio components), and slot-object
    discovery. Reports `matched`/`truncated` so capped results are never silent.
    (c) **`get_widget_slot_info`** now returns `slotObjectPath` (the exact subobject path to feed
    `inspect set_property` — no more guessing `OverlaySlot_N` names) + a generic `slotProperties`
    dump for ALL slot types (was canvas-only).
    Also fixed in the same pass: `add_common_button`/`add_common_text`/`add_common_border` accept
    `name` as an alias for `slotName` (the param mismatch that silently named a widget "CommonText"
    when the caller passed `name`, forcing a rename_widget detour).
  - 📝 **ui-nav suite operational constraints (2026-06-09):** the drive layer requires an
    **idle editor** — concurrent human use of the same editor session breaks Slate focus
    determinism (observed: a manual floating-window PIE session left state that made
    `SetUserFocusToGameViewport` silently fail until an editor restart; all specs red, restart →
    all green). `simulate_nav` now reports `gameViewportRegistered` + `focusInViewportAfterStabilize`
    diagnostics so a failing run says WHY. Also remember: live-coding patches are session-scoped —
    first bridge call after an editor restart should be `live_coding_compile` (or do a full
    Build.bat rebuild to refresh the on-disk DLL).
  - ✅ **Style-asset creation** DONE 2026-06-07: `create_common_button_style` /
    `create_common_text_style` (via `manage_blueprint`). CommonUI styles ARE classes (assigned with
    `SetStyle(TSubclassOf<...>)`), so each creates a Blueprint subclass of `UCommonButtonStyle` /
    `UCommonTextStyle` via `FKismetEditorUtilities::CreateBlueprint` + `McpSafeAssetSave`, returning
    the generated-class path (`/Game/.../Name.Name_C`) that `set_common_button_style` /
    `set_common_text_style` accept directly. Params: `name` (required, bare), `path` (default
    `/Game/UI/Styles`). Idempotent (`alreadyExisted`). Closes the "could assign but not create" gap.
    Verified live: created both styles, then `add_common_text` + `set_common_text_style` accepted the
    created text style (passes the `IsChildOf(UCommonTextStyle)` check) — full create→assign pipeline.
    Follow-up if wanted: optional CDO property params (text size/color; button brushes are heavier).
- 🟡 **Asset import pipeline** — `import` works (Interchange) but there's no post-import
  configuration: texture compression/sRGB/streaming, audio codec/sample-rate, mesh
  LOD/Nanite/collision. Add a post-import `configure_import` pass (or params on `import`).
- 🟢 **GAS ability-graph depth** — GAS scaffolding is broad (create ability/effect/attribute/
  cue BPs), but multi-step ability *logic* (wait-for-input → montage → apply-effect) is manual
  graph wiring. Templated ability-task chains would speed the boss/combat work.
- ✅ **Montage introspection (read-back) — DONE 2026-06-16.** `get_animation_info` now returns
  `sections[]` ({name,startTime,nextSection}), `blendInTime`/`blendOutTime`/`autoBlendOut`, and
  `slots[]` ({slotName, segments:[{anim,startPos,length}]}); verified live on AM_Block. Also
  expanded `get_input_info` to list IMC `mappings[]` (action/key/trigger+modifier counts) and to
  accept `blueprintPath` (the `manage_networking` re-dispatch exposes that, not `assetPath`, so it
  was previously uncallable). Original report below.
  - 🟡 found 2026-06-16 (block hold-to-block
  work). `animation_physics get_animation_info` returns only counts (`numSections/numSlots/
  numNotifies/duration`); verifying montage *state* required `execute_python` for everything that
  matters: per-section `{name, startTime, nextSectionName}` (`composite_sections`), `BlendIn/
  BlendOut.BlendTime`, `enable_auto_blend_out`, and **slot names + segment anim refs** — the last
  isn't even Python-reflectable (`slot_anim_tracks` is not an exposed UPROPERTY; needs native
  `FSlotAnimationTrack` access, which is exactly why this belongs in the bridge). The authoring
  side (`add_montage_section`/`set_blend_in`/`set_blend_out`/`link_sections`) exists, but there's no
  read-back to confirm it landed — every montage verify this session went through raw python. Add
  the fields to `get_animation_info` (or a new `get_montage_details`): `sections[]`, blendIn/out,
  autoBlendOut, `slots[]` with segment animation paths. Aligns with the prefer-a-real-action rule.

### C. Runtime / verify loop — mostly HAVE; close the narrow gaps
The bridge already enters/exits PIE (`control_editor play`/`stop`/`eject` via
`RequestPlaySession`/`RequestEndPlayMap`), frame-steps (`single_frame_step`,
`set_fixed_delta_time`), injects `simulate_input`, and reads live state (`inspect
runtime_report`/`pie_report`/`find_by_class`).
- ✅ **Visual verify (synchronous screenshot)** — DONE 2026-06-06. `control_editor screenshot`
  now captures the active viewport synchronously (`FViewport::Draw` + `ReadPixels` →
  `FImageUtils::PNGCompressImageArray` → file) so the PNG is fully written to the returned
  absolute `path` *before* the call returns — an agent can read it back immediately to evaluate
  the result (replaces the old fire-and-forget `FScreenshotRequest`, which only wrote at
  end-of-frame and couldn't be awaited from a GameThread handler). Optional `maxWidth` downscales
  (caps only). Caveat: needs a laid-out viewport — a **minimized** editor window has a zero-size
  viewport and returns `VIEWPORT_NOT_AVAILABLE`. Future: an offscreen render-target path would
  make capture fully window-independent (truly headless).
  Verified 2026-06-06 it also captures **PIE/gameplay**: `GetActiveViewport()` returns the PIE
  viewport during `control_editor play`, so the same call grabs the live third-person gameplay view
  (player + combat) with no extra code. author->play->screenshot->evaluate is now a complete
  self-verify loop for gameplay, not just the editor.

Remaining gaps:
- ✅ **Input-injection fidelity — VERIFIED + analog injection BUILT (2026-06-10).**
  Verified live in PIE (HubWorld, main menu → Play → gameplay):
  - `key_down`/`key_up` (existing) DO drive Enhanced Input gameplay — W moved the pawn at
    MaxWalkSpeed via the `PlayerController->InputKey` PIE-direct path; release stopped it dead
    (positions byte-identical across 1.5s, velocity 0).
  - **NEW `simulate_input type:analog`** `{key, value, route?}` — the missing axis injection for
    gamepad sticks/triggers and mouse look. `route:'pie'` (default) builds
    `FInputKeyEventArgs::CreateSimulated(Key, IE_Axis, value)` → `PC->InputKey` (focus-immune);
    `route:'slate'` sends a real `FAnalogInputEvent` through `ProcessAnalogInputEvent` with the
    simulate_nav inactive-app bracket, exercising input preprocessors (CommonUI analog cursor) —
    use it to test the layer the pie route bypasses. Verified: LeftY=+1 → pawn runs (value
    persists from ONE sample, exactly like hardware's change-driven delivery); LeftY=0 → clean
    stop. RightX drives ControlRotation continuously, zero freezes it. MouseX/MouseY take
    per-event deltas (300 → 52.5° yaw snap; mouse axes reset each frame, no persistence —
    faithful). Slate route passes gameplay input through CommonUI unconsumed and stops clean too.
  - Note: `mouse_move` remains absolute-cursor-only (`SetCursorPos`, handledBySlate, never
    routedToPIE) — camera look injection is `analog MouseX/MouseY`, not mouse_move.
  - **Movement-latch finding (Aaron's 2026-06-09 report) — SOLVED 2026-06-10, root cause was
    MISSING DEAD ZONES, not a latch.** First pass (perfect 0.0 injections) wrongly exonerated the
    project: every route stopped clean. Aaron reported it still repro'd single-editor → the gap
    was that real sticks never return to exact 0 (settle at ±0.03–0.10). String-scan showed ZERO
    `InputModifierDeadZone` in all 10 IAs + 3 IMCs (Enhanced Input has no default dead zone;
    legacy input's 0.25 was lost in migration). Reproduced over the bridge with the new analog
    injection: LeftY=0.08/RightX=0.05 → continuous 80 u/s walk + 0.46°/s pan, forever. Fix:
    `InputModifierDeadZone` (default 0.2 radial) on the 3 stick mappings (Gamepad_Left2D→Move,
    Gamepad_LeftX→Right, Gamepad_Right2D→Look, dead zone ordered before Negate) in BOTH
    IMC_CharacterContext variants — mapping-level, NOT action-level, so Mouse2D look deltas stay
    raw. Verified: drift values now produce byte-identical position/rotation over 4s; 0.9
    deflection still runs full speed and zeroes to a dead stop. Main-repo asset changes left
    uncommitted for Aaron. Gameplay regression specs (hold → velocity>0; release+drift →
    velocity→0) are now scriptable — a drift-tolerance spec would pin this permanently.
- ✅ **C++ automation test runner** — DONE 2026-06-07 (see §A "Automation test results").
  `run_tests`/`get_test_results` execute real `FAutomationTestFramework` tests and return
  pass/fail/errors, so the author→(play)→assert loop is scriptable for any C++ automation test
  (incl. functional/latent tests, which the worker drives over real frames). Gap that remains:
  authoring NEW tests still needs a `.cpp` + rebuild (or live-coding patch) — there's no
  bridge action to generate a test stub.
- 🟢 **Gameplay-aware assertion helpers** — optional sugar over `runtime_report`/`inspect`
  (read an attribute, confirm an ability tag) for combat verification.

### D. Long tail — official `docs/Roadmap.md` phases 27+ (note & defer)
`Roadmap.md` lists phases 27+ as unstarted: PCG, sky/weather/water, advanced rendering
(ray tracing/post-process), cinematics & Movie Render Queue, **data/persistence & SaveGame**,
build/deploy, editor utilities, and plugin integrations (MetaHuman, Quixel/Megascans,
Wwise/FMOD, EOS/Steam, Chaos, accessibility, modding). Mostly not relevant to
RhyaTowerOfWishes' near-term needs — tracked there, not re-listed here. The one to watch for
a shipping game: **SaveGame / persistence authoring** (Phase 31) — promote if the game needs it.

---

## Bugs (found while using the bridge — track, fix when convenient)

### 2026-06-18c — Cleanup pass LANDED (Batches 1–3, built + live-verified against the editor)
Worked the 34-finding audit + the 2026-06-18b friction list into three batches, each rebuilt/live-coded
and verified against the running editor. Bridge source updated (local `dev` fork, uncommitted). Final
bake-rebuild done so the on-disk DLL matches source.

**Batch 1 — fail-fast / no lying-success.** All verified to return NOT_IMPLEMENTED or an honest result:
- WidgetAuthoring stubs → NOT_IMPLEMENTED with guidance: `bind_text`/`bind_visibility`/`bind_color`/
  `bind_enabled`, `create_property_binding`, `set_widget_binding`, `apply_style_to_widget`,
  `set_animation_speed`, `add_animation_keyframe`, `set_animation_loop` (no more success-while-wiring-nothing).
- `add_ability`/`grant_ability` (GAS), `add_eqs_context` (AI), `set_datalayer` `#else` (WorldPartition) → honest errors.
- Inventory `add_inventory_functions`/`add_equipment_functions` → dropped the fake "(implement in Blueprint)"
  function entries; report only the real vars/dispatchers actually added (`dispatchersAdded`).
- `GetSlotName` accepts `name`; `add_progress_bar` honors it (no more auto-"ProgressBar").
- `add_event` accepts `eventName` as an alias for `eventType` on the override path (was → blank GUID custom event).

**Batch 2 — graph-authoring correctness.** Live-verified:
- `bind_on_hovered`: was a lying stub; now shares `bind_on_clicked`'s real ComponentBoundEvent path
  (default delegate `OnHovered`, `functionName` alias for `targetFunction`) — dedups the two handlers.
- `remove_function`: now compiles + saves after `RemoveGraph` (was false success — removal never persisted;
  verified ScratchFn actually gone afterward).
- `add_node` CallFunction: `memberName`(+`memberClass`) delegates to the `create_node` factory (real resolved
  node, verified Print String with pins); empty function → fail-fast INVALID_ARGUMENT. No more "None" node /
  compile-break / PIE-hang.

**Batch 3 — widget-authoring save discipline (data-loss fix).** Disk-verified (mtime/size advance):
- New `FinalizeWidgetEdit(WidgetBP, Payload)` (mark + opt-in save, default true) routed into all 26 `add_*`
  tree-construction handlers — edits now persist (were lost on editor close/GC). Pass `"save": false` to batch.
- `create_widget_blueprint` now saves the new `.uasset` (was created in-memory only).
- CommonUI `add_common_button`/`add_common_text`/`add_common_border` + style setters + `set_common_button_input_action`
  now save (added `McpSafeOperations` include/`using` — they were relying on a unity-build using-leak).

**Still open (deliberately deferred — not bugs / higher risk / gated):**
- **C `add_*` dedup — DONE (2026-06-18d).** Extracted file-local helpers `ConstructWidgetForAdd`
  (load+construct+register, returns the widget) + `AddFinalizeRespondWidget` (SafeAddToTree + FinalizeWidgetEdit
  + Validate + respond) in the `WidgetAuthoringHelpers` namespace — they call the subsystem's public send helpers
  via a `Self` pointer, so they stay `.cpp`-only. Collapsed **all 23 add_* handlers** onto them: 11 §19.2 layout
  panels (canvas/hbox/vbox/overlay/grid/uniform_grid/wrap_box/scroll_box/size_box/scale_box/border) + 12 §19.3
  leaf widgets (text_block/rich_text_block/image/button/check_box/slider/progress_bar/text_input/combo_box/
  spin_box/list_view/tree_view). Pure adds are ~5 lines; property widgets do `construct → Cast<T> → set props →
  addfinalize` (property blocks preserved verbatim); text_input picks single/multi-line class first. ~1,100
  boilerplate lines removed. Build clean (both waves) + live-verified (size_box WidthOverride=250, progress_bar
  Percent=0.7 persisted; correct trees via name alias). **Remaining C (🟡, optional):** split the still-large
  `HandleManageWidgetAuthoringAction` function + fold the duplicated error literals / response-tail; not a bug.
- **D** — extract the shared graph-event-chain helper; add `FScopedTransaction` + `TryCreateConnection` to
  `bind_on_clicked`/`bind_on_value_changed` (🟢 robustness).
- **B 🟡/🟢** — Inventory/Interaction structural adds save-without-compile (stale CDO); InputHandlers
  `add_mapping`/`remove_mapping` missing `Context->Modify()`.
- `manage_audio` `create_sound_class`/`create_sound_mix` hang (needs in-editor debugger — Aaron-gated).
- Widget-authoring params undeclared in the `manage_blueprint` schema (doc-only); no runtime component-function call.

### 2026-06-18e — Adversarial review of the cleanup change set (cd3c711..HEAD)
Ran a 15-agent verify workflow (6 parallel reviewers by dimension → independent skeptic refutes each finding)
over tonight's Batches 1–3 + the full C dedup. Verdict: **the work is sound** — 7 confirmed findings, 6 minor/
benign, only 1 real bug. Fixed in `651bbe4`:
- [x] **`add_event` eventName→eventType alias could mis-route a custom-event request — FIXED.** When a caller
  passed BOTH `customEventName` and `eventName` but no `eventType`, the alias copied eventName into eventType →
  override path → EVENT_NOT_FOUND (the old code created the custom event). Guard: alias only applies when
  `customEventName` is also absent. Verified both paths live.
- [x] **Helper error messages genericized — RESTORED.** `ConstructWidgetForAdd`/`AddFinalizeRespondWidget` now
  name the widget class again in the construct-failure / tree-add-failure messages (error codes were already kept).
- Benign/intentional (left as-is): `GetSlotName` now honors `name`/`widgetName` for add_* (the deliberate Batch-1
  alias); all add_* now emit the `verification` field (additive, harmonizes the 13 outliers with their siblings).

Real bug FOUND by the review but DEFERRED (off critical path, niche inventory handler — do deliberately):
- [ ] **`configure_inventory_slots` silent no-op when `MaxSlots` doesn't pre-exist** (InventoryHandlers.cpp ~483-506).
  When the CDO has no `MaxSlots` property it `AddMemberVariable`s one then Mark+Save WITHOUT compiling, so the var
  lands at its type-default 0 instead of `slotCount` (the requested value is never written to the recompiled CDO).
  Fix: after AddMemberVariable, `McpSafeCompileBlueprint(Blueprint)` then re-find the CDO `MaxSlots` and
  `ApplyJsonValueToProperty(SlotCount)` before saving. (Same compile-before-CDO-write class as the audit's B-🟡
  Inventory/Interaction note.)

### 2026-06-18b — Friction found building the HUD `bind_event_to_delegate` (live MCP authoring)
- [x] **`MakePinType` PC_Float → INT — FIXED (Live-Coding-only; needs a full rebuild to persist).**
  `McpBlueprintUtils::MakePinType` (McpHandlerUtils.cpp ~682) used the legacy bare `PC_Float` category for
  "float"/"double", so user-defined pins (add_function/add_event params) compiled to an **INT** — silent
  truncation; the schema then spliced a `Truncate` node into float→float connections. Caught when a
  `SetPercent(float)` param became int and truncated 0..1 health to 0. Fixed → `PC_Real` + `PC_Float`/`PC_Double`
  subcategory. **Same bug family as the add_variable PC_Real fix (`c0bff6a`) — that fix never covered MakePinType.**
- [ ] **`add_node` CallFunction (`memberName`+`memberClass`) → blank "None" node — HIGH (compile-break + PIE hang).**
  Authoring a general `Class::Function` call via `add_node` did NOT resolve the function — it created a pinless
  `K2Node_CallFunction` titled "None". That node breaks compile (`Could not find a function named "None"`) and,
  when the broken widget is later instantiated at runtime, **HUNG PIE** (game thread frozen, log frame counter
  stuck — required a force-kill). **WORKAROUND CONFIRMED: use `create_node` (NOT `add_node`)** — `create_node`
  CallFunction DOES resolve a general call (`memberName`+`memberClass` → `ResolveUClass`→`FindFunctionByName`→
  `SetFromFunction`, BlueprintGraphHandlers.cpp ~L1045; verified: produced a real "Set Percent" node, then wired
  the body via `connect_pins`). The bug is `add_node`-specific: it routes elsewhere and `NewObject`s a bare
  function-less node. Fix: make `add_node` route function-call requests through the `create_node` CallFunction path
  (or fail-fast with FUNCTION_NOT_FOUND) instead of leaving a compile-breaking "None" node.
- [ ] **`remove_function` false success — HIGH.** `remove_function SetPercent` returned `removed:true,success:true`
  but left the function (with its broken node) in place; only a later `get_graph_details`/compile revealed it.
  False success left a compile-broken asset. Workaround: `delete_node` the offending node(s), then `compile`.
- [ ] **`add_event` override path keyed on `eventType`, not `eventName`.** To implement a parent
  BlueprintImplementableEvent override, pass `eventType:"<EventName>"` (walks ParentClass→FindFunctionByName).
  Passing `eventName` leaves `eventType` empty → defaults "custom" → creates a GUID-named blank custom event
  (silent wrong result). Accept `eventName` as an alias on the override path, or document.
- [ ] **Widget-authoring params undeclared in the `manage_blueprint` schema.** `get_widget_info` requires
  `widgetPath`, but `widgetPath`/`slotName`/`parentSlot`/`widgetClass` etc. are NOT in `BuildInputSchema` — they
  only work because the client forwards undeclared params. A schema-strict client can't discover them. Add them
  (doc-only; handlers already read them from the payload).
- [ ] **`set_position` rejects bare `x`/`y`** — wants `position:{x,y}` or `posX`/`posY`. (`set_size` took
  `width`/`height` fine.) Minor param-spelling inconsistency.
- [ ] **`add_progress_bar` (and siblings) ignore `name`, auto-name "ProgressBar"** — pass-through uses `slotName`,
  not `name`; same `name`↔`slotName` alias gap noted before for other widget adds.
- [ ] **No runtime component-function call / no runtime ApplyDamage.** `control_actor call_actor_function` is
  actor-only; firing `AttributeComponent::ReceiveDamage`/`AddHealth` to test the HUD needed `execute_python`
  (`UnrealEditorSubsystem.get_game_world()` → pawn → `get_component_by_class(AttributeComponent)` →
  `call_method("ReceiveDamage")`). `manage_combat apply_damage` is authoring-only (confirmed). Consider a
  `call_component_function` and/or runtime `apply_damage`. (NB: a protected UPROPERTY like `HudWidget` is not
  readable via python `get_editor_property`.)

### 2026-06-18 — Automated cleanup audit batch (34 confirmed findings, HUD-work session)
Multi-agent read-only audit of `Source/McpAutomationBridge/Private/` for lying-success stubs,
save/compile discipline, duplication, and graph-authoring robustness. Each finding was
adversarially re-verified against source. Grouped below by class; tackle sequentially. (Surfaced
while building the HUD `bind_event_to_delegate` work above — the "report success while wiring
nothing" anti-pattern is the same disease as the binding stubs, and violates [[feedback_fail_fast_no_guessing]].)

**A. Lying-success stubs — return `success:true` while doing/wiring nothing (fail-fast violation).**
Fix each to either do the real work or return `SendAutomationError(..., NOT_IMPLEMENTED)`; do NOT
return success with only an advisory `instruction` string. (Several also needlessly
`MarkBlueprintAsStructurallyModified` for a no-op — drop that on the NOT_IMPLEMENTED path.)
- 🔴 `bind_text` (WidgetAuthoringHandlers.cpp 3784-3829), `bind_visibility` (3832-3874),
  `bind_color` (3877-3919), `bind_enabled` (3922-3964) — find the widget, then only set an
  `instruction` string + return "…binding configured". No `WidgetBP->Bindings` entry, no getter.
- 🔴 `bind_on_hovered` (4163-4205) — casts to `UButton` only (misses Common UI), wires no
  `UK2Node_ComponentBoundEvent`; the exact old-stub shape `bind_on_clicked` was already fixed away from.
  Reimplement on the shared binder helper (delegateName default `OnHovered`), drop the UButton cast.
- 🔴 `create_property_binding` (4435-4488) — "use the Property Binding dropdown" advisory; no `FDelegateRuntimeBinding`.
- 🔴 `set_widget_binding` (5473-5585) — only verifies the prop is bindable, then `saved:true` (a save
  of no change) + "binding target verified". Especially misleading. Comment claims it "wraps bind_text…" but doesn't.
- 🔴 `apply_style_to_widget` (7542-7582) — hardcoded `success:true` + "Applied style to widget"; only probes the style var exists.
- 🔴 `set_animation_speed` (7588-7638) — true no-op: `SetPlaybackRange(GetPlaybackRange())`; speed param never applied.
- 🔴 `add_animation_keyframe` (4663-4712) — echoes time/value, writes no MovieScene channel.
- 🟡 `set_animation_loop` (4715-4763) — echoes loop/loopCount, persists nothing.
- 🔴 GAS `add_ability` (GASHandlers.cpp 3063-3078) — "Ability validated for set"; never touches GrantedAbilities.
- 🟡 GAS `grant_ability` (3173-3216) — adds an EMPTY `InitialAbilities` var; never inserts the requested ability.
- 🔴 AI `add_eqs_context` (AIHandlers.cpp 1509-1529) — `MarkPackageDirty` only; no context created/assigned, no save.
- 🟡 WorldPartition `set_datalayer` `#else` fallback (WorldPartitionHandlers.cpp 548-561) — returns
  `success:true` "Actor added to DataLayer (Simulated…)" with `added:false`. Return SUBSYSTEM_NOT_FOUND like its sibling.
- 🟡 Inventory `add_inventory_functions` (InventoryHandlers.cpp 602-629) / `add_equipment_functions`
  (1552-1579) — list function names with " (implement in Blueprint)" suffix in `functionsAdded`; no UFunctions created.

**B. Silent no-persist / stale-class (save+compile discipline — the documented IMC dirty class).**
- 🔴 ALL widget-construction `add_*` (WidgetAuthoringHandlers.cpp: add_canvas_panel 1242 … add_tree_view
  2976, 23 subactions) `MarkBlueprintAsStructurallyModified` but **never `McpSafeAssetSave`** — edits live
  in memory, lost on editor close/GC. Slot setters (set_position 3245 etc.) DO save — inconsistent.
  Fix: factor `FinalizeWidgetEdit(WidgetBP, Payload)` (mark + dirty + save, opt-in `save` flag default true), call from every tree-mutating branch.
- 🔴 `create_widget_blueprint` (1044) — creates the BP, marks dirty, **never saves** the new .uasset.
  (NOTE: this contradicts the older "create_widget_blueprint savePath" Done entry — savePath is read, but no save call fires.)
- 🔴 CommonUI mutations (CommonUIHandlers.cpp 196/270/337/422/595) — never save; only the StyleBP create-branch (502) does.
- 🟡 Inventory (InventoryHandlers.cpp 424/502/615/685) + Interaction (InteractionHandlers.cpp 492/558)
  structural var/event adds saved WITHOUT an intervening `McpSafeCompileBlueprint` → stale generated class/CDO
  on disk; pre-compile CDO writes (e.g. configure_inventory_slots MaxSlots) silently no-op. Combat/GameFramework compile-before-save is the correct pattern to mirror.
- 🟢 InputHandlers `add_mapping`/`remove_mapping` (411/497) mutate IMC without `Context->Modify()` (set_input_modifier does). Benign today (MapKey Modifies internally) but inconsistent near the known DefaultKeyMappings dirty trap.

**C. Duplication / monolith (widget-authoring refactor — pairs with the binder helper below).**
- 🔴 ~28 `add_*` handlers are copies of one ~50-line skeleton (load BP → ConstructWidget → RegisterWidgetGuid
  → SafeAddWidgetToTree → MarkModified → Validate → respond); ~1,400+ mechanical lines incl. CommonUIHandlers.
  Extract `ConstructAndAddWidget(WidgetBP, UClass*, slot, parentSlot, OutError)` into WidgetAuthoringHelpers.
- 🟡 `HandleManageWidgetAuthoringAction` is ONE ~7,350-line function (872-8223), `#pragma warning(disable:4883)`
  to silence too-large. Split into per-family members / a `TMap<FString, handler>` after the helpers land.
- 🟡 Hard-coded error literals duplicated (≈"Widget blueprint not found"×81, "Missing…widgetPath"×48) with
  divergence (WIDGET_NOT_FOUND vs NOT_FOUND for same msg). Fold into the resolve/respond helpers.
- 🟡 Response-tail (success+slotName+AddVerification+SendAutomationResponse) hand-rolled 87×; `success` set
  twice (also the bool arg). Add `SendWidgetAuthoringSuccess(...)`.
- 🟢 Half-applied helper layer: `GetColorFromJsonWidget`/`GetObjectField` exist but color/x-y still parsed inline (add_text_block 1484, add_image 1567, set_anchor 3031). Add `TryGetColorField`/`GetVector2DField`.

**D. Graph-authoring robustness (informs the new bind_event_to_delegate — build it clean from the start).**
- 🟡 Three delegate/event-authoring sites duplicate boilerplate: `bind_on_clicked` (3967-4161),
  `bind_on_value_changed` (4208-4429), `bind_anim_notify` (AnimationAuthoringHandlers.cpp 3945-4100). Extract a
  shared graph-event-chain helper (reused-or-created event node + BaseX/Y anchor + the FObjectProperty
  widget-var resolver w/ compile-once retry, dup'd verbatim at 4036-4042 vs 4279-4285). **The new
  `bind_event_to_delegate` should be a thin wrapper over this helper.**
- 🟢 `bind_on_clicked`/`bind_on_value_changed` author nodes WITHOUT an `FScopedTransaction` (bind_anim_notify
  has one) → not one undo unit; partial wiring can't roll back. **My new action will use FScopedTransaction.**
- 🟢 `bind_on_value_changed` links via raw `MakeLinkTo` guarded only by null-pin checks (bypasses schema) →
  can report `wiredLiveUpdate:true` with a type-mismatched wire. **My new action will use `K2Schema->TryCreateConnection`.**
- 🟡 BlueprintGraphHandlers.cpp `create_node` dynamic fallback (1487-1514): hand-rolls
  NewObject→AddNode→AllocateDefaultPins, no `Node->Modify()`, and `SaveLoadedAssetThrottled` WITHOUT a
  CompileBlueprint (every other structural path compiles first). Route through `FGraphNodeCreator` + compile.



### [ ] WATCH: native MCP session is connection-bound (found 2026-06-17, direct-HTTP driving)
Low priority — normal Claude Code MCP client keeps one persistent connection and never hits this.
Surfaced only when driving the bridge directly: an `initialize` returns an `Mcp-Session-Id`, but
reusing that id from a *new* TCP connection (e.g. one `Invoke-WebRequest` per call) gets
`-32600 "Invalid or expired session ID"`. Reusing a single keep-alive connection (`HttpClient`)
for the whole exchange works. Per the MCP streamable-HTTP spec the session should resume across
connections via the header alone; the pull-architecture rewrite likely tied session state to the
live socket. Worth confirming if any headless/cron client ever opens per-call connections.

### [ ] Raw `execute_python` IMC / BP-default authoring traps (found 2026-06-13, Option C IA_Block)
Two silent persistence traps hit authoring input via **raw `execute_python`** instead of the
dedicated Enhanced-Input / `manage_blueprint set_default` actions (lesson — prefer the real bridge
action; these almost certainly handle both):
- **IMC key-mapping didn't persist.** This project's IMC mappings live in the nested
  `DefaultKeyMappings.Mappings` (`InputMappingContextMappingData`) struct, NOT the empty top-level
  `Mappings`. `set_editor_property` on that nested struct does **not** mark the IMC package dirty, so
  `EditorAssetLibrary.save_asset` (default `only_if_is_dirty=True`) silently no-ops. In-session
  readback looked correct, but after the editor relaunch the mappings were gone — caught only because
  `git status` showed the IMC `.uasset` unchanged. Workaround: `imc.modify(True)` +
  `save_asset(only_if_is_dirty=False)`, then confirm via `git status`. Verify the bridge's
  Enhanced-Input mapping action dirties+saves the nested struct correctly.
- **BP class-default via CDO needs a recompile.** Setting an inherited C++ `UPROPERTY` default on a
  BP via `set_editor_property` on the CDO + `save_asset` did NOT propagate to spawned instances (the
  PIE pawn read the new `UInputAction*` as None); had to `BlueprintEditorLibrary.compile_blueprint(bp)`
  before saving. The dedicated `manage_blueprint set_default` presumably compiles — use it over raw CDO writes.

### [ ] (deprioritized 2026-06-11) Graph-node authoring has no overlap-aware placement
**Priority dropped now that `arrange_graph` is fixed (size-aware, collision-free):**
the workflow "author blindly → arrange_graph once at the end" covers the readability
goal without per-insert nudging. If still wanted, the building block exists —
`ArrangeEstimateNodeSize` in McpAutomationBridge_BlueprintGraphHandlers.cpp; hoist it
to McpAutomationBridgeHelpers.h (header change = full rebuild) and nudge +Y until the
estimated rect is clear, reporting `nudged:true`. The `removeGhostEvents` idea stands.
Original report:
**FOUND 2026-06-11** (Aaron's review of the training-dummy graphs). `add_node`/`add_event`/
`create_node` place nodes at exactly the caller-supplied (or default) coordinates with zero
awareness of what's already there — including the engine's ghost template events
(BeginPlay/ActorBeginOverlap/Tick + `Parent:` pairs) that every child BP is seeded with.
Repro: all three `BP_TrainingDummy_*` EventGraphs had authored nodes overlapping ghost
nodes and each other; row spacing also didn't account for tall nodes (Set Timer by Function
Name, 11 pins ≈ 330px). Cleaned up by hand 2026-06-11 (ghosts deleted; positions set via
`set_node_property NodePosX/NodePosY` — which works fine and is the current workaround).
Improvement: overlap-avoiding default placement (offset until the estimated node rect is
clear), and consider a `removeGhostEvents` option since disabled template nodes are pure
clutter in bridge-authored BPs. The 1-call hygiene readback (`get_graph_details` x/y/
enabledState) already exists for *detection* — authoring just never consults it.

### [ ] WATCH: editor crashed during shutdown after bridge-issued QUIT_EDITOR (1× 2026-06-11)
EXCEPTION_ACCESS_VIOLATION reading nullptr, callstack entirely UnrealEditor-Slate.dll
(teardown path), AFTER "Engine exit requested" — i.e. a shutdown-order crash, not a hang.
Editor had been up ~all day across many PIE cycles + heavy bridge use; assets were saved,
no data loss; the lingering process was the crash-reporter dialog. NOT attributed to the
bridge (no bridge frames in the stack) — but QUIT_EDITOR via bridge is our standard
clean-quit path, so log recurrences here. If it repeats, symbolize the stack and check
whether a bridge-held Slate reference (e.g. a cached widget from ui_focus/find_objects)
outlives its window.
- 2026-06-16: clean QUIT_EDITOR via bridge worked (no crash, no hang). Real lesson this time was
  process disambiguation — `Get-Process UnrealEditor` matched a *different* project's editor
  (HandPanic), so confirm the target by `.uproject` in the command line before waiting/killing.

### [x] Graph-authoring papercuts batch (found building BP_GymRespawner, 2026-06-11)
**ALL FIXED 2026-06-11**, verified live on a scratch BP: (1) add_node now DELEGATES
cast/variable-get/variable-set node types (K2Node_DynamicCast→Cast etc., memberName
mapped to variableName) to the create_node factories that do proper TargetType/
VariableReference setup — no more "Bad cast node"/pinless getters; (2) create_node and
the reroute path read posX/posY as fallback for x/y; (3) add_event reads posX/posY
first (and from LocalPayload, fixing a latent null-Payload risk); (4) auto-exec-wiring
(both the VarSet wire and the EnsureExecLinked graph sweep) is now opt-in via
`autoConnect` (default false) — verified no surprise wire without it, sweep wires the
first open exec node with it. Schema gained autoConnect + posX/posY descriptions
(visible after next editor restart; parsing is live). Original report:
All worked around in-session; each cost a delete-and-retry round trip:
- `add_node {nodeType:K2Node_DynamicCast, targetClass}` silently creates a **"Bad cast
  node"** (TargetType never set, wildcard pins). The working path is
  `create_node {nodeType:Cast, targetClass}` — add_node should either route to the same
  factory or fail fast with guidance.
- `add_node {nodeType:K2Node_VariableGet, memberName, memberClass}` creates an empty
  "Get" node (0 pins). Working path: `create_node {nodeType:VariableGet, variableName,
  memberClass}` — same fix wanted.
- `create_node` ignores `posX`/`posY` (lands at 0,0); `add_event` ignores them too
  (Custom event landed on top of BeginPlay). Both need the position params honored
  (related: the overlap-aware-placement entry below).
- Auto-exec-wiring surprise: each new exec node self-connects to the most recent open
  exec chain. Convenient for BeginPlay→SetTimer, but it silently wired
  `CheckPlayer.then → ExecuteConsoleCommand.execute` across an 800px gap — a link the
  caller never asked for and must notice + break. Should be opt-in (`autoConnect`).
- `set_node_property` rejects `TargetType` (PROPERTY_NOT_SUPPORTED) — fine that it's
  allowlisted, but the error should point at create_node for cast retyping.

### [x] `arrange_graph` ignores node geometry — stacks nodes at IDENTICAL coordinates
**FIXED 2026-06-11.** Recon traced TWO collision sources: parent-centering produces
unconstrained row averages that can land exactly on a leaf's integer row (single-child
parent = exactly the child's row), and X pass 2 relocates pure getters into
(consumerCol−1), which an unrelated exec node can occupy via longest-path. Fix in
`ArrangeBlueprintGraph`: new `ArrangeEstimateNodeSize` (height via the engine's
canonical headless `UEdGraphSchema_K2::EstimateNodeHeight` — which exists precisely
because Slate sizes don't exist until the next tick; width = max(224, 32 + 7·titleLen),
224 being the engine's own K2 width guess) + size-aware placement: per-column max-width
X advance (fixed ColStepX removed) and a per-column collision pass (sort by computed
row, push down past the previous node's bottom + 48px). Verified live on a duplicate of
BP_TrainingDummy_AoE (the original failure): the colliding pair now lands (640,180) vs
(640,276), extents clear, columns sized to content. Original report:
**FOUND 2026-06-11** (first real use, on BP_TrainingDummy_AoE EventGraph). The layout
algorithm appears to be pure depth-column × row-index: it placed `Get AIPerception` and
`Execute Console Command` at the **same exact spot** (640,180), and its row pitch (~270)
is smaller than a tall node (Set Timer, 11 pins ≈ 330px), so rows overlap vertically.
Pure data nodes (variable getters) seem to share a depth slot with the exec-chain node they
feed instead of being offset below it. Needs size-aware layout: estimate node extent from
pin count + title width (or read live Slate geometry when the graph editor is open), give
data-only nodes their own sub-row, and assert no two nodes share a rect. Until then,
hand-positioning via `set_node_property` is more reliable than `arrange_graph`.

### [x] `add_event` Custom DUPLICATED default pins → authored event chains died on editor restart
**FOUND + FIXED 2026-06-11** (TODO-clearing session; discovered when the combat-gym
training dummies went dead after the overnight rebuild). The add_event custom-event path
in `McpAutomationBridge_BlueprintHandlers.cpp` called `CustomEventNode->AllocateDefaultPins()`
AFTER `NodeCreator.Finalize()` — but Finalize already allocates default pins, so every
bridge-authored custom event carried DUPLICATE delegate/then pins (pinCount 4 instead
of 2). `connect_pins` then linked the duplicate `then`, everything verified fine
in-session, and on the NEXT editor load `ReconstructNode` silently dropped the duplicate
pins AND the links on them: BP_TrainingDummy_Melee/AoE both lost their Swing/Lob exec
chains overnight (events fired, did nothing). Fix: removed the redundant call (only
occurrence in the codebase — swept every `Finalize()` site). Verified: fresh custom event
now reports pinCount 2; dummies re-linked on the post-reconstruction canonical nodes,
re-verified live in PIE (melee kills again, AoE lobs again); link survival across restart
gets confirmed at this session's end-of-night rebuild. LESSON for graph authoring:
in-session verification is not enough for node-creation bugs — pin duplication only
manifests at the serialize→reload boundary.

### [x] Benign "Editor is currently in a play mode" log line false-fails PIE-time calls
**FIXED 2026-06-11** in two layers. (1) Guard downgrade (live now): the captured-error
promoter's benign-line list (`IsKnownBenignMcpCompilerWarning`,
`McpAutomationBridgeSubsystem.cpp`) now treats "The Editor is currently in a play mode"
as a warning — engine editor-scripting utilities log it at Error severity whenever called
during PIE and the handlers all have PIE-safe fallbacks; the guard only promotes on
SUCCESS, so this can't mask a real failure. Verified live: `spawn_blueprint` into a PIE
world and `get_graph_details` mid-PIE both return clean success (both used to
ENGINE_ERROR). (2) Per-site swaps (in the end-of-night rebuild batch, header change):
`McpLoadAssetPieSafe` helper (plain LoadObject, no play-mode gate) replacing
`UEditorAssetLibrary::LoadAsset` at the spawn/spawn_blueprint/FindActorByName/
add_component/get_components sites + `FPackageName::DoesPackageExist` replacing
`DoesAssetExist` in LoadBlueprintAsset Method 4 — so PIE-time loads actually work
instead of leaning on fallbacks. Original report:
2026-06-10 (combat verification session). During PIE, several control_actor/manage_blueprint
calls return `ENGINE_ERROR: [LogUtils] The Editor is currently in a play mode.` even though
the operation SUCCEEDED (verified repeatedly: `spawn_blueprint` spawned into the PIE world
fine; `get_graph_details` read fine after stopping PIE). Source: editor-only utilities
(`UEditorAssetLibrary::LoadAsset` in the spawn class-resolution path at
`McpAutomationBridge_ControlHandlers.cpp` ~L345, and whatever asset loader graph-detail
calls use) log an Error-severity line when called during PIE, and the post-op error guard
promotes it to a call failure. Two fixes: (a) replace `UEditorAssetLibrary::LoadAsset` with
plain `LoadObject`/`StaticLoadObject` in the spawn path (no play-mode guard, identical
result); (b) teach the error guard to downgrade this specific benign line (like the
handled-ensure downgrade). Note the retry idempotency guard worked as designed here — the
"failed" spawns had actually landed and the retry caught it.

### [x] `create_montage` ignores `savePath`; `add_montage_section` can't find its montage
**FIXED 2026-06-11.** Root cause: an alias-routing layer sends these sub-actions to the
animation AUTHORING handler (`McpAutomationBridge_AnimationAuthoringHandlers.cpp`), not the
legacy handler — and the authoring handler's params disagreed with the tool schema:
create_montage read only `path` (schema advertises `savePath`); add_montage_section read
only `assetPath`/`startTime` (schema advertises neither — only `name`/`time`). Fixes:
savePath fallback chain (mirrors the fixed create_widget_blueprint); add_montage_section
accepts assetPath → montagePath → name with a fail-fast INVALID_ARGUMENT when no path-ish
value arrives, reads time as fallback for startTime, calls `Modify()` before mutating, and
fail-fasts `SECTION_EXISTS` on AddAnimCompositeSection returning INDEX_NONE (used to
report success "added at index -1"). BONUS root-cause of the third symptom: the engine
montage factory seeds a "Default" composite section WITHOUT calling Link() — create_montage
now links all sections after creation (engine AddAnimCompositeSection pattern), so the
reflection-write workaround is no longer needed. Schema: added `assetPath` + `startTime`
fields. All verified live: savePath honored, section added via name+time (LinkValue
correct), duplicate rejected, sections linked. Original report:
2026-06-10 (authoring AM_BlockImpact). (a) `animation_physics create_montage
{savePath:/Game/Blueprints/Characters/MainRobo/Montages}` created the asset at
`/Game/Animations/` — same bug family as the fixed create_widget_blueprint savePath issue;
worked around with `move_asset`. (b) `add_montage_section {name:<montage path>,
sectionName, time}` → `MONTAGE_NOT_FOUND` — the handler evidently reads a different param
for the montage path than the schema suggests (the `name` param was taken as something
else). Worked around entirely with `set_asset_property` subpath writes
(`CompositeSections[0]`, `SlotAnimTracks`, `SequenceLength`) — which round-tripped
perfectly and produced a working montage (block react plays in PIE), so the reflection
route is a viable montage-authoring fallback. Also: `create_montage` DOES seed a "Default"
composite section — only linking (SegmentIndex/SegmentLength/LinkedSequence) was missing.

### [x] `manage_asset get_referencers` / `get_dependencies` ASSET_NOT_FOUND on valid paths
**FIXED 2026-06-11.** Recon found TWO coupled defects in
`McpAutomationBridge_AssetWorkflowHandlers.cpp`: (1) the existence gate used
`UEditorAssetLibrary::DoesAssetExist`, which hard-fails for EVERY asset while PIE runs
(engine `CheckIfInEditorAndPIE` gate — the original session had live PIE, hence blanket
ASSET_NOT_FOUND); (2) the registry FName overloads take PACKAGE names, so the
`.Name` object-path form silently returned `success:true` with empty arrays. Fix: both
handlers now derive `FPackageName::ObjectPathToPackageName(...)`, existence-check via
`GetAssetsByPackageName` (registry-native, PIE-immune, load-free), and query with the
package FName; dead `EDependencyCategory` local removed. Verified live: both path forms
return identical results (BP_MiliBot: 10 referencers), AND the same call succeeds
mid-PIE. Original report:
2026-06-10 (BP_MiliBot architecture walkthrough). `get_referencers
{assetPath:/Game/Blueprints/Characters/Enemies/MiliBot/BP_MiliBot}` → `ASSET_NOT_FOUND`,
and the same with the full object path (`...BP_MiliBot.BP_MiliBot`). The asset definitely
exists: `search_assets` returns it and `get_asset_properties`/`get_graph_details` work on
the exact same path in the same session. So the referencer handler resolves paths
differently from every other asset handler (probably querying the asset registry with the
wrong path form — registry wants package path `FName`, not object path). Check whether
`get_dependencies` shares the broken resolution. Workaround: none over the bridge;
used live-world `find_by_class` + asset search instead.

### [x] `manage_blueprint get_blueprint`/`get` advertised in schema but router rejects them
**FIXED 2026-06-11** (full drift sweep via recon workflow — every tool's schema enum
cross-referenced against its router). Repairs, all live-verified:
- `manage_blueprint`: `get_blueprint` alias added to the blueprint_get branch (returns the
  rich snapshot; plain `get` already worked); `remove_variable`/`rename_variable`
  unprefixed aliases added (only blueprint_-prefixed forms matched before).
- `system_control` registry lambda now re-dispatches: `console_command`/`execute_command`
  → `HandleConsoleCommandAction` (inherits the security blocklist, same as control_editor);
  NEW `set_cvar` implementation (composes "key value" through the audited console path,
  INVALID_ARGUMENT without key); `subscribe`/`unsubscribe` → `HandleLogAction(manage_logs)`;
  `spawn_category` → `HandleDebugAction(manage_debug)`; `lumen_update_scene` →
  `HandleRenderAction(manage_render)`.
- LATENT BUG found by wiring subscribe: `HandleLogAction` answered via a hand-rolled
  envelope + raw `RequestingSocket->Send` (pre-pull-architecture) — native MCP callers
  timed out. Now uses standard `SendAutomationResponse`.
- `control_actor`: `find_actors_by_tag` alias (matches its _name/_class siblings).
- `manage_effect`: `activate`/`activate_effect`/`deactivate` now alias-rewrite to the real
  `*_niagara` handlers — NOTE the create_effect path derives its sub-action from the
  payload's `action` field, not `subAction`; both are rewritten.
- Schema truth-in-advertising (takes effect at next full rebuild — schema registry is
  built at module startup): SystemControlCore drops the 7 never-implemented actions
  (profile, set_quality, set_resolution, set_fullscreen, play_sound, show_widget,
  validate_assets); manage_effect drops dead `reset` (it's a payload FIELD of
  activate_niagara) and advertises activate_niagara/deactivate_niagara.
Original report:
2026-06-10. `manage_blueprint {action:get_blueprint}` → `UNKNOWN_ACTION: Unknown blueprint
action: get_blueprint` — yet both `get_blueprint` and `get` are in the action enum the
native schema advertises (`McpTool_ManageBlueprint`?). Schema/router drift: either the
router lost the alias or the schema advertises an action that was never wired. Same bug
family as the fixed `rename_widget` schema/param mismatch. Meanwhile
`get_asset_properties {propertyName:ParentClass}` is a fine substitute for parent-class
queries (returned `BP_BaseEnemy_C` for BP_MiliBot).
Same drift found 2026-06-11 (gym build session): `system_control {action:execute_command}`
→ `NOT_IMPLEMENTED` despite being in the schema enum (`control_editor console_command`
works). Sweep the schema enums against the routers in one pass.

### [x] `control_actor spawn` ignores `className` for engine classes (works via `classPath`)
**FIXED 2026-06-11.** `HandleControlActorSpawn` only read `classPath`; the schema's other
two aliases (`className`, `actorClass`) were never read — `ResolveClassByName("")` then
nullptr'd silently (the StaticMeshActor case "worked" only because the mesh fallback
forced the class, masking the drop). Fix: coalesce classPath → className → actorClass
(mirrors `HandleControlActorFindByClass`), plus a fail-fast `INVALID_ARGUMENT` ("No class
specified: pass classPath, className, or actorClass...") when all are empty, replacing the
old "Class not found: ." message. Verified live: `{className:PlayerStart}` spawns; empty
payload errors clearly. NOTE found in passing: `spawn_blueprint` reads only
`blueprintPath` — same alias-dropping family if a caller sends classPath; harmless today
(schema steers to blueprintPath) but worth aligning whenever that handler is next touched.
Original report:
2026-06-11 (gym build). `spawn {className:PlayerStart}` and `{className:SkyLight}` →
`CLASS_NOT_FOUND: Class not found: .` — note the EMPTY class name in the message: the
handler never read the `className` param on this code path. The same spawns succeed with
`classPath:/Script/Engine.PlayerStart`. Oddly `{className:StaticMeshActor}` + `meshPath`
worked — so there are (at least) two resolution paths and only one honors `className`.
Align them + include the requested name in the error.

### [x] `set_node_property` supports NodePosX/NodePosY but not `EnabledState`
**FIXED 2026-06-11.** Added an `EnabledState` branch to the property dispatch in
`McpAutomationBridge_BlueprintGraphHandlers.cpp` (strict parse
Enabled|Disabled|DevelopmentOnly, unknown → `INVALID_VALUE` fail-fast) calling
`UEdGraphNode::SetEnabledState` (bUserAction=true, mirroring a user toggle). Because
enabled state changes what gets COMPILED, this branch marks the Blueprint
**structurally** modified (`MarkBlueprintAsStructurallyModified`) like the editor's own
toggle — the other property writes keep the cheaper `MarkBlueprintAsModified`. Verified
live: ghost `Event ActorBeginOverlap` flipped Disabled→Enabled→Disabled (round-trip
confirmed via get_graph_details), `Bogus` → INVALID_VALUE. Original report:
2026-06-11 (gym build). Wanted to flip a ghost (Disabled) `Event BeginPlay` node to
Enabled; `set_node_property {propertyName:EnabledState, value:Enabled}` →
`PROPERTY_NOT_SUPPORTED`. Workaround: delete the ghost node (+ its auto-added
`Parent: BeginPlay`) and re-run `add_event` — the fresh node comes in Enabled with no
parent call. Supporting EnabledState would make "override an event WITHOUT calling the BP
parent" a first-class authoring flow instead of a delete/recreate dance.

### [x] `get_asset_properties` returns `[]` for `InputMappingContext.Mappings`
**FIXED 2026-06-11 — and the original hypothesis was WRONG.** The export machinery is
fine (field-by-field struct export + `{"__class"}` instanced export already work; round
trips verified previously). Root cause: UE 5.7 DEPRECATED `UInputMappingContext::Mappings`
— `PostLoad()` MoveTemps the data into `DefaultKeyMappings.Mappings`, so the legacy array
is GENUINELY EMPTY in memory; the bridge silently resolved the deprecated property and
returned a truthful-but-misleading `[]`. The correct read (`DefaultKeyMappings.Mappings`)
worked all along. Fix: fail-fast deprecated-property guard in `ResolveAssetPropertyPath`
(shared by get/set_asset_property, so it also stops silent writes to dead arrays):
errors with the engine's own DeprecationMessage — "Field 'Mappings' on
'InputMappingContext' is deprecated: Use the DefaultKeyMappings struct instead." KEY
DETAIL: UHT only sets CPF_Deprecated for `_DEPRECATED`-suffixed names; the
`meta=(DeprecatedProperty)` metadata check is the load-bearing half. Verified live both
ways. Original report:
2026-06-11 (gym build). `get_asset_properties {assetPath:IMC_CharacterContext,
propertyName:Mappings}` → `{"value":[]}` with success=true, but the IMC demonstrably has
mappings (the game plays; the dead-zone work edited them). Likely the array-of-struct
export path bails on `FEnhancedActionKeyMapping` (instanced Modifiers/Triggers inside) and
silently returns empty instead of erroring — which violates fail-fast and cost a detour
(had to discover the attack key by simulating LeftMouseButton instead of reading the
binding). Related to the (fixed) instanced-struct IMPORT work — the EXPORT side of the
same struct family needs the equivalent treatment.

### [x] Investigate: consecutive `find_by_class` calls in one PIE session report different worlds
**RESOLVED 2026-06-11.** Investigation verdict (engine-source recon): the observed
IceMap→HubWorld flip-flop was a REAL mid-PIE level travel (a gameplay/level-BP OpenLevel —
LoadMap swaps the world on the same FWorldContext; the bridge drains requests on the core
ticker AFTER GEngine->Tick, so it can never read mid-LoadMap state). The "false empties"
were true empties of a freshly traveled world. HOWEVER a real LATENT bug was found:
`GEditor->PlayWorld` is per-frame scratch that UEditorEngine::Tick reassigns per PIE
context (last-context-wins, unchecked null in the render loop) — nondeterministic under
multi-instance PIE. Fix: find_by_class now resolves via `GEditor->GetPIEWorldContext()`
(deterministic, instance 0) with editor-world fallback; `isPieWorld` derives from the same
resolution; response gains `worldPackage` (UEDPIE_<instance>_ prefix) and `hasBegunPlay`
so real travels are diagnosable. Verified live in both modes. NOTE: sibling handlers
(FindActorByName, spawn, set_game_speed, exec at ControlHandlers ~220/409/626/3125/3616/
3649) still read raw PlayWorld — identical in single-instance PIE; migrate to a shared
helper if multi-instance PIE ever matters. Original report:
2026-06-10. Two back-to-back `control_actor find_by_class` calls during a single live PIE
session returned `worldName:"IceMap"` then `worldName:"HubWorld"` (both `isPieWorld:true`,
both 0 hits, ~seconds apart). Could be legitimate (level transition mid-play, or World
Partition/streamed-level multi-world) or could mean the PIE-world picker grabs the first
PIE-flagged world it finds nondeterministically — if so, by-class searches can silently
scan the WRONG world and return false empties. The fix from the earlier world-scoping bug
(see [x] entry below) chose *a* PIE world; verify it chooses *the* game world (use
`UEditorEngine::GetPIEWorldContext()` / the game instance's world) when multiple worlds
are live.

### [x] `add_node` couldn't author function-call nodes by name (`Class::Function`)
**FIXED 2026-06-10** (found when `nodeType:"KismetSystemLibrary::PrintString"` →
`UNSUPPORTED_NODE`). Three changes in `McpAutomationBridge_BlueprintHandlers.cpp`:
(1) a `nodeType` containing `::` now routes to the CallFunction branch with the whole spec
as the function name — the form callers naturally try; (2) `FMcpAutomationBridge_ResolveFunction`
now splits `Class.Function` on the LAST dot and resolves the class via `ResolveClassByName`,
so `KismetSystemLibrary::PrintString`, `KismetSystemLibrary.PrintString`, and
`/Script/Engine.KismetSystemLibrary.PrintString` all work (the old first-dot
`FindObject<UClass>` split broke every `/Script/`-prefixed form and all short names);
(3) **fail-fast**: a non-empty `functionName` that doesn't resolve now returns
`FUNCTION_NOT_FOUND` instead of silently creating a bare misconfigured `K2Node_CallFunction`
that breaks the next compile while the add reports success. Verified live: `::` form creates
a fully-bound Print String node (all pins + defaults), `/Script/` form binds, unknown
function errors with guidance. Ops note: live-coding `McpAutomationBridge_BlueprintHandlers.cpp`
takes ~60s (huge TU) — just over the MCP client timeout; the compile still completes, confirm
via `LogLiveCoding` in the editor log instead of retrying.

### [x] `execute_python` + `EditorLoadingAndSavingUtils.reload_packages` → modal → permanent GameThread freeze (bridge dead)
**GUARDED 2026-06-10** — the exec wrapper now scans the user code for known modal-popping
APIs (`reload_packages` / `*_with_dialog` / `EditorDialog`) and raises a RuntimeError with
guidance BEFORE exec; explicit opt-out via the new `allowModalApis:true` param (added to the
system_control schema) for calls that provably can't prompt. Source-scan chosen over
monkey-patching (unreal extension types may refuse setattr) — it's deterministic and the
goal is protecting cooperative agents from foot-guns, not sandboxing adversaries. Verified
live: pattern in code → `PYTHON_ERROR` with the full incident message; benign probes
unaffected; `allowModalApis:true` (via mcp-call.ps1 — note its `-Arguments` takes a
HASHTABLE, not a JSON string) runs the same code. Possible future hardening: the (c)
watchdog idea below (log loudly when a queued request hasn't started for N minutes and a
modal is up). Original report:
2026-06-10 ~06:46 UTC (the dead-zone session's last call). Repro: `execute_python` running
`unreal.EditorLoadingAndSavingUtils.reload_packages([pkg])` on `IMC_CharacterContext` → the
default `INTERACTIVE` mode pops a confirmation dialog that can never be dismissed headlessly →
GameThread parks in the modal's nested loop forever (log frame counter frozen at [498] from
06:46 onward; no py output/status files written). The bridge's socket thread keeps ACCEPTING
requests (sessions init, tools/call logged) but nothing executes — every call times out, port
3123 stays occupied by a dead editor. Required a force-kill + fresh editor instance.
Directions: (a) workflow rule (already policy): `execute_python` is for READ-ONLY probes —
package reload/save/mutation goes through typed bridge actions; (b) the py exec harness could
prepend a guard that monkey-patches known-modal Python APIs (`reload_packages`,
`save_dirty_packages_with_dialog`, …) to their non-interactive overloads or a hard error;
(c) consider a watchdog: if a queued bridge request hasn't STARTED for N minutes and a modal
window is up, log loudly (can't auto-dismiss safely, but the log line tells the next session
why everything is timing out).

### [x] `set_asset_property` can't import struct `InputMappingContextMappingData` (+ no indexed subpaths)
**FIXED 2026-06-10 — all three layers, verified live on a duplicated IMC + montage:**
1. **Dotted/indexed property paths** — new `ResolveAssetPropertyPath` (AssetWorkflowHandlers):
   `"DefaultKeyMappings.Mappings[19].Modifiers"` resolves segment-by-segment (struct fields,
   `[i]` array indices, and descent through object refs into e.g. an Instanced modifier).
   Wired into BOTH `set_asset_property` (writes fire `PostEditChangeProperty` with the ROOT
   class-level property so editor rebuild logic reacts) and `get_asset_properties` (new
   `propertyName` param → single-value subpath read, so writes verify symmetrically).
   Fail-fast errors name the exact problem (unknown field on which struct, index out of
   bounds with the actual size, non-array indexed, descent into None).
2. **String→JSON fallback for structs** — the struct string branch now parses the text and
   RE-ENTERS the importer (so a client-stringified object takes the identical code path,
   instanced handling included), falling back to `ImportText_Direct` for UE-syntax text
   forms. Previously it went straight to `JsonObjectToUStruct`, bypassing instanced logic.
3. **Root cause of the struct failure** — `JsonObjectToUStruct` fails an entire struct when it
   meets the `{"__class"}` instanced form, and the old direct-children strip-and-patch missed
   instanced fields nested under arrays-of-structs (`InputMappingContextMappingData` →
   `Mappings[]` → `Modifiers[]`). Now: structs whose value tree holds instanced references
   ANYWHERE (new `StructTreeContainsInstanced`, flag check + explicit walk) import
   **field-by-field** through `ApplyJsonValueToProperty`, which handles nested structs,
   containers, and owner-threaded re-instancing at every depth; everything else keeps the
   engine converter. Strip-and-patch deleted.
   Verified: `Mappings[19].Modifiers` indexed write (the original repro) lands 2 instanced
   modifiers; whole-`DefaultKeyMappings` object write round-trips (DeadZone values, triggers
   incl. the quirky `LastValue:"()"` text field, FKey, enums, null object refs); montage
   `Notifies[0]` (instanced `AnimNotify_PlaySound` + FGuid + nested EndLink) round-trips
   byte-identically — the field-by-field path regression-checked against the old behavior.
**Python gotcha kept for the notes:** `new_object(..., outer=asset)` yields a template-flagged
object and `set_editor_property` refuses with "cannot be edited on templates".

### [x] `control_actor` world-scoping is inconsistent: by-class misses PIE actors, by-name finds them
**FIXED 2026-06-10.** `HandleControlActorFindByClass` hardcoded
`GEditor->GetEditorWorldContext().World()`; now prefers `GEditor->PlayWorld` when PIE is active
(matching `HandleControlActorList` — which was *already* PIE-aware — and `FindActorByName`), and
the response gained `isPieWorld` + `worldName` for diagnosability. Verified live: in PIE,
`find_by_class {className:Character}` returns `BP_CPP_Character_C_0` in `UEDPIE_0_HubWorld`
(`isPieWorld:true`); without PIE, `find_by_class PlayerStart` still walks the editor world
(`isPieWorld:false`). Original report:
2026-06-10, single-editor session (so NOT the cross-client noise from the entry below). During PIE:
`control_actor find_by_class {className:Character}` → 0 actors, but the pawn existed —
`inspect find_objects {className:Character, pathContains:UEDPIE}` returned
`UEDPIE_0_HubWorld...BP_CPP_Character_C_0`, and `control_actor get_transform
{actorName:BP_CPP_Character_C_0}` found it fine and tracked it across frames. So the by-name path
resolves into the PIE world while the by-class enumeration only walks the editor world.

### [x] Live Coding can't patch UBA-built binaries: `Cannot find image section .voltbl`
**RESOLVED 2026-06-10 — build with `-NoUBA`.** Confirmed the hypothesis: a full rebuild via
`Build.bat <target> Win64 Development -Project=<uproject> -WaitMutex -NoUBA` uses the Parallel
executor (log line: `Using Parallel executor to run N action(s)` — verify this, it's the proof
UBA is off), and Live Coding patches the result fine. Verified live with an end-to-end
observable probe: changed a handler's response string → `live_coding_compile` → call returned
the patched string → reverted → second patch cycle → original string back. Two activations,
zero `.voltbl` errors. **New build protocol: ALWAYS pass `-NoUBA` on Build.bat rebuilds**, or
live-coding iteration is dead until the next full rebuild. (User-level
`BuildConfiguration.xml` is currently empty; making `-NoUBA` persistent there is Aaron's call —
it would slow his big full builds in exchange for never losing Live Coding.) Original report:
2026-06-10. After a full `Build.bat` rebuild (which ran via Unreal Build Accelerator), the next
`system_control live_coding_compile` failed: LiveCodingConsole.log shows hundreds of
`Cannot find image section .voltbl` + `Patch could not be activated.` (.voltbl = MSVC volatile
metadata; the UBA-compiled objects evidently drop it, and Live Coding refuses to patch a
mismatched image).

### [x] Investigate: unexplained `tools/call: control_actor` requests hitting the bridge
**Diagnosability landed 2026-06-10** — the `tools/call` log line now carries
`session=<id>, peer=<ip:port>` (the session id joins to the init line's
`client: <name> <version>`), so the next occurrence is attributable on sight. Verified live:
`tools/call: inspect (RequestId=…, session=60D7…, peer=127.0.0.1:50617)`. The original
incident itself remains unexplained (coincided with a two-editor session, attribution murky)
but is now a one-grep diagnosis if it recurs. Original report:
2026-06-10 ~04:55 (RhyaTowerOfWishes.log): two `control_actor` calls reached this bridge
between this client's `manage_blueprint` calls, but this client never issued them. Suspect
cross-client traffic on port 3123 (another MCP session — e.g. the handpanic project's config —
pointed at the wrong port).

### [x] `rename_widget` was a bare `UObject::Rename` (stale GUID map, broken references) + schema/param mismatch
**FIXED 2026-06-09** (found renaming `WBP_OptionsScreen`'s slider row). Two bugs:
1. The handler did `TargetWidget->Rename()` + `MarkBlueprintAsStructurallyModified` only — skipping
   everything the designer's rename does: `OnVariableRenamed` (variable→GUID map — the same stale-GUID
   ensure family as the old `remove_widget` bug), `ReplaceVariableReferences` (graph getters/setters),
   delegate bindings, animation bindings, explicit nav rules, and the blueprint's own
   `DesiredFocusWidget`. Rewrote it to mirror the headless-safe core of
   `FWidgetBlueprintEditorUtils::RenameWidget` (UMGEditor `WidgetBlueprintEditorUtils.cpp:305-465`),
   incl. the public `ReplaceDesiredFocus(UWidgetBlueprint*, Old, New)` overload (CDO-level, editor-open
   not required) and the FKismetNameValidator unique-name check with the BindWidget-property exception.
   Skipped (editor-UI only): preview rename, orphan-pin getter reconstruction,
   `FUIComponentUtils::OnWidgetRenamed`. Response now reports `desiredFocusUpdated`.
2. The handler required `slotName` but the `manage_blueprint` schema advertises `oldName` → the action
   was uncallable via MCP. Now accepts `oldName` (canonical) with `slotName`/`name` aliases.
Verified live (live-coding): renamed options' `VolumeSlider` row → `Master_Volume_Slider`;
`desiredFocusUpdated:true`, CDO `DesiredFocusWidget` followed automatically, clean compile+save, no
ensures, templates kept their instance values, pause spec still 4/4 green.

### [ ] `manage_audio` create_sound_class / create_sound_mix HANG the bridge (300s) — audio-mixing-asset creation modal
Found 2026-06-07 during the audio sweep. `create_sound_class` (and `create_sound_mix`) never return —
the parked tools/call times out at 300s. The GameThread keeps ticking (other tools respond, CleanupStaleRequests
fires), i.e. a **Slate modal** is up and the handler is blocked in its nested pump waiting for a dismissal
that never comes headlessly. Two distinct causes:
1. **`IAssetTools::CreateAsset` modal (FIXED).** The old `HandleAudioAction` create_sound_cue/class/mix used
   `AssetToolsModule.CreateAsset(...)`, which can pop an "overwrite existing object?" dialog (the AudioAuthoring
   handlers already avoid it for exactly this reason — "recursive FlushRenderingCommands / D3D12 crashes").
   Replaced all three with the safe `CreatePackage`+`NewObject`(+`AssetCreated`+`MarkPackageDirty`) pattern
   (cue re-seeds its wave-player node to match `USoundCueFactoryNew::InitialSoundWaves`).
2. **RESIDUAL (OPEN): audio-mixing-asset creation still hangs even via `NewObject`.** With the safe pattern,
   `create_attenuation_settings` works (33ms) but `create_sound_class`/`create_sound_mix` STILL hang. So the
   residual modal is **asset-type-specific to audio-mixing assets** (USoundClass / USoundMix register with the
   audio device; USoundAttenuation doesn't) — NOT the bridge's creation call. **Not diagnosable remotely**
   (the Slate modal is invisible over MCP). Needs in-editor debugging: attach a debugger, create a SoundClass
   via the bridge, and read the hung GameThread callstack / identify the modal window. Likely an audio-device
   registration interaction in a headless/-unattended editor.
Also note: **duplicate handlers** — the old `HandleAudioAction` (dispatched first) claims all `create_sound_*`
via a prefix gate and shadows the safe `HandleManageAudioAuthoringAction` versions. Worth deduplicating once
the residual hang is understood.

### [x] Shutdown wrote a raw SSE frame to parked tools/call connections (de-stream leftover)
**RESOLVED 2026-06-06** (found during the `FSSEConnection`→`FPendingConnection` rename). `Shutdown()`'s
close-all loop still wrote an `event: message\ndata: …` SSE frame to each parked connection — but under
the pull-only transport those sockets are awaiting a single plain HTTP response, so a shutdown with a
request in flight sent a malformed reply (no status line/headers). Now sends a proper `503` +
JSON-RPC-error body via `SendHttpResponse` (mirrors `CompletePendingRequest`). Same pass removed the
now-dead `SendSSEHeaders` / `WriteSSEEvent` / transport `SendSSEProgressUpdate`, renamed the struct/map
to pending-connection naming, and corrected the stale SSE/WebSocket comments + `AGENTS.md`.

### [x] `remove_widget` leaves a stale variable→GUID entry → handled ensure on next compile (+ gated save)
**RESOLVED 2026-06-06.** (a) `remove_widget` now calls `FWidgetBlueprintEditorUtils::DeleteWidgets(WBP, {Widget}, DeleteSilently)`
— the engine's own designer-delete path — instead of a hand-rolled `WidgetTree->RemoveWidget`. It strips the
widget+children variable→GUID entries (`OnVariableRemoved`), so the ensure no longer fires at all (verified: clean
log). (b) `FMcpRequestErrorDevice::Serialize` now tracks a *handled-ensure block* (opens on `=== Handled ensure ===`,
closes when non-Error logging resumes) and downgrades the whole block to a warning; `bActive` made `std::atomic`
to keep a lock-free fast path. (c) the compile handler passes `bForce=true` to `SaveLoadedAssetThrottled` for
explicit `saveAfterCompile:true`, so the write isn't silently swallowed by the time-throttle. Verified live
(WBP_RemoveTest2): `compiled:true, saved:true`, no ensure, fresh `.uasset` on disk.

Original report:
Removing a *named* widget (e.g. a `BindWidget`-style member like a `TextBlock` label) deletes it
from the `WidgetTree` but does NOT remove its entry from the WidgetBlueprint's variable→GUID map.
The next compile then fires a handled ensure — `WidgetBlueprintCompiler.cpp:828`
`Ensure condition failed: SeenVariableNames.Contains(It.Key())` /
`Variable [<Name>] was deleted but still has a GUID referenced by WidgetBlueprint [<WBP>]` — which
the bridge's "Unreal logged errors" guard surfaces as `ENGINE_ERROR` even though the delete and
compile actually succeeded. Two follow-on problems: (a) the ensure-gated `compile {save:true}`
returns `saved:false` and skips the package write, so the deletions sit **unpersisted on disk**
until something re-dirties + saves (e.g. a later `set_position`, whose `saveSucceeded:true` path
is NOT gated and does write); (b) the ensure clears itself after one more compile pass (the
compiler prunes the stale GUID), so a second compile is clean — meaning the asset is fine, the
noise is just the bridge mis-reporting a handled ensure as failure.
Repro 2026-06-05 (Pause-menu cleanup): `remove_widget TextBlock` → next `compile` logs the ensure
+ returns `saved:false`; `remove_widget` ×3 then `compile` (ensure, no save) → `compile` again
(clean, but `saved:false` because no longer dirty) → disk stale until a `set_position` saved it.
Fix directions: (a) on `remove_widget`, also strip the deleted widget's name from the WBP's
variable/GUID map (mirror `FWidgetBlueprintEditorUtils::DeleteWidgets`, which does this cleanup) so
no stale GUID survives; (b) treat the `SeenVariableNames` ensure as benign in the post-op
"engine errors" guard (don't fail the call on a self-healing handled ensure); (c) make the
compile-path save not gate on the dirty flag when `save:true` was explicitly requested (or fall
back to a direct package save) so structural deletes always persist.

### [x] Common UI `add_common_button` / `add_common_text` fire a handled ensure on a dirty WBP
**RESOLVED 2026-06-06.** Root cause: `SafeAddWidgetToTree`'s non-panel-root *replacement* path
removed the old root with a bare `WidgetTree->RemoveWidget()`, which only nulls the root pointer and
leaves the old widget outered to the WidgetTree. The compiler's `ForEachSourceWidget`
(`ForEachObjectWithOuter`) then still saw that GUID-stripped orphan → `Widget [X] was added but did
not get a GUID`. Fix: after removing the old root, rename it out to the transient package
(`Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors|REN_NonTransactional)`) so it
leaves the tree's outer — matching what the engine's `DeleteWidgets` does, but WITHOUT calling
`DeleteWidgets` here (its trailing `MarkBlueprintAsStructurallyModified` reinstances mid-operation
while the just-constructed new widget is still an orphan, trashing/renaming it to e.g.
`TRASH_Text2_0` and desyncing it from its GUID entry — observed and ruled out during the fix). Also
removed the now-dead `UnregisterWidgetAndChildren` helper. Verified live: create WBP →
`add_common_text` ×2 → no ensure, root correctly named `Text2`, `compile {saveAfterCompile}` clean.
See the new open item below re: the *replace* semantics, which is unchanged.

Original report:
The FIRST `add_common_*` on a freshly created (clean) Widget Blueprint succeeds silently, but
a SECOND add — or an add after another structural edit — logs a `Handled ensure` (the native
handler returns success but the bridge's "Unreal logged errors" guard surfaces it as
`ENGINE_ERROR`). Repro 2026-06-04: create WBP → `add_common_button` (ok) → `add_common_text`
(ensure) → `add_common_button` (ensure). Likely the construct / `SafeAddWidgetToTree` path on
an already-modified, unsaved CommonUI WidgetTree. Related: runtime-added (unsaved) widgets can
vanish from the tree across later ops — a `set_common_button_input_action` couldn't find a
button an earlier call had set, because nothing saved the WBP in between. Worth deciding
whether `add_common_*` should compile+save (or at least be ensure-clean) so multi-widget
authoring in one session is reliable.

### [x] DECISION: `SafeAddWidgetToTree` silently *replaces* a non-panel root (data loss)
**RESOLVED 2026-06-06 — chose (a) error with guidance.** `SafeAddWidgetToTree` now refuses the
non-panel-root + no-`parentSlot` case instead of replacing: it cleans up the new widget
(`DiscardUnaddedWidget`) and returns false with a specific message ("the root '%s' is not a panel …
pass a parentSlot naming a panel, or add a panel root first"). Threaded an optional `FString* OutError`
through the helper and surfaced it in the Common UI callers (add_common_button/text/border). Also
gave clear guidance for the `parentSlot`-not-found / not-a-panel cases. (Original analysis below.)

**Follow-up DONE 2026-06-07:** threaded `OutError` through all 23 regular `add_*` widget callers in
`McpAutomationBridge_WidgetAuthoringHandlers.cpp` (add_canvas_panel/horizontal_box/vertical_box/
overlay/text_block/image/button/progress_bar/slider/grid_panel/uniform_grid/wrap_box/scroll_box/
size_box/scale_box/border/rich_text_block/check_box/text_input/combo_box/spin_box/list_view/
tree_view). Each now passes `&AddErr` and surfaces the specific guidance (falling back to its
generic message only if empty); the now-redundant manual `UnregisterWidgetGuid`+`RemoveWidget`
cleanup was dropped since `SafeAddWidgetToTree` already discards on every failure path. Verified
live (live-coding patch): `add_text_block` with a bogus `parentSlot` returns
`parentSlot 'NoSuchSlot' was not found in the widget tree.`, the happy path still adds, and the
failed add leaves the WBP intact (a subsequent add into a real panel succeeds).

Surfaced while fixing the ensure above. When you add a widget with no `parentSlot` and the current
root is a **non-panel** widget (e.g. a `TextBlock`/`CommonTextBlock`), `SafeAddWidgetToTree` discards
the existing root and installs the new widget as root. This is now *clean* (no ensure/orphan/trash),
but it still silently throws away the previous widget — which is almost certainly never intended and
is the "widgets vanish" symptom from the entry above. It only bites the parent-less + non-panel-root
edge; the normal menu flow (panel root + `parentSlot`) hits the add-as-child path and is unaffected.
Three options — **Aaron's call** (left as-is for now):
(a) **error** with guidance ("root '%s' is not a panel; pass parentSlot or add a panel root first") —
safest, no silent loss; (b) **auto-wrap** — create a VerticalBox/CanvasPanel, reparent the old root
under it, then add the new widget (most convenient, keeps both, more opinionated); (c) keep the
replace but make it loud. Leaning (a). Not changing semantics without a decision.

### [x] Native MCP transport drops mid-call while the editor GameThread is busy — RESOLVED (pull architecture, 2026-06-06)
When the editor is busy (asset-registry scan / GC / still initializing right after launch, or a
long synchronous op), a `tools/call` can fail with "MCP server transport dropped mid-call;
response was lost" even though the operation **completed**. Repro 2026-06-04: two back-to-back
`manage_asset` calls (`delete_asset`, then `exists`) dropped right after a relaunch; the editor
stayed alive and the delete had in fact succeeded (a later `exists` showed the asset gone) — only
the responses were lost. So the client can't tell success from failure for a mutating op.

Root cause (read `McpNativeTransport.cpp`): `HandleToolsCall` (~L1271) sends the SSE response
headers on the socket thread, parks the connection in `SSEConnections`, then
`QueueAutomationRequest`s the work onto the subsystem queue — which is *"drained by the core ticker
after world ticking"* (~L1419), i.e. on the **GameThread, once per tick**. So nothing runs and **no
bytes flow on the per-request SSE stream** until the GameThread ticks. Crucially the per-request
stream (`FSSEConnection`) has **no keepalive** — the `:keepalive` frames
(`WriteNotificationKeepalive`) only cover the long-lived `GET /mcp` notification streams
(`FNotificationStream`). So while the GameThread is saturated the response stream is silent and the
connection is torn down (client read timeout, well under the server's 300s `RequestTimeoutSeconds`,
or a reset during init). The GameThread later drains the queue and runs the op — mutation lands —
but `CompletePendingRequest` then writes the result to a dead socket (write fails under the 5s
`WriteTimeoutSeconds`), so it's logged as lost and is unrecoverable.

**RESOLVED 2026-06-06** (commits 9d96cc6 / 5361a5f / bce9344) — took a simpler path than the
keepalive/result-cache directions originally sketched here: went **pull-only**. De-streamed
`tools/call` to a plain HTTP request/response (no SSE → the parked-silent-stream failure mode is
gone), removed all server→client push + keepalive, disabled and deleted the WebSocket transport,
and made mutating ops idempotent (fail-fast on name conflict) so a dropped response is recovered by
retry / re-querying editor state — no result cache needed. Full design: `docs/pull-architecture.md`
(the old brief `docs/transport-mid-call-drop-problem.md` is marked superseded).

### [x] `ReadHttpRequest` recv loop busy-polls (1ms sleep-spin, byte-at-a-time headers) instead of a readiness wait
**RESOLVED 2026-06-06.** Both loops now drain the socket in 2KB chunks and block on
`Socket->Wait(ESocketWaitConditions::WaitForRead, slice)` when idle (mirroring the send-side
`Wait(WaitForWrite)`), instead of `Sleep(0.001)` spinning + 1-byte `Recv`. Header scan uses a
3-byte cross-chunk overlap for the `\r\n\r\n` terminator; over-read bytes carry over as the body
start; readable-with-no-data ⇒ peer close. 5s deadline kept. Verified live with normal calls and a
9 KB multi-chunk-body import (500 rows intact). Original report:
The HTTP receive path in `McpNativeTransport.cpp` (`ReadHttpRequest`, ~L681) reads request
**headers one byte at a time** — `HasPendingData()` → on empty `Sleep(0.001)` and spin →
`Recv(&Byte, 1, …)` (~L699-711) — and the **body** loop does the same 1ms-sleep-on-empty spin
(~L819-843), each bounded by a 5s deadline. It's a mini busy-wait, not a real readiness wait:
puts a ~1ms latency floor on every read and burns a little idle CPU per in-flight connection.
It's also inconsistent with the **send** side (`SendAllBytes`, ~L283), which correctly uses
`Socket->Wait(ESocketWaitConditions::WaitForWrite, timeout)` to block until the socket is ready.
Found by code-read 2026-06-05; harmless at one agent on localhost, would only bite if connection
volume grew. Pure cleanup, not a correctness bug.
Fix: replace the `HasPendingData()` + `Sleep(0.001)` spins with
`Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan)` (mirroring `SendAllBytes`), and read
headers in chunks into a buffer (scan for `\r\n\r\n`) instead of one byte per `Recv`. Keep the 5s
deadline; removes the latency floor + idle-CPU spin and makes recv/send symmetric.

---

## Done

_(completed items, newest first)_

### [x] Bug fix: `create_widget_blueprint` ignored `savePath`
`manage_blueprint create_widget_blueprint` read `path`/`folder` but not `savePath` (the
`manage_blueprint` schema's param), so callers passing `savePath` silently got the `/Game/UI`
default. Now accepts `savePath` as an alias (precedence: `path` → `savePath` → `folder` →
`/Game/UI`). `.cpp`-only change in `McpAutomationBridge_WidgetAuthoringHandlers.cpp`. Verified
live: `savePath:/Game/McpTmp` created the WBP at `/Game/McpTmp/WBP_SaveTest`.

### [x] Bug fix: `delete` / `delete_asset` ignored `assetPath`
`HandleDeleteAssets` read `path`/`paths` but not `assetPath`/`assetPaths` (which the
`manage_asset` schema advertises), so `delete_asset { assetPath: … }` failed with "No paths
provided". Now accepts `assetPath` (singular) and `assetPaths` (array) alongside `path`/`paths`.
`.cpp`-only change in `McpAutomationBridge_AssetWorkflowHandlers.cpp`. Verified live:
`delete_asset { assetPath: "/Game/McpTmp/WBP_SaveTest" }` deleted it (`deletedCount:1`).

### [x] `set_common_button_input_action` (Common UI)
Binds a `UCommonButtonBase`'s `TriggeringInputAction` (`FDataTableRowHandle`) on a button
widget inside a Widget Blueprint. Params: `widgetPath`, `widgetName`, `dataTable`, `rowName`.
Validates the widget is a `UCommonButtonBase` and the DataTable's `RowStruct->IsChildOf(
FCommonInputActionDataBase)` (`/Script/CommonUI.CommonInputActionDataBase`). Writes the handle
**directly via reflection** (`FindFProperty` → `ContainerPtrToValuePtr`) rather than calling
`UCommonButtonBase::SetTriggeringInputAction()`, whose runtime input binding
(`BindTriggeringInputActionToClick`) fires when `!IsDesignTime()` — true for a
programmatically-loaded WidgetTree template, i.e. it would do runtime binding on an archetype
with no owning player. The direct write is what the details panel persists. Reports `rowFound`
(non-blocking). Routed via `CommonUi()` (also feeds the `manage_blueprint` schema enum).
Verified live on a fabricated `WBP_MenuButton` instance + a `CommonInputActionDataBase` table:
set → independent read-back of `TriggeringInputAction` returned the exact `{DataTable, RowName}`
(twice), and the `WRONG`/`INVALID_ROW_STRUCT`/`NOT_FOUND`/`WIDGET_NOT_FOUND` guards all reject.

### [x] `manage_asset` action `create_data_table`
Create a `UDataTable` with a given row struct. Params: `name`, `path`, `rowStruct` (full
object path or bare struct name → `LoadObject` then `TryFindTypeSlow` fallback), optional
`save` (default true). Validates the struct derives from `FTableRowBase`; saves via
`McpDirectPackageSave` + registers with the asset registry. Routed in `HandleAssetAction`
+ declared in the subsystem header + listed in `ManageAssetCore()` (which also feeds the
tool schema enum, so it is advertised automatically). Verified live: full-path + bare-name
creation, `RowStruct` read-back, and rejection of a non-row struct (`Vector`) and an unknown
name. Generally useful, and unblocks `set_common_button_input_action`.

### [x] Generic reflection widening (R3): helpers-twin `FText` parity
The `McpAutomationBridgeHelpers.h` twin (`inspect get/set_property`, `set_object_property`)
lagged the asset twin on `FText`: export had no `FTextProperty` branch (text dropped to
`nullptr` on reads) and import ended in a hard "unsupported" error (no string fallback, so
`FText`/text-importable types couldn't be written). Added the `FTextProperty` export branch
(display string) + the same generic `ImportText_Direct` string fallback the asset twin had,
bringing the two twins to parity. Verified on a fabricated BP CDO fixture: an `FText` var
round-trips through **both** twins to the identical value incl. embedded quotes/commas
(`Say "hi" to all, friend — don't stop`). `FClassProperty` was already covered (derives from
`FObjectProperty`) and re-verified round-tripping `/Script/Engine.PointLight`.

### [x] Numeric reflection correctness: float/double variables + map/set export
Two distinct bugs found by thoroughly probing every numeric type in every position
(scalar / Array / Set / Map key / Map value) on a fabricated BP CDO fixture:
- `c0bff6a` — `add_variable` mapped `float`/`double` to the legacy `PC_Float` *category*,
  which UE5 doesn't honor → the BP compiler produced an **int** property → fractional values
  truncated (`1.5 -> 1`) for scalar AND container float/double vars (pre-existing for
  scalars). Fixed: `PC_Real` + float/double `PinSubCategory` at every position.
- `40c3bf9` — the map-value and set-element **export** (in BOTH reflection twins) only
  handled str/int/float/bool, dropping double/int64/byte/enum/object/struct to an
  `ExportText` string; non-str/name/int map keys became `"key_%d"`. Fixed: delegate to
  `ExportPropertyToJsonValue` (full coverage), keys preserved via `ExportText`.
Verified (rebuild + relaunch): scalar float/double + `Array<Float>`/`Set<Float>`/`Set<Double>`/
`Map<Name,Float>`/`Map<Name,Double>`/`Map<Name,Int64>` all round-trip correct JSON numbers.

### [x] `add_variable` container variables (Set / Array / Map)
`3728d75`. Accepts `Set<Inner>`/`Array<Inner>`/`Map<Key,Value>` (T-prefixes ok) →
`PinType.ContainerType` (+ `PinValueType`). Verified by creating + value round-tripping.

### [x] Fix `TMap` value import + runtime-verify `TSet`
`6bea6a0`. `TMap` import (`da29c05`) double-offset the value (`GetValuePtr` vs the
`*_InContainer` accessors) → all primitive values dropped to default (empty-struct IMC
fixture masked it). Now `GetPairPtr`. Verified `Map<Name,Int>` round-trips; `TSet` import
runtime-verified via a fabricated `Set<Name>`.

### [x] `FText` property export
`6b98c69`. `ExportPropertyToJsonValue` emits an `FText`'s display string. Verified via a BP
CDO fixture. Import already worked via the generic `ImportText` fallback.

### [x] `TMap` / `TSet` property import (initial)
`da29c05`. Maps from a JSON object, sets from a JSON array; keys/values route through the
importer; both tolerate a stringified form. (Value-offset bug fixed in `6bea6a0`.)

### [x] Instanced subobject round-trip — export + import (all shapes)
EXPORT `d72f8c3`. IMPORT `7589789` (top-level arrays — owner threaded → `NewObject` Outered
to the asset) and `685282f` (struct-nested — strip instanced fields before
`JsonObjectToUStruct`, re-instance after). Verified on `IA_Mute.Triggers` and
`AM_AttackMontage1.Notifies`.

### [x] Deep export/import of struct & object-ref array properties
Recurses struct/object/other inners (depth-capped, `ExportText` fallback). Mirrored into the
helpers twin. Verified on `IMC_GameCommands.DefaultKeyMappings.Mappings`.

### [x] Advertise the new `manage_asset` actions (native schema)
`get_referencers`/`get_asset_properties`/`set_asset_property` were already routed; added
`propertyName`/`includeTransient` param docs. (The planned TypeScript-schema edits were a
no-op for this native-only fork and were removed with the TypeScript bridge.)
