// McpTool_ManageLevelStructure.cpp — manage_level_structure tool definition (17 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageLevelStructure : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_level_structure"); }

	FString GetDescription() const override
	{
		return TEXT("Create levels and sublevels. Configure World Partition, streaming, "
			"data layers, HLOD, and level instances.");
	}

	FString GetCategory() const override { return TEXT("world"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ManageLevelStructure.cpp).
		// The fold dedups shared params; this facade only owns the cross-cutting
		// 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ManageLevelStructure(),
			TEXT("Level structure action to perform."));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("manage_level_structure")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageLevelStructure);
