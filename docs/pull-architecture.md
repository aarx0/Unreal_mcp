# Pull Architecture — Transport Simplification & Idempotency

**Status:** DONE (2026-07-02). All deletion buckets executed: the transport cutover
shipped earlier; the remaining residue (`FMcpBridgeWebSocket` class + OpenSSL dep,
WS-era settings, Pattern B dispatch, `OnToolsChanged`, notification builders) was
deleted in the arch-review F5 sweep. Handler signatures now thread the inert
`FMcpResponseHandle` alias; responses route by RequestId via the native transport.
**Supersedes:** the SSE-keepalive + result-cache approach in
`docs/transport-mid-call-drop-problem.md` and the matching 🔴 roadmap item in `TODO.md`
("Per-request SSE keepalive + result cache re-queryable by request id"). Those are **not**
the direction anymore — see Context.

---

## Context

The original bug ("Native MCP transport drops mid-call while the editor GameThread is busy")
was going to be fixed with per-request SSE keepalives + a re-queryable result cache. Working
through it, we concluded that machinery is solving the wrong problem. The drop happens because
a `tools/call` response can't be produced until the single GameThread consumer ticks, and the
client (Claude Code) kills a stream that produces no bytes within ~60s. Keepalive + result
cache would *paper over* that with more long-lived state.

The cleaner answer, and the one we're committing to: **a pure pull / request→response bridge**
with **no server→client push, no long-lived streams, no keepalive**, where a lost response is
recovered the same way every other failure is — **re-query the editor's state** (the editor is
the source of truth), which is safe because **mutating ops are idempotent**. This deletes more
than it adds.

Confirmed facts driving scope:
- The client connects over **native HTTP** (`~/.claude.json`: `unreal-mcp` →
  `http://127.0.0.1:3123/mcp`). The **WebSocket transport is unused.**
- Per-request "60s first-byte" budget is a hard, non-removable client policy; the overall
  per-call timeout defaults to ~28h. So the rule is "produce the response well under 60s," not
  "don't take long." Genuinely-long ops (only lightmap, in practice) already ack fire-and-forget
  and run off the GameThread; everything else fits the budget. (See the long-op audit.)

## Target architecture

- **One HTTP request → one HTTP response.** No SSE, no progress frames, no push.
- **Single serial GameThread consumer** (the existing `PendingAutomationRequests` queue, drained
  once per tick) — kept as-is; it's the correct and only legitimate concurrency point.
- **Recovery = idempotent retry / re-query.** A dropped response is not specially handled by the
  transport; the caller re-queries state (asset exists? actor present?) or re-runs the op, which
  is safe because ops converge on a caller-supplied identity.
- **No off-GameThread handler tier.** Reads are answered through the same queue; engine stalls are
  tolerated (drop + retry), not engineered around.

## Three buckets

### DELETE

1. **The entire WebSocket transport** — `FMcpConnectionManager` (reconnect/heartbeat/telemetry/
   0.25s ticker) and `FMcpBridgeWebSocket` (RFC6455 framing, ping/pong), plus every
   `SendControlMessage`/`automation_event` push site (~7 in `*_BlueprintHandlers.cpp`),
   `AuthenticatedSockets`, rate-limit windows, and the WS-only settings. It's unused (HTTP client)
   and is where most of the push/heartbeat/cross-request state lives.
2. **The notification stream (server push)** — `FNotificationStream`, `NotificationStreams` map +
   mutex + limits, `HandleGetMcp` (answer `GET /mcp` with **405**, spec-allowed),
   `WriteNotificationEvent`, `WriteNotificationKeepalive`, `CloseNotificationStream`, and the
   keepalive loop inside `CleanupStaleRequests`.
3. **`tools/list_changed` push** — `OnToolsListChanged`, `BroadcastToolsListChanged`, the
   `ToolManager.OnToolsChanged` bind; set `tools.listChanged = false` in `initialize`.
4. **Progress** — `SendSSEProgressUpdate`, `BuildProgressNotification`, and the native branch of
   `SendProgressUpdate` (deleted). Deviation from the plan: the handler call sites were kept,
   calling a permanent no-op `SendProgressUpdate` (see its doc comment) so a future streaming
   channel has one wiring point — they were not pruned.
5. **The SSE `tools/call` machinery (full de-stream)** — `FSSEConnection`, `SSEConnections` map +
   mutex, `CompletePendingRequest`, `HasPendingRequest`, `WriteSSEEvent`, `SendSSEHeaders`,
   `PendingAsyncWrites`, and the async-write dispatch. Replace with the synchronous design below.
6. **Dead code** — `FMcpBridgeWebSocket::SendHeartbeatPing` (no caller), `TouchPendingRequest`
   (no caller).

**De-stream design (as shipped — differs from the blocking-promise sketch first drafted here):**
the worker parks the connection (socket + JSON-RPC id) in `PendingConnections` keyed by RequestId
and returns to the pool immediately — no per-request blocking signal. `QueueAutomationRequest`
enqueues the work for the GameThread; when the handler finishes, `CompletePendingRequest` looks the
connection back up, builds the JSON-RPC response, and offloads the single plain-HTTP write + close
to `Async(EAsyncExecution::ThreadPool, …)`, tracked by the `PendingAsyncWrites` atomic and guarded
by `FPendingConnection::WriteMutex`. So the parked-connection map, the async cross-thread write, and
the write mutex/atomic all remain — what de-streaming deleted is the SSE framing, multi-write
dispatch, and keepalives. One request still yields exactly one plain HTTP response.

