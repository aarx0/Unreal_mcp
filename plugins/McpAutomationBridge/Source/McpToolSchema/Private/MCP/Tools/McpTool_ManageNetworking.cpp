// McpTool_ManageNetworking.cpp — manage_networking tool definition (63 actions:
// 27 core networking + 20 game framework + 16 sessions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageNetworking : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_networking"); }

	FString GetDescription() const override
	{
		return TEXT("Multiplayer setup: property replication (conditions, "
			"RepNotify, push model, dormancy, relevancy), RPCs (Server/Client/Multicast), "
			"authority/ownership, network prediction, net driver settings; game framework "
			"classes (GameMode/GameState/PlayerController/PlayerState/GameInstance/HUD, match "
			"and scoring rules); split-screen/local players, LAN host/join, and voice chat.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_ManageNetworking.cpp). The
		// fold dedups shared params; this facade only owns the cross-cutting
		// 'action'. (The Enhanced Input actions moved to the manage_input tool.)
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::ManageNetworking(),
			TEXT("Networking action to perform"));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("manage_networking")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageNetworking);
