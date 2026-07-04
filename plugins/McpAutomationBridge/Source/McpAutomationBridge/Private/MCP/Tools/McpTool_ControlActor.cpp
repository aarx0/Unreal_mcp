// McpTool_ControlActor.cpp — control_actor tool definition (43 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ControlActor : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("control_actor"); }

	FString GetDescription() const override
	{
		return TEXT("Spawn actors, set transforms, enable physics, add components, "
			"manage tags, and attach actors.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ControlActor(), TEXT("Action"))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.Array(TEXT("actorNames"), TEXT("Actor names for bulk actor operations."))
			.String(TEXT("childActor"),
				TEXT("Name of the child actor (for attach/detach operations)."))
			.String(TEXT("parentActor"),
				TEXT("Name of the parent actor (for attach operations)."))
			.String(TEXT("classPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("actorClass"), TEXT("Actor class alias or path."))
			.String(TEXT("className"), TEXT("Actor class name."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("offset"), TEXT("3D offset (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("force"), TEXT("3D vector."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("componentType"), TEXT(""))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.FreeformObject(TEXT("properties"), TEXT(""))
			.String(TEXT("propertyName"), TEXT("Name of the property."))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.Bool(TEXT("visible"), TEXT("Whether the item/actor is visible."))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.String(TEXT("name"),
				TEXT("find_by_name query; alias of actorName for spawn actions."))
			.String(TEXT("tag"), TEXT("Name of the tag."))
			.String(TEXT("matchType"), TEXT("find_by_tag: match mode, "
				"'exact' (default) or 'contains'."))
			.FreeformObject(TEXT("variables"), TEXT(""))
			.String(TEXT("snapshotName"), TEXT(""))
			.Integer(TEXT("limit"), TEXT("Maximum number of actors to return."))
			.String(TEXT("filter"), TEXT("Optional actor label/name filter for list."))
			.Bool(TEXT("collisionEnabled"), TEXT("Whether actor collision is enabled."))
			.String(TEXT("functionName"), TEXT("Name of the function."))
			.Array(TEXT("arguments"), TEXT("Arguments to pass to an actor function."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ControlActor);