What stays in `CleanupStaleRequests`: only the **session-expiry** sweep (if sessions are kept).
Sessions (`ActiveSessions` + `ValidateSession` + `Mcp-Session-Id`) are independent of the notification
stream and carry no per-session state beyond the activity timestamp (the `manage_tools` enablement
overlay that once rode on them was removed with that tool, 2026-07-04).

### ADD — idempotency (10 unsafe action families)

Rule: **find-by-caller-supplied-key before mutating → return existing (or fail on conflict) instead
of allocating.** Where a natural stable key exists, prefer **fail-fast on name conflict**; where it
doesn't, the op stays "create" but the caller recovers via re-query (same as any dropped call).

| # | Action | File:line | Fix |
|---|--------|-----------|-----|
| 1 | spawn actor (`control_actor` spawn) | `ControlHandlers.cpp:415` / helper `Helpers.h:2792` | pre-check `FindActorByName` (**exists @ControlHandlers:240**), return/skip if present |
| 2 | duplicate actor | `ControlHandlers.cpp:1463` | check `newName` exists first |
| 3 | scatter/duplicate-along-spline | `GeometryHandlers.cpp:5170` | key the batch (tag/label prefix); skip if present |
| 4 | add_spline_point | `SplineHandlers.cpp:476/481` | treat `index`/position as upsert key |
| 5 | create_node (BP graph) | `BlueprintGraphHandlers.cpp:572-582` | accept caller `nodeId`; return existing if present (id is what's lost on a drop) |
| 6 | add_node (MetaSound) | `AudioAuthoringHandlers.cpp:958` | caller-supplied node GUID / check by class+name |
| 7 | add SmartObject slot | `AIHandlers.cpp:2860` | key slot by caller index/id; upsert |
| 8 | add Niagara emitter/module/renderer | `NiagaraAuthoringHandlers.cpp:359/372/505/580/1465` | pre-check stack/handle list by name |
| 9 | add_*_widget family (44 sites) | `WidgetAuthoringHandlers.cpp` (helper `CreateAndRegisterWidget@626`) | `CreateAndRegisterWidget` returns the existing widget (via `WidgetTree->FindWidget`) instead of letting `ConstructWidget` auto-suffix a duplicate |
| 10 | spawn WP volumes / level-instances | `LevelStructureHandlers.cpp:1076/2172/2634/2717` | find existing by requested name before `MakeUniqueObjectName` |

#1 and #9 are near-free (the helpers already exist, just unwired). #5 is the most important
(its identity lives only in the dropped response). Already-safe ops (create_asset/blueprint,
add_variable, add_component, set_*, connect_pins) need no change.

**Implementation status (branch `feat/pull-architecture`):**
- DONE — fail-fast on name conflict, compile-verified: **#1 spawn + spawn_blueprint**,
  **#2 duplicate actor**, **#9 widget template builders** (`CreateAndRegisterWidget`).
- Relying on **state-verification recovery** (agent re-queries on a dropped response before a
  blind retry — acceptable per the agreed model, not individually guarded): the direct
  `add_*_widget` sites (#9), #4 add_spline_point, #5 create_node, #6 MetaSound add_node,
  #7 SmartObject slot, #8 Niagara emitter/module/renderer, #10 WP volumes / level-instances.
  These are additive or niche; promote any to fail-fast if it proves a real duplication source
  in practice.

### DON'T BUILD

Per-request keepalive, result cache / `get_result`, `Last-Event-ID` resumability, off-GameThread
handler tier, push/Channels. All made unnecessary by pull + idempotency.

## Verification (needs a build + the editor)

1. **Build** the editor target with the editor closed (header changes can't live-code).
2. **Smoke test over MCP** (`http://127.0.0.1:3123/mcp`):
   - a fast op (`manage_asset exists`) returns a normal JSON response (not SSE).
   - `GET /mcp` returns **405**.
   - no listener on the old WebSocket port(s).
   - a long op (`build_lighting`) still acks immediately.
   - **idempotency:** `control_actor spawn {actorName:"X"}` twice → exactly one actor "X";
     `add_*_widget` twice → one widget.
3. Confirm `initialize` advertises `tools.listChanged:false` and no progress notifications appear.

## Execution order (lowest risk first)

1. Idempotency fixes (self-contained, mechanical; #1, #9 first). ✅ safe to do without a build loop.
2. Dead code removal (`SendHeartbeatPing`, `TouchPendingRequest`).
3. Notification stream + `tools/list_changed` + progress removal (GET /mcp → 405).
4. De-stream `tools/call` (the core rewrite) — needs the build/iterate loop.
5. Delete the WebSocket transport (largest deletion; unwind all references) — needs build/iterate.
6. Update `transport-mid-call-drop-problem.md` + the `TODO.md` roadmap item to point here.
