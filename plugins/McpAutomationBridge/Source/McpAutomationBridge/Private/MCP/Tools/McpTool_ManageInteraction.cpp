// McpTool_ManageInteraction.cpp — manage_interaction tool definition (22 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageInteraction : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_interaction"); }

	FString GetDescription() const override
	{
		return TEXT("Create interactive objects: doors, switches, chests, levers. "
			"Set up destructible meshes and trigger volumes.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ManageInteraction(), TEXT("The interaction action to perform."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("folder"), TEXT("Destination folder for create_* actions ('path'/'savePath' accepted as aliases; default /Game/Interactables)."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.StringEnum(TEXT("traceType"), {
				TEXT("line"),
				TEXT("sphere"),
				TEXT("box")
			}, TEXT("Type of interaction trace."))
			.Number(TEXT("traceDistance"), TEXT("Trace distance."))
			.Number(TEXT("traceRadius"), TEXT("Trace radius."))
			.String(TEXT("widgetClass"), TEXT("Widget class path."))
			.Bool(TEXT("showOnHover"), TEXT("Show widget when hovering."))
			.Bool(TEXT("showPromptText"), TEXT("Show interaction prompt text."))
			.String(TEXT("promptTextFormat"), TEXT("Format string for prompt (e.g., \"Press {Key} to {Action}\")."))
			.String(TEXT("doorPath"), TEXT("Path to door actor blueprint."))
			.Number(TEXT("openAngle"), TEXT("Door open rotation angle in degrees."))
			.Number(TEXT("openTime"), TEXT("Time to open/close door in seconds."))
			.Bool(TEXT("locked"), TEXT("Whether the item is locked."))
			.Bool(TEXT("autoClose"), TEXT("Automatically close after opening."))
			.Number(TEXT("autoCloseDelay"), TEXT("Delay before auto-close in seconds."))
			.Bool(TEXT("requiresKey"), TEXT("Whether interaction requires a key item."))
			.String(TEXT("switchPath"), TEXT("Path to switch actor blueprint."))
			.StringEnum(TEXT("switchType"), {
				TEXT("button"),
				TEXT("lever"),
				TEXT("pressure_plate"),
				TEXT("toggle")
			}, TEXT("Type of switch."))
			.Bool(TEXT("canToggle"), TEXT("Whether switch can be toggled."))
			.Number(TEXT("resetTime"), TEXT("Time to reset switch in seconds."))
			.String(TEXT("chestPath"), TEXT("Path to chest actor blueprint."))
			.String(TEXT("lootTablePath"), TEXT("Path to loot table asset."))
			.String(TEXT("triggerPath"), TEXT("Path to trigger actor blueprint."))
			.StringEnum(TEXT("triggerShape"), {
				TEXT("box"),
				TEXT("sphere"),
				TEXT("capsule")
			}, TEXT("Shape of trigger volume."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageInteraction);
