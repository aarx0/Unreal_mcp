# Extending the Bridge — Adding & Fixing Actions

A practical, end-to-end guide to adding or changing an MCP action in the **native**
plugin (the C++ bridge that serves pull-only Streamable HTTP at `/mcp`; no SSE).

This doc is the "how do I actually change behavior" companion to:

- [`Private/MCP/AGENTS.md`](../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/AGENTS.md) — native transport + tool-registration internals.
- [`handler-mapping.md`](./handler-mapping.md) — the action → handler-file/function lookup table.

---

## The three layers an action passes through

A `tools/call` request flows through three places. To add or fix an action you usually
touch **layer 3 only**; touch layer 1 only when you need a new parameter the schema
must advertise.

```
 MCP client (Claude / VS Code)
        │  POST /mcp  { "method":"tools/call",
        │               "params":{ "name":"manage_blueprint",
        │                          "arguments":{ "action":"set_style", ... } } }
        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ LAYER 1 — Tool definition & schema                                           │
│   Private/MCP/Tools/McpTool_<Tool>.cpp  (e.g. McpTool_ManageBlueprint.cpp)    │
│   • Declares the tool name + BuildInputSchema() via FMcpSchemaBuilder.        │
│   • The schema is what the CLIENT validates arguments against before sending. │
└─────────────────────────────────────────────────────────────────────────────┘
        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ LAYER 2 — Action routing                                                     │
│   Private/MCP/McpConsolidatedActionRouting.h                                  │
│   • Maps the `action` string to a handler group on the subsystem.            │
└─────────────────────────────────────────────────────────────────────────────┘
        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ LAYER 3 — Handler implementation                                             │
│   Private/McpAutomationBridge_<Area>Handlers.cpp                             │
│   • A long if-chain on the sub-action does the real UE work and responds.    │
│       if (SubAction.Equals(TEXT("set_style"), ESearchCase::IgnoreCase)) { … } │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Key consequence:** the client validates `arguments` against the **Layer 1 schema**
before the request ever reaches the bridge. A value that fails schema validation can be
dropped *with the rest of the arguments*, and the handler then sees no `action` at all
(it errors with `Unknown blueprint action`). See the `value` gotcha below.

---

## Add a new action (handler-only, the common case)

Most new actions reuse an existing tool + existing parameters. Then you only edit Layer 3.

> **Header freeze (2026-07-02):** do not add new `Handle*` member declarations to
> `McpAutomationBridgeSubsystem.h` — any edit to that header rebuilds nearly the whole
> module. New handler logic goes in TU-local `static` functions, or a small static
> handler class with its own private header when it must be shared (see `FSCSHandlers`
> / `FBlueprintCreationHandlers` / GeometryHandlers' static free functions for the
> pattern). When you touch a handler file for other reasons, prefer migrating its
> declarations out of the subsystem header the same way.

> **Action vocabulary:** the schema enum for every tool comes from the shared lists in
> `Private/MCP/McpConsolidatedActionRouting.h`, and `McpStartupValidation` cross-checks
> lists, routing families, and published schemas at editor boot. Add your action name to
> the right list there — a mismatch shows up as `LogMcpStartupValidation: Error` +
> ensure at startup, not as a client-side UNKNOWN_ACTION surprise later.

1. **Pick the handler file** for the area (see `handler-mapping.md`). Widget/UMG work →
   `McpAutomationBridge_WidgetAuthoringHandlers.cpp`. Graph nodes →
   `McpAutomationBridge_BlueprintGraphHandlers.cpp`. CDO/defaults →
   `McpAutomationBridge_BlueprintHandlers.cpp`.

2. **Add an `if` block** to that file's dispatch, copying the shape of a neighbor:

   ```cpp
   if (SubAction.Equals(TEXT("my_action"), ESearchCase::IgnoreCase))
   {
       FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
       FString SlotName   = GetSlotName(Payload);            // reads "slotName"
       if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
       {
           SendAutomationError(RequestingSocket, RequestId,
               TEXT("Missing required parameters: widgetPath and slotName"),
               TEXT("MISSING_PARAMETER"));
           return true;
       }

       UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
       if (!WidgetBP) { /* SendAutomationError NOT_FOUND */ return true; }

       UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
       if (!Widget) { /* SendAutomationError WIDGET_NOT_FOUND */ return true; }

       // …do the work on UE objects…

       WidgetBP->MarkPackageDirty();
       McpSafeAssetSave(WidgetBP);

       ResultJson->SetBoolField(TEXT("success"), true);
       SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Done"), ResultJson);
       return true;   // ALWAYS return true after responding
   }
   ```

3. **Make sure the action is routable.** If the action name is brand new, add it to the
   enum/allow-list in `McpConsolidatedActionRouting.h` (Layer 2) and, if you want the
   client to advertise/validate it, to the tool's `action` `StringEnum` in
   `McpTool_<Tool>.cpp` (Layer 1). Reusing an already-listed action skips this.

4. **Build** (see *Build workflow* below) and call it.

### Helper cheat-sheet (Layer 3)

| Need | Use |
|------|-----|
| Read string param (with default) | `GetJsonStringField(Payload, TEXT("name"), TEXT("def"))` |
| Read the widget name param | `GetSlotName(Payload)` (reads `slotName`) |
| Load a widget BP | `LoadWidgetBlueprint(Path)` |
| Find a widget in the tree | `WidgetBP->WidgetTree->FindWidget(FName(*Name))` |
| Success response | `SendAutomationResponse(RequestingSocket, RequestId, true, Msg, ResultJson)` |
| Error response | `SendAutomationError(RequestingSocket, RequestId, Msg, TEXT("ERROR_CODE"))` |
| Persist an asset | `Obj->MarkPackageDirty();` then `McpSafeAssetSave(Obj)` |
| Structural BP change | `FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP)` |
| Compile a BP (graph/var changes) | `FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection)` |

Always `return true;` from a handled sub-action (the dispatcher treats `true` as
"handled"; falling through lets another block or the unknown-action error fire).

A handler that returns `true` **must have sent exactly one response first** — the
dispatcher now fails a consumed-but-unanswered request with `HANDLER_NO_RESPONSE`
immediately. If you genuinely must complete after returning (next-tick deferral),
call `MarkRequestDeferred(RequestId)` before returning; don't re-queue work with
`AsyncTask(GameThread, …)` from a handler — that pattern was removed (it broke the
engine-error capture and added client-visible latency).

5. **Verify your action live**: with the editor + bridge up, call it once and check
   the response *and* the boot log:

   ```powershell
   pwsh -File scripts/mcp-call.ps1 -Tool manage_blueprint -ArgumentsJson '{ "action": "my_action", ... }'
   # or, from an existing pwsh session (hashtables can't cross the -File
   # process boundary, so this form is in-process only):
   #   ./scripts/mcp-call.ps1 -Tool manage_blueprint -Arguments @{ action = 'my_action' }
   # and after an editor restart, confirm the boot log line:
   #   LogMcpStartupValidation: Action routing validation passed (...)
   ```

6. **Run the schema tests.** `tests/schema/schema-snapshot-test.ps1` diffs the live
   `tools/list` against a checked-in golden (needs the bridge up; `-UpdateGolden`
   re-pins). `tests/schema/param-reconciliation-test.ps1` is offline: it parses the
   `McpTool_*.cpp` schema builders and the handler sources and fails on params
   declared but never read, or read off the request payload but never declared —
   both directions of the drift the 2026-07-02 dogfood audit found. Matching is
   repo-global by name (v1); pre-existing exceptions live in
   `tests/schema/param-reconciliation-allowlist.txt` (`-UpdateAllowlist` re-pins).
   Prefer fixing the schema over growing the allowlist.

7. **Update the docs in the same commit.** If the change renames/deletes a file or
   symbol, grep `*.md` for it; if it ships something a design doc proposed, flip that
   doc's Status header. `tests/docs/docs-reference-test.ps1` (offline) fails on doc
   references to files/symbols that no longer exist and on known-rot tripwire phrases.

---

## Gotchas (learned the hard way)

### 1. Unknown params pass through; declared params get validated
Parameters **not** declared in the Layer-1 schema (`widgetPath`, `slotName`, `parentSlot`,
`delegateName`, `targetFunction`, `text`, …) are forwarded to the handler untouched — the
client doesn't validate what it doesn't know about. That's why most handlers read params
the schema never lists.

The flip side: a param that **is** declared with a restrictive type **is** validated
client-side, and a bad value can nuke the whole `arguments` object.

### 2. The `value` / `defaultValue` "object" trap
`FMcpSchemaBuilder::FreeformObject(...)` previously emitted `{"type":"object"}`. The
generic `value` (e.g. `set_style`) and `defaultValue` (e.g. `set_default`) params use it.
Passing a **scalar** (`"value":"Back"`) failed client-side type validation, the client
dropped *all* arguments, and the bridge reported `Unknown blueprint action: manage_blueprint`.

Fixed by making `FreeformObject` emit **no** `type` (JSON-Schema idiom for "any value").
If you reintroduce a typed generic-value param, expect scalar writes to break again.

### 3. `set_style` is a generic reflection setter (not just styles)
Despite the name, `set_style` (in `WidgetAuthoringHandlers.cpp`) takes `propertyName` +
`value` and does `FindPropertyByName` → `ImportText_Direct` on the widget **instance**.
With no `value` field it runs in **read** mode and returns the current value. It's the way
to read/write arbitrary properties on a widget in the tree (e.g. a Common UI button's
`ButtonText`). It marks dirty + saves and deliberately does **not** recompile.

### 4. `inspect` can't reach widget-tree subobjects
`inspect get_property` / `set_property` resolve assets/actors, **not** widgets inside a
WidgetBlueprint's `WidgetTree`. To touch a child widget's property use `set_style`
(resolves via `WidgetTree->FindWidget`), not an `objectPath` like `…:WidgetTree.Button`.

### 5. Recompile can wipe widget instance overrides
For a plain property write on a widget **template** (instance override in the tree),
`MarkPackageDirty` + save is enough — do **not** `CompileBlueprint`, which can reset
instance values. Save *graph/variable* changes by compiling; save *instance-property*
changes without one. If you must do both, set instance properties **after** any compile,
then save.

### 6. Widget delegate events need a `ComponentBoundEvent`, not a generic node
`create_node` can't make a working widget-`OnClicked` node — the engine uses
`UK2Node_ComponentBoundEvent`, which must be initialized with the widget's variable
(`FObjectProperty` on the BP class) **and** the delegate (`FMulticastDelegateProperty` on
the widget's class). See the real `bind_on_clicked` in `WidgetAuthoringHandlers.cpp` for
the pattern (it also optionally creates + wires a self `targetFunction` call). The widget
must be a variable (`bIsVariable`) for the bound event to reference it.

### 7. Editing an asset that's OPEN in the editor desyncs (partial!)
The bridge mutates the real `UObject`s, but the editor's open designer/Details views are
**cached** — they refresh on reselect / recompile / reopen, NOT on external property
writes. So an MCP property change (font, size, color) won't appear in an already-open tab
even though a `set_style` read-back confirms it on the object. The desync is *partial* and
that's the trap: **structural** changes (adding/removing widgets) tend to show up (the
designer rebuilds the tree on interaction), while **property** edits on already-displayed
widgets keep showing stale values. Worst of all, **Saving from the stale tab overwrites the
MCP edits** with the editor's in-memory copy. Rules: keep the asset **closed** during an MCP
pass (or close-without-save → reopen to resync); never trust the Details panel over a
`set_style` read; if you must have it open, reselect/recompile to refresh and don't Save the
stale tab.

### 8. Container variable types are spelled `Array<Inner>`, not `Inner[]`
`manage_blueprint add_variable` parses `variableType` container syntax as
`Array<String>` / `Set<Int>` / `Map<String,Float>` (`T`-prefixed forms also work);
C-style `String[]` is not recognized and fails as an unresolvable class.

---

## Build workflow — what compiles how

| Change | How to apply | Editor stays open? |
|--------|--------------|--------------------|
| `.cpp` body edits (new code in existing functions, new `#include`s from already-linked modules) | **Live Coding** — `Ctrl+Alt+F11` | ✅ yes |
| New/changed `.h`, `*.Build.cs`, `*.uplugin` (new module dep, new class member/function) | **Full UBT rebuild** | ❌ close **all** editors for that engine install first |
| Tool **inputSchema** change (`McpTool_*.cpp` / `McpSchemaBuilder`) | Compile (Live Coding is fine), **then reconnect the MCP client** | ✅ yes, but client must re-handshake |

