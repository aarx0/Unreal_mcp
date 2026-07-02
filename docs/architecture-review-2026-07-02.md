# McpAutomationBridge — Architecture Review

**Scope:** structure and design of the native MCP bridge plugin (`plugins/McpAutomationBridge/Source/McpAutomationBridge`). Line-level bugs were audited separately (2026-06-21); this report covers layering, coupling, contracts, failure modes, and extensibility. All claims verified against code at the cited `file:line`. Audience: the maintainer, extending this weekly, solo, with no automated test net.

---

## 1. How it works

### Request lifecycle, end to end

```
AI client (single local MCP client, HTTP)
   │  POST /mcp  (JSON-RPC 2.0, one request → one plain JSON response; no SSE, GET→405)
   ▼
FMcpNativeTransport (Private/MCP/McpNativeTransport.{h,cpp})
   • FRunnable accept thread blocks on FSocket::Accept (Run, cpp:311-429)
   • each connection → engine ThreadPool worker (Async dispatch, cpp:405)
   • HandleConnection: bounded read (5s deadline, 8KB header / 5MB body caps, cpp:653-865)
   • initialize / tools/list / DELETE session / notifications / manage_tools:
     answered synchronously on the pool thread
   • tools/call ONLY: socket parked in PendingConnections under a fresh GUID
     RequestId (cpp:1235-1247), request enqueued cross-thread ↓
   ▼
UMcpAutomationBridgeSubsystem (UEditorSubsystem — the plugin's one central object)
   • QueueAutomationRequest → mutex-guarded TArray (Subsystem.cpp:672-692)
   • FTSTicker Tick (registered 0.0f = every frame, after world tick, :443-447)
     drains the queue serially, gated on !GIsSavingPackage && !IsGarbageCollecting()
     && !IsAsyncLoading() (:656-670)
   • ProcessAutomationRequest (McpAutomationBridge_ProcessRequest.cpp):
     reentrancy guard (bProcessingAutomationRequest, :62-69), per-request GLog
     error capture (:134-145), single O(1) TMap<FString,FAutomationHandler>
     lookup — miss or handler-returns-false → UNKNOWN_ACTION (:151-171),
     exceptions → INTERNAL_ERROR (:172-192)
   ▼
Handler layer (~60 McpAutomationBridge_*Handlers.cpp TUs, ~138K lines)
   • per-domain member functions of the SAME subsystem UCLASS
   • second-level dispatch = linear if-chain on sub-action strings
   • consolidated tools (manage_blueprint, manage_asset, system_control, …)
     are longhand lambdas in InitializeHandlers (Subsystem.cpp:925-1347) that
     classify sub-actions via McpConsolidatedActions:: membership lists and
     re-dispatch to domain handlers under legacy action names
   ▼
Response
   • SendAutomationResponse (Subsystem.cpp:718-814): coerces handler-reported
     success into ENGINE_ERROR when the per-request GLog capture recorded engine
     errors (:728-782); sanitizes/redacts paths + secrets (:219-362, :785-788)
   • Origin resolution → NativeTransport->CompletePendingRequest keyed by
     RequestId (transport cpp:1318-1394); JSON-RPC body built on game thread;
     blocking socket write + close offloaded back to the ThreadPool
   • Wire format: FMcpJsonRpc::BuildToolResult flattens everything into ONE
     content[0].text string — "Message\n\n<condensed JSON>" on success,
     "Error [CODE]: message" on failure (McpJsonRpc.cpp:99-137)
```

### The layers

| Layer | Files | Role |
|---|---|---|
| Transport / protocol | `Private/MCP/McpNativeTransport.*`, `McpJsonRpc.*`, `McpToolRegistry.*`, `McpSchemaBuilder.*`, `McpDynamicToolManager.*` | Hand-rolled HTTP/1.1 + JSON-RPC server, pull-only profile of MCP Streamable HTTP (2025-03-26). Sessions via `Mcp-Session-Id` GUIDs (3600s expiry); loopback bind by default, optional capability token. |
| Tool facades | `Private/MCP/Tools/McpTool_*.cpp` (exactly 22) | Metadata-only `FMcpToolDefinition` subclasses, self-registered at static init via `MCP_REGISTER_TOOL` into a 22-name-allowlisted registry. Schemas built once and cached. **No execution logic.** This layer is cleanly decoupled — facades include only the 4 MCP headers + a thin version shim, never the subsystem header. |
| Routing vocabulary | `Private/MCP/McpConsolidatedActionRouting.h` | Shared static action lists + `Is*Action` predicates. For 8 of 22 tools, the same list feeds both the schema enum and the routing test. |
| Subsystem core | `Public/McpAutomationBridgeSubsystem.h` (1,519 lines) + `Subsystem.cpp`, `ProcessRequest.cpp` | Queue, ticker, dispatch registry, error capture, response sanitization, log ring, handler registration. |
| Handlers | ~60 `McpAutomationBridge_*Handlers.cpp` TUs | All member functions of the one subsystem UCLASS. Domain logic against raw editor APIs. |
| Helpers | `McpAutomationBridgeHelpers.h` (3,020 lines, header-only, global namespace), `McpSafeOperations.h` (2,679 lines, header-only), `McpHandlerUtils.{h,cpp}`, `McpPropertyReflection.{h,cpp}`, `McpGraphLayoutUtils.{h,cpp}` | Path validation, JSON getters, property reflection (×2, "twins"), safe-save wrappers, response senders. |

