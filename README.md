# MCP Automation Bridge — Unreal Engine (native)

A Model Context Protocol (MCP) server built **directly into a C++ Unreal Engine editor plugin**.
AI clients (Claude Code, Cursor, VS Code) connect over HTTP/SSE straight to the running editor —
there is no Node/TypeScript bridge process.

> **Native-only fork.** The upstream TypeScript MCP server (`src/`, Node tooling, tests, CI) has been
> removed; the C++ plugin in [`plugins/McpAutomationBridge/`](plugins/McpAutomationBridge/) is the whole
> product. Upstream — with the TypeScript bridge — remains at `ChiR24/Unreal_mcp` (the `upstream` remote)
> if a piece of it is ever wanted back (`git checkout upstream/dev -- <path>`).

## How it works

```
AI client ──HTTP POST /mcp (JSON-RPC 2.0 + SSE)──► McpAutomationBridge plugin (in the editor)
```

The plugin runs a raw-socket HTTP server inside the editor that speaks MCP (2025-03-26):
`initialize` / `tools/list` / `tools/call`. It exposes ~22 consolidated parent tools
(`manage_asset`, `manage_blueprint`, `inspect`, `control_actor`, `system_control`, …), each
multiplexing many `action`s that dispatch to C++ handlers.

A `tools/call` passes through three native layers (see [docs/extending-the-bridge.md](docs/extending-the-bridge.md)):

1. **Tool schema** — `Private/MCP/Tools/McpTool_<Tool>.cpp` (`FMcpSchemaBuilder`); the `action` enum is
   sourced from `Private/MCP/McpConsolidatedActionRouting.h`.
2. **Action routing** — `McpConsolidatedActionRouting.h` maps the `action` string to a handler group.
3. **Handler** — `Private/McpAutomationBridge_<Area>Handlers.cpp` does the engine work and responds.

## Install

It's a normal UE plugin. For this project it already lives at
`Plugins/Unreal_mcp/plugins/McpAutomationBridge`, and UE discovers the nested `.uplugin` automatically.
For another project, clone this repo into the project's `Plugins/` directory (or copy just
`plugins/McpAutomationBridge` with [`scripts/sync-mcp-plugin.js`](scripts/sync-mcp-plugin.js)), then
rebuild the editor.

## Enable & connect

1. **Enable native MCP** in plugin settings (`Saved/Config/<Platform>/Game.ini`):
   ```ini
   [/Script/McpAutomationBridge.McpAutomationBridgeSettings]
   bEnableNativeMCP=True
   NativeMCPPort=3123
   ```
2. **Register the server** with your client. For Claude Code (user scope, `~/.claude.json`):
   ```json
   { "mcpServers": { "unreal-mcp": { "type": "http", "url": "http://127.0.0.1:3123/mcp" } } }
   ```
3. Open the project in UE (plugin built + running), then (re)start the client so the tools load. The
   editor status bar shows the MCP endpoint when it's live.

Tools only work while the editor is open with the plugin running. Changing a tool's `inputSchema`
reaches the client only on a fresh MCP handshake — restart the client after a rebuild that touches schemas.

## Build / iterate

- **Full rebuild** (headers, new files, `Build.cs`): close the editor, then
  `Engine/Build/BatchFiles/Build.bat <Project>Editor Win64 Development -Project=<uproject> -WaitMutex`.
- **`.cpp`-only** changes can hot-patch a running editor via `system_control` → `live_coding_compile`.
- **Package** the plugin standalone with [`scripts/package-plugin.bat`](scripts/package-plugin.bat) /
  [`scripts/package-plugin.sh`](scripts/package-plugin.sh) (RunUAT `BuildPlugin`).

## Docs

- [docs/extending-the-bridge.md](docs/extending-the-bridge.md) — add or fix an action (the three native layers).
- [docs/handler-mapping.md](docs/handler-mapping.md) — action → handler-file/function lookup.
- [plugins/McpAutomationBridge/docs/safe-asset-saving.md](plugins/McpAutomationBridge/docs/safe-asset-saving.md) — saving assets without crashing the editor.
- [docs/Engine-API-Reference.md](docs/Engine-API-Reference.md) · [docs/native-automation-progress.md](docs/native-automation-progress.md) · [docs/Roadmap.md](docs/Roadmap.md)
- [plugins/McpAutomationBridge/AGENTS.md](plugins/McpAutomationBridge/AGENTS.md) — plugin internals / agent guide.

## License

MIT — see [LICENSE](LICENSE).