**Why schema changes need a reconnect:** the schema is sent to the client during the MCP
handshake (`initialize` / `tools/list`). Compiling updates `BuildInputSchema()` in the
running editor, but a client that already handshook keeps validating against the *old*
schema. Reconnect the client (e.g. reload the VS Code window) so it re-fetches `tools/list`.
Handler-only changes (Layer 3) take effect immediately after Live Coding — no reconnect.

**Triggering Live Coding without the editor UI:** the bridge exposes
`system_control live_coding_compile` (calls `ILiveCodingModule::Compile` with
`WaitForCompletion`). It compiles changed `.cpp` and patches the running editor in place —
the headless / MCP-driven equivalent of `Ctrl+Alt+F11`, so an agent can apply `.cpp`-body
bridge changes without a close/rebuild/relaunch cycle. Same limits as the shortcut:
`.h` / `*.Build.cs` / `*.uplugin` changes still need a full rebuild, and Live Coding must be
enabled for the session. The response carries the outcome structured: `compileResult`,
captured `log` lines, parsed compiler `errors` (scraped from the UBT log on Failure — the
LiveCoding console keeps them off the editor log), and a `reason` on NoChanges. All
completion states arrive as a *successful* MCP response; read the body's `success`/
`compileResult`, not `isError`.

**Full builds from the bridge:** `system_control run_ubt` is fire-and-poll — it launches
UBT detached (default `-NoUBA -WaitMutex`; a UBA-built binary can't be Live Coding-patched
afterwards) with output redirected to `Saved/McpBuilds/<buildId>.log` and returns a
`buildId` immediately. Poll `system_control get_build_status` for `running|succeeded|failed`,
`[N/M]` progress, parsed errors, and the final result line. It never blocks the game thread
(the pre-2026-07-03 implementation pumped a pipe on the game thread for the whole build).

> "Live Coding active" blocking a full rebuild is an **engine-install-wide** lock: close
> every editor using that engine, not just this project.

---

## Quick reference — files

| Layer | File(s) |
|-------|---------|
| Transport / sessions | `Private/MCP/McpNativeTransport.cpp` |
| JSON-RPC envelope | `Private/MCP/McpJsonRpc.*` |
| Tool defs + schema | `Private/MCP/Tools/McpTool_*.cpp`, `Private/MCP/McpSchemaBuilder.*` |
| Action routing | `Private/MCP/McpConsolidatedActionRouting.h` |
| Handlers (the work) | `Private/McpAutomationBridge_*Handlers.cpp` |
| Action → handler map | `docs/handler-mapping.md` |
