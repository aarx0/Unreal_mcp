// McpTool_ManageGeometry.cpp — manage_geometry tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageGeometry : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_geometry"); }

	FString GetDescription() const override
	{
		return TEXT("Create procedural meshes using Geometry Script: booleans, "
			"deformers, UVs, collision, and LOD generation.");
	}

	FString GetCategory() const override { return TEXT("world"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ManageGeometry.cpp). The fold
		// dedups shared params; this facade only owns the cross-cutting 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ManageGeometry(), TEXT("Geometry action to perform"));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("manage_geometry")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageGeometry);
