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
		// Phase 3 pilot — per-action `oneOf` (vs the flat fold every other tool
		// still uses). One branch per action, each advertising EXACTLY that action's
		// params (from its AppendSchema fragment) plus its real required set, keyed
		// on an `action` const discriminator. control_editor is fully classed, so
		// CallsForTool covers every action. If the client honors a top-level oneOf,
		// the other 21 tools convert the same way; if not, revert to the flat fold.
		TArray<TSharedPtr<FJsonValue>> Branches;
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("control_editor")))
		{
			FMcpSchemaBuilder Bi;
			Bi.StringConst(TEXT("action"), Call->GetDecl().Action);
			Bi.Required({TEXT("action")});
			Call->AppendSchema(Bi);   // the action's own params + their Required()
			Branches.Add(MakeShared<FJsonValueObject>(Bi.Build()));
		}
		auto Schema = MakeShared<FJsonObject>();
		Schema->SetStringField(TEXT("type"), TEXT("object"));
		Schema->SetArrayField(TEXT("oneOf"), Branches);
		return Schema;
	}
};

MCP_REGISTER_TOOL(FMcpTool_ControlEditor);
