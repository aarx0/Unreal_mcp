// McpToolDefinition.h — Base class for self-describing MCP tool definitions

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Descriptor for one MCP tool. Pure metadata — no execution logic.
 *
 * Each tool .cpp defines a static subclass and registers it via MCP_REGISTER_TOOL.
 * The registry queries these at startup to build tools/list responses and
 * resolve dispatch actions for tools/call.
 */
class FMcpToolDefinition
{
public:
	virtual ~FMcpToolDefinition() = default;

	/** MCP tool name (e.g. "build_environment"). Used as the "name" field in tools/list. */
	virtual FString GetName() const = 0;

	/** Human-readable description shown to the LLM. */
	virtual FString GetDescription() const = 0;

	/** Category for dynamic tool management (core, world, gameplay, utility). */
	virtual FString GetCategory() const = 0;

	/**
	 * Build the inputSchema JSON object for this tool.
	 * Called once at startup; result is cached by the registry.
	 */
	virtual TSharedPtr<FJsonObject> BuildInputSchema() const = 0;

	/**
	 * The schema property that carries this tool's action enum.
	 * Consulted by startup validation when extracting the published enum.
	 */
	virtual FString GetActionFieldName() const { return TEXT("action"); }
};
