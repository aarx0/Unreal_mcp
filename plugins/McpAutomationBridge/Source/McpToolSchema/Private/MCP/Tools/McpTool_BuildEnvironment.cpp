// McpTool_BuildEnvironment.cpp — build_environment tool definition (22 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_BuildEnvironment : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("build_environment"); }

	FString GetDescription() const override
	{
		return TEXT("Create/sculpt landscapes, paint foliage, and generate procedural "
			"terrain/biomes.");
	}

	FString GetCategory() const override { return TEXT("world"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_BuildEnvironment.cpp). The fold
		// dedups shared params; this facade only owns the cross-cutting 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::BuildEnvironment(),
			TEXT("Action"));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("build_environment")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_BuildEnvironment);
