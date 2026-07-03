> ⚠️ **SUPERSEDED (2026-06-05)** — the fix direction sketched here (per-request SSE keepalive +
> a re-queryable result cache) was **abandoned**. The chosen approach is the pull-only
> architecture in [`pull-architecture.md`](pull-architecture.md): one request → one response,
> no server push, no keepalive; a dropped response is recovered by idempotent retry / re-querying
> editor state. This file is kept for its root-cause analysis only.

# Problem statement: native MCP transport drops mid-call when the editor GameThread is busy

> Status: **unfixed**, root-caused. This is a planning brief for a fresh instance — it
> describes the bug, the architecture, the confirmed mechanism, the hard constraints, and
> candidate fix directions. It deliberately does **not** prescribe a single solution; pick the
> approach during planning. All line numbers are as of 2026-06-05 and may drift — treat the
> function/symbol names as the durable anchors.

## 1. Symptom

A `tools/call` to the native MCP endpoint (`POST /mcp`, port 3123) intermittently fails on the
**client** side with:

```
MCP server "unreal-mcp" transport dropped mid-call; response for tool "<name>" was lost
```

…even though **the operation actually executed and completed** on the server. Reproduced
2026-06-05 (and 06-04): immediately after a fresh editor launch, two back-to-back `manage_asset`
calls (`delete_asset`, then `exists`) both reported the dropped-transport error — but the editor
stayed alive and the delete **had in fact succeeded** (a later `exists` showed the asset already
gone). Only the *responses* were lost. The window is "editor is busy": right after launch (map
load, asset-registry scan, GC, sync loads) or during any long synchronous GameThread operation.

## 2. Why it matters

The lost response is **unrecoverable**. There is no "did request `<id>` land?" query. For a
**non-idempotent mutation** (create/delete/rename/set-property) the client cannot safely retry —
a retry might double-apply or fail spuriously — so the only recourse is to independently re-read
state and infer what happened. That makes every mutating call during a busy window a
correctness hazard for any automation built on the bridge: the client genuinely cannot tell
success from failure.

## 3. Architecture background (how a tools/call flows)

Two threads matter. **All real work must run on the GameThread** (UObject/editor APIs are not
thread-safe); the socket server runs on its own thread and may not touch UObjects.

1. **Socket thread** — `FMcpNativeTransport::Run()`
   (`Private/MCP/McpNativeTransport.cpp`, ~L304), an `FRunnable`. Accepts connections, parses
   HTTP/JSON-RPC, and for `tools/call` invokes `HandleToolsCall` (~L1271) **on this thread**.

2. `HandleToolsCall` (~L1271), still on the socket thread:
   - Validates, then **sends the SSE response headers immediately** via `SendSSEHeaders` (~L1358)
     — the connection is now a live `text/event-stream`.
   - **Parks** the connection: builds an `FSSEConnection` (socket + JSON-RPC id + `StartTime` +
     tool name), stores it in the `SSEConnections` map keyed by a freshly generated `RequestId`
     (~L1370–1379).
   - **Queues** the work: `Subsystem->QueueAutomationRequest(RequestId, DispatchAction, Args, …,
     ERequestOrigin::NativeHTTP)` (~L1428). Returns. No work has run yet.

3. **GameThread** — the subsystem registers a core ticker in `Initialize()`:
   `TickHandle = FTSTicker::GetCoreTicker().AddTicker(… &UMcpAutomationBridgeSubsystem::Tick …)`
   (`Private/McpAutomationBridgeSubsystem.cpp`, L379). The comment at L376–378 is explicit: *"Automation requests are drained here, after the engine world tick has completed."* That same
   `Tick` also calls `NativeTransport->CleanupStaleRequests()` (L602). **`FTSTicker::GetCoreTicker()`
   ticks on the GameThread.**

4. When the GameThread tick eventually drains the queued request, the handler runs (the real
   delete/create/etc.), then `CompletePendingRequest` (`McpNativeTransport.cpp` ~L1436) writes the
   final JSON-RPC result back over the parked SSE socket. The blocking socket write is **offloaded
   to the thread pool** (`Async(EAsyncExecution::ThreadPool, …)`, ~L1466/1473) so it does not block
   the GameThread.

## 4. Root-cause mechanism

When the GameThread is saturated, the core ticker (step 3) does not run. Therefore:

- The queued automation request **does not execute** — no progress events, no result.
- **`CleanupStaleRequests` also does not run** (same GameThread ticker), so the server's own
  300 s request timeout cannot fire during the stall either.
- **The per-request SSE stream has no keepalive.** The only keepalive frames (`:keepalive`,
  `WriteNotificationKeepalive`, ~L1151) are written to **`FNotificationStream`** objects — the
  long-lived `GET /mcp` *notification* channel — **not** to the per-request `FSSEConnection`
  response streams. So a slow call's response stream is completely silent.