### Key mechanics to know before extending

- **Threading contract:** off-thread code may only enqueue (or append to the log/error rings under their mutexes); all UObject access happens on the game thread, one request at a time. Handlers never need their own locking — *unless* they defer completion via `AsyncTask(GameThread)`, which ~10 sites still do (see F4).
- **Two dispatch tiers you must keep in sync by hand:** the schema enum (facade), the routing membership list (`McpConsolidatedActionRouting.h` or an inline TSet in the registration lambda), and the handler if-chain. Nothing enforces agreement (see F1).
- **The always-null socket:** every handler signature carries `TSharedPtr<FMcpBridgeWebSocket>` — a dead transport type, always nullptr. Real response routing is `RequestId → PendingConnections` + `CurrentRequestOrigin`; the socket parameter does nothing (see F5).
- **Pull-only is a documented decision:** `docs/transport-mid-call-drop-problem.md` root-causes the mid-call-drop failure (game-thread stall > client's ~60s budget; the server's own 300s sweep runs on the same stalled thread), and `docs/pull-architecture.md` commits to the fix: no streaming, recovery = idempotent retry. The protocol surface matches (GET→405, `listChanged:false`, `SendProgressUpdate` no-op). Two of the plan's six deletion buckets were never executed (see F5).

---

## 2. What's sound — preserve these

1. **Single serialized game-thread pipeline.** Mutex-guarded enqueue → per-frame drain gated on save/GC/async-load → one request in flight behind a reentrancy guard (Subsystem.cpp:656-692, ProcessRequest.cpp:35-71). Socket I/O never touches UObjects. This is the plugin's correctness spine; every refactor below must keep handlers synchronous functions called from `ProcessAutomationRequest`. Codify the invariant in a header comment on `QueueAutomationRequest` (h:266-269 currently has none).

2. **ENGINE_ERROR coercion + protocol/tool error separation.** Per-request GLog capture converts false handler success into ENGINE_ERROR with sanitized `engineErrors[]` attached (Subsystem.cpp:728-782); tool failures ship as `isError` over HTTP 200 while protocol errors get JSON-RPC error objects with spec-correct statuses (McpJsonRpc.cpp:85-137; transport 565/576, 1489-1500). The capture device has a lock-free atomic fast path for the non-error log stream (Subsystem.cpp:84-94). Structural, not conventional — the strongest fail-loud mechanism in the codebase. Caveat: only covers synchronous completions (F4).

3. **Save-path discipline is enforced, not just documented.** Zero raw `UPackage::Save` / `SaveLoadedAsset` call sites across 60+ handler files; everything routes through `McpSafeAssetSave` (240 refs/36 files) / `McpDirectPackageSave` / `McpFinalizeBlueprint` (137 refs/13 files). `McpSafeAssetSave` itself is rigorously honest: forces `GIsRunningUnattendedScript` around modal-prone saves and proves writes by file-timestamp advance (McpSafeOperations.h:267-271, 306-313). `docs/safe-asset-saving.md` matches the code line-for-line. The dishonesty lives in wrappers *above* it (F3).

4. **Shared `McpConsolidatedActions` lists** — the one place schema and routing share a single source (8 of 22 tools, e.g. McpTool_ManageBlueprint.cpp:25 and Subsystem.cpp:1147-1150 consume the same array). This is the migration target, not something to replace (F1).

5. **Transport-edge hygiene.** Non-busy `Socket->Wait` reads with hard input caps; response write + teardown offloaded to the pool with a drain counter and per-connection write mutex (transport cpp:1343-1391); handshaked bind (Start blocks on BindCompleteEvent); shutdown drains in-flight writes while pumping the game-thread task graph to dodge a documented AsyncTask deadlock, then answers parked sockets with well-formed 503s (cpp:133-273). Timed-out parked requests log the active Slate modal title so modal starvation is attributable (cpp:26-40, 1420-1431 — note: only fires *after* a blocking modal dismisses, since the sweep runs from the starved ticker).

6. **Optional-plugin gating in the gameplay handlers.** Three tiers — compile-time `__has_include`, per-action `#elif` fallbacks, runtime `FModuleManager` checks — each with distinct machine-readable codes (`GAS_NOT_AVAILABLE` vs `GAS_PLUGIN_NOT_ENABLED`; `STATETREE_HEADERS_UNAVAILABLE` vs `UNSUPPORTED_VERSION`) (GASHandlers.cpp:100-117, 387-408; AIHandlers.cpp:2176-2185; Build.cs:610-629).

7. **Proven escape patterns already in-tree.** `FSCSHandlers` / `FBlueprintCreationHandlers` static classes with small private headers; GeometryHandlers' 87 uniform static free functions behind one router member; the Widget file's helper namespace + family sub-handlers that collapsed ~28 copy-paste handlers; `McpGraphLayoutUtils` as a pure Core-only, headless-testable layout core. Each is the template for the corresponding refactor below.

8. **`docs/extending-the-bridge.md`** is genuinely accurate and operational (three-layer flow, helper cheat-sheet, build workflow all verify) — the most load-bearing doc in the repo. It needs maintenance (F9) but is the right place to codify every convention this review recommends.

