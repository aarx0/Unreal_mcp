// McpTool_ManageAsset.cpp — manage_asset tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageAsset : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_asset"); }

	FString GetDescription() const override
	{
		return TEXT("Create, import, duplicate, rename, delete assets. "
			"Edit Material graphs and instances. Analyze dependencies.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ManageAsset.cpp). The fold
		// dedups shared params; this facade only owns the cross-cutting 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ManageAsset(),
			TEXT("Action to perform"));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("manage_asset")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageAsset);
