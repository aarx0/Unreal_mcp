// McpTool_ManageInput.cpp — manage_input tool definition (9 Enhanced Input actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageInput : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_input"); }

	FString GetDescription() const override
	{
		return TEXT("Enhanced Input authoring: InputActions, InputMappingContexts, key "
			"mappings, triggers, and modifiers.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
				.StringEnum(TEXT("action"), McpConsolidatedActions::ManageInput(),
					TEXT("Input action to perform"))
			// Enhanced Input authoring params (moved verbatim from manage_networking).
			// Handlers read these from the payload regardless; declared here so the params
			// are discoverable / passable through a schema-validating client.
			.String(TEXT("name"), TEXT("Asset name (create_input_action / create_input_mapping_context)."))
			.String(TEXT("path"), TEXT("Destination folder for a created input asset."))
			.String(TEXT("assetPath"), TEXT("Input asset to read (get_info)."))
			.StringEnum(TEXT("valueType"), {
				TEXT("Boolean"), TEXT("Axis1D"), TEXT("Axis2D"), TEXT("Axis3D")
			}, TEXT("InputAction value type (create_input_action; default Boolean)."))
			.String(TEXT("actionPath"), TEXT("InputAction asset path (mappings / triggers)."))
			.String(TEXT("contextPath"), TEXT("InputMappingContext asset path (add_mapping / remove_mapping)."))
			.String(TEXT("key"), TEXT("Key id for a mapping (e.g. SpaceBar, Gamepad_FaceButton_Bottom)."))
			.StringEnum(TEXT("triggerType"), {
				TEXT("Pressed"), TEXT("Released"), TEXT("Down"), TEXT("Tap"), TEXT("Hold"),
				TEXT("HoldAndRelease"), TEXT("Pulse"), TEXT("RepeatedTap")
			}, TEXT("Input trigger class (set_input_trigger)."))
			.String(TEXT("modifierType"), TEXT("Input modifier class (set_input_modifier, e.g. Negate, SwizzleAxis)."))
			.Bool(TEXT("replace"), TEXT("set_input_trigger: clear existing triggers before adding (default false = idempotent dedupe)."))
			.Number(TEXT("priority"), TEXT("enable_input_mapping: mapping context priority (default 0)."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageInput);