9. **The two black-box regression suites** (`tests/material/material-authoring-test.ps1`, 18 assertions; `tests/ui-nav/` declarative specs) are real tests — non-zero exit codes, self-cleaning fixtures, negative-path assertions, flakiness root-caused in the bridge rather than retried. Thin coverage, but the pattern is right.

---

## 3. Findings

Grouped by theme; ordered by real-world impact on a solo maintainer extending weekly. Each finding dedups multiple lens reports; strongest evidence cited.

---

### F1 — Action vocabulary is smeared across 3–4 hand-synced copies with zero enforcement; drift has recurred ≥4 documented times — **HIGH**

This is the failure class that bites *every single extension*. The contract for "what actions exist" lives in: (a) the tool schema enum — shared `McpConsolidatedActions::` lists for exactly 8 of 22 facades, inline literals for the other 14 (e.g. inspect's 38 actions, control_actor's 43); (b) routing membership tests in the registration lambdas — *plus* parallel inline sets like the hardcoded `GraphSubActions` TSet (Subsystem.cpp:1154-1167), a fourth copy of the ManageBlueprintCore tail; (c) the handler if-chains, which are what actually executes; (d) a hand-maintained 1,130-line `docs/handler-mapping.md`.

The code documents its own drift history, in both directions:
- Widget actions with handlers that were **never routed** — twice (McpConsolidatedActionRouting.h:199-211).
- Wave 6B: AnimBP graph actions missing from `GraphSubActions` → client-visible UNKNOWN_ACTION (Subsystem.cpp:1161-1165).
- 2026-06-11 "truth in advertising" sweep: **7 schema-advertised actions with no handler anywhere** removed (Routing.h:417-423); `set_cvar` "was advertised … but had no handler anywhere" (Subsystem.cpp:1036-1053).
- manage_gas today: schema declares 28 actions, handler dispatches 32 — `create_ability_set`/`create_execution_calculation` are real implementations invisible to schema-validating clients (McpTool_ManageGAS.cpp:24-53 vs GASHandlers.cpp:2823, 3128). The file's own header comment says 27 — three disagreeing counts in one tool.

Compounding it, **overlapping lists are resolved by lambda branch order**: 8 action names (create_material, add_material_node, remove_material_node, …) appear in both `ManageAssetCore` and `MaterialAuthoring`; because the manage_asset lambda checks `IsMaterialAuthoringAction` first (Subsystem.cpp:1107-1114), the asset-handler implementations of those names (AssetWorkflowHandlers.cpp:261-353) are permanently unreachable. Which handler serves an action is emergent, invisible in the schema.

**Recommendation (incremental):**
1. Add a **startup self-check** in `InitializeHandlers`/transport Start (~50-100 lines): walk each registered tool's schema action enum, assert every name is claimed by exactly one routing predicate/handler family, `UE_LOG(Error)` + `ensureMsgf` on mismatch. This converts the recurring runtime failure class into an editor-boot log line and would have caught all documented incidents.
2. Migrate the 14 inline facade enums into `McpConsolidatedActionRouting.h`, one tool per change (mechanical; manage_audio is already half-done — routing uses the shared list, schema duplicates it inline, McpTool_ManageAudio.cpp:25 vs Subsystem.cpp:1224).
3. Fold the inline `GraphSubActions` TSet and system_control's duplicated string literals into the header.

---### F2 — Both registration points and the tool-name gate fail silently or misleadingly, violating the project's own fail-fast doctrine — **HIGH**

- `FMcpToolRegistry::Register` bare-returns (no log) for any name outside the hardcoded 22-name allowlist and silently ignores duplicates (McpToolRegistry.cpp:55-63). Registration is static-init, so a new tool .cpp whose name wasn't added to `IsCanonicalMcpToolName` **vanishes with no trace** — no log, no tools/list entry.
- `RegisterHandler` is `if (Handler) TMap::Add` — null handlers silently dropped, duplicate action keys silently last-wins (Subsystem.cpp:890-895).
- A typo'd or unknown tool name in tools/call is reported as `TOOL_DISABLED` / "Tool 'X' is not enabled" (McpDynamicToolManager.cpp:68-75; transport cpp:1219-1230), steering an LLM client toward a `manage_tools enable_tools` remediation that cannot work (and whose failure is itself masked — the transport summarizes the enable attempt as success "OK" with `notFound` buried in the payload).

**Recommendation:** ~10 lines each. `UE_LOG(Error)` + `ensure()` on non-canonical registration, duplicate tool name, duplicate action key (Contains-check before Add), null handler. In `HandleToolsCall`, add a pre-gate `FMcpToolRegistry::FindTool` miss branch returning a distinct `UNKNOWN_TOOL` error (the registry lookup already happens post-gate at cpp:1271).

---

### F3 — Silent-success persistence: throttled saves report success for writes that never happened; "verification" fields assert rather than observe; the `save` parameter means three different things — **HIGH**

