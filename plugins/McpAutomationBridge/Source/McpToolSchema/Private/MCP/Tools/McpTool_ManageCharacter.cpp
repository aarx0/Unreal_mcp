// McpTool_ManageCharacter.cpp — manage_character tool definition (27 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageCharacter : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_character"); }

	FString GetDescription() const override
	{
		return TEXT("Create Character Blueprints with movement setup and animation "
			"assets. Advanced gameplay actions scaffold convention-named variables "
			"and defaults; they do not implement runtime logic.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ManageCharacter.cpp). The
		// fold dedups shared params; this facade only owns the cross-cutting
		// 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ManageCharacter(),
			TEXT("Character action to perform."));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("manage_character")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageCharacter);
