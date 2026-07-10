// McpTool_ManageBlueprint.cpp — canonical manage_blueprint tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageBlueprint : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_blueprint"); }

	FString GetDescription() const override
	{
		return TEXT("Create Blueprints, add SCS components (mesh, collision, camera), "
			"and manipulate graph nodes.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ManageBlueprint.cpp). The fold
		// dedups shared params; this facade only owns the cross-cutting 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ManageBlueprint(),
			TEXT("Blueprint action"));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("manage_blueprint")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageBlueprint);