The save primitive is honest (see Sound #3); the layers above it lie:

- **`SaveLoadedAssetThrottled` returns true on a skipped save** inside its 0.5s window, with the rationale comment "Treat skip as success to avoid bubbling save failures into tests" (Helpers.h:1462-1474). `McpFinalizeBlueprint` — the blessed blueprint-edit tail at ~136 call sites/13 files — routes through it unforced (Helpers.h:1504-1514). ~119 of those sites discard the bool entirely, so even honestly detected failures never reach the MCP response. The maintainer's own comment names this exact defect — "return saved:true WITHOUT writing … leaving structural edits unpersisted on disk" (BlueprintHandlers.cpp:4550-4557) — and it was patched at ~13 sites with `bForce=true` while the shared tail retains it. A *second*, non-throttling `SaveLoadedAssetThrottled` exists in `McpSafeOperations` (h:951-964, params `(void)`-cast), so two same-named helpers with opposite semantics coexist.
- **Verification is an echo:** `AddAssetVerification` hardcodes `existsAfter:true` for any non-null in-memory pointer (Helpers.h:2898-2906) — 414 call sites across 40 files, vs the genuinely observing `VerifyAssetExists` at 4. `compile_material` reports `"saved": bSave` — the *request flag*, not the outcome (MaterialAuthoringHandlers.cpp:2999).
- **The `save` parameter trichotomy:** widgets/anim/material-property actions do real disk writes; **32 of 34 Niagara `bSave` sites map save:true to `MarkPackageDirty()` only** (NiagaraAuthoringHandlers.cpp:240, 505-508, …); **all pre-2026-06-28 material graph edits never read the flag at all** — every `add_*` node, connect/disconnect/delete ends in `FINALIZE_HOST` = PostEditChange+dirty (MaterialAuthoringHandlers.cpp:804-808) — while the schemas promise "Save the asset(s) after the operation" for exactly those actions (McpTool_ManageAsset.cpp:49, McpTool_ManageEffect.cpp:93). The widget file's `FinalizeWidgetEdit` comment documents this exact pattern as a **fixed data-loss bug — fixed for widgets only** (WidgetAuthoringHandlers.cpp:873-905). Dirty-only edits survive until crash or a declined close prompt; the responses carry no persistence indicator, so a client cannot detect the gap; and disk-reading consumers (git commits of .uassets, `inspect diff_asset`) see stale state behind saved:true.

**Recommendation:** (a) make `McpFinalizeBlueprint` pass `bForce=true` or return tri-state `{saved,skipped,failed}` surfaced in the response; (b) port the `FinalizeWidgetEdit` pattern (PostEditChange → dirty → if bSave `McpSafeAssetSave` → report result) into `FINALIZE_HOST` and the 32 Niagara sites — the correct implementation already exists in the same directory, and even in the same file (`set_node_value`/`arrange_graph` added 2026-06-28 do it right, MaterialAuthoringHandlers.cpp:4003-4008, 5034-5036); (c) make `AddAssetVerification` observe (`FPackageName::DoesPackageExist` + dirty flag); (d) sweep discarded save results to propagate `SAVE_FAILED`. All mechanical and local.

---

### F4 — Handler response contract is an unenforced convention: no exactly-one-response guarantee, and ~10 deferred AsyncTask completions escape every per-request invariant — **HIGH**

`FAutomationHandler` returns a bare bool meaning "consumed":
- A handler returning **true without responding** (non-throwing early return; exceptions *are* caught and answered) leaves the parked socket for the 300s reaper — a 5-minute silent hang for a deterministic server bug. Nothing checks post-dispatch; the primitive exists (`HasPendingRequest`, transport cpp:1396-1400) but is only used for origin resolution.
- A handler returning **false** is reported UNKNOWN_ACTION, conflating not-mine / declined / gate-failure — this bit in production (the Wave 6B comment), and `HandleBlueprintAction`'s tail still returns false expecting a fall-through chain the O(1) dispatcher deleted (BlueprintHandlers.cpp:5714-5720 vs ProcessRequest.cpp:151-156).
- **Ten sites** (AssetWorkflowHandlers.cpp:421, 2791, 2885, 3264; LandscapeHandlers.cpp:385, 737, 1103, 1398, 1656, 1844) return true immediately and complete later via `AsyncTask(GameThread)`. These escape the ENGINE_ERROR coercion *unconditionally* — capture ends at scope exit (ProcessRequest.cpp:96) and the coercion is gated on `bProcessingAutomationRequest` (Subsystem.cpp:728) — so those actions **always report success regardless of engine errors**. They also depend on the origin-sentinel heuristic (below), and the *same file* documents that this exact deferral pattern was removed from the thumbnail handler because it caused 30s client timeouts (AssetWorkflowHandlers.cpp:1256-1260, issues #138/#139). A half-finished migration: both conventions coexist with different failure envelopes.
- **`ERequestOrigin::WebSocket` is a triple-duty sentinel** — dead transport identity, "handler didn't specify" default (zero of ~290 SendAutomationResponse calls pass Origin), and post-reset value — patched by a `CurrentRequestOrigin` substitution + `HasPendingRequest` override (Subsystem.cpp:790-811, with a stale "over SSE" comment at :796 and a *silent* drop fallthrough at :812-814 that is reachable today for expired async completions).

**Recommendation:** (1) finish the documented migration — inline the 10 AsyncTask bodies to synchronous execution (they already run on the game thread; the wrapper only re-queues them behind the current dispatch cycle, i.e. the exact timeout bug already fixed once); (2) after a handler returns true, if origin is NativeHTTP and `HasPendingRequest(RequestId)` is still true and no explicit "deferred" marker was set, send an immediate INTERNAL_ERROR ("handler consumed request without responding") instead of waiting 300s; (3) rename `ERequestOrigin::WebSocket` → `Unspecified` and log the silent fallthrough.

---

### F5 — The WebSocket→pull migration is functionally done but structurally unfinished: a dead 1,982-line transport is the parameter currency of ~300 handler signatures — **HIGH (structural debt, zero correctness risk)**

`docs/pull-architecture.md` still reads "Status: plan / in progress"; buckets 1 and 5 of its six deletion buckets never executed:
- **`FMcpBridgeWebSocket`** (1,982+173 lines, RFC6455 + OpenSSL TLS, Build.cs:169-170) compiles with zero instantiation paths (sole `MakeShared` is inside its own unreachable accept loop) yet is the parameter type of `FAutomationHandler`, `SendAutomationResponse/Error`, `QueueAutomationRequest`, and every handler signature — **69 files, 955 references, 317 in the subsystem header alone** — always carrying nullptr (transport cpp:1310-1312 is the sole producer; the value is never dereferenced anywhere). Real routing (`RequestId → CompletePendingRequest` + origin fallback) is invisible at every call site. Intent is documented in exactly one comment (Subsystem.cpp:396-399); nothing at the 300 call sites tells a reader this is residue.
- The SSE-era parked/async machinery survives with `FSSEConnection` renamed `FPendingConnection`, including a dead `bMarkedForRemoval` check (transport cpp:1412-1413 — its only writer runs after map removal), an unused stored Accept header, and orphaned `BuildProgressNotification`/`BuildNotification`.
- **~19 of 27 `UDeveloperSettings` properties are dead WS-era knobs** (TLS paths, heartbeats, rate limits, `TickerIntervalSeconds` — contradicted by the hardcoded 0.0f ticker) that persist to DefaultGame.ini and render authoritatively in Project Settings while nothing reads them (Settings.h:33-132; live consumers only at Subsystem.cpp:417-431, transport cpp:482-485/928-932).
- Further dead extension surface: Pattern B dispatch (`GetDispatchAction`/`GetActionFieldName`, overridden by zero tools, live branch at transport cpp:1272-1290), `OnToolsChanged` (fired 5 places, bound nowhere, `listChanged:false`), the phantom `McpAutomationBridgePCH.h` (self-described shared PCH, zero includers, NoPCHs configured), dozens of legacy top-level action registrations "so TS can call directly" (Subsystem.cpp:1117-1133) unreachable through the 22-name gate, HandleLevelAction's 11/12 unreachable top-level aliases (LevelHandlers.cpp:1137-1144), and two protocol self-test actions (`test_progress_protocol`/`test_stale_progress`) that **sleep the game thread up to ~100s** to exercise the no-op `SendProgressUpdate` (SystemControlHandlers.cpp:696-767) — hidden live actions absent from the schema.

**Recommendation (codemod-shaped, the project has the muscle — scripts/apply-finalize*.ps1):** Step 1: `using FMcpResponseHandle = TSharedPtr<FMcpBridgeWebSocket>;` and mechanically rewrite all signatures (pure rename, zero behavior risk). Step 2: retarget the alias to an empty struct, delete `McpBridgeWebSocket.{h,cpp}` + the OpenSSL dep. Step 3: deletion sweep — dead settings (or `UPROPERTY`-hide), Pattern B, `OnToolsChanged`, the PCH file, the two sleep self-tests, the dead `bMarkedForRemoval` check, the legacy alias registrations. Each step compiles independently. Update `pull-architecture.md`'s status line when done — per the project's own finish-migrations rule.

---

### F6 — God-object subsystem + build economics: 310 Handle* declarations in one 1,519-line public header; a header touch rebuilds 97% of module lines with no PCH — **HIGH (weekly iteration tax)**

- One UCLASS declares 310 `Handle*` methods implemented across 58-60 TUs (~138K of ~150K module .cpp lines). 67 of 98 TUs (96.9% of lines) include the header directly or via helpers; any header edit is a near-full-module rebuild, and the docs workflow institutionalizes it (".h changes → full UBT rebuild, close all editors", extending-the-bridge.md:189-192).
- Build.cs's heroics are self-attributed externalities of this shape: `PCHUsage=NoPCHs` (:135), reflection-forced 256KB unity chunks (:138), memory probing whose result is only printed (:132), all justified by "for a module with 50+ handler files, unity builds cause heap exhaustion" (:127-130). Note the two rationales are distinct — NoPCHs targets PCH-creation C3859/C1076, the chunk cap targets C1060 — so re-testing a shared PCH now that chunks are capped is an experiment, not a guaranteed win.
- **The helpers layer inverts layering:** `McpAutomationBridgeHelpers.h` (3,020 lines, ~60 `static inline` global-namespace functions, no .cpp, included by 62 TUs) includes the subsystem header at :43 solely so two response senders (:2701-2771) can call up into `Subsystem->SendAutomationResponse` — senders consumed by exactly ONE file (ControlHandlers.cpp). It also transitively pulls the 2,679-line all-inline `McpSafeOperations.h` plus Editor.h/RenderingThread.h/KismetEditorUtilities.h into every chunk.
- The public header also leaks: `UMcpGenericDataAsset` (h:35-52 — a content data asset **instantiated into /Game by inventory/loot actions**, making game .uassets permanently depend on the bridge plugin, InventoryHandlers.cpp:142-144), a dead `FMcpAutomationMessage`/`OnMessageReceived` delegate (zero feeders), and all coordination state as public members (NativeTransport, queue+mutex, log-ring internals, busy-blueprint flags, h:263-368) — plus 11 raw globals in `McpAutomationBridgeGlobals` of which only 2 are mutex-guarded (safe only by the game-thread-only convention that the F4 deferrals already bend).
- The counter-pattern exists in-tree: `FSCSHandlers`, `FBlueprintCreationHandlers`, GeometryHandlers' static functions. Applied to 3 of ~60 domains.

**Recommendation:** Do NOT split the class wholesale. (1) **Freeze the header** — written rule in extending-the-bridge.md/AGENTS.md: new handlers are TU-local static functions or small static handler classes taking `(Subsystem*, RequestId, Payload)`; no new `Handle*` members. (2) Each time a domain file is touched, migrate its declarations out. (3) Move the two response senders into a small `McpResponseHelpers.h` and delete the subsystem include from the helpers header (one TU needs the new include). (4) Re-test a shared PCH via `PrivatePCHHeaderFile` (the dead PCH header shows it was intended); measure. (5) Evict `UMcpGenericDataAsset` to its own header (keep class name/module — content references it), flip the coordination block private (`friend FMcpNativeTransport` already exists; only the status-bar widget needs one accessor), delete the dead delegate. (6) Longer term: the Private/MCP protocol layer already avoids the subsystem header entirely; a two-module split (protocol vs handlers) is one interface-extraction away (transport's only subsystem call is `QueueAutomationRequest`), but defer until the above lands.

---

### F7 — Client-facing contract fragmentation: no server-side schema validation, three response envelopes, an undocumented wire format, and schemas that promise systems the handlers don't build — **HIGH (it's an AI-client product; the schema is the UX)**

- **No argument validation against the published inputSchema.** The transport checks only arguments-is-object (cpp:1173-1194). The schema's sole consumer is tools/list serialization. Declared enums/required fields are enforced only if the client chooses to; misspelled params are silently defaulted by the `GetJson*Field` family (**2,158 call sites/39 files**). Concretely: `configure_asc` with an invalid `replicationMode` matches no branch, changes nothing, and returns success echoing the bad value (GASHandlers.cpp:494-514); same for `set_effect_duration` (:1617-1648). The FreeformObject comment (McpSchemaBuilder.cpp:166-187) documents that client-side schema enforcement already caused a whole-argument-drop incident — the schema is load-bearing for clients while the server ignores it.
- **Semantic inflation in the gameplay tools.** Most Combat/Inventory/advanced-Character actions (roughly two-thirds of ~170 gameplay sub-actions) create only convention-named BP variables + CDO defaults yet respond "Damage application configured." / "Shield system configured." (CombatHandlers.cpp:2336-2456; run_behavior_tree adds a variable engine code never reads, AIHandlers.cpp:4218-4258). Param descriptions are literally empty strings (McpTool_ManageCombat.cpp:71-74). The honest-failure retrofit exists but is per-action and reactive (grant_ability NOT_IMPLEMENTED, GASHandlers.cpp:3112-3120). A schema-driven agent cannot distinguish "authored a working system" from "declared variables"; execution context (asset-CDO vs live-PIE) is likewise undeclared per action.
- **Three internal envelopes, two failed one-file migrations:** raw `SendAutomationResponse` (2,368 sites/62 files) vs `SendStandard*Response` (ControlHandlers only, 177 uses — error as nested object) vs `McpHandlerUtils::Build*Response` (AudioAuthoring only — error as string+code). `McpHandlerUtils.h:15`'s banner "standardized across all 56 handler files" is false. Mitigating fact: `BuildToolResult` flattens all failures to uniform text, so the wire *error shape* is consistent — but the structured error details both "standard" helpers build are **silently dropped** at the choke point, and success payload shapes diverge client-visibly. The error-code vocabulary is free-form per file (NO_WORLD 101×/18 files vs WORLD_NOT_FOUND; unknown-subaction = INVALID_SUBACTION / UNKNOWN_ACTION / UNKNOWN_SUBACTION / a misleading tool-level miss for widgets).
- **The wire format itself** — `content[0].text = "Message\n\n<condensed JSON>"` (McpJsonRpc.cpp:99-137) — is documented nowhere and hand-parsed with silent fallbacks by three separate PowerShell clients (mcp-call.ps1:52, ui-nav-test.ps1:66, material-authoring-test.ps1:43-45).
- **Aliasing policy is contradictory:** permanent triple aliases (slotName/name/widgetName; path/savePath/folder) coexist with a deliberate alias-free breaking rename ("No aliases — old from/to names return NODE_NOT_FOUND", CHANGELOG.md:47); the sub-action key is 'subAction' in some handlers, 'action' in others, papered over by three overlapping shims (transport mirror cpp:1292-1299, `GetPayloadSubAction`, `WithPayloadSubAction`) that hold only because there is exactly one ingress.
- **The response sanitizer** redacts any '/'-token not in a hardcoded 5-mount allowlist (Subsystem.cpp:251-306), greedily consuming spaces — plugin-mount paths the *input* side deliberately accepts (CHANGELOG.md:54 fixed input to `IsValidLongPackageName`) come back as "[path redacted]", and 'and/or' gets mangled. Applied only to failure messages; success payloads pass raw.

**Recommendations, all at choke points (no 2,300-site migration):**
1. One thin validation pass in `HandleToolsCall` after the enabled gate: required fields present, supplied enum-typed fields ∈ declared enum, warn on unknown top-level keys. Reject with INVALID_PARAMS naming the field.
2. Rewrite gameplay tool/action descriptions to state the actual deliverable and add `"scaffoldOnly": true` + created-variable list to scaffold responses — schema-side honesty, zero handler behavior change.
3. Normalize envelopes inside `BuildToolResult`/`SendAutomationResponse`; additively emit the Data object as `structuredContent` (~15 lines; needs a protocolVersion bump or a second content item — server pins 2025-03-26). Publish one header of blessed error-code constants; declare raw `SendAutomationResponse` the winner and deprecate the two orphan helpers.
4. Replace the sanitizer's hardcoded mounts with `FPackageName::QueryRootContentPaths` (matches the input-side fix pattern).
5. Freeze aliases: canonical names only going forward, existing aliases documented as deprecated in schema descriptions; collapse sub-action normalization to `GetPayloadSubAction` only.

---

### F8 — Robustness gaps accepted or half-mitigated: undo bypass, stalled-thread timeout, no dispatch budget — **MEDIUM**

- **Undo:** exactly 15 `FScopedTransaction` sites across 5 of 60 handler files; the world/actor/asset/blueprint files have zero (actor deletes are undoable only because the engine's `DestroyActors` opens its own transaction; the direct `World->DestroyActor` at VolumeHandlers.cpp:2098 is not). The transacted widget binders commit partial state on error paths (mutation before validation, no Cancel, un-Modify()'d flag flips — WidgetAuthoringHandlers.cpp:3474-3526). Widget recipes `ClearWidgetTreeForRebuild` with `REN_NonTransactional` then save to disk (:697-747, :5546-5596) — a *successful* create_main_menu irreversibly destroys the prior tree. Fix incrementally: Cancel-on-error in the 4 transacted sites; build recipes into a transient tree and swap on success; written rule that new multi-object mutations open a transaction.
- **Timeout on the stalled thread:** the 300s parked-request sweep runs from the game-thread ticker (Subsystem.cpp:665-668), so in the flagship failure (blocking modal / long sync handler) it is starved along with everything else — documented as unfixed-root-caused in `transport-mid-call-drop-problem.md`. The modal-title diagnosis fires only post-dismissal (blocking modal loops never tick the core ticker). Fix: move the sweep off the game thread (needs a timer thread or timed accept — the accept socket is blocking, cpp:333) and refresh the modal snapshot from `FSlateApplication::GetOnModalLoopTickEvent()`; cap parked requests (~64 → 503); delete the dead `bMarkedForRemoval` check; add a hard 30s cap to the unbounded Shutdown drain loop (cpp:208-223).
- **No time budget:** `ProcessPendingAutomationRequests` drains the whole queue per tick (Subsystem.cpp:1360-1381); handlers have unbounded runtime (sync shader compile, blocking LoadMap, 101 Finish/Flush calls across 13 files, plus ~14 blind `FPlatformProcess::Sleep` "stability" delays — one of which gates a one-shot success check that turns an undersized delay into a false FILE_NOT_FOUND, LevelHandlers.cpp:2751-2761). Per-request duration is logged Verbose-only. Cheap fixes: drain one request per tick; promote DurationMs to Warning >1s and put it in the response; replace blind sleeps with the conditions they approximate via one named `McpStabilityDelay(reason)` helper. Also: move `BuildToolResult`/`BuildResponse` serialization into the existing ThreadPool write lambda (payloads are double-serialized+escaped on the game thread with no outbound cap, McpJsonRpc.cpp:113/82); gate the always-on 5,000-entry GLog ring on `bEnableNativeMCP` (attached unconditionally at Subsystem.cpp:409-413, unreachable with the transport off — which is the default config).
- Also filed here: the screenshot handler permanently flips `bThrottleCPUWhenNotForeground=false` on the live editor settings object and never restores it (ControlHandlers.cpp:3498-3505) — a cross-request state leak.

---

### F9 — Docs and tests: the extension entry path is doc-contradicted, and the ~200-action surface has zero automated coverage — **MEDIUM**

- **Docs freshness cliff:** `Private/MCP/AGENTS.md:14` says 36 tool definitions (22 exist); `docs/handler-mapping.md` — the designated action→handler lookup — frames a live TypeScript layer (zero .ts in repo) and carries wrong rows (drift-swept system_control actions; BT actions under manage_asset); the plugin AGENTS.md demands "TS schema/action coverage and integration tests" for a layer that no longer exists; `Roadmap.md`/`native-automation-progress.md` (frozen 2026-05-23) describe the deleted Node product; even `extending-the-bridge.md` says "HTTP/SSE" (lines 4, 13) against the pull-only transport and links a deleted doc (line 11). Version identity disagrees three ways (.uplugin 0.1.4 / transport-hardcoded 0.6.0 with a missing server-info.json override / CHANGELOG last release v0.1.3 + one giant [Unreleased]).
- **Tests:** 3 C++ self-tests exercising the runner only; two host-game-coupled black-box suites (ui-nav asserts WBP_PauseScreen_C etc.; material test defaults to /Game/VFX/Telegraph); no CI despite both READMEs claiming "usable as a CI gate now"; verification = 59 "verified live" prose entries in TODO.md. `generate_test_stub` works end-to-end but its output dir has never been created. The extension how-to contains zero testing guidance. The schema-drift incident class (undeclared params silently stripped by typed clients, TODO.md:66-77) is exactly what a schema snapshot test catches.

**Recommendation:** one editing session for docs (fix 36→22 and both HTTP/SSE mentions; shrink handler-mapping.md to a per-tool file table pointing at the routing header as source of truth; tombstone the two pre-fork docs; add a "verify your action" step with an mcp-call.ps1 invocation to extending-the-bridge.md). For tests: a schema snapshot test (serialize all 22 cached schemas, diff against a checked-in golden) is nearly free and pins the highest-value contract; a "dispatch probe" (fire each declared action with empty payload, assert not-UNKNOWN_ACTION) piggybacks on the F1 startup check; a minimal fixture .uproject decouples the suites from the game when the plugin is next packaged.

---

## 4. Refactoring roadmap

Ordered by leverage ÷ risk for one maintainer with no test net. Each step compiles/verifies independently; nothing here requires a big-bang rewrite. Steps 1-4 are days-scale total and remove the failure classes that recur; 5-8 are the structural payoff.

| # | Refactor | Risk | Payoff |
|---|---|---|---|
| 1 | **Loud registration + startup drift check** (F1, F2). Error+ensure in `Register`/`RegisterHandler`; UNKNOWN_TOOL branch in the transport gate; startup pass asserting every schema action is claimed by a routing predicate/handler. ~150 lines, no behavior change on the happy path. | Very low | Kills the two most-recurring extension failure classes at editor boot instead of client runtime. Do this first — everything after is safer with it in place. |
| 2 | **Honest persistence** (F3). `McpFinalizeBlueprint` → bForce or tri-state; port `FinalizeWidgetEdit` into `FINALIZE_HOST` + the 32 Niagara sites; observing `AddAssetVerification`; propagate discarded save failures. Mechanical, local, pattern exists in-tree. | Low (behavior change is "more saving" — verify with the material test suite) | Eliminates the silent data-staleness/loss class; makes the responses' verification story true. |
| 3 | **Finish the deferral migration + response guarantee** (F4). Inline the 10 `AsyncTask` completions; forgot-to-respond check via `HasPendingRequest`; rename origin sentinel to `Unspecified`; log the silent drop. | Low-medium (the 10 inlines change timing — smoke each action) | Restores ENGINE_ERROR coverage to 100% of actions; converts 300s hangs into immediate typed errors. |
| 4 | **WebSocket codemod + deletion sweep** (F5). Alias → empty handle → delete class/OpenSSL; sweep dead settings, Pattern B, OnToolsChanged, PCH file, sleep self-tests, legacy aliases. Update pull-architecture.md status. | Low (pure deletions; each compiles) | Removes the widest ripple surface in the codebase (~955 refs), the misleading Project Settings page, and ~2,300 lines of residue; makes the real routing contract legible. |
| 5 | **Header freeze + helpers de-inversion** (F6). Written rule: no new subsystem members; extract-on-touch to the SCS/Geometry static pattern; move the 2 response senders out of the helpers header; evict `UMcpGenericDataAsset` + dead delegate; privatize coordination state; re-test shared PCH (measure). | Low per step | Monotonic shrink of the rebuild blast radius; compile-time wins compound weekly. |
| 6 | **Vocabulary consolidation** (F1 cont.). Migrate the 14 inline facade enums into `McpConsolidatedActionRouting.h` one tool per change; fold `GraphSubActions` and system_control literals into the header; single `RouteToHandler` helper for all cross-handler re-dispatch. | Low-medium (each tool independently smoke-testable via mcp-call.ps1) | Makes the step-1 startup check exhaustive; deletes the shadowing/order-dependence class. |
| 7 | **Contract hardening at choke points** (F7). Schema validation pass in `HandleToolsCall`; `structuredContent` in `BuildToolResult`; error-code constants header; honest gameplay-tool descriptions + `scaffoldOnly` markers; sanitizer mount fix; alias freeze. | Medium (client-visible — do validation as warn-first, then reject) | The product is the schema; this is the biggest UX improvement per line for AI clients, including your own future sessions. |
| 8 | **Docs + minimal test net** (F9). One-session doc sweep; schema snapshot test; dispatch probe; verification step in extending-the-bridge.md. | Very low | Cheap insurance for every subsequent refactor; ends the stale-doc trap at the extension entry point. |
| 9 | **Operational polish** (F8). One-request-per-tick drain; duration Warning + response field; off-thread timeout sweep + parked cap; shutdown hard cap; transaction Cancel-on-error + recipe swap-on-success; serialize responses on the write thread; gate the log ring. | Low-medium each | Better editor responsiveness under agent load; stall attribution that actually fires during the stall. |
| 10 | **Deferred: module split + per-domain contract tests.** Protocol layer (Private/MCP + registry) as its own module — it already avoids the subsystem header; the only edge to invert is the transport's single `QueueAutomationRequest` call. Do after 4-6; not before. | Medium | Structural endpoint; only worth it once the cycles above are cut. |

**Anti-goals** (respect the design decisions already made): don't reintroduce streaming/SSE or async handler execution (the sync single-threaded model is deliberate and correct for this use); don't split the subsystem UCLASS wholesale; don't migrate 2,300 response call sites — normalize at the choke point; don't replace the shared-list pattern — extend it.
