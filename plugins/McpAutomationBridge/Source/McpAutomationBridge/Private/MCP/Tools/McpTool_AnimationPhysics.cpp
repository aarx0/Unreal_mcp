// McpTool_AnimationPhysics.cpp — animation_physics tool definition (85 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_AnimationPhysics : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("animation_physics"); }

	FString GetDescription() const override
	{
		return TEXT("Create animation blueprints, blend spaces, montages, "
			"state machines, Control Rig, IK rigs, ragdolls, and vehicle physics.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }


	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_AnimationPhysics.cpp). The
		// fold dedups shared params; this facade only owns the cross-cutting
		// 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::AnimationPhysics(), TEXT("Action"));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("animation_physics")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_AnimationPhysics);
