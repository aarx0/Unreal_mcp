// McpTool_ManageEffect.cpp — manage_effect tool definition 

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageEffect : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_effect"); }

	FString GetDescription() const override
	{
		return TEXT("Niagara particle systems, VFX, debug shapes, and GPU simulations. "
			"Create systems, emitters, modules, and control particle effects.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }


	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ManageEffect.cpp). The fold
		// dedups shared params; this facade only owns the cross-cutting 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ManageEffect(), TEXT("Effect/Niagara action to perform."));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("manage_effect")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageEffect);
