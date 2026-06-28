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
- ✅ **`compile_material` now reports real compile status** — RESOLVED 2026-06-24 (live-verified).
  It reads `Material->GetMaterialResource(GMaxRHIShaderPlatform)->GetCompileErrors()` after its
  `PostEditChange` recompile and returns `compiled:false` + an `errors[]` array instead of the old
  hardcoded `{compiled:true}`. Verified against a Substrate decal fed a `SubstrateUnlitBSDF` →
  `{"compiled":false,"errors":["Decals only support Substrate Slabs and Shading Model nodes, not Unlit
  node...","Substrate material errors encountered."]}`. Translation errors are populated synchronously
  by `PostEditChange`; **async shader-backend errors are NOT covered** (e.g. `no member named 'TexCoords'
  in 'FMaterialPixelParameters'` from a deferred-decal Custom node — compile_material returned
  `compiled:true` while the shader actually failed; caught only in the shader log). Deferred deliberately
  2026-06-27: reliably catching these needs `FinishCompilation()` on the `FMaterialResource` (block on the
  shader workers) + a version-safe way to read shader-backend errors (they don't reliably land in
  `GetCompileErrors()`), which is uncertain engine-internal territory — not worth rushing into a schema
  rebuild and risking a game-thread stall or breaking a working handler. Meanwhile **verify decal shaders
  via `tail_log` (LogShaderCompilers/LogMaterial), not compile_material**. **Follow-up — `FinishCompilation`
  pass DONE 2026-06-28** (opt-in, verified): `compile_material {waitForShaders:true}` now calls
  `FMaterialResource::FinishCompilation()` (blocks on the shader workers) then reads the backend signal via
  `UMaterial::IsCompilingOrHadCompileError(GMaxRHIShaderPlatform)` — the **EShaderPlatform** overload, since
  the `ERHIFeatureLevel::Type` one is `UE_DEPRECATED(5.7)` (verified against engine source; using it would
  fail a deprecation-as-error build). A backend failure with no recoverable string still fails loudly (a
  synthesized errors[] entry pointing at the shader log). Default `false` preserves the fast path exactly, so
  no existing caller is slowed and the working handler is untouched; response carries `waitedForShaders`.
  Verified: a valid scratch material → `{compiled:true, waitedForShaders:true}`, no game-thread stall.
  **Read-back errors[] DONE 2026-06-28** (cpp-only, Live-Coding hot-patched + verified): `get_material_info`
  and `get_material_stats` now both carry `compiled` + (on failure) the same `errors[]` from
  `GetCompileErrors()`, so a read call diagnoses a broken material instead of a silent value;
  `get_material_stats` keeps `instructionCount:-1` but adds an honest `instructionCountNote` (the real count
  needs a representative `FShaderType*` via the MaterialEditor stats path, which isn't linked here — not worth
  a module dependency for a stat). Verified compiled-true path on M_AoE_Telegraph; failure path reuses the
  exact `GetCompileErrors()` already proven in compile_material. Origin: the gap caused a live misdiagnosis —
  blind to the real error, the assistant guessed the cause from docs.
- ✅ **`add_custom_expression`/`update_custom_expression` `inputs`/`additionalOutputs` were undeclared in
  the `manage_asset` schema** — FOUND + FIXED 2026-06-27. The handlers parse `inputs:[{name}]` /
  `additionalOutputs:[{name,type}]` correctly, but the params were never declared in
  `McpTool_ManageAsset.cpp::BuildInputSchema`, so a schema-validating client (Claude Code) strips the
  arrays before they reach the bridge. Result: the Custom node keeps its single **default input named
  `None`**, so the HLSL's named pin (e.g. `UV`) can never be wired and the shader references an undefined
  symbol. Cost a long misdiagnosis — an `inputCount:1` readback is the *default* input, NOT proof the
  array applied; always confirm via `get_node_properties` input **names**. Fix: declared both as
  `.ArrayOfObjects(...)` (mirrors `manage_blueprint`'s `inputs`). ⚠️ The tool schema is built once and
  cached (`FMcpToolRegistry::EnsureCache`, invalidated only on tool *registration*), so a schema change
  needs a **rebuild + editor restart + MCP reconnect** to go live — Live Coding patches `BuildInputSchema`
  but the cache still serves the old schema, and a plain restart reverts the in-memory patch.
- ✅ **`set_node_value` — material-expression constant VALUE setter DONE 2026-06-28** (built + rebuilt
  `-NoUBA` + verified end-to-end over direct-HTTP). `manage_asset set_node_value {assetPath, nodeId, ...}`
  writes the literal value(s) the old graph couldn't set node-by-node (forcing a single `add_custom_expression`
  with constants baked into HLSL). Coverage by node class: `Constant` ← `value` (R); `Constant2Vector` ←
  `r`/`g`; `Constant3Vector`/`Constant4Vector` ← `r`/`g`/`b`/`a` on the `Constant` FLinearColor (omitted
  channel = partial update; a bare `value` sets every colour channel uniformly); `ScalarParameter.DefaultValue`
  ← `value`; `VectorParameter.DefaultValue` ← `r`/`g`/`b`/`a`; arithmetic nodes ← `constA`/`constB` (and a
  bare `value`→ConstA). The ConstA/ConstB path is **reflection-based** (`FindFProperty<FFloatProperty>`), so it
  covers the open set of arithmetic classes (Add/Subtract/Multiply/Divide/Min/Max/Fmod/…) with no per-class
  cast or include. Unsupported nodes return `UNSUPPORTED_NODE`. `FINALIZE_HOST()` + `save` (default true).
  Verified: Constant3Vector→`(0.1,0.2,0.3)`, ScalarParameter→`0.7`, Add→`ConstA=1.5 ConstB=2.5` (all confirmed
  by `get_node_properties` readback), TextureCoordinate→`UNSUPPORTED_NODE`, scratch asset deleted clean.
  Routing in `MaterialAuthoring()` (so it lands in both the `ManageAsset()` schema enum and the
  `IsMaterialAuthoringAction` dispatch). Schema declares `r/g/b/a/constA/constB`. **Caveat:** the existing
  `value` param is schema-typed `FreeformObject`, so the *typed* client may not forward a bare numeric `value`
  (direct-HTTP is unaffected); the dedicated `r/g/b/a/constA/constB` are `.Number()`. Tighten `value`'s schema
  type if a typed-client scalar set ever misbehaves.
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
- ✅ **Log + profiling VISIBILITY (pull-based) — DONE 2026-06-20** (built + verified live on a baked
  `-NoUBA` build). The old `manage_logs subscribe` was a **lying-success stub**: it registered an
  output device that formatted every line to JSON and dispatched it to `SendRawMessage` — a no-op
  since the WebSocket push path was removed (pull-only architecture). So `subscribe` reported
  `subscribed:true` while NO log ever reached the caller, burning CPU per line. Replaced with a
  pull-correct **server-side ring buffer** (always-on from Initialize, 5000 lines, O(1) circular
  append, formatting deferred to read time) + poll actions on `system_control` (→ `HandleLogAction`):
  - `get_log`/`tail_log` — recent lines with `count` (≤2000), incremental `sinceSeq` cursor
    (response carries `nextSeq` + `dropped` so an evicted gap is never silent), and `category` /
    `verbosity` (min-severity) / `contains` filters. `clear_log` empties the ring (seq stays
    monotonic so cursors don't alias). `subscribe`/`unsubscribe` kept but now HONEST (toggle capture;
    read via tail, no fake push). Verified: 1539 boot lines captured, filter → exactly the 3
    LogStaticMesh Nanite warnings, `sinceSeq` returned only newer seqs with nextSeq advanced,
    clear → ringCount 3 / oldestSeq 1548 (monotonic).
  - **`get_profile`** (alias `capture_stats`/`get_perf_stats`, on `manage_performance`) — READS BACK
    live counters as JSON instead of writing a `.ue4stats` file the agent can't consume:
    `averageFPS`/`averageFrameMs` (the `extern ENGINE_API` globals — no public header, declared
    locally like the engine's own TUs), per-thread `gameThreadMs`/`renderThreadMs`/`rhiThreadMs`
    (RenderTimer.h `RENDERCORE_API`) + `gpuMs` (`RHIGetGPUFrameCycles()` — the non-deprecated
    replacement for `GGPUFrameTime`), and memory (`FPlatformMemory::GetStats()` → used/peak/avail MB).
    Verified live: real game-thread 87ms / GPU 10ms / 5.26GB used.
  - **Still future (🟡):** true real-time PUSH streaming + remote Unreal Insights trace capture
    (`FTraceAuxiliary` — TraceLog/TraceAnalysis ARE linked). Push doesn't fit the pull-only transport,
    so a poll loop on `tail_log`/`get_profile` is the current model; an Insights `start_trace`/
    `stop_trace` to a `.utrace` is the natural next step if wanted.

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
    `add_subnode`. One guard in `HandleManageAIAction`. (2026-06-21 `0014826`: confirmed the dead branches were
    already removed — only the guard handles these sub-actions; corrected the stale "delete the dead branches" comment.)
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
- ✅ **Enhanced Input authoring depth — 3 papercuts FIXED + verified live 2026-06-20** (baked build;
  verified via direct-HTTP since the typed `manage_networking` schema didn't carry these params — now
  declared, see below). The modifier/trigger factories are otherwise functional; these were the rough
  edges found authoring IA_Heal end-to-end 2026-06-17.
    1. ✅ **`create_input_action` now takes an optional `valueType`** (Boolean|Axis1D|Axis2D|Axis3D, +
       aliases Bool/Float/Vector2D/Vector/1D/2D/3D) — parsed+validated before creation (fail-fast, no
       half-made asset), applied on both the new- and existing-asset (idempotent) paths; response echoes
       `valueType`. Verified: `create_input_action{valueType:"Axis2D"}` → `valueType:2`.
    2. ✅ **`get_input_info` now emits an IA's own `triggers[]`** (class names) + `triggerCount`.
       Verified: `IA_Heal` → `triggers:["InputTriggerDown"]` (previously un-confirmable via the bridge).
    3. ✅ **`set_input_trigger` is no longer blindly append-only** — defaults to idempotent
       (dedup-by-class: a re-run reports `alreadyPresent:true, triggerSet:false`, no duplicate); pass
       `replace:true` to `Empty()` then add for a clean re-author. Response carries `triggerSet`/
       `alreadyPresent`/`replaced`/`triggerCount`. Verified: Hold→Hold (count stays 1)→Pressed replace
       (count 1, `triggers:["InputTriggerPressed"]`).
  - Schema: the Input params (`name`/`path`/`valueType`/`assetPath`/`actionPath`/`contextPath`/`key`/
    `triggerType`/`modifierType`/`replace`) were undeclared in the `manage_networking` tool schema, so a
    schema-validating client (the normal Claude client) couldn't pass them — declared now (takes effect
    next MCP session). This also resolves the schema-discoverability half of [[2026-06-18b]] for Input.
