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
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ControlEditor.cpp). The fold
		// dedups shared params; this facade only owns the cross-cutting 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ControlEditor(), TEXT("Editor action. screenshot captures the active "
			"viewport synchronously; the PNG is written to the returned "
			"absolute 'path' before the call returns, so it can be read back "
			"immediately to evaluate the result."));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("control_editor")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ControlEditor);
