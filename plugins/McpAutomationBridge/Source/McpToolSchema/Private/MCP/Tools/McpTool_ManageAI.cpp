// McpTool_ManageAI.cpp — manage_ai tool definition (60 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageAI : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_ai"); }

	FString GetDescription() const override
	{
		return TEXT("Create AI Controllers, configure Behavior Trees, Blackboards, "
			"EQS queries, perception systems, Behavior Tree graphs, and navigation.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ManageAi.cpp). The fold
		// dedups shared params; this facade only owns the cross-cutting 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ManageAI(),
			TEXT("AI action to perform"));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("manage_ai")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageAI);
