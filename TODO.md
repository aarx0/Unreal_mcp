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
- 🟡 **Synchronous results / streaming** — `system_control run_tests` is fire-and-forget (no
  result capture); `docs/Roadmap.md` Phase 5 ("real-time streaming for logs/test results",
  remote Insights profiling) is unstarted. Add wait/poll-by-request-id (pairs with the
  transport result cache) so automation/test outcomes are retrievable, not just logged.

### B. Authoring capability gaps — new features that unblock real workflows
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
- 🟡 **Enhanced Input authoring depth** — IMC/IA creation + key mapping exist (Input group);
  verify round-trips and fill the sparse trigger/modifier authoring (modifier/trigger
  factories are thin).
- 🟡 **Common UI completeness** — have: add button/text/border, assign style, bind input
  action. Missing / runtime-only: activatable-widget stack push/pop + focus/nav, style-**asset
  creation** (only assignment today), input-action→click wiring. Some is inherently runtime
  (see `COMMONUI_INTEGRATION_PLAN.md`).
- 🟡 **Asset import pipeline** — `import` works (Interchange) but there's no post-import
  configuration: texture compression/sRGB/streaming, audio codec/sample-rate, mesh
  LOD/Nanite/collision. Add a post-import `configure_import` pass (or params on `import`).
- 🟢 **GAS ability-graph depth** — GAS scaffolding is broad (create ability/effect/attribute/
  cue BPs), but multi-step ability *logic* (wait-for-input → montage → apply-effect) is manual
  graph wiring. Templated ability-task chains would speed the boss/combat work.

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
- 🟡 **Input-injection fidelity** — `simulate_input` routes through `FSlateApplication` key/
  mouse events: right for **menu/UI** testing (works today), but may not exercise **Enhanced
  Input gameplay** mappings (movement/abilities). Verify; if needed, add player-input /
  Enhanced-Input injection for gameplay tests.
- 🟡 **Synchronous test/assert** — see A "synchronous results." Closing it makes the
  author→play→assert loop scriptable end-to-end.
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
through the helper and surfaced it in the Common UI callers (add_common_button/text/border). The 24
regular `add_*` callers still error safely but with their generic "Failed to add X to widget tree"
message — minor follow-up to thread `OutError` there too for identical guidance. Also gave clear
guidance for the `parentSlot`-not-found / not-a-panel cases. (Original analysis below.)

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
