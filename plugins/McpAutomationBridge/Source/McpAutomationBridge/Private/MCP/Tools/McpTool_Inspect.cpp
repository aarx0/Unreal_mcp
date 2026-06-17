// McpTool_Inspect.cpp — inspect tool definition (32 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_Inspect : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("inspect"); }

	FString GetDescription() const override
	{
		return TEXT("Inspect any UObject: read/write properties, list components, export snapshots, "
			"and query class info. Actions: inspect_cdo (Blueprint CDO properties + all components "
			"without spawning an actor; use blueprintPath, optional detailed/componentName/propertyNames), "
			"inspect_class (class metadata), inspect_object (world actor), get_property/set_property, "
			"get_components, list_objects, find_by_class, find_by_tag, runtime_report, "
			"ui_focus (CommonUI PIE runtime snapshot: focused widget + path, the activatable "
			"owning focus + its desired-focus target, active activatable stack, current input "
			"type, and the active root's bound actions), find_objects (loaded-object discovery: "
			"className + optional pathContains substring + propertyNames readback — use for "
			"widget-tree template subobjects, slot objects, and live PIE widget state), "
				"diff_asset (structural diff of two on-disk .uasset versions: oldFilePath vs "
				"newFilePath — parent/interfaces/components/variables/graphs + CDO defaults + "
				"gasSignals).");
	}

	FString GetCategory() const override { return TEXT("core"); }

	// Pattern A: registered as "inspect" in O(1 map
	// Default GetDispatchAction() returns GetName() — correct

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("inspect_object"),
				TEXT("get_actor_details"),
				TEXT("get_blueprint_details"),
				TEXT("get_mesh_details"),
				TEXT("get_texture_details"),
				TEXT("get_material_details"),
				TEXT("get_level_details"),
				TEXT("get_component_details"),
				TEXT("set_property"),
				TEXT("get_property"),
				TEXT("get_components"),
				TEXT("get_component_property"),
				TEXT("set_component_property"),
				TEXT("inspect_class"),
				TEXT("inspect_cdo"),
				TEXT("diff_asset"),
				TEXT("runtime_report"),
				TEXT("pie_report"),
				TEXT("ui_focus"),
				TEXT("find_objects"),
				TEXT("list_objects"),
				TEXT("get_metadata"),
				TEXT("add_tag"),
				TEXT("find_by_tag"),
				TEXT("create_snapshot"),
				TEXT("restore_snapshot"),
				TEXT("export"),
				TEXT("delete_object"),
				TEXT("find_by_class"),
				TEXT("get_bounding_box"),
				TEXT("get_project_settings"),
				TEXT("get_world_settings"),
				TEXT("get_viewport_info"),
				TEXT("get_selected_actors"),
				TEXT("get_scene_stats"),
				TEXT("get_performance_stats"),
				TEXT("get_memory_stats"),
				TEXT("get_editor_settings")
			}, TEXT("Action"))
			.String(TEXT("objectPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("propertyName"), TEXT("Name of the property."))
			.String(TEXT("propertyPath"), TEXT(""))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.String(TEXT("className"), TEXT(""))
			.String(TEXT("classPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("tag"), TEXT("Name of the tag."))
			.String(TEXT("filter"), TEXT(""))
			.String(TEXT("snapshotName"), TEXT(""))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.Bool(TEXT("detailed"), TEXT(""))
			.String(TEXT("pathContains"), TEXT("find_objects: case-insensitive substring filter on the object's full path."))
			.Bool(TEXT("exactClass"), TEXT("find_objects: require the exact class (default false = IsA semantics)."))
			.Bool(TEXT("includeCdo"), TEXT("find_objects: include class default objects (default false)."))
			.Integer(TEXT("limit"), TEXT("find_objects: max objects returned (default 50, cap 200)."))
			.Array(TEXT("propertyNames"), TEXT(""))
			.Array(TEXT("componentNames"), TEXT("Component names to include detailed property readback for."))
			.String(TEXT("oldFilePath"), TEXT("diff_asset: filesystem path to the OLD .uasset version (e.g. a git revision extracted to a temp file)."))
			.String(TEXT("newFilePath"), TEXT("diff_asset: filesystem path to the NEW .uasset version (e.g. the working-tree file)."))
			.String(TEXT("assetName"), TEXT("diff_asset: object name inside the package (default = newFilePath filename stem)."))
			.Bool(TEXT("includeDefaults"), TEXT("diff_asset: also diff CDO default properties (default true)."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_Inspect);
