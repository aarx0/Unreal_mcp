// McpTool_ControlEditor.cpp — control_editor tool definition 

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ControlEditor : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("control_editor"); }

	FString GetDescription() const override
	{
		return TEXT("Start/stop PIE, control viewport camera, run console commands, "
			"take screenshots, simulate input.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// Flat fold, same as every other tool. A per-action `oneOf` was piloted
		// here and REVERTED: the MCP client flattens a top-level oneOf by merging
		// branch properties and keeping only the first branch's `action` const, so
		// the model saw a schema whose only action was "play". Server-side branch
		// validation support remains in McpNativeTransport (CollectSchemaViolations)
		// should clients ever render oneOf faithfully.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ControlEditor(),
			TEXT("Action"));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("control_editor")))
		{
			Call->AppendSchema(B);
		}
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ControlEditor);
