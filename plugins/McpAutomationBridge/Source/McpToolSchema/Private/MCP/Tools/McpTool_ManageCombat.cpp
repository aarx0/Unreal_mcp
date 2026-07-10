// McpTool_ManageCombat.cpp — manage_combat tool definition (39 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageCombat : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_combat"); }

	FString GetDescription() const override
	{
		return TEXT("Scaffold combat data on Blueprints: weapon/combat assets plus "
			"convention-named variables and CDO defaults (damage types, hitboxes, "
			"reload, melee combo/parry/block parameters). Most actions declare data "
			"for game code to consume; they do not implement runtime combat logic.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ManageCombat.cpp). The fold
		// dedups shared params; this facade only owns the cross-cutting 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ManageCombat(),
			TEXT("Combat action to perform"));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("manage_combat")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageCombat);
