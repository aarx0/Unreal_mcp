// McpTool_ManageAsset.cpp — manage_asset tool definition (45 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageAsset : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_asset"); }

	FString GetDescription() const override
	{
		return TEXT("Create, import, duplicate, rename, delete assets. "
			"Edit Material graphs and instances. Analyze dependencies.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
				.StringEnum(TEXT("action"), McpConsolidatedActions::ManageAsset(),
					TEXT("Action to perform"))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("propertyName"), TEXT("set_asset_property: name of the reflected UPROPERTY to write."))
			.Bool(TEXT("includeTransient"), TEXT("get_asset_properties: also dump transient (non-serialized) properties (default false)."))
			.String(TEXT("directory"), TEXT("list: directory to list (default /Game)."))
			.Bool(TEXT("recursive"), TEXT("list: recurse into subfolders (default true)."))
			.Array(TEXT("classNames"), TEXT(""))
			.Array(TEXT("packagePaths"), TEXT(""))
			.Bool(TEXT("recursivePaths"), TEXT(""))
			.Bool(TEXT("recursiveClasses"), TEXT(""))
			.Number(TEXT("limit"), TEXT(""))
			.Number(TEXT("offset"), TEXT(""))
			.String(TEXT("sourcePath"), TEXT("Source path for import/duplicate/rename/move (assetPath also accepted)."))
			.String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
			.Array(TEXT("assetPaths"), TEXT(""))
			.Number(TEXT("lodCount"), TEXT(""))
			.FreeformObject(TEXT("reductionSettings"), TEXT("generate_lods: FMeshReductionSettings overrides applied to every generated LOD — percentTriangles/percentVertices (0-1, replace the progressive 50%/25%/... defaults), maxDeviation, pixelError, weldingThreshold, hardAngleThreshold, baseLODModel, recalculateNormals."))
			.String(TEXT("nodeName"), TEXT("Name identifier."))
			.String(TEXT("eventName"), TEXT("Name of the event."))
			.String(TEXT("memberClass"), TEXT(""))
			.Number(TEXT("posX"), TEXT(""))
			.Number(TEXT("posY"), TEXT(""))
			.String(TEXT("newName"), TEXT("rename/duplicate/move: new asset name, resolved into the source asset's folder (alternative to destinationPath)."))
			.Bool(TEXT("overwrite"), TEXT("Overwrite if the asset/file already exists."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Bool(TEXT("fixupRedirectors"), TEXT(""))
			.String(TEXT("directoryPath"), TEXT("Path to a directory."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("parentMaterial"), TEXT("Material asset path."))
			.FreeformObject(TEXT("parameters"), TEXT(""))
			.Number(TEXT("width"), TEXT(""))
			.Number(TEXT("height"), TEXT(""))
			.String(TEXT("format"), TEXT("import_data_table: 'csv' or 'json' (inferred from sourceText if omitted)."))
				.String(TEXT("rowStruct"), TEXT("create_data_table (required): row struct deriving from FTableRowBase — full path ('/Script/Module.Struct' or '/Game/.../UserStruct') or bare struct name."))
				.String(TEXT("rowName"), TEXT("DataTable row CRUD: the row's key (FName)."))
				.FreeformObject(TEXT("rowData"), TEXT("DataTable add/edit row: JSON object of row-struct fields (partial edit merges)."))
				.String(TEXT("sourceText"), TEXT("import_data_table: the CSV or JSON text to import (replaces all rows)."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("tag"), TEXT("find_by_tag: tag key to match (set_tags metadata or asset-registry tag)."))
			.Bool(TEXT("searchAssets"), TEXT("find_by_tag: match asset registry/metadata tags (default true)."))
			.Bool(TEXT("searchActors"), TEXT("find_by_tag: match level-actor tags (default true)."))
			.Bool(TEXT("searchComponents"), TEXT("find_by_tag: match component tags (default false)."))
			.Number(TEXT("maxResults"), TEXT("find_by_tag: result cap (default 100, max 1000)."))
			.FreeformObject(TEXT("metadata"), TEXT(""))
			.String(TEXT("graphName"), TEXT("Name of the graph."))
			.String(TEXT("nodeType"), TEXT(""))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.String(TEXT("sourceNodeId"), TEXT("ID of the source node."))
			.String(TEXT("targetNodeId"), TEXT("ID of the target node."))
			.String(TEXT("inputName"), TEXT("Name of the pin."))
			.String(TEXT("parameterName"), TEXT("Name of the parameter."))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.Number(TEXT("x"), TEXT(""))
			.Number(TEXT("y"), TEXT(""))
			.String(TEXT("comment"), TEXT(""))
			.String(TEXT("parentNodeId"), TEXT("ID of the node."))
			.String(TEXT("childNodeId"), TEXT("ID of the node."))
			.Number(TEXT("maxDepth"), TEXT(""))
			.String(TEXT("prefix"), TEXT(""))
			.String(TEXT("suffix"), TEXT(""))
			.String(TEXT("searchText"), TEXT(""))
			.String(TEXT("replaceText"), TEXT(""))
			.Array(TEXT("paths"), TEXT(""))
			.String(TEXT("description"), TEXT(""))
			.Bool(TEXT("checkoutFiles"), TEXT(""))
			.Bool(TEXT("showConfirmation"), TEXT(""))
			.String(TEXT("pinName"), TEXT("Name of the pin."))
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.String(TEXT("texturePath"), TEXT("Texture asset path."))
			.String(TEXT("expressionClass"), TEXT(""))
			.Number(TEXT("coordinateIndex"), TEXT(""))
			.String(TEXT("parameterType"), TEXT(""))
			.ArrayOfObjects(TEXT("nodes"), TEXT(""))
			.Array(TEXT("tags"), TEXT(""))
			.String(TEXT("folderPath"), TEXT("Path to a directory."))
			.String(TEXT("type"), TEXT(""))
			.FreeformObject(TEXT("defaultValue"), TEXT("Generic value (any type)."))
			.String(TEXT("expressionIndex"), TEXT("ID of the node."))
			.ArrayOfObjects(TEXT("inputs"), TEXT("add_custom_expression/update_custom_expression: named HLSL input pins, e.g. [{\"name\":\"UV\"}]. Each name becomes a connect_nodes inputName."))
			.ArrayOfObjects(TEXT("additionalOutputs"), TEXT("add_custom_expression/update_custom_expression: extra named outputs beyond the primary return, e.g. [{\"name\":\"Mask\",\"type\":\"Float1\"}]."))
			.Number(TEXT("r"), TEXT("set_node_value: red/X channel of a Constant2/3/4Vector or VectorParameter default."))
			.Number(TEXT("g"), TEXT("set_node_value: green/Y channel."))
			.Number(TEXT("b"), TEXT("set_node_value: blue/Z channel."))
			.Number(TEXT("a"), TEXT("set_node_value: alpha/W channel (Constant4Vector / VectorParameter)."))
			.Number(TEXT("constA"), TEXT("set_node_value: ConstA default of an arithmetic node (Add/Multiply/…) when input A is unconnected."))
			.Number(TEXT("constB"), TEXT("set_node_value: ConstB default of an arithmetic node when input B is unconnected."))
			.Bool(TEXT("waitForShaders"), TEXT("compile_material: block on the async shader workers so backend (not just translation) errors are reported. Slower; off by default."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageAsset);
