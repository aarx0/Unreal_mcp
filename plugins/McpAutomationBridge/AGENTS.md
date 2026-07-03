# Plugins/McpAutomationBridge

Editor-only Unreal Engine 5.0-5.8 Preview plugin. Version source is `McpAutomationBridge.uplugin` (`VersionName` currently `0.1.4`). It IS the MCP server: a native, pull-only Streamable HTTP endpoint (`POST /mcp`, JSON-RPC 2.0) served from inside the editor. The legacy WebSocket bridge and the TypeScript server that consumed it were deleted (2026-06/07); there is no other transport.

## STRUCTURE
```
McpAutomationBridge/
|-- McpAutomationBridge.uplugin
|-- Config/                       # plugin defaults and packaging filters
`-- Source/McpAutomationBridge/
    |-- McpAutomationBridge.Build.cs
    |-- Public/
    |   |-- McpAutomationBridgeSettings.h
    |   `-- McpAutomationBridgeSubsystem.h
    `-- Private/
        |-- McpAutomationBridgeSubsystem.cpp
        |-- McpAutomationBridge_ProcessRequest.cpp
        |-- MCP/                  # native MCP transport, tool schemas, action routing
        |-- McpSafeOperations.h   # UE 5.7-safe save/load wrappers
        |-- McpAutomationBridgeHelpers.h
        `-- McpAutomationBridge_*Handlers.cpp  # 58 domain handler files
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Add action handler | `Private/McpAutomationBridge_*Handlers.cpp` | Keep domain naming aligned with existing files |
| Register handler | `Private/McpAutomationBridgeSubsystem.cpp` | Add to `InitializeHandlers()` |
| Declare handler | `Public/McpAutomationBridgeSubsystem.h` (member) or a file-local fwd decl | Handlers are subsystem methods / free functions; there is no separate declarations header |
| Route requests | `Private/McpAutomationBridge_ProcessRequest.cpp` | Game-thread dispatch, unsafe-state deferral, reentrancy guard |
| Action routing | `Private/MCP/McpConsolidatedActionRouting.h` | Single source for action→handler-family lists AND schema enums; see `docs/handler-mapping.md` |
| Native MCP transport | `Private/MCP/` | See nested AGENTS for `/mcp` transport and tool registry rules |
| Settings | `Public/McpAutomationBridgeSettings.h`, `Private/McpAutomationBridgeSettings.cpp` | Loopback, capability token, native MCP port, debug knobs (`defaultconfig` → `Config/DefaultGame.ini`) |
| Packaging | `scripts/package-plugin.*`, `Config/FilterPlugin.ini` | RunUAT package, installed flag, zip output |

## CONVENTIONS
- Handlers run through the subsystem request queue and game-thread dispatch. Do not execute editor API calls from socket threads.
- `InitializeHandlers()` registers exactly the 22 canonical tool names (boot validation enforces this); per-action routing inside a tool lives in `McpConsolidatedActionRouting.h`.
- Defer work while Unreal is saving packages, garbage collecting, or async loading; do not add bypasses around unsafe-state checks.
- Build configuration is intentionally version-aware: keep `Build.cs` feature probes and optional dependency guards when adding engine modules.
- Optional plugin features should fail gracefully when the UE module/plugin is unavailable.
- Docs ride in the same commit as the change that outdates them. Renaming/deleting a file or symbol? Grep `*.md` for it. Shipping something a design doc proposed? Flip its **Status** header (Proposed → SHIPPED / PARKED / MOOT) in that commit — status lines are where docs rot first, and `tests/docs/docs-reference-test.ps1` can only catch dangling references, not a lying status.

## UE SAFETY
- Use `McpSafeAssetSave`, `McpSafeLevelSave`, and `McpSafeLoadMap` from `McpSafeOperations.h` instead of raw package save/load calls.
- For **unattended edits to existing assets** (e.g. `set_asset_property`), prefer `McpDirectPackageSave` over `McpSafeAssetSave`: the latter routes through the editor save helpers (validate-on-save, source-control checkout), both of which have crashed the editor here. See `docs/safe-asset-saving.md`.
- For Blueprint SCS work, create nodes/templates through SCS ownership patterns (`CreateNode`, `AddNode`) instead of assigning arbitrary outers.
- Avoid `ANY_PACKAGE`; use modern lookup helpers or `nullptr`-based lookups.
- Avoid modal asset saves on newly created assets; they can crash editor/D3D12 paths.

## SECURITY
- Default binding is loopback-only. `bAllowNonLoopback` must be explicit before binding LAN addresses.
- Capability-token auth gates the native MCP transport when `bRequireCapabilityToken` is enabled (`X-MCP-Capability-Token` header).
- Path helpers must reject traversal and absolute host paths before touching the filesystem.

## ANTI-PATTERNS
- Blocking the game thread from socket accept/read/write loops.
- Adding handler actions without declaring them in the tool's `McpTool_*.cpp` schema AND the routing list in `McpConsolidatedActionRouting.h` (the param-reconciliation and schema-snapshot tests in `tests/schema/` catch drift).
- Hardcoded `C:\`/`X:\` paths or project-local absolute paths in handlers/scripts.
- Editing generated plugin outputs: skip `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, and repo-root `build/` packaging output.
