// McpTool_ManageInteraction.cpp — manage_interaction tool definition (22 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageInteraction : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_interaction"); }

	FString GetDescription() const override
	{
		return TEXT("Create interactive objects: doors, switches, chests, levers. "
			"Set up destructible meshes and trigger volumes.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ManageInteraction.cpp). The
		// fold dedups shared params; this facade only owns the cross-cutting 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ManageInteraction(),
			TEXT("The interaction action to perform."));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("manage_interaction")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageInteraction);