- ✅ **Common UI completeness — effectively DONE 2026-06-20** (only optional runtime test-tooling left).
  Shipped + verified: authoring (add button/text/border, assign style, **style-asset creation**,
  `set_common_button_input_action` = the input-action→click wiring), runtime introspection
  (`inspect ui_focus`), faithful drive (`control_editor simulate_nav`), and the focus-relay/desired-focus
  authoring (✅ sub-bullets below). **Only genuinely-open piece: a runtime activatable-stack push/pop drive**
  — deliberately not built (🟢 low value): it's PIE test-tooling only (CommonUI pushes/pops natively via
  button clicks at runtime, so the menus don't need a bridge action for it), and it can't be meaningfully
  verified without a `UCommonActivatableWidgetStack` live in PIE. Promote only if a future test workflow
  needs to script stack transitions. See `COMMONUI_INTEGRATION_PLAN.md`.
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
- ✅ **Asset import pipeline — effectively COVERED (assessed 2026-06-20); only a narrow static-mesh-asset
  collision sliver is uncovered, and it's not needed.** On audit, post-import config is mostly already there:
  texture compression/group/LOD-bias/streaming/virtual-texture via `set_compression_settings`/
  `set_texture_group`/`set_lod_bias`/`set_streaming_priority`/`configure_virtual_texture`; mesh
  LOD/Nanite via `generate_lods`/`nanite_rebuild_mesh`; **sRGB, audio codec/sample-rate/compression,
  collision *complexity* (`BodySetup.CollisionTraceFlag`), and any other reflected UPROPERTY via the
  generic `set_asset_property`** (dotted subpaths reach component/struct fields). **Collision *generation*
  already exists** too — `manage_geometry generate_collision` (box/sphere/capsule/convex/convex_decomposition
  via Geometry Script) — but it targets a placed **`ADynamicMeshActor`** (`actorName`), NOT a `UStaticMesh`
  asset. So the ONE genuine hole is generating simple collision on a static-mesh **asset** at import — which
  the project doesn't need (combat/UI focus) and which collides on both the action name and the C++
  `HandleGenerateCollision` symbol with the geometry one. **Decision: not built** (tried 2026-06-20, hit the
  name collision, reverted — don't ship a confusingly-duplicate action for an unneeded capability). If ever
  needed, the design is ~30 lines: load `UStaticMesh` by `meshPath` → `GetBodySetup()`/`CreateBodySetup()` →
  size an `FKBoxElem`/`FKSphereElem`/`FKSphylElem` from `GetBoundingBox()` → `AggGeom.*Elems.Add` →
  `InvalidatePhysicsData`/`CreatePhysicsMeshes` → `McpSafeAssetSave`; register it under a DISTINCT name
  (e.g. `manage_asset add_static_mesh_collision` / `HandleAddStaticMeshCollision`) to avoid both collisions.
- ❎ **GAS ability-graph depth — N/A for this project (won't build), 2026-06-20.** Verified the game uses
  **zero GAS**: no `UAbilitySystemComponent`/`UGameplayAbility`/`UGameplayEffect`/`UAttributeSet` anywhere in
  `Source/` — combat runs on the custom `UAttributeComponent` ([[project_combat_prep]]). Templated
  ability-task chains would be capability the project never executes, so this is intentionally not built.
  (The GAS scaffolding actions remain in the bridge for any future/other project; only the *depth* item is dropped.)
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
  (incl. functional/latent tests, which the worker drives over real frames).
  - ✅ **Test-stub authoring — DONE 2026-06-20** (closes the "authoring NEW tests needs a .cpp"
    gap). New `system_control generate_test_stub` writes a correct, registerable automation-test
    `.cpp` — the value isn't the boilerplate (an agent can write a file) but **correct-by-construction
    flags**: it emits the known-good `EAutomationTestFlags::EditorContext | EngineFilter` +
    `WITH_AUTOMATION_TESTS` shape matching the self-tests (wrong flags = compiles but never appears
    under run_tests). Params: `testName` (the registered path + run_tests filter), `testType`
    (simple|complex|latent), `className`/`outputPath`/`flagsExpr`/`overwrite` (defaults: derived
    symbol, the plugin's `Private/GeneratedTests/` dir, editor+engine flags). **Verified the WHOLE
    author→compile→run loop with NO editor restart:** generate_test_stub → `live_coding_compile`
    (Live Coding DOES pick up brand-new files) → list_tests shows it registered → run_tests/
    get_test_results → `passed:1`. So a test can now be authored AND run entirely over MCP.
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

### [x] 2026-06-22a — `create_node nodeType:"SpawnActorFromClass"` hard-crashes the editor (assert `EdGraphNode.h:586`)
Repro (live, 2026-06-22, building a telegraph-spawner BP in the combat gym): `create_node` on a fresh
Actor BP's `EventGraph` with `nodeType:"SpawnActorFromClass"` (only `posX`/`posY` set) → client call
times out, then the editor hard-crashes — `Assertion failed: Result [EdGraphNode.h:586]` (generic
node-construction assert; ~1 min to CrashReporter). Process gone, port 3123 refused on next call.
The assert is in base `UEdGraphNode` (not SpawnActor-specific), so other `create_node` node types may
share the fault — treat bridge BP-graph authoring as crash-risky until root-caused. Likely the spawn
factory adds the node without a valid graph/schema or mis-orders `AllocateDefaultPins`.
Workaround used this session: wrote the spawner in C++ (`ATelegraphSpawner`) instead of authoring a graph.

**ROOT CAUSE (confirmed against engine source 2026-06-22):** the generic fallback runs
`PostPlacedNewNode()` BEFORE `AllocateDefaultPins()` (`McpAutomationBridge_BlueprintGraphHandlers.cpp`
:1506 then :1507). `UK2Node_SpawnActorFromClass::PostPlacedNewNode()` calls `GetScaleMethodPin()` →
`FindPinChecked(TransformScaleMethodPin)` on a pin that `AllocateDefaultPins()` only creates →
`check(Result)` at `EdGraphNode.h:586`. (`FGraphNodeCreator::Finalize` uses the same PostPlaced→Allocate
order, so it'd crash too; the engine's menu path only survives by duplicating a *pre-allocated template*
whose pins already exist.)

**FIX (live-verified + committed 2026-06-23):** dedicated branch for the
`UK2Node_ConstructObjectFromClass` family — allocate pins BEFORE `PostPlacedNewNode`, then if `targetClass`
is supplied set the class pin + `ReconstructNode` so expose-on-spawn pins appear. Generic fallback left
untouched (correct for static-pin nodes; a global reorder would risk Timeline etc.).
Live test (scratch `BP_McpSpawnNodeTest`, `SpawnActorFromClass`): no crash; bare node creates; with
`targetClass:/Game/VFX/Telegraph/BP_Telegraph.BP_Telegraph_C` the `Class` pin is set, the `ReturnValue`
pin re-specializes to `BP_Telegraph_C`, and the `TransformScaleMethod` pin (whose absence triggered the
assert) is present. Gotcha for callers: Blueprint classes need the `.<Name>_C` generated-class suffix in
`targetClass` (a bare asset path returns `CLASS_NOT_FOUND` — by design, fails loud).

### [ ] 2026-06-22b — Anim state-machine reads via `get_graph_details`: works, two gaps
Live-verified 2026-06-22 against `ABP_Robo_MainStates`. `get_graph_details` reads anim graphs cleanly:
`AnimGraph` returns the root + state-machine nodes + cached-pose nodes; a state-machine graph addressed
by its title (`Main States`, `Ground Locomotion`) returns its states + transition nodes (titles like
"GroundLocomotion to InAir"). So topology IS readable, including nested state machines — the earlier
"fuzzy/maybe-unsupported" worry was wrong. Two real gaps remain:
1. **False dead-node flags on dataflow AnimGraphs.** Reading the root `AnimGraph` reported all 6 nodes
   as `deadNodes` / reason `feeds-nothing-live`. The dead-node heuristic uses exec-reachability, which is
   meaningless in a pose-dataflow graph (no exec pins) → every anim node is mislabeled "dead." (The
   state-machine graphs correctly report `deadNodeCount:0`.) Fix: skip/relax dead-node detection for
   AnimGraph-class graphs, or treat pose-pin links as liveness.
2. **Transition conditions not surfaced.** Transition nodes appear by title but the rule graph that
   decides *when* they fire (IsFalling / GroundSpeed compares) isn't returned — you get topology, not the
   "why." Would need each `AnimStateTransitionNode`'s bound rule graph exposed.

### [x] 2026-06-22c — Recurring: first `tools/call` after editor launch hangs the game thread
Hit on 3/3 post-rebuild relaunches 2026-06-22. Symptom: bridge connects (`MCP session initialized`
logged), the FIRST `tools/call` is received (logged) but never completes; every later call queues behind
it and times out. Editor process is ALIVE, `Responding=True`, CPU LOW/idle (not busy-looping), log goes
silent after the first call's line, no visible modal (Aaron is remote, confirms not a dialog). So the
request executor blocks on the first request and never returns — a deadlock / blocking-wait, not slowness
(waited 7 min on one, never returned). Force-kill + relaunch is the only recovery (clean `QUIT_EDITOR`
can't be sent — bridge wedged), but it reproduced 3×. Aaron confirms PRE-EXISTING (seen before this
session). Likely a lock/ordering issue between request dispatch and the game-thread tick, possibly
sensitive to heavy post-launch state (shader-autogen compile was in flight). Caveat: appeared correlated
with a bridge-DLL rebuild, but the hung handlers (manage_level/asset) never touch the rebuilt create_node
code and the module loads + receives requests fine → not obviously the create_node change. HIGH impact:
wedges all bridge ops; needs root-cause.

**DIAGNOSED 2026-06-23 — it's the unclean-shutdown "Restore Packages" modal.** Back at the machine (not
remote), the wedged editor was sitting on the engine's startup *"Editor did not shut down cleanly — restore
auto-saves?"* dialog. It blocks the game thread the bridge tick rides on, so the first `tools/call` is
received then never serviced; `Responding=True` because the modal still pumps the window message loop; CPU
idle because it's waiting on a click. The earlier "not a dialog" was a bad inference from remote (no eyes on
the screen). ROOT CAUSE is self-inflicted: this modal only appears after a **force-kill** — which is exactly
the hang's own "recovery" (force-kill + relaunch), so the kill-loop *manufactured* the next hang. WORKAROUND
(already standing policy): never force-kill; clean-quit via `QUIT_EDITOR`/console so no crash auto-save state
is left. Clicking `Skip Restore` immediately freed the game thread and the bridge serviced calls normally.
Potential hardening: extend the `GIsRunningUnattendedScript` auto-dismiss guard (added for save modals) to
the startup package-restore prompt so a force-killed editor self-recovers instead of wedging.

### [ ] 2026-06-23 — `add_montage_notify` w/ `notifyClass` never sets the Notify object's NotifyName → PlayMontageNotify broadcasts NAME_None
FOUND (source-inferred, not live-repro'd) 2026-06-23, wiring a MiliBot dummy's montage→SpawnAoE. The
authoring handler (`McpAutomationBridge_AnimationAuthoringHandlers.cpp` ~1688-1709) builds the
`FAnimNotifyEvent`, sets `NotifyEvent.NotifyName` (the EVENT's name) and `NotifyEvent.Notify =
NewObject<UAnimNotify>(class)` — but never sets `NewNotify->NotifyName` (the notify OBJECT's own member).
The "Play Montage" BP node keys off the OBJECT member: `UAnimNotify_PlayMontageNotify::BranchingPointNotify`
broadcasts `OnPlayMontageNotifyBegin.Broadcast(NotifyName)` using *its own* `NotifyName`
(`AnimNotify_PlayMontageNotify.cpp:30`, the ONLY broadcast site engine-wide). So a bridge-created
PlayMontageNotify fires `On Notify Begin` with `None` → can't be matched by name in Blueprint, the whole
"Play Montage → On Notify Begin → branch on name" pattern silently no-ops. Fix: when the resolved class
`IsChildOf(UAnimNotify_PlayMontageNotify::StaticClass())` (or the Window variant), also
`NewNotify->NotifyName = FName(*NotifyName)`. Workaround until then: add "Montage Notify" in the Persona
montage editor (it sets the object name). NB: two `add_montage_notify` handlers exist
(`AnimationAuthoringHandlers.cpp:1615` has `notifyClass`; `AnimationHandlers.cpp:3205` doesn't) — confirm
which one the `animation_physics` route actually dispatches to.

### [x] `.uplugin` `InterchangeOpenUSD` reference force-enables USDCore → editor exits on engines with non-loadable USD binaries
**FOUND + FIXED 2026-06-12** (HandPanicVR, Meta UE 5.7.3 fork, headless `-unattended` launch).
The bridge `.uplugin` referenced `InterchangeOpenUSD` (`Enabled:true, Optional:true`). On the
Meta fork, USDCore's `UnrealUSDWrapper` module fails to load its third-party DLLs → plugin-load
error dialog → `EngineExit()`. Killer detail: **plugin-to-plugin references override project-level
disables** — HandPanicVR.uproject `"USDCore": Enabled:false` did NOT stick while the bridge ref
existed (verified via `Mounting Engine plugin InterchangeOpenUSD` in the log). Fixed by removing
the `InterchangeOpenUSD` reference from the bridge `.uplugin`; nothing in the bridge hard-requires
it (Interchange features degrade gracefully). Audit the other `Optional:true, Enabled:true` refs
for the same footgun on engines where their binaries don't load. (Same-symptom but NOT bridge:
`GameplayInsights/RewindDebuggerRuntime` also fails to load on this fork — engine-side; disabled
per-project in HandPanicVR.uproject.)

### [ ] `live_coding_compile` reports `NoChanges` for sources edited outside the editor + returns zero diagnostics
**FOUND 2026-06-01** (HandPanicVR FingerGun work). Files edited on disk (not via the editor's
own watcher) → `live_coding_compile` returned `{"success":true,"compileResult":"NoChanges"}`
while the edits sat uncompiled; cost a multi-step diagnosis (DLL-timestamp archaeology). Two
gaps: (a) change detection misses external edits — needs a pre-compile rescan or a `force`
param; (b) the response carries only the result enum — on Failure there are no compiler
errors/warnings, no patched-module list. Should return structured diagnostics (file:line errors)
and, on NoChanges, the *reason* (no dirty files seen vs. session inactive).

### [ ] No build/state visibility: `get_build_status` action wanted (+ structured `run_ubt` results)
**FOUND 2026-06-01** (same session). There's no way to ask the bridge "is the loaded module
stale vs. its sources?" — had to resort to PowerShell timestamp diffs to learn the editor was
serving a months-old DLL (which also produced a confusing `NOT_IMPLEMENTED` for a freshly-added
action). Proposal: `get_build_status` returning per-module loaded-DLL path+timestamp, newest
source timestamp, dirty flag, and Live Coding session state; `run_ubt` should return structured
pass/fail + parsed error list. Related: `get_log` (editor/UBT log tail with filters) implemented
2026-06-12 in `HandleSystemControlAction` + `SystemControlCore()` — pending live verification.

### [ ] `inspect_class` / Python can't resolve native `/Script/<Module>.<Class>` classes
**FOUND 2026-06-01** (HandPanicVR). `inspect_class classPath=/Script/HandPanicVR.FingerGunComponent`
→ `CLASS_NOT_FOUND`; Python `unreal.FingerGunComponent` missing and `unreal.load_object(None,
"/Script/...")` → None (game-module classes aren't exposed as Python attrs). Net effect: no
one-call way to verify "did my new UPROPERTY land in the running editor?" `inspect_class` should
resolve native classes via `FindFirstObject<UClass>`/`StaticFindObject` on the `/Script/` path and
read CDO property lists, not just Blueprint assets.


### 2026-06-20e — Graph-authoring papercuts found building the SaveGame demo (BP_SaveManager)
Surfaced authoring a real save/load graph end-to-end via the bridge (create_node → connect_pins → set_pin_default_value).
- ✅ **FIXED: `set_pin_default_value` ignored object/class/soft pins.** It only called `Schema->TrySetDefaultValue(*Pin, Value)`,
  which sets the string `DefaultValue` — but `PC_Object`/`PC_Class`/`PC_Interface`/`PC_SoftObject`/`PC_SoftClass` pins store their
  default in `DefaultObject`, so e.g. `CreateSaveGameObject.SaveGameClass` silently stayed unset (→ creates null at runtime). Now
  detects object-like pins, `StaticLoadObject`s the value, and routes through `Schema->TrySetDefaultObject` (fail-fast OBJECT_NOT_FOUND
  if unresolved). `.cpp`-only (BlueprintGraphHandlers.cpp), live-coded + verified: `SaveGameClass` now reads back
  `defaultObjectPath=/Game/.../BP_ShinSaveGame_C` and the BP compiles clean.
- ✅ **RESOLVED 2026-06-21 (`707735d`): kept C++ UFUNCTION name as the canonical identifier; improved the not-found error instead of adding a DisplayName fallback.** Decided against a ScriptName/DisplayName-metadata fallback — DisplayNames are localizable and collision-prone, so a fuzzy fallback could silently resolve the wrong overload. `FUNCTION_NOT_FOUND` now states the expected identifier (the C++ name) and that `memberClass` disambiguates the owning class.
- ✅ **RESOLVED 2026-06-21 (`707735d`): `connect_pins` standardized to `sourceNodeId`/`sourcePinName`/`targetNodeId`/`targetPinName`** (handler + schema); Niagara `connect_pins` aligned the same way (`0014826`); material connect already read source/target and its 4 dead `from`/`to` schema params were removed. No aliases (Aaron's call) — old `from`/`to` names cleanly return NODE_NOT_FOUND, verified live. (BT `parentNodeId`/`childNodeId` + state `fromState`/`toState` left as-is — domain-correct, not pin connections.)
- 📝 **Incremental wiring trips the post-op compile-error guard.** Connecting exec before data leaves the graph transiently invalid (e.g.
  "Object is undetermined" until the cast's input lands), and the guard surfaces those mid-sequence compiles as ENGINE_ERROR even though each
  link is made and the final graph compiles clean. Benign (wire data pins before exec to avoid, or ignore until the final compile), but noisy.

### 2026-06-20f — Self-review notes on the session's 4 commits (32855c8, dfb4389, e6991b6, 953a6bd)
Adversarial pass over the log/profiling/test-stub/pin-default work. **No live bugs** — every commit is correct for its
verified path. The following are hardening notes I deliberately did NOT blind-fix while away (each needs a rebuild and one
carries real regression risk), logged so they're a conscious decision, not a miss:
- ✅ **Log ring buffer is sound.** All ring mutations + reads are under `LogRingMutex`; matched entries are deep-copied out
  under the lock (no dangling `FString`); seq/`dropped` math is underflow-guarded; `Deinitialize` `RemoveOutputDevice`s then
  `Reset`s the device (no dangling `GLog` pointer — mirrors `RequestErrorDevice`). Unlocked reads of `bLogCaptureEnabled`/
  `LogRingCapacity` are benign races on stable values. No action.
- 📝 **`set_pin_default_value` reports success it can't confirm.** `Schema->TrySetDefaultObject`/`TrySetDefaultValue` are
  **void** in UE — there's no return to check, so a validation no-op (e.g. wrong object type for the pin) would still respond
  "Pin default value set." Honest fix = read back `Pin->DefaultObject`/`Pin->GetDefaultAsString()` and compare. NOT done blind:
  pin-default string normalization (whitespace/`None`/numeric formatting) makes a naive readback prone to false-negative errors
  that would REGRESS the verified-working path. Needs a careful readback + a test, not a one-liner.
- 📝 **Class pins (`PC_Class`/`PC_SoftClass`) need the `_C`-suffixed generated-class path.** A bare asset path
  (`/Game/.../BP_Foo`) loads the `UBlueprint`, which class-pin validation silently rejects (void return hides it — see above).
  Verified SaveGame path used the right `..._C` path. Could auto-append `_C` for class pins when the bare load returns a
  `UBlueprint`. Low priority.
- 📝 **`generate_test_stub` trusts caller-supplied `className`/`testName`.** A supplied `className` isn't validated as a C++
  symbol (a space/digit-first → non-compiling file; fail-fast at compile, just unfriendly). `testName` is interpolated into a
  C++ string literal unescaped — a `"` or `\` in it breaks/injects the generated file (pathological, local-only caller). And
  the filename does `ClassName.RightChop(1)` assuming a leading `F`, so a non-`F` className loses its first char in the
  *filename* only (symbol + registration still correct). All cosmetic/pathological; pre-validate + escape if revisited.

### 2026-06-20a — ✅ FIXED: `control_editor set_camera` doesn't move the editor viewport (location+rotation ignored)
**Fixed + verified live 2026-06-20.** Root cause: handler pre-extracted the `location` object then called `ReadVectorField(*Loc, TEXT(""), ...)`, but that helper unwraps the *named* field itself — an empty name found no nested field and returned the default `(0,0,0)`, so the camera always went to origin/zero rotation. Now reads `location`/`rotation` off the parent payload and seeds from the current pose (partial updates preserve the other axis). Verified: `set_camera {500,100,800 / pitch -60 yaw 30}` then `get_viewport_info` round-trips the exact pose.

Hit framing a top-down screenshot of a ground telegraph. `control_editor set_camera {location:{x:400,y:200,z:700}, rotation:{pitch:-90}}`
returns `{success:true}` but the perspective viewport did not move (stayed at the prior low/side angle); repeated calls had no effect.
Workaround that DID work: `execute_python` → `get_editor_subsystem(UnrealEditorSubsystem).set_level_viewport_camera_info(loc, rot)`.
Fix: route set_camera/set_viewport_camera through `set_level_viewport_camera_info` on the active level viewport (and confirm the rotation convention).

### 2026-06-20b — ✅ FIXED: `get_viewport_info` returns only width/height (no camera pose)
**Fixed + verified live 2026-06-20.** Added `location`/`rotation` from `UUnrealEditorSubsystem::GetLevelViewportCameraInfo` (same source `set_camera` writes, so set/get round-trip). Verified above.

Returned `{width,height}` only — no camera location/rotation/FOV, so you can't read back where the viewport camera is (made the set_camera
bug above harder to diagnose). Fix: include the active viewport camera's location/rotation/FOV.

### 2026-06-20c — ✅ FIXED: Actor-name resolution inconsistent across actions
**Fixed + verified live 2026-06-20.** `focus_actor` matched only `GetActorLabel()`; `FindActorByName` (control_actor) matched only `GetName()`. Both now match either object name OR label. Verified: `focus_actor` on object name `BP_Telegraph_C_1` (previously ACTOR_NOT_FOUND) now focuses.

`control_editor focus_actor {actorName:"BP_Telegraph_C_0"}` → ACTOR_NOT_FOUND, but the same actor resolved as `"BP_Telegraph0"` (label),
and `control_actor set_actor_location`/`destroy_actor` accepted `"BP_Telegraph_C_0"`. focus_actor matches a different name field than
control_actor. Fix: unify actor lookup (label + object name + path) across control_editor and control_actor.

### 2026-06-20d — ✅ FIXED: `inspect_cdo detailed:true` on a component-heavy BP overflows the token cap (minor)
**Fixed + verified live 2026-06-20.** The schema advertised `componentNames` but the all-components loop ignored it. Now `componentNames` filters the components array (native + SCS), so callers can scope a large CDO. Verified: `inspect_cdo {componentNames:["Disc"]}` → `componentCount:1`. (Default behaviour unchanged when no filter is given.)

inspect_cdo on BP_TrainingDummy_AoE returned 146k chars and had to be dumped to file — mostly verbose per-component BodyInstance/collision
defaults at engine defaults. Fix: terser default (skip props equal to engine defaults) or honor `propertyNames`/`componentNames` to scope it.

### 2026-06-19e — ✅ RESOLVED: Read UInputMappingContext.Mappings via `get_input_info`
**Verified live 2026-06-20** (baked build). The action was already in source (`InputHandlers.cpp` ~L861,
routed via `manage_networking`/`manage_input`) — the original report's "ToolSearch finds none" was just the
*running* build predating the feature, not a missing implementation. `get_input_info` on an IMC returns
`mappingCount` + a `mappings[]` of `{action, actionName, key, triggerCount, modifierCount}` straight from
`UInputMappingContext::GetMappings()` (the canonical accessor — reads the live storage that the raw
`Mappings` UPROPERTY reflects empty under UE5.7). Verified: `IMC_CharacterContext` → 28 mappings
(IA_Heal→R / Gamepad_FaceButton_Top, IA_Block→RightMouseButton / Gamepad_LeftShoulder, etc.). No code
change — verify-only. Original report below.

Hit while diagnosing a "heal input does nothing" report. `inspect get_property {objectPath:<IMC>, propertyName:"Mappings"}`
returns `value:[]` for EVERY IMC (IMC_CharacterContext / _Inverted / GameCommands) even though the game has
working bindings — generic reflection can't serialize the `TArray<FEnhancedActionKeyMapping>` (same blind
spot as python `get_editor_property('mappings')`). No exposed `get_input_info` action in the running build
(ToolSearch finds none) though memory says one was pushed to dev (1377b81) — so it either didn't make this
build or isn't registered. Net: no way to read which key an IA is bound to. Fix: expose/verify a
`get_input_info` (IMC -> [{action, key, modifiers, triggers}]) readback; until then IMC bindings can only be
confirmed by opening the asset in-editor (or by simulate_input through PIE and observing the effect).

### 2026-06-19f — ✅ FIXED: GameMode / default-pawn readback added to `get_level_info`
**Fixed + verified live 2026-06-20** (baked build). Extended the loaded-level branch of `get_level_info`
(`LevelHandlers.cpp`, reached via `manage_level get_summary`/`get_level_info`) to add `gameModeOverride`
(World Settings' `AWorldSettings::DefaultGameMode`, "" when unset), `effectiveGameMode` (the override, else
the project `UGameMapsSettings::GetGlobalDefaultGameMode()`), and `defaultPawnClass` (off the effective game
mode's CDO). Additive — existing JSON shape unchanged. Verified: `get_summary` on HubWorld →
`gameModeOverride:""`, `effectiveGameMode:/Game/Blueprints/GameMode/BP_GameMode_C`,
`defaultPawnClass:/Game/Blueprints/Characters/MainRobo/BP_CPP_Character_C` — "what pawn does map X spawn"
with no python. Original report below.

Diagnosing "does L_CombatGym spawn BP_CPP_Character" needed the level's World Settings GameModeOverride and,
when null, the project GlobalDefaultGameMode + its DefaultPawnClass. `inspect get_world_settings` returns only
worldName/levelName (no gamemode), and there's no project-settings readback for the default gamemode. Resorted
to execute_python (load_asset -> get_world_settings().default_game_mode; GameMapsSettings.global_default_game_mode
-> CDO.default_pawn_class). Fix: add a get_level_info / get_world_info readback returning
{gameModeOverride, effectiveGameMode, defaultPawnClass} so "what pawn does map X use" doesn't need python.

### 2026-06-19c — Graph-authoring papercuts hit wiring the HUD pull-binding (BP_CPP_Character / WBP_HUD)
Found while converting the player-HUD binding to a self-contained pull model. None block work (all had
workarounds) but each cost round-trips:
- ✅ **FIXED 2026-06-20: `create_node` cast accepts `nodeType:"DynamicCast"`/`"K2Node_DynamicCast"`.** The
  cast branch condition (`BlueprintGraphHandlers.cpp` ~L1449) now matches those engine UClass names (what
  `get_graph_details` reports) in addition to `"Cast"`/`"CastTo<Class>"`, so it reads `targetClass`, resolves
  the class, and sets `TargetType` (no class → CLASS_NOT_FOUND fail-fast). Verified live: `create_node`
  `{nodeType:"DynamicCast", targetClass:"/Script/Engine.Pawn"}` → "Cast To Pawn" node with a typed `AsPawn`
  object pin (+ CastFailed/bSuccess), NOT the old wildcard "Bad cast node".
- ✅ **FIXED 2026-06-20: `add_event` materializes an engine override (Construct/Tick/PreConstruct).** Root
  cause confirmed live: a fresh Widget BP's EventGraph is pre-seeded with DISABLED ghost placeholders for
  those events, so the `bExists` check found the ghost and skipped — reporting success on a node that "will
  not be called". Fix (`BlueprintHandlers.cpp` add_event): when the override node already exists and is not
  Enabled, `SetEnabledState(Enabled)` + `NotifyGraphChanged()` (mirrors set_node_property; the existing
  structural-mark + compile + save tail persists it). Verified live: `add_event {eventName:"Construct"}` on a
  fresh WBP → "Event Construct" flips Disabled→**Enabled** (Pre Construct/Tick untouched). NB: the report's
  "set_node_property EnabledState left it disabled" was a stale-build artifact — it works in the current build
  (used it to diagnose). connect_pins-to-materialize still works too; this just makes add_event self-sufficient.
- 📝 **Usage note (UE behavior, not a bridge bug): a C++ `BlueprintImplementableEvent` can't be *called*
  from another Blueprint** — `create_node CallFunction` to `UCombatHudWidget::InitializeHud` from
  `BP_CPP_Character` compiled to "Function 'InitializeHud' ... should not be called from a Blueprint"
  (FUNC_BlueprintCallable not granted for the cross-BP call). Worked around with a pull model (HUD binds
  itself on Construct). If a push API is wanted later, the canonical fix is a C++ `BlueprintCallable`
  wrapper that calls a protected BIE, or expose a BP custom event (custom events ARE cross-BP callable).

### 2026-06-19b — ✅ `control_actor call_actor_function` couldn't reach components or pass args — FIXED
Found while trying to drive the game's magic bar in PIE (`RemoveMagic(40)` on the player's
`Attributes` component → `FUNCTION_NOT_FOUND`). Two gaps in `HandleControlActorCallFunction`
(`McpAutomationBridge_ControlHandlers.cpp` ~L2597):
1. **Actor-class-only resolution.** It called `Actor->FindFunction(FunctionName)` + `Actor->ProcessEvent`,
   and never read `componentName` on this path — so any UFUNCTION living on a *component*
   (`UAttributeComponent::RemoveMagic`, etc.) was invisible.
2. **Arguments ignored.** Even on a found function it `Malloc`+`Memzero`'d the param frame and called
   with all-zero params; the schema's `arguments` array was never parsed. So `RemoveMagic(40)` would
   have run as `RemoveMagic(0)` regardless.
**Fix (`.cpp`-only, live-coding patched):** resolve `Target` = component (via the existing fuzzy
`FindComponentByName`) when `componentName` is set, else the actor; `Target->FindFunction` +
`Target->ProcessEvent`. Build a real param frame (`InitializeValue_InContainer` per `CPF_Parm`, fill
input params in order from `arguments` via `ImportText_Direct`, skipping `CPF_ReturnParm` and pure
`CPF_OutParm`, then `DestroyValue_InContainer`) so non-POD args don't leak and values actually pass.
Response now echoes `componentName` + `argumentsApplied`. Verify: PIE → `call_actor_function`
`{actorName, componentName:"Attributes", functionName:"RemoveMagic", arguments:["40"]}` → player magic
60, bound MagicBar Percent 0.6. (Possible follow-up: read back the function's return value into the
response.)

### 2026-06-19 — ✅ FIXED: Failed save pops a blocking modal that starves the bridge
**RESOLVED 2026-06-19** (bridge `dev`, full rebuild `-NoUBA` + verified live). `McpSafeAssetSave` +
`McpSafeLevelSave` now bracket the save in `GIsRunningUnattendedScript = true` — **note the global is
`GIsRunningUnattendedScript`, NOT `GIsRunningUnattended`** (that symbol does not exist; first build
failed C2065 on it). `FMessageDialog::Open` then returns its default (Cancel) for the "failed to save"
dialog without showing UI, so a failed save returns `success=false` and the bridge stays responsive.
Verified with a read-only-`.uasset` test: modal auto-dismissed (~72ms/attempt, `Message dialog closed,
result: Ok` in the log), follow-up calls returned immediately. Commits 790e77a / 27705c0 / 4c76569 /
20a9f9e. The guard fixes the starvation for ANY save-failure cause, so the open sub-question below
(transient lock vs read-only) no longer gates it. Original report kept below.
Repro (live, hit while wiring the game's magic bar — item B(b) above): `manage_blueprint add_progress_bar`
on `/Game/UI/WBP_HUD` (a widget already loaded + live in the open HubWorld). The widget add itself
SUCCEEDED in-memory (`MagicBar` present in `WBP_HUD_C:WidgetTree` *and* in the running
`HubWorld:WBP_HUD_C_0.WidgetTree_0`), but the subsequent save failed and the editor surfaced an
**interactive modal** — "The asset '/Game/UI/WBP_HUD' failed to save. Cancel / Retry / Continue."
That modal runs a **nested Slate message loop on the game thread** (`FMessageDialog::Open` →
`FSlateApplication::AddModalWindow`): the loop pumps Slate (the buttons are clickable) but never returns
to the caller, so `FEngineLoop::Tick` and any `AsyncTask(GameThread)` the bridge queues are starved →
**every subsequent MCP call times out** until a human clicks a button. Confirmed: 3 reads in a row timed
out; the instant Aaron hit **Cancel**, the bridge was responsive again and the in-memory `MagicBar` was
intact.

Two distinct problems:
1. **Detection is impossible in-band.** The bridge services requests on the game thread, which is the
   thread blocked inside the modal loop — so it physically cannot reply "a modal is up." The MCP
   client-side timeout (write hangs after reads just worked) IS the only available signal.
2. **The bridge should never let an interactive modal fire at all.** A failed save inside an automated
   op must become a structured error, not a blocking prompt.

Fix direction (preventive + optional detection):
- **Preventive (real fix):** bracket bridge asset writes in unattended semantics — `TGuardValue<bool>
  UnattendedGuard(GIsRunningUnattended, true)` / `FScopedUnattendedScope` — so `FMessageDialog::Open`
  returns a default answer instead of showing UI. Better: don't route through the interactive
  `FEditorFileUtils::PromptForCheckoutAndSave` path; save via `UPackage::Save`/`SavePackage` and inspect
  the returned `FSavePackageResultStruct`, returning a `SAVE_FAILED` MCP error with the reason. This is
  the same `McpSafeAssetSave` path the 2026-06-18f compile-before-save change touches — fold the
  no-modal guarantee in there so it covers every handler at once.
- **Pre-flight:** before saving, check the target `.uasset` is writable (`IFileManager::IsReadOnly`) +
  source-control state; fail fast with a clear error if not.
- **Detection (only if wanted):** a non-game watchdog thread reading a game-thread heartbeat timestamp
  could report "game thread unresponsive (likely modal)" — can't read the dialog text (needs the blocked
  thread), so low value vs. the preventive fix.

Open sub-question (not the bug, but worth a pre-flight probe): WHY did this particular save fail? The
`.uasset` is writable on disk (`-rw-r--r--`) and git-clean, so it wasn't a filesystem read-only flag —
likely a transient lock / second open handle (asset live in the running PIE-less editor world + possibly
a second editor instance). Reproduce in isolation before assuming the modal is the only issue.

### 2026-06-19d — ✅ FIXED: Save-failure honesty papercuts (found verifying the modal fix, 2026-06-19)
**Fixed + verified live 2026-06-20** (baked build). `McpSafeAssetSave` now distinguishes "THIS call wrote the
file" from "the file merely pre-existed": it snapshots the package file's on-disk mtime *before* any save
attempt, and the fallback success gate is `bPromptSaveSucceeded || bEditorSaveSucceeded || (bExistsOnDisk &&
bFileRewritten)` where `bFileRewritten = PackageFileTimestamp() > PreSaveTimestamp` (a new-file appearance
also advances past `FDateTime::MinValue`). A failed read-only/locked overwrite leaves the old file with its
old stamp → no longer flips a genuine failure to success. Verified: read-only `.uasset` → `add_variable
{save:true}` → log shows `McpSafeAssetSave: failed to save package ...` (that warning only fires when
`bResult==false`) + `SaveLoadedAssetThrottled: failed to save` — both bools now honest, no modal (unattended
guard held). Both papercuts below fixed by the one change (the second is downstream of the first).
- [x] **`McpSafeAssetSave` false-positive `true` when the file EXISTS but the overwrite FAILED.** (mtime gate above.)
- [x] **`add_variable` reports `saved=true verified=true` on a failed save.** Downstream of `McpSafeAssetSave`; now honest.

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
- **D** — ✅ `FScopedTransaction` + `TryCreateConnection` ADDED to `bind_on_clicked`/`bind_on_value_changed`
  2026-06-20 (mirrors `bind_event_to_delegate`): both now group their graph mutations as one undo unit, and
  the raw `MakeLinkTo` calls (esp. value_changed's 4 data-pin links) are schema-checked so a type-mismatched
  pin can't be wired while still reporting `wiredLiveUpdate:true`. Verified live: slider→label live-update
  chain wired `wiredLiveUpdate:true` (4 links) via the new path. (Still open 🟢: extracting the SHARED
  graph-event-chain helper across the three binders — pure dedup, not a bug.)
- **B 🟡/🟢** — Inventory/Interaction structural adds save-without-compile (stale CDO) [structural-only
  class now covered centrally by `McpSafeAssetSave` compile-on-save, 2026-06-18f]; ✅ InputHandlers
  `add_mapping`/`remove_mapping` `Context->Modify()` ADDED 2026-06-20 (baked) — now matches
  set_input_modifier; MapKey/UnmapKey dirty internally but the explicit Modify aligns the inconsistency.
- ✅ `manage_audio` `create_sound_class`/`create_sound_mix` hang — RESOLVED 2026-06-21 (`0e022d6`): not a modal — a dispatch misroute + an inverted skeleton gate. See the detailed entry below.
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

### 2026-06-18f — Exhaustive compile-before-save audit (workflow, 11 handler files, adversarially verified)
The "structural blueprint change saved WITHOUT a compile (stale generated class/CDO)" anti-pattern is **pervasive**:
60 confirmed sites (Inventory 19, Interaction 17, AI 13, AnimationAuthoring 7, GAS 2, Misc 2). Triaged into two classes:
- **Genuine DATA LOSS (10) — FIXED `256d9ec`.** A CDO default written to a *freshly-added* property BEFORE any
  compile → `FindPropertyByName` misses it → the value silently no-ops to the type default. Fixed by compiling
  before the CDO write (AI re-fetches the CDO after compile, since compile regenerates it):
  Inventory `configure_pickup_respawn`/`configure_pickup_effects`/`configure_equipment_effects`/
  `configure_equipment_visuals`/`configure_loot_drop`/`configure_inventory_weight`/`configure_inventory_slots`
  (fallback now writes slotCount, not 0 — verified live: MaxSlots=15); AI `assign_behavior_tree`/`assign_blackboard`;
  Interaction `configure_door_properties`.
- [x] **Structural-only (~50) — FIXED `cf4638e` via the one-place change (Aaron chose (b)).** `McpSafeAssetSave`
  now compiles a `BS_Dirty` UBlueprint before persisting, so any structural change saved without an explicit compile
  gets a fresh generated class on disk + closes the in-session staleness window + auto-covers future handlers. Gated
  to `BS_Dirty` (already-compiled / value-only saves untouched). Inlined `FKismetEditorUtilities::CompileBlueprint`
  (can't call `McpSafeCompileBlueprint` — it's in McpAutomationBridgeHelpers.h which includes McpSafeOperations.h →
  circular). **Verified the key risk** (save-triggered recompile resetting directly-written CDO values does NOT
  happen — reinstancing preserves them): set_default value survived 2 recompiles, configure_inventory_slots→15,
  widget add persisted, normal authoring unaffected.

### 2026-06-18 — Overnight autonomous pass: game architecture deep-dive + bridge robustness + simplification
Aaron green-lit an overnight budget-burn ("Both": game audit + bridge hardening) under a read-only-game / verified-low-risk-bridge contract.

**Game C++ (read-only, REPORT only — no game code changed):** 42-agent deep-dive of `Source/RhyaTowerOfWishes`,
adversarially verified. 104 findings (8 bug / 34 robustness / 17 simplify / 34 best-practice / 11 design); report
written to `Saved/game_architecture_review.md` (gitignored) with a curated top-priorities section. Headline real
issues for Aaron: `AC_HitStop` resolves owner in ctor (null) + `materialInstance` no `UPROPERTY` (GC dangling);
`AttributeComponent` percent getters div-by-zero → NaN to HUD; `Weapon::OnBoxOverlap` null-derefs for a non-player
wielder; `GetCurrentAttack` combo off-by-one; Enemy `DirectionalHitReact` always "FromRight". Caveats flagged
(weapon `damage` likely BP-set; `RotationRate 50000` maybe intended).

**Bridge robustness sweep (4 fresh classes, 16-agent workflow, adversarially verified): 11 confirmed. FIXED + built + committed:**
- [x] **null-deref: 9 widget layout/styling subactions** deref `WidgetBP->WidgetTree` after only `if (!WidgetBP)`.
  Hardened ALL bare `if (!WidgetBP)` guards → `if (!WidgetBP || !WidgetBP->WidgetTree)` (covers the 9 + makes the file
  consistent with the 43 already-guarded sites; fail-safe — a null-WidgetTree WBP is malformed).
- [x] **null-deref: `get_inventory_info`** `Blueprint->GeneratedClass->GetName()` + **`create_node` CallFunction**
  `Blueprint->GeneratedClass->FindFunctionByName` — guarded GeneratedClass (matching each file's other sites).
- [x] **modal-hang (bridge-bricking): `create_animation_bp`, generic `CREATE_ASSET` (EditorFunction), `create_material_instance`**
  — added `DoesAssetExist(SavePath/Name)` pre-check before `IAssetTools::CreateAsset` (which pops an "overwrite?" modal
  headlessly → frozen game thread → force-kill). Same class as the known audio hang.
- [x] **✅ DONE 2026-06-20: modal-hang guards on the 5 dormant anim/save handlers.** All baked + verified-compile;
  blend-space guard verified fail-fast end-to-end. (a) `CreateBlendSpaceAsset` helper (`AnimationHandlers.cpp` ~L290):
  one `DoesAssetExist(PackagePath/Name.Name)` → return null + "Asset already exists" — covers **all 3 callers**
  (`create_blend_space` / `_1d` / `_2d`). (b) `create_aim_offset` + (c) `create_pose_library`: inserted an `else if
  (DoesAssetExist(...))` → `ALREADY_EXISTS` ahead of the create. (d) `HandleCreateAnimBlueprint` (`create_animation_blueprint`):
  `DoesAssetExist` guard before the factory, `SendAutomationError ALREADY_EXISTS`. (e) `SAVE_CURRENT_LEVEL`
  (`EditorFunctionHandlers.cpp` ~L644): bracketed `SaveCurrentLevel()` in `GIsRunningUnattendedScript` so an
  untitled/read-only prompt returns its default (no modal) — same guard McpSafeAssetSave uses. **Verified:** second
  `create_blend_space` (2D caller, no pre-check of its own) → `ASSET_CREATION_FAILED: Asset already exists: ...`
  instantly, **no modal/hang** — proving the helper guard fires and the bridge stays responsive.

**Function-split (the 🟡 widget-monolith) — ✅ DONE 2026-06-20 (056b292, 978ebd5, 1e8b486).**
`HandleManageWidgetAuthoringAction` was one 6,278-line `if (SubAction.Equals(...))` chain (86 blocks / 92 subactions,
order-independent — unique names). Split by SUBACTION FAMILY into 10 private member sub-handlers
(`HandleWidgetAuthoring_Lifecycle/Containers/Leaves/Slot/Binding/Animation/Style/Tree/Recipes/Misc`); the dispatcher is
now 38 lines (prologue → 10 family calls, first-true-wins → terminal). Member fns (so `SendAutomation*` stay
unqualified; 10 decls in the subsystem header → one full rebuild); each family fn gets a local `ResultJson` preamble
only if its blocks use it. The "do by hand, exact-match edits are error-prone" worry was sidestepped by a byte-exact
RELOCATION SCRIPT (`scripts/move-widget-family.ps1`) that moves whole `if{}` blocks by indentation markers and
self-aborts unless the code-line multiset is preserved; `scripts/clean-widget-dispatcher.ps1` then stripped the unused
dispatcher-scope `ResultJson` + 196 lines of orphaned section-divider comments. Verified: code-line multiset identical
before/after the whole reorg (only the 8 needed preambles added, ZERO code lines changed) + clean -NoUBA rebuild
(warnings-as-errors) + live smoke across 7 families. Pattern is reusable for any other `if`-chain monolith.
- ✅ **FIXED 2026-06-20: 19 widget subactions were HANDLED but NOT in the `WidgetAuthoring()` routing list**
  (`McpConsolidatedActionRouting.h`), so they returned "Unknown blueprint action" before reaching the handler:
  `add_quest_tracker, add_safe_zone, add_spacer, add_widget_component, add_widget_switcher, apply_style_to_widget,
  bind_localized_text, create_credits_screen, create_shop_ui, create_widget_style, delete_animation, get_animation_info,
  set_animation_speed, set_font, set_localization_key, set_margin, set_widget_binding, show_widget, create_widget`.
  Pre-existing (routing list lagged the handler as it grew). Added all 19 to `WidgetAuthoring()`; rebuild + smoked each:
  19/19 now reach their handler (success or a handler-specific validation error, never "Unknown blueprint action").

**Simplification sweep (3 classes, adversarially verified — each removal independently confirmed zero callers across
the module + routing tables, reflection/string-dispatch ruled out). DONE + built + committed (943912c): ~1,880 lines deleted.**
- [x] Deleted `McpHandlerDeclarations.h` (843 lines, never #included — orphaned IMcpHandler/MCP_DECLARE_HANDLER API
  never adopted; real dispatch is the subsystem's RegisterHandler/FAutomationHandler). Fixed AGENTS.md pointer.
- [x] `McpHandlerUtils.h/.cpp`: 12 unused inline helpers, 6 never-invoked dispatch/error macros (+ now-dead
  ActionMatches/NormalizeAction), MakeUniqueAssetName/ToSafeAssetName, 4 unused McpBlueprintUtils Find* fns.
- [x] `McpPropertyReflection.h/.cpp`: 7 unused fns + dead color converters (incl. a LinearColor pair duplicated in both headers).
- [x] `BlueprintHandlers.cpp`: transitively-dead static cluster (BuildVariableJson/AnnotateVariableJson/CollectVariableMetadata).
- [x] `McpToolRegistry.h/.cpp`: 3 uncalled methods (GetAllTools/GetToolCategories/InvalidateCache).
- [x] Stripped "thinking out loud" narration + a rejected commented-out impl + an empty else (AnimationHandlers, AssetWorkflowHandlers).
- [x] **✅ DONE 2026-06-20: deleted the dead `add_notify_old_unused` subaction branch** (AnimationHandlers ~L2634, ~126
  lines). Confirmed unrouted (grep across the plugin: only the branch itself + this TODO referenced the literal; the
  `_old_unused` suffix is never a client-sent action) — a renamed-aside of the live `add_notify`. Removed; module
  compiles clean (baked).

**Refactor backlog (from the simplify sweep — boilerplate dedup; NOT auto-applied: these transform LIVE code at
hundreds of sites, a different risk class than dead-code deletion. Do deliberately / greenlight individually):**
- [x] **DONE 0c7ccb3 (-536 lines, routing smoke-tested). (high, ~575 lines) `InitializeHandlers()` RegisterHandler boilerplate** — ~115 trivial registrations are each a
  5-line forwarding lambda `[this](R,A,P,S){ return HandleXxx(R,A,P,S); }`. Add `#define MCP_REGISTER_HANDLER(name, fn)`
  and collapse each to one line. Leave the genuine multi-branch routing lambdas alone (judgement per-site → why not auto-applied).
- [x] **DONE bd1568f (89 sites, -790 lines; helper named ResolveBlueprintOrError; 27 GeneratedClass/sanitized-path/etc. sites correctly skipped). (med) load-blueprint prologue** repeated ~90× — `ResolveBlueprintParamOrError(Payload, RequestId, Socket, field="blueprintPath")`
  returning the BP or sending the error + nullptr (handler: `if (!BP) return true;`). Collapses ~10 lines → 2 per site.
- [x] **(med) compile/save tail → `McpFinalizeBlueprint`. ✅ DONE 2026-06-20 (Aaron greenlit the full sweep).**
  Added `McpFinalizeBlueprint(BP, bStructural, bSave=true)` to `McpAutomationBridgeHelpers.h` (mark → compile → save,
  throttled save) and consolidated **135 finalize tails across 12 handler files** into it. Verified the compile-before-save
  is SAFE even at the compile-then-write-CDO sites (live test: `configure_inventory_slots` → CDO `MaxSlots` persists
  through the added compile, because UE's reinstancer copies old-CDO→new-CDO); fixes the stale-generated-class-on-disk
  case for pure structural adds. Applied via four strict, backref-var-matched regex passes (no handler logic ever touched —
  any real code line between mark and save breaks the match), each git-diff-reviewed + Live-Coding-compiled, plus a final
  `-NoUBA` rebuild (warnings-as-errors, clean) + runtime smoke (add_variable → var persists). Commits 3fbec2a (Inventory 19),
  4304f57 (92 adjacent), a01758c (CommonUI 5 + 14 comment/blank-separated), 6fcbf30 (5 redundant-MarkPackageDirty + 2 orphan-
  comment trims). Save mode kept **throttled** (codebase's preferred wrapper; dirty-flag backstops the skip-as-success).
  Scripts: `apply-finalize{,2,3,4}.ps1`.
  **Deliberately LEFT (not mechanically collapsible — collapsing would lose info or reorder logic):**
  - ~13 save-tails with a separate `bCompiled = McpSafeCompileBlueprint(...)` capture used in the response (the helper
    returns only the save bool), a runtime-conditional mark (`if/else` choosing structural vs not, e.g. BlueprintGraph
    ~L1873), or real code between mark and save. These already mark+compile+save correctly — just not via the helper.
  - ~70 **mark-only** sites (mark dirty, no save — Character/AnimAuthoring/GAS/WidgetAuth) that defer persistence to the
    save-on-close prompt. These aren't finalize-with-save tails; "should they save eagerly?" is a SEPARATE behaviour
    question, not part of this DRY consolidation.
- [ ] **(med, SKIPPABLE) success tail** (`AddVerification` + `SendAutomationResponse(true)`, ~471 sites) →
  `SendSuccessWithVerification(...)`. Pure cosmetic dedup, largest churn, no correctness component — left unless explicitly wanted.
- [x] **DONE d57ffb5 (28 macros removed, 1,184 call sites renamed). per-file JSON-getter alias macros** — dropped
  the per-file `GetStringFieldGAS`/`…Combat`/`…AI`… aliases (all pure 1:1 renames) and call the shared
  `GetJsonStringField`/`Number`/`Bool`/`IntField` directly. (Aaron: the `#define` indirection obscured that they were all the same fn.)
- [ ] **DEFERRED (Aaron 2026-06-19: "add to TODO, see if it comes up later"). error-code string literals** —
  `TEXT("INVALID_ARGUMENT")` etc. at 1000+ sites across THREE mechanisms (SendAutomationError arg, manual `errorCode`
  field, BuildErrorResponse `code` field). FINDING that downgraded it: nothing matches on the codes — no TS/JS/Py
  client exists (the only consumer is the LLM reading responses), so there's no typo-safety-with-teeth, only
  consistency/discoverability; the "do we match on strings / have a taxonomy?" gate came back NO on both. If revisited,
  prefer the LIGHT version: define `namespace McpErrorCode { inline constexpr … }` as the canonical discoverable set +
  adopt opportunistically — NOT a 1,000-site/3-mechanism sweep (and don't leave it half-and-half).
- [x] **DONE d57ffb5. two-headers path helpers** — `McpHandlerUtils::ValidateAssetPath` was a near-duplicate of
  Helpers.h `SanitizeProjectRelativePath` (same reject-abs/reject-".."/require-root logic + identical warnings);
  deleted it + migrated its 3 callers to the canonical one. (Aaron: divergent duplicates are a real bug class — the
  LLM updates one copy and misses the other.) Note: `IsValidAssetPath` is complementary (validation, not sanitization), left as-is.

### 2026-06-18b — Friction found building the HUD `bind_event_to_delegate` (live MCP authoring)
- [x] **`MakePinType` PC_Float → INT — FIXED (Live-Coding-only; needs a full rebuild to persist).**
  `McpBlueprintUtils::MakePinType` (McpHandlerUtils.cpp ~682) used the legacy bare `PC_Float` category for
  "float"/"double", so user-defined pins (add_function/add_event params) compiled to an **INT** — silent
  truncation; the schema then spliced a `Truncate` node into float→float connections. Caught when a
  `SetPercent(float)` param became int and truncated 0..1 health to 0. Fixed → `PC_Real` + `PC_Float`/`PC_Double`
  subcategory. **Same bug family as the add_variable PC_Real fix (`c0bff6a`) — that fix never covered MakePinType.**
- [x] **`add_node` CallFunction (`memberName`+`memberClass`) → blank "None" node — HIGH (compile-break + PIE hang).** ✅ FIXED + VERIFIED 2026-06-19: `add_node` now delegates a `memberName` function call to the `create_node` CallFunction factory (BlueprintHandlers.cpp ~L4944). Live test: `add_node CallFunction memberName=GetPlayerPawn memberClass=GameplayStatics` → real "Get Player Pawn" node (proper pins), `compiled:true` — no None node. (Original report below.)
  Authoring a general `Class::Function` call via `add_node` did NOT resolve the function — it created a pinless
  `K2Node_CallFunction` titled "None". That node breaks compile (`Could not find a function named "None"`) and,
  when the broken widget is later instantiated at runtime, **HUNG PIE** (game thread frozen, log frame counter
  stuck — required a force-kill). **WORKAROUND CONFIRMED: use `create_node` (NOT `add_node`)** — `create_node`
  CallFunction DOES resolve a general call (`memberName`+`memberClass` → `ResolveUClass`→`FindFunctionByName`→
  `SetFromFunction`, BlueprintGraphHandlers.cpp ~L1045; verified: produced a real "Set Percent" node, then wired
  the body via `connect_pins`). The bug is `add_node`-specific: it routes elsewhere and `NewObject`s a bare
  function-less node. Fix: make `add_node` route function-call requests through the `create_node` CallFunction path
  (or fail-fast with FUNCTION_NOT_FOUND) instead of leaving a compile-breaking "None" node.
- [x] **`remove_function` false success — HIGH.** ✅ FIXED + VERIFIED 2026-06-19: now `RemoveGraph` → `McpSafeCompileBlueprint` → save (BlueprintHandlers.cpp ~L3700); live test: add `TestFunc` → `remove_function` (`removed:true,saved:true`) → `list_functions` confirms it's actually gone. Original bug: `remove_function SetPercent` returned `removed:true,success:true`
  but left the function (with its broken node) in place; only a later `get_graph_details`/compile revealed it.
  False success left a compile-broken asset. Workaround: `delete_node` the offending node(s), then `compile`.
- [ ] **`add_event` override path keyed on `eventType`, not `eventName`.** To implement a parent
  BlueprintImplementableEvent override, pass `eventType:"<EventName>"` (walks ParentClass→FindFunctionByName).
  Passing `eventName` leaves `eventType` empty → defaults "custom" → creates a GUID-named blank custom event
  (silent wrong result). Accept `eventName` as an alias on the override path, or document.
- [x] **✅ DONE 2026-06-20: declared the widget-authoring navigation params in the `manage_blueprint` schema**
  (`McpTool_ManageBlueprint.cpp`): `widgetPath`/`widgetName`/`slotName`/`parentSlot`/`widgetClass`. Handlers
  already read them from the payload; this makes them discoverable/passable through a schema-validating client
  (per-widget value props still go via the generic `value`/`properties` fields). Baked; takes effect next MCP
  session (a running session keeps the connect-time schema cached). NB this gap is more than cosmetic — the
  typed Claude client DROPS undeclared params, so e.g. Input's `name`/`path` (same class of gap, also fixed
  this pass for `manage_networking`) were un-passable through the typed tool until declared.
- [x] **`set_position` rejects bare `x`/`y`** — ✅ FIXED 2026-06-19 (commit 4c76569, built clean): now also accepts bare top-level `x`/`y`. Was: wanted `position:{x,y}` or `posX`/`posY` only. (`set_size` took
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

**✅ RESOLVED — entire batch landed (commits 2d21b13 / 0bd0017 / cf4638e / 256d9ec), spot-verified
2026-06-19. Section A lying-success stubs now return honest NOT_IMPLEMENTED or do real work
(bind_text/visibility/color/enabled, create_property_binding, apply_style_to_widget, set_animation_*,
GAS add_ability/grant_ability, add_eqs_context, inventory add_*_functions; bind_on_hovered reimplemented
on the real ComponentBoundEvent path). Section B save/compile discipline fixed centrally in
McpSafeAssetSave (compile-on-save for dirty BPs) + 10 CDO-value-loss fixes. Section C widget add_* dedup
landed (ConstructWidgetForAdd). The per-item 🔴/🟡/🟢 markers below are STALE — kept for history.**

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
- ✅ DONE 2026-06-20 (056b292/978ebd5/1e8b486): `HandleManageWidgetAuthoringAction` split into 10 per-family member
  sub-handlers (dispatcher now 38 lines). See the "Function-split" entry above. (Recipes is still ~2.5k lines — could
  sub-split menus/HUD/game-UI later if it bites; the `#pragma warning(disable:4883)` likely now belongs on that fn,
  not the dispatcher.)
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
- ✅ **DONE 2026-06-20: `create_node` dynamic fallback now Modify()s + compiles before save.** Added
  `TargetGraph->Modify()` (records the add for undo, like the FGraphNodeCreator branches), upgraded
  `MarkBlueprintAsModified`→`MarkBlueprintAsStructurallyModified`, and a `CompileBlueprint` before
  `SaveLoadedAssetThrottled` so the saved generated class/CDO reflects the new node (was saved stale).
  Left the raw NewObject→AddNode→AllocateDefaultPins (a generic UEdGraphNode can't go through the typed
  `FGraphNodeCreator<T>`). Verified live: fallback `nodeType:"K2Node_IfThenElse"` → real "Branch" node
  (execute/Condition/then/else pins), BP compiles clean.



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

### [x] `manage_audio` create_sound_class / create_sound_mix HANG the bridge (300s) — RESOLVED 2026-06-21 (`0e022d6`)
**RESOLVED — the "Slate modal" theory below was WRONG.** A minidump + cdb (`~* kn`) showed the GameThread idle with NO handler
on any stack — there was no modal. Two real root causes: (1) the `manage_audio` registered lambda passed the TOOL NAME
(`"manage_audio"`) to `HandleAudioAction`, whose gate matches the sub-action prefix (`create_sound_`), so the request never routed
and parked; (2) `HandleManageSkeleton`'s gate was inverted (`return true` for non-skeleton actions), so it swallowed the unrouted
request with no response until the 300s watchdog. The legacy dispatch if-chain that let an inverted gate matter at all was then
deleted (`e0c1bcb`). create_sound_class/mix now return in ~0.3s; diagnostics added so a future hang reports the active modal +
slate-active state instead of guessing. Original (incorrect) modal analysis kept below for the record.

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