A silent SSE stream looks dead, and the connection is torn down — by the **client's** read
timeout (which is shorter than the server's 300 s `RequestTimeoutSeconds`, so it wins), or by a
TCP reset during the launch/init window. Then the GameThread finally ticks, drains the queue, and
**runs the operation** (mutation lands). `CompletePendingRequest` writes the result to the
now-dead socket; the write fails (under the 5 s `WriteTimeoutSeconds` in `SendAllBytes`, ~L272),
the connection is marked `bMarkedForRemoval`, and the result is logged as lost.

**Net: op applied, response lost, client sees a dropped transport.**

## 5. The hard constraint any fix must respect

During a GameThread stall, the **only** live execution contexts are the **socket thread**
(`Run`) and the **thread pool**. The GameThread core ticker — which drives *both* the automation
queue drain *and* `CleanupStaleRequests` — is stalled. So **any liveness signal that must keep
flowing while a tool call is in flight (a keepalive, an "accepted" ack, a timeout) cannot be
driven from the GameThread ticker.** It has to come from the socket thread or a dedicated
non-GameThread timer/thread. This is the central design pressure of the fix.

## 6. Relevant code map

| Location | What's there |
|---|---|
| `McpNativeTransport.cpp:304` `Run()` | Socket accept loop (FRunnable); only thread alive during a GameThread stall |
| `McpNativeTransport.cpp:1271` `HandleToolsCall` | Sends SSE headers, parks `FSSEConnection` in `SSEConnections`, queues work |
| `McpNativeTransport.cpp:~1358` `SendSSEHeaders` | Begins the streaming response |
| `McpNativeTransport.cpp:1436` `CompletePendingRequest` | Writes final result to the parked socket; thread-pool offloaded (~1466/1473) |
| `McpNativeTransport.cpp:1151` `WriteNotificationKeepalive` | `:keepalive` — **notification streams only**, not per-request |
| `McpNativeTransport.cpp:1598` `CleanupStaleRequests` | Expires `SSEConnections` after `RequestTimeoutSeconds` or `bMarkedForRemoval` |
| `McpNativeTransport.cpp:~270` `SendAllBytes` | Socket write helper; `WriteTimeoutSeconds = 5.0` |
| `McpNativeTransport.h:187/193` | `SessionTimeoutSeconds = 3600`, `RequestTimeoutSeconds = 300` |
| `McpNativeTransport.h:93/94` (`FSSEConnection`) | `FCriticalSection WriteMutex` (per-conn write lock), `std::atomic<bool> bMarkedForRemoval` |
| `McpAutomationBridgeSubsystem.cpp:379` | Registers `Tick` on `FTSTicker::GetCoreTicker()` (GameThread) |
| `McpAutomationBridgeSubsystem.cpp:602` | `Tick` calls `NativeTransport->CleanupStaleRequests()` |
| `McpAutomationBridgeSubsystem.cpp:607` `QueueAutomationRequest` | Enqueues; drained by the GameThread tick |

Two distinct stream maps to keep straight: **`SSEConnections`** (per-request response streams,
`FSSEConnection`, the ones with no keepalive) vs **`NotificationStreams`** (long-lived `GET /mcp`,
`FNotificationStream`, the ones that *do* get keepalives).

## 7. Candidate fix directions (evaluate during planning — not a prescription)

- **(a) Socket-thread keepalive pump for per-request SSE streams.** Periodically write a
  `:keepalive` comment frame to every parked `FSSEConnection` from the socket thread (or a
  dedicated timer thread), so an in-flight call whose GameThread work hasn't run yet does not look
  dead. Highest-leverage and self-contained. Watch: write/lock interaction with
  `CompletePendingRequest`'s thread-pool write (both touch the socket — `FSSEConnection::WriteMutex`
  exists for exactly this), and avoid pumping a connection already marked for removal.
- **(b) Immediate "accepted" SSE event.** Emit one event right when the request is queued (from
  the socket thread, before returning from `HandleToolsCall`), so the client sees bytes promptly
  and learns the server-assigned `RequestId`.
- **(c) Re-queryable results.** Keep a short-lived result cache keyed by `RequestId`, plus a
  `get_result`/status probe, so a dropped response is recoverable instead of forcing a blind retry
  or an independent state re-read. Pairs naturally with (b) handing the client a `RequestId`.
- **(d) "Warming up" fast-path.** Until the editor finishes initial load, answer tool calls fast
  with a definitive "not ready" status rather than silently queueing into the stall.

(a)+(c) together remove the failure mode: the connection stays alive through the stall, and even
if it does drop, the result is recoverable by id.

## 8. Open questions / things to verify while planning

- **Client timeout is not visible from this repo.** The exact trigger that severs the connection
  (client read-timeout value vs. a TCP reset during init) lives in the Claude MCP client, not
  here. The server-side mechanism (silent stream during GameThread saturation) is confirmed; the
  precise client cutoff is not. A keepalive (a) defends regardless of the exact client value.
- **Keepalive cadence vs. client tolerance** — pick an interval comfortably under typical SSE/HTTP
  idle timeouts (single-digit seconds is the usual safe range) without spamming.
- **Thread-safety of writing to a parked `FSSEConnection` from the socket thread** while
  `CompletePendingRequest` may also write from the thread pool — confirm `WriteMutex` covers both
  paths and that a connection removed under `SSEConnectionsMutex` can't be written after free.
- **Interaction with the existing 300 s `RequestTimeoutSeconds` and 5 s `WriteTimeoutSeconds`** —
  ensure a keepalive failure correctly marks `bMarkedForRemoval` and that timeouts still bound a
  genuinely abandoned request.
- **Don't regress the working `FNotificationStream` keepalive** — the new pump is for the *other*
  map (`SSEConnections`).

## 9. Acceptance criteria

A tool call issued while the GameThread is busy must end in one of two honest states, **never**
"silently applied but reported as a transport drop":

1. It completes and the client receives the real result, **or**
2. It returns a definitive, client-visible error (timeout / not-ready), **and** if the mutation
   nonetheless applied, the client can discover that by re-querying the `RequestId`.

A good regression test: stall the GameThread (e.g. a deliberate long sync op or the
launch/asset-scan window), issue a mutating `tools/call`, and assert the client never sees a
"dropped mid-call; response lost" for an operation that actually executed.
