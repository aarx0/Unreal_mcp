# Private/MCP

Native plugin MCP implementation. It exposes the plugin directly over a **pull-only** MCP Streamable HTTP transport at `/mcp` — one request → one response, no SSE streaming and no server→client push (the legacy WebSocket automation bridge has been removed). See `docs/pull-architecture.md`.

## STRUCTURE
```
MCP/
|-- McpNativeTransport.cpp/.h     # raw socket HTTP transport (pull-only) and JSON-RPC handling
|-- McpJsonRpc.cpp/.h             # JSON-RPC response/error helpers
|-- McpToolRegistry.cpp/.h        # canonical self-describing tool registry
|-- McpSchemaBuilder.cpp/.h       # JSON schema builder DSL
|-- McpToolDefinition.h           # base class and dispatch patterns
`-- Tools/McpTool_*.cpp           # 21 self-registering tool definitions
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Change HTTP behavior | `McpNativeTransport.cpp` | `POST /mcp`, `DELETE /mcp` (GET → 405), sessions, parked-connection completion |
| Change JSON-RPC shape | `McpJsonRpc.*` | Centralize result/error envelope formatting |
| Add native tool metadata | `Tools/McpTool_*.cpp` | Subclass `FMcpToolDefinition` and use `MCP_REGISTER_TOOL` |
| Change schema construction | `McpSchemaBuilder.*` | Keep schema JSON generated through builder helpers |
| Change canonical list | `McpToolRegistry.cpp` | Only 21 parent tool names are accepted |

## TRANSPORT CONVENTIONS
- `GET /mcp` returns 405 Method Not Allowed (pull-only — no notification stream).
- `POST /mcp` handles JSON-RPC methods including `initialize`, `tools/list`, and `tools/call`. A `tools/call` parks its connection until the GameThread handler finishes, then writes one `application/json` response.
- `DELETE /mcp` tears down an initialized session.
- `initialize` creates the session and returns protocol version, `capabilities.tools.listChanged` (false), server info, and optional instructions.
- Requests after initialization require `Mcp-Session-Id`.
- No progress streaming (pull-only): `SendProgressUpdate` is a no-op. Tool results are MCP `content[]` + `isError` + a `structuredContent` envelope `{success, message, errorCode?, data?}`; the handler's result object rides the text block on success AND failure (`BuildToolResult`).

## TOOL CONVENTIONS
- Tools self-register statically with `MCP_REGISTER_TOOL`.
- Registry accepts only canonical parent tools: do not expose child/legacy tool names.
- Every tool dispatches by its own name; handlers read the sub-action from the payload (`action`, mirrored to `subAction` by the transport).
- The tool set is fixed at compile time; there is no runtime enable/disable.

## SECURITY AND LIFECYCLE
- Binding is loopback-only unless plugin settings explicitly allow non-loopback.
- Capability-token auth uses `X-MCP-Capability-Token` when enabled.
- Keep request-size, session-timeout, and async-write accounting intact; shutdown pumps game-thread tasks to avoid deadlocks.
- Do not block socket threads on Unreal editor work; route execution through the subsystem/game-thread path.

## ANTI-PATTERNS
- Hand-building tool schemas with ad hoc JSON when `McpSchemaBuilder` can express them.
- Registering non-canonical names or duplicate tool definitions.
- Returning raw handler JSON without MCP `content[]` wrapping.
- Adding transport behavior that only works for one client and bypasses JSON-RPC helpers.
