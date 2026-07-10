// McpTool_ManageInventory.cpp — manage_inventory tool definition (33 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageInventory : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_inventory"); }

	FString GetDescription() const override
	{
		return TEXT("Create item data assets, world pickups, loot tables, and crafting "
			"recipes. Component/system actions scaffold convention-named variables "
			"and defaults for game code to consume; they do not implement runtime "
			"inventory logic.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ManageInventory.cpp). The fold
		// dedups shared params; this facade only owns the cross-cutting 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ManageInventory(), TEXT("Inventory action to perform."));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("manage_inventory")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageInventory);
