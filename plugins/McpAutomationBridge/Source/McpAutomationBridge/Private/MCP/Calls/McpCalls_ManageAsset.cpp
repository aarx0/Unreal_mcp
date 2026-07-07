// LINT-TOOL: manage_asset
// LINT-SCHEMA-DERIVED
// manage_asset as FMcpCall classes — schema-from-decls
// (docs/action-declarations.md). Each class AUTHORS its schema fragment in a
// S_<Suffix>() function; the published facade schema folds those fragments and
// GetDecl() derives the validation decl from the same fragment via
// McpDeriveDecl(), so schema and decl are one source and cannot drift. Run()
// delegates to the subsystem member handlers — Core actions to their existing
// AssetWorkflow/AssetQuery members, MaterialAuthoring actions to HandleMaterial*
// members (MaterialAuthoringHandlers.cpp), Texture actions to HandleTexture*
// members (TextureHandlers.cpp) — until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageAsset
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action's
// handler reads (the fold dedups shared params to one entry). Builder method +
// description are the tool's authored help text, copied verbatim; McpDeriveDecl()
// reads the param kinds + required-set back out to build the transport
// validation decl. Ported from this family's retired P_* param arrays, then
// drift-corrected against the live handler bodies: the five mega-bag actions
// (search_assets / create_thumbnail / create_render_target / save / save_all)
// dropped their union residue down to the keys their handler actually reads;
// rename dropped levelPath/overwrite; get_dependencies dropped
// includeSoftDependencies; exists dropped the path-resolver residue; list gained
// pagination (its handler reads it, the facade lacked it); create_render_target
// gained format and dropped packagePath. The dead advertised param
// luminanceFactors is carried on desaturate (allowlisted, kept per request);
// targetPin (mega-bag-only, no handler read, not allowlisted) is dropped. Shared
// param names author an identical builder call in every fragment so the fold is
// unambiguous.

static void S_List(FMcpSchemaBuilder& B)
{
	B.String(TEXT("filter"), TEXT("get_material_info: what to include — 'parameters'|'expressions'|'connections'|'all' (default all)."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("directory"), TEXT("list: directory to list (default /Game)."))
	 .String(TEXT("directoryPath"), TEXT("Path to a directory."))
	 .Bool(TEXT("recursive"), TEXT("list: recurse into subfolders (default true)."))
	 .Bool(TEXT("recursivePaths"), TEXT(""))
	 .Object(TEXT("pagination"), TEXT("list: pagination window."),
		[](FMcpSchemaBuilder& S) { S.Integer(TEXT("offset")).Integer(TEXT("limit")); })
	 .Number(TEXT("depth"), TEXT("list: recursion depth filter relative to the queried path. get_node_connections: BFS hop limit (default 1; -1 unlimited)."));
}

static void S_Import(FMcpSchemaBuilder& B)
{
	B.String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
	 .String(TEXT("sourcePath"), TEXT("Source path for import/duplicate/rename/move (assetPath also accepted)."))
	 .Required({TEXT("destinationPath"), TEXT("sourcePath")});
}

static void S_Duplicate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sourcePath"), TEXT("Source path for import/duplicate/rename/move (assetPath also accepted)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
	 .String(TEXT("newName"), TEXT("rename/duplicate/move: new asset name, resolved into the source asset's folder (alternative to destinationPath)."))
	 .Required({TEXT("sourcePath"), TEXT("assetPath"), TEXT("destinationPath"), TEXT("newName")});
}

static void S_Rename(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sourcePath"), TEXT("Source path for import/duplicate/rename/move (assetPath also accepted)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
	 .String(TEXT("newName"), TEXT("rename/duplicate/move: new asset name, resolved into the source asset's folder (alternative to destinationPath)."));
}

static void S_Move(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sourcePath"), TEXT("Source path for import/duplicate/rename/move (assetPath also accepted)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
	 .String(TEXT("newName"), TEXT("rename/duplicate/move: new asset name, resolved into the source asset's folder (alternative to destinationPath)."))
	 .Required({TEXT("sourcePath"), TEXT("assetPath"), TEXT("destinationPath"), TEXT("newName")});
}

static void S_Delete(FMcpSchemaBuilder& B)
{
	B.Array(TEXT("paths"), TEXT(""))
	 .Array(TEXT("assetPaths"), TEXT(""))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("paths"), TEXT("assetPaths"), TEXT("path"), TEXT("assetPath")});
}

static void S_CreateFolder(FMcpSchemaBuilder& B)
{
	B.String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("directoryPath"), TEXT("Path to a directory."))
	 .Required({TEXT("path"), TEXT("directoryPath")});
}

static void S_SearchAssets(FMcpSchemaBuilder& B)
{
	B.Array(TEXT("classNames"), TEXT(""))
	 .Array(TEXT("packagePaths"), TEXT(""))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("searchText"), TEXT(""))
	 .Bool(TEXT("recursivePaths"), TEXT(""))
	 .Bool(TEXT("recursiveClasses"), TEXT(""))
	 .Number(TEXT("offset"), TEXT(""))
	 .Number(TEXT("limit"), TEXT(""));
}

static void S_GetDependencies(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("recursive"), TEXT("list: recurse into subfolders (default true)."))
	 .Required({TEXT("assetPath")});
}

static void S_GetReferencers(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_GetAssetProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("includeTransient"), TEXT("get_asset_properties: also dump transient (non-serialized) properties (default false)."))
	 .String(TEXT("propertyName"), TEXT("set_asset_property: name of the reflected UPROPERTY to write."))
	 .Required({TEXT("assetPath")});
}

static void S_SetAssetProperty(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("propertyName"), TEXT("set_asset_property: reflected UPROPERTY name or dotted/indexed path (e.g. Mappings[0].Triggers[0])."))
	 // Discriminated value: populate exactly ONE typed field matching the target
	 // property's reflected type. structValue/arrayValue are the escape for structs,
	 // instanced subobjects ({"__class",...}) and arrays; ApplyJsonValueToProperty
	 // is the gate for whether the value actually fits the resolved property.
	 .Bool(TEXT("boolValue"), TEXT("Set a bool property."))
	 .Number(TEXT("intValue"), TEXT("Set an integer property."))
	 .Number(TEXT("floatValue"), TEXT("Set a float/double property."))
	 .String(TEXT("stringValue"), TEXT("Set a string / name / text / enum / object-reference-by-path property."))
	 .Object(TEXT("colorValue"), TEXT("Set an FLinearColor/FColor property (r,g,b,a, 0..1)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Object(TEXT("vectorValue"), TEXT("Set an FVector property (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("structValue"), TEXT("Set a struct / instanced subobject ({\"__class\",...}) / map property."))
	 .Array(TEXT("arrayValue"), TEXT("Set an array property (items are structs/instanced objects)."), TEXT("object"))
	 .Required({TEXT("assetPath"), TEXT("propertyName")});
}

static void S_GetSourceControlState(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Array(TEXT("assetPaths"), TEXT(""))
	 .Required({TEXT("assetPath"), TEXT("assetPaths")});
}

static void S_AnalyzeGraph(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."))
	 .Required({TEXT("assetPath"), TEXT("materialPath")});
}

static void S_GetAssetGraph(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("maxDepth"), TEXT(""))
	 .Required({TEXT("assetPath")});
}

static void S_CreateThumbnail(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("width"), TEXT(""))
	 .Number(TEXT("height"), TEXT(""))
	 .String(TEXT("outputPath"), TEXT("create_thumbnail/generate_report: file path (relative to project dir) to write output to. channel_extract: destination texture path."))
	 .Required({TEXT("assetPath")});
}

static void S_SetTags(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Array(TEXT("tags"), TEXT(""))
	 .Required({TEXT("assetPath")});
}

static void S_GetMetadata(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_SetMetadata(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Object(TEXT("metadata"), TEXT(""))
	 .Required({TEXT("assetPath")});
}

static void S_Validate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_FixupRedirectors(FMcpSchemaBuilder& B)
{
	B.String(TEXT("directoryPath"), TEXT("Path to a directory."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("checkoutFiles"), TEXT(""))
	 .Required({TEXT("directoryPath"), TEXT("path")});
}

static void S_FindByTag(FMcpSchemaBuilder& B)
{
	B.String(TEXT("tag"), TEXT("find_by_tag: tag key to match (set_tags metadata or asset-registry tag)."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Number(TEXT("maxResults"), TEXT("find_by_tag: result cap (default 100, max 1000)."))
	 .Bool(TEXT("searchActors"), TEXT("find_by_tag: match level-actor tags (default true)."))
	 .Bool(TEXT("searchComponents"), TEXT("find_by_tag: match component tags (default false)."))
	 .Bool(TEXT("searchAssets"), TEXT("find_by_tag: match asset registry/metadata tags (default true)."))
	 .Required({TEXT("tag")});
}

static void S_GenerateReport(FMcpSchemaBuilder& B)
{
	B.String(TEXT("directory"), TEXT("list: directory to list (default /Game)."))
	 .String(TEXT("reportType"), TEXT("generate_report: report kind, e.g. 'Summary' (default)."))
	 .String(TEXT("outputPath"), TEXT("create_thumbnail/generate_report: file path (relative to project dir) to write output to. channel_extract: destination texture path."));
}

static void S_CreateDataTable(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("rowStruct"), TEXT("create_data_table (required): row struct deriving from FTableRowBase — full path ('/Script/Module.Struct' or '/Game/.../UserStruct') or bare struct name."))
	 .String(TEXT("rowStructPath"), TEXT("create_data_table: alias for rowStruct."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name"), TEXT("path"), TEXT("rowStruct"), TEXT("rowStructPath")});
}

static void S_AddDataTableRow(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("dataTablePath"), TEXT("DataTable row CRUD: alias for assetPath (the DataTable)."))
	 .String(TEXT("tablePath"), TEXT("DataTable row CRUD: alias for assetPath (the DataTable)."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("rowName"), TEXT("DataTable row CRUD: the row's key (FName)."))
	 .Object(TEXT("rowData"), TEXT("DataTable add/edit row: JSON object of row-struct fields (partial edit merges)."))
	 .Required({TEXT("rowName")});
}

static void S_EditDataTableRow(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("dataTablePath"), TEXT("DataTable row CRUD: alias for assetPath (the DataTable)."))
	 .String(TEXT("tablePath"), TEXT("DataTable row CRUD: alias for assetPath (the DataTable)."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("rowName"), TEXT("DataTable row CRUD: the row's key (FName)."))
	 .Object(TEXT("rowData"), TEXT("DataTable add/edit row: JSON object of row-struct fields (partial edit merges)."))
	 .Required({TEXT("rowName")});
}

static void S_RemoveDataTableRow(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("dataTablePath"), TEXT("DataTable row CRUD: alias for assetPath (the DataTable)."))
	 .String(TEXT("tablePath"), TEXT("DataTable row CRUD: alias for assetPath (the DataTable)."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("rowName"), TEXT("DataTable row CRUD: the row's key (FName)."))
	 .Required({TEXT("rowName")});
}

static void S_GetDataTableRows(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("dataTablePath"), TEXT("DataTable row CRUD: alias for assetPath (the DataTable)."))
	 .String(TEXT("tablePath"), TEXT("DataTable row CRUD: alias for assetPath (the DataTable)."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."));
}

static void S_ImportDataTable(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("dataTablePath"), TEXT("DataTable row CRUD: alias for assetPath (the DataTable)."))
	 .String(TEXT("tablePath"), TEXT("DataTable row CRUD: alias for assetPath (the DataTable)."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("sourceText"), TEXT("import_data_table: the CSV or JSON text to import (replaces all rows)."))
	 .String(TEXT("csv"), TEXT("import_data_table: alias for sourceText (CSV)."))
	 .String(TEXT("json"), TEXT("import_data_table: alias for sourceText (JSON)."))
	 .String(TEXT("content"), TEXT("import_data_table: alias for sourceText."))
	 .String(TEXT("format"), TEXT("import_data_table: 'csv' or 'json' (inferred from sourceText if omitted)."));
}

static void S_CreateRenderTarget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("renderTargetPath"), TEXT("create_render_target: full asset path (alternative to name+path)."))
	 .Number(TEXT("width"), TEXT(""))
	 .Number(TEXT("height"), TEXT(""))
	 .String(TEXT("format"), TEXT("import_data_table: 'csv' or 'json' (inferred from sourceText if omitted)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_GenerateLods(FMcpSchemaBuilder& B)
{
	B.String(TEXT("landscapePath"), TEXT("generate_lods: landscape asset path (alternative to assetPath/assetPaths)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Array(TEXT("assetPaths"), TEXT(""))
	 .Array(TEXT("assets"), TEXT("generate_lods: alias for assetPaths."))
	 .Number(TEXT("lodCount"), TEXT(""))
	 .Number(TEXT("numLODs"), TEXT("generate_lods: alias for lodCount."))
	 .Object(TEXT("reductionSettings"), TEXT("generate_lods: FMeshReductionSettings overrides for every generated LOD (percentTriangles/percentVertices 0-1 replace the progressive defaults)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("percentTriangles")).Number(TEXT("percentVertices")).Number(TEXT("maxDeviation")).Number(TEXT("pixelError")).Number(TEXT("weldingThreshold")).Number(TEXT("hardAngleThreshold")).Integer(TEXT("baseLODModel")).Bool(TEXT("recalculateNormals")); });
}

static void S_AddMaterialParameter(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("type"), TEXT("scalar | vector | texture | staticswitch."))
	 // Default value, typed per `type`: scalar->floatValue, vector->colorValue,
	 // texture->stringValue (asset path), staticswitch->boolValue.
	 .Number(TEXT("floatValue"), TEXT("type=scalar: default scalar value."))
	 .Object(TEXT("colorValue"), TEXT("type=vector: default RGBA color (0..1)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .String(TEXT("stringValue"), TEXT("type=texture: default texture asset path."))
	 .Bool(TEXT("boolValue"), TEXT("type=staticswitch: default switch value."))
	 .Required({TEXT("assetPath"), TEXT("name"), TEXT("type")});
}

static void S_ListInstances(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_ResetInstanceParameters(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_Exists(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_GetMaterialStats(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_NaniteRebuildMesh(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("meshPath"), TEXT("Mesh asset path."))
	 .Bool(TEXT("enableNanite"), TEXT("nanite_rebuild_mesh: enable Nanite virtualized geometry (default true)."))
	 .Bool(TEXT("preserveArea"), TEXT("nanite_rebuild_mesh: preserve surface area during Nanite fallback mesh reduction (default true)."))
	 .Number(TEXT("trianglePercent"), TEXT("nanite_rebuild_mesh: Nanite fallback mesh triangle percent 0-100 (default 100)."))
	 .Number(TEXT("fallbackPercent"), TEXT("nanite_rebuild_mesh: Nanite fallback relative error percent 0-100 (default 0)."))
	 .Required({TEXT("assetPath"), TEXT("meshPath")});
}

static void S_BulkRename(FMcpSchemaBuilder& B)
{
	B.String(TEXT("prefix"), TEXT(""))
	 .String(TEXT("suffix"), TEXT(""))
	 .String(TEXT("searchText"), TEXT(""))
	 .String(TEXT("replaceText"), TEXT(""))
	 .Bool(TEXT("checkoutFiles"), TEXT(""))
	 .Array(TEXT("assetPaths"), TEXT(""))
	 .String(TEXT("folderPath"), TEXT("Path to a directory."))
	 .Required({TEXT("assetPaths"), TEXT("folderPath")});
}

static void S_BulkDelete(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("showConfirmation"), TEXT(""))
	 .Bool(TEXT("fixupRedirectors"), TEXT(""))
	 .Array(TEXT("assetPaths"), TEXT(""))
	 .String(TEXT("folderPath"), TEXT("Path to a directory."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("pattern"), TEXT("bulk_delete: substring filter applied to matching asset names when deleting by folderPath."))
	 .Required({TEXT("assetPaths"), TEXT("folderPath"), TEXT("path")});
}

static void S_SourceControlCheckout(FMcpSchemaBuilder& B)
{
	B.Array(TEXT("assetPaths"), TEXT(""))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPaths"), TEXT("assetPath")});
}

static void S_SourceControlSubmit(FMcpSchemaBuilder& B)
{
	B.Array(TEXT("assetPaths"), TEXT(""))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("description"), TEXT(""))
	 .Required({TEXT("assetPaths"), TEXT("assetPath")});
}

static void S_Save(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_SaveAll(FMcpSchemaBuilder&) {}

static void S_CreateMaterial(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("materialDomain"), TEXT("create_material/set_material_domain: Surface|DeferredDecal|LightFunction|Volume|PostProcess|UI."))
	 .String(TEXT("blendMode"), TEXT("create_material/set_blend_mode: Opaque|Masked|Translucent|Additive|Modulate|AlphaComposite|AlphaHoldout. combine_textures: Normal|Multiply|Screen|Overlay|Add."))
	 .String(TEXT("shadingModel"), TEXT("create_material/set_shading_model: Unlit|DefaultLit|Subsurface|SubsurfaceProfile|PreintegratedSkin|ClearCoat|Hair|Cloth|Eye|TwoSidedFoliage|ThinTranslucent."))
	 .Bool(TEXT("twoSided"), TEXT("create_material/set_two_sided: render both polygon faces."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_SetBlendMode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("blendMode"), TEXT("create_material/set_blend_mode: Opaque|Masked|Translucent|Additive|Modulate|AlphaComposite|AlphaHoldout. combine_textures: Normal|Multiply|Screen|Overlay|Add."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("blendMode")});
}

static void S_SetShadingModel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("shadingModel"), TEXT("create_material/set_shading_model: Unlit|DefaultLit|Subsurface|SubsurfaceProfile|PreintegratedSkin|ClearCoat|Hair|Cloth|Eye|TwoSidedFoliage|ThinTranslucent."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("shadingModel")});
}

static void S_SetMaterialDomain(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("materialDomain"), TEXT("create_material/set_material_domain: Surface|DeferredDecal|LightFunction|Volume|PostProcess|UI."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("materialDomain")});
}

static void S_AddTextureSample(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("texturePath"), TEXT("Texture asset path."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .String(TEXT("samplerType"), TEXT("add_texture_sample: Color|LinearColor|Normal|Masks|Alpha (default Color)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddTextureCoordinate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Number(TEXT("coordinateIndex"), TEXT(""))
	 .Number(TEXT("uTiling"), TEXT("add_texture_coordinate: horizontal tiling (default 1)."))
	 .Number(TEXT("vTiling"), TEXT("add_texture_coordinate: vertical tiling (default 1)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddScalarParameter(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .Number(TEXT("floatValue"), TEXT("Default value for the new scalar parameter (optional)."))
	 .String(TEXT("group"), TEXT("add_scalar_parameter/add_vector_parameter/add_static_switch_parameter: UI group name."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("parameterName")});
}

static void S_AddVectorParameter(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .String(TEXT("group"), TEXT("add_scalar_parameter/add_vector_parameter/add_static_switch_parameter: UI group name."))
	 .Object(TEXT("colorValue"), TEXT("Default RGBA color, each channel 0..1 (optional)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("parameterName")});
}

static void S_AddStaticSwitchParameter(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .Bool(TEXT("boolValue"), TEXT("Default value for the new static switch parameter (optional)."))
	 .String(TEXT("group"), TEXT("add_scalar_parameter/add_vector_parameter/add_static_switch_parameter: UI group name."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("parameterName")});
}

static void S_AddMathNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("operation"), TEXT("add_math_node: Add|Subtract|Multiply|Divide|Lerp|Clamp|Power|Frac|OneMinus|Append."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("operation")});
}

static void S_AddWorldPosition(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddVertexNormal(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddPixelDepth(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddFresnel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddReflectionVector(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddPanner(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddRotator(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddNoise(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddVoronoi(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddIf(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddSwitch(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddCustomExpression(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("code"), TEXT("add_custom_expression/update_custom_expression: HLSL source code body."))
	 .String(TEXT("outputType"), TEXT("add_custom_expression/update_custom_expression: Float1|Float2|Float3|Float4|MaterialAttributes (default Float1)."))
	 .String(TEXT("description"), TEXT(""))
	 .ArrayOfObjects(TEXT("inputs"), TEXT("add_custom_expression/update_custom_expression: named HLSL input pins, e.g. [{\"name\":\"UV\"}]. Each name becomes a connect_nodes inputName."))
	 .ArrayOfObjects(TEXT("additionalOutputs"), TEXT("add_custom_expression/update_custom_expression: extra named outputs beyond the primary return, e.g. [{\"name\":\"Mask\",\"type\":\"Float1\"}]."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("code")});
}

static void S_ConnectNodes(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("sourceNodeId"), TEXT("ID of the source node."))
	 .String(TEXT("targetNodeId"), TEXT("ID of the target node."))
	 .String(TEXT("inputName"), TEXT("Name of the pin."))
	 .String(TEXT("sourcePin"), TEXT("connect_nodes: named output pin on the source node (e.g. an MF call output); defaults to output 0."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("sourceNodeId")});
}

static void S_DisconnectNodes(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("pinName"), TEXT("Name of the pin."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_CreateMaterialFunction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("description"), TEXT(""))
	 .Bool(TEXT("exposeToLibrary"), TEXT("create_material_function: show in the Material Function Library picker (default true)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_AddFunctionInput(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("inputName"), TEXT("Name of the pin."))
	 .String(TEXT("inputType"), TEXT("add_function_input/add_function_output: Float1|Float2|Float3|Float4|Texture2D|TextureCube|Bool|MaterialAttributes."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("functionPath"), TEXT("use_material_function: material function asset path to call."))
	 .Required({TEXT("assetPath"), TEXT("inputName")});
}

static void S_AddFunctionOutput(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("inputName"), TEXT("Name of the pin."))
	 .String(TEXT("inputType"), TEXT("add_function_input/add_function_output: Float1|Float2|Float3|Float4|Texture2D|TextureCube|Bool|MaterialAttributes."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("inputName")});
}

static void S_UseMaterialFunction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("functionPath"), TEXT("use_material_function: material function asset path to call."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("functionPath")});
}

static void S_GetMaterialFunctionInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_CreateMaterialInstance(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("parentMaterial"), TEXT("Material asset path."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name"), TEXT("parentMaterial")});
}

static void S_SetScalarParameterValue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .Number(TEXT("floatValue"), TEXT("Scalar value to set."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("parameterName"), TEXT("floatValue")});
}

static void S_SetVectorParameterValue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .Object(TEXT("colorValue"), TEXT("RGBA color, each channel 0..1."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("parameterName"), TEXT("colorValue")});
}

static void S_SetTextureParameterValue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .String(TEXT("texturePath"), TEXT("Texture asset path."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("parameterName"), TEXT("texturePath")});
}

static void S_CreateLandscapeMaterial(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_CreateDecalMaterial(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_CreatePostProcessMaterial(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_AddLandscapeLayer(FMcpSchemaBuilder& B)
{
	B.String(TEXT("layerName"), TEXT("add_landscape_layer: landscape layer name."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Number(TEXT("hardness"), TEXT("add_landscape_layer: layer info hardness 0-1 (default 0.5)."))
	 .String(TEXT("physicalMaterialPath"), TEXT("add_landscape_layer: optional physical material asset path."))
	 .Bool(TEXT("noWeightBlend"), TEXT("add_landscape_layer: disable weight-blending for this layer (default false)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("layerName")});
}

static void S_ConfigureLayerBlend(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."))
	 .ArrayOfObjects(TEXT("layers"), TEXT("configure_layer_blend: layers to add, e.g. [{\"name\":\"Rock\",\"blendType\":\"...\"}]."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("layers")});
}

static void S_CompileMaterial(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("waitForShaders"), TEXT("compile_material: block on the async shader workers so backend (not just translation) errors are reported. Slower; off by default."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_GetMaterialInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("filter"), TEXT("get_material_info: what to include — 'parameters'|'expressions'|'connections'|'all' (default all)."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Array(TEXT("nodeIds"), TEXT("delete_node: batch-delete multiple node IDs (alternative to nodeId)."))
	 .Required({TEXT("assetPath")});
}

static void S_FindNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("nodeType"), TEXT(""))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .Required({TEXT("assetPath")});
}

static void S_GetNodeConnections(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("direction"), TEXT("get_node_connections: inputs|outputs|both (default both)."))
	 .Number(TEXT("depth"), TEXT("list: recursion depth filter relative to the queried path. get_node_connections: BFS hop limit (default 1; -1 unlimited)."))
	 .Bool(TEXT("upstream"), TEXT("get_node_connections: walk backward to all sources (overrides direction/depth)."))
	 .Bool(TEXT("downstream"), TEXT("get_node_connections: walk forward to all consumers (overrides direction/depth)."))
	 .Required({TEXT("assetPath"), TEXT("nodeId")});
}

static void S_GetNodeProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Required({TEXT("assetPath"), TEXT("nodeId")});
}

static void S_SetNodeValue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Number(TEXT("floatValue"), TEXT("set_node_value: scalar value for a Constant / named-float property / arithmetic ConstA."))
	 .Number(TEXT("r"), TEXT("set_node_value: red/X channel of a Constant2/3/4Vector or VectorParameter default."))
	 .Number(TEXT("g"), TEXT("set_node_value: green/Y channel."))
	 .Number(TEXT("b"), TEXT("set_node_value: blue/Z channel."))
	 .Number(TEXT("a"), TEXT("set_node_value: alpha/W channel (Constant4Vector / VectorParameter)."))
	 .Number(TEXT("constA"), TEXT("set_node_value: ConstA default of an arithmetic node (Add/Multiply/…) when input A is unconnected."))
	 .Number(TEXT("constB"), TEXT("set_node_value: ConstB default of an arithmetic node when input B is unconnected."))
	 .Required({TEXT("assetPath"), TEXT("nodeId")});
}

static void S_SetStaticSwitchParameterValue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .Bool(TEXT("boolValue"), TEXT("Static switch value (on/off)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("parameterName"), TEXT("boolValue")});
}

static void S_DeleteNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Array(TEXT("nodeIds"), TEXT("delete_node: batch-delete multiple node IDs (alternative to nodeId)."))
	 .Required({TEXT("assetPath")});
}

static void S_UpdateCustomExpression(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("code"), TEXT("add_custom_expression/update_custom_expression: HLSL source code body."))
	 .String(TEXT("description"), TEXT(""))
	 .String(TEXT("outputType"), TEXT("add_custom_expression/update_custom_expression: Float1|Float2|Float3|Float4|MaterialAttributes (default Float1)."))
	 .ArrayOfObjects(TEXT("inputs"), TEXT("add_custom_expression/update_custom_expression: named HLSL input pins, e.g. [{\"name\":\"UV\"}]. Each name becomes a connect_nodes inputName."))
	 .ArrayOfObjects(TEXT("additionalOutputs"), TEXT("add_custom_expression/update_custom_expression: extra named outputs beyond the primary return, e.g. [{\"name\":\"Mask\",\"type\":\"Float1\"}]."))
	 .Required({TEXT("assetPath"), TEXT("nodeId")});
}

static void S_GetNodeChain(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("startNodeId"), TEXT("get_node_chain: node ID to trace the signal path from."))
	 .String(TEXT("endNodeId"), TEXT("get_node_chain: node ID (or \"Main\") to trace the signal path to."))
	 .String(TEXT("endPin"), TEXT("get_node_chain: named material main-pin to trace to, e.g. 'BaseColor' (alternative to endNodeId)."))
	 .Required({TEXT("assetPath"), TEXT("startNodeId")});
}

static void S_GetConnectedSubgraph(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("orphansOnly"), TEXT("get_connected_subgraph: return all nodes not connected to any output, ignoring nodeId."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Required({TEXT("assetPath")});
}

static void S_ArrangeGraph(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AddMaterialNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("nodeType"), TEXT(""))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("nodeType")});
}

static void S_RebuildMaterial(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("waitForShaders"), TEXT("compile_material: block on the async shader workers so backend (not just translation) errors are reported. Slower; off by default."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_SetMaterialParameter(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .String(TEXT("parameterType"), TEXT(""))
	 .Required({TEXT("assetPath"), TEXT("parameterName")});
}

static void S_GetMaterialNodeDetails(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Required({TEXT("assetPath"), TEXT("nodeId")});
}

static void S_RemoveMaterialNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath"), TEXT("nodeId")});
}

static void S_SetTwoSided(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("twoSided"), TEXT("create_material/set_two_sided: render both polygon faces."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_CreateNoiseTexture(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("noiseType"), TEXT("create_noise_texture: Perlin (default) — FBM noise type."))
	 .Number(TEXT("width"), TEXT(""))
	 .Number(TEXT("height"), TEXT(""))
	 .Number(TEXT("scale"), TEXT("create_noise_texture: noise frequency scale (default 1)."))
	 .Number(TEXT("octaves"), TEXT("create_noise_texture: FBM octave count (default 4)."))
	 .Number(TEXT("persistence"), TEXT("create_noise_texture: FBM amplitude falloff per octave (default 0.5)."))
	 .Number(TEXT("lacunarity"), TEXT("create_noise_texture: FBM frequency growth per octave (default 2)."))
	 .Number(TEXT("seed"), TEXT("create_noise_texture: noise seed (default 0)."))
	 .Bool(TEXT("seamless"), TEXT("create_noise_texture: tile seamlessly (default false)."))
	 .Bool(TEXT("hdr"), TEXT("create_noise_texture/create_gradient_texture: create a float/HDR texture (default false)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_CreateGradientTexture(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("gradientType"), TEXT("create_gradient_texture: Linear (default)|Radial|Angular."))
	 .Number(TEXT("width"), TEXT(""))
	 .Number(TEXT("height"), TEXT(""))
	 .Number(TEXT("angle"), TEXT("create_gradient_texture: Linear gradient direction in degrees (default 0)."))
	 .Number(TEXT("centerX"), TEXT("create_gradient_texture: Radial/Angular center U 0-1 (default 0.5)."))
	 .Number(TEXT("centerY"), TEXT("create_gradient_texture: Radial/Angular center V 0-1 (default 0.5)."))
	 .Number(TEXT("radius"), TEXT("create_gradient_texture: Radial falloff radius (default 0.5). blur: box-blur pixel radius 1-10 (default 2)."))
	 .Bool(TEXT("hdr"), TEXT("create_noise_texture/create_gradient_texture: create a float/HDR texture (default false)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Object(TEXT("startColor"), TEXT("create_gradient_texture: start color, channels 0..1 (default black)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Object(TEXT("endColor"), TEXT("create_gradient_texture: end color, channels 0..1 (default white)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Required({TEXT("name")});
}

static void S_CreatePatternTexture(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("patternType"), TEXT("create_pattern_texture: Checker (default)|Grid|Brick|Stripes|Dots."))
	 .Number(TEXT("width"), TEXT(""))
	 .Number(TEXT("height"), TEXT(""))
	 .Number(TEXT("tilesX"), TEXT("create_pattern_texture: horizontal tile count (default 8)."))
	 .Number(TEXT("tilesY"), TEXT("create_pattern_texture: vertical tile count (default 8)."))
	 .Number(TEXT("lineWidth"), TEXT("create_pattern_texture: Grid/Brick line thickness 0-1 (default 0.02)."))
	 .Number(TEXT("brickRatio"), TEXT("create_pattern_texture: Brick length-to-height ratio (default 2)."))
	 .Number(TEXT("offset"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Object(TEXT("primaryColor"), TEXT("create_pattern_texture: foreground color, channels 0..1 (default white)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Object(TEXT("secondaryColor"), TEXT("create_pattern_texture: background color, channels 0..1 (default black)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Required({TEXT("name")});
}

static void S_CreateNormalFromHeight(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sourceTexture"), TEXT("create_normal_from_height: source height-map texture path."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Number(TEXT("strength"), TEXT("create_normal_from_height: normal map intensity (default 1)."))
	 .String(TEXT("algorithm"), TEXT("create_normal_from_height: Sobel (default) or finite-difference gradient method."))
	 .Bool(TEXT("flipY"), TEXT("create_normal_from_height: flip the green channel (DirectX vs OpenGL normal convention)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("channelMode"), TEXT("create_normal_from_height: height source channel — luminance (default)|red|green|blue|alpha|average."))
	 .Required({TEXT("sourceTexture")});
}

static void S_CreateAoFromMesh(FMcpSchemaBuilder& B)
{
	B.String(TEXT("meshPath"), TEXT("Mesh asset path."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Number(TEXT("width"), TEXT(""))
	 .Number(TEXT("height"), TEXT(""))
	 .Number(TEXT("sampleCount"), TEXT("create_ao_from_mesh: vertex samples per pixel (default 64)."))
	 .Number(TEXT("rayDistance"), TEXT("create_ao_from_mesh: occlusion ray distance (default 100)."))
	 .Number(TEXT("bias"), TEXT("create_ao_from_mesh: AO bias (default 0.01)."))
	 .Number(TEXT("uvChannel"), TEXT("create_ao_from_mesh: source UV channel index (default 0)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("meshPath"), TEXT("name")});
}

static void S_ResizeTexture(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sourcePath"), TEXT("Source path for import/duplicate/rename/move (assetPath also accepted)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Number(TEXT("newWidth"), TEXT("resize_texture: output width in pixels (default 512)."))
	 .Number(TEXT("newHeight"), TEXT("resize_texture: output height in pixels (default 512)."))
	 .String(TEXT("filterMethod"), TEXT("resize_texture: nearest|bilinear (default)|bicubic|lanczos."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("sourcePath")});
}

static void S_AdjustLevels(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("inBlack"), TEXT("adjust_levels: input black point 0-1 (default 0)."))
	 .Number(TEXT("inWhite"), TEXT("adjust_levels: input white point 0-1 (default 1)."))
	 .Number(TEXT("gamma"), TEXT("adjust_levels: gamma correction (default 1)."))
	 .Number(TEXT("outBlack"), TEXT("adjust_levels: output black point 0-1 (default 0)."))
	 .Number(TEXT("outWhite"), TEXT("adjust_levels: output white point 0-1 (default 1)."))
	 .Bool(TEXT("inPlace"), TEXT("invert/desaturate/adjust_curves: modify the source texture instead of writing a new one (default true)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_AdjustCurves(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("inPlace"), TEXT("invert/desaturate/adjust_curves: modify the source texture instead of writing a new one (default true)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Array(TEXT("inputR"), TEXT("adjust_curves: red-channel curve input control points."), TEXT("number"))
	 .Array(TEXT("outputR"), TEXT("adjust_curves: red-channel curve output control points."), TEXT("number"))
	 .Array(TEXT("inputG"), TEXT("adjust_curves: green-channel curve input control points."), TEXT("number"))
	 .Array(TEXT("outputG"), TEXT("adjust_curves: green-channel curve output control points."), TEXT("number"))
	 .Array(TEXT("inputB"), TEXT("adjust_curves: blue-channel curve input control points."), TEXT("number"))
	 .Array(TEXT("outputB"), TEXT("adjust_curves: blue-channel curve output control points."), TEXT("number"))
	 .Array(TEXT("input"), TEXT("adjust_curves: master curve input control points 0-1 (paired with output)."), TEXT("number"))
	 .Array(TEXT("output"), TEXT("adjust_curves: master curve output control points 0-1 (paired with input)."), TEXT("number"))
	 .Required({TEXT("assetPath")});
}

static void S_Blur(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("radius"), TEXT("create_gradient_texture: Radial falloff radius (default 0.5). blur: box-blur pixel radius 1-10 (default 2)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_Sharpen(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("amount"), TEXT("desaturate: blend amount 0-1 (default 1). sharpen: unsharp-mask amount 0-5 (default 1)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_Invert(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("inPlace"), TEXT("invert/desaturate/adjust_curves: modify the source texture instead of writing a new one (default true)."))
	 .Bool(TEXT("invertAlpha"), TEXT("invert: also invert the alpha channel (default false)."))
	 .String(TEXT("channel"), TEXT("invert: All (default)|Red|Green|Blue|Alpha. channel_extract: R (default)|G|B|A."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_Desaturate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("amount"), TEXT("desaturate: blend amount 0-1 (default 1). sharpen: unsharp-mask amount 0-5 (default 1)."))
	 .Bool(TEXT("inPlace"), TEXT("invert/desaturate/adjust_curves: modify the source texture instead of writing a new one (default true)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Object(TEXT("luminanceFactors"), TEXT("add_desaturation: optional luminance weights, channels 0..1 (default 0.3/0.59/0.11)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")); })
	 .Required({TEXT("assetPath")});
}

static void S_ChannelPack(FMcpSchemaBuilder& B)
{
	B.String(TEXT("redTexture"), TEXT("channel_pack: source texture for the output red channel."))
	 .String(TEXT("greenTexture"), TEXT("channel_pack: source texture for the output green channel."))
	 .String(TEXT("blueTexture"), TEXT("channel_pack: source texture for the output blue channel."))
	 .String(TEXT("alphaTexture"), TEXT("channel_pack: source texture for the output alpha channel."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_ChannelExtract(FMcpSchemaBuilder& B)
{
	B.String(TEXT("texturePath"), TEXT("Texture asset path."))
	 .String(TEXT("channel"), TEXT("invert: All (default)|Red|Green|Blue|Alpha. channel_extract: R (default)|G|B|A."))
	 .String(TEXT("outputPath"), TEXT("create_thumbnail/generate_report: file path (relative to project dir) to write output to. channel_extract: destination texture path."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("texturePath")});
}

static void S_CombineTextures(FMcpSchemaBuilder& B)
{
	B.String(TEXT("baseTexture"), TEXT("combine_textures: base texture path."))
	 .String(TEXT("overlayTexture"), TEXT("combine_textures: overlay texture path (alias blendTexture)."))
	 .String(TEXT("blendTexture"), TEXT("combine_textures: alias for overlayTexture."))
	 .String(TEXT("blendMode"), TEXT("create_material/set_blend_mode: Opaque|Masked|Translucent|Additive|Modulate|AlphaComposite|AlphaHoldout. combine_textures: Normal|Multiply|Screen|Overlay|Add."))
	 .Number(TEXT("opacity"), TEXT("combine_textures: overlay blend opacity 0-1 (default 1)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("baseTexture")});
}

static void S_SetCompressionSettings(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("compressionSettings"), TEXT("set_compression_settings: TC_Default|TC_Normalmap|TC_Masks|TC_Grayscale|TC_Displacementmap|TC_VectorDisplacementmap|TC_HDR|TC_EditorIcon|TC_Alpha|TC_DistanceFieldFont|TC_HDR_Compressed|TC_BC7."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_SetTextureGroup(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("textureGroup"), TEXT("set_texture_group: e.g. World (default)|Character|Weapon|Vehicle|Cinematic|Effects|Skybox|UI|Lightmap|RenderTarget|Bokeh|Pixels2D."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_SetLodBias(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("lodBias"), TEXT("set_lod_bias: additional texture LOD bias (default 0)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_ConfigureVirtualTexture(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("virtualTextureStreaming"), TEXT("configure_virtual_texture: enable VT streaming (default false)."))
	 .Number(TEXT("tileSize"), TEXT("configure_virtual_texture: VT tile size (default 128)."))
	 .Number(TEXT("tileBorderSize"), TEXT("configure_virtual_texture: VT tile border size (default 4)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_SetStreamingPriority(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("neverStream"), TEXT("set_streaming_priority: disable mip streaming for this texture (default false)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("assetPath")});
}

static void S_GetTextureInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are authored per action (not baked in the macro): RequiresEditor on the
// Core actions (their AssetWorkflow/AssetQuery members are whole-body
// #if WITH_EDITOR with an editor-required #else) and on all MaterialAuthoring
// actions (the retired dispatcher was whole-body #if WITH_EDITOR with an
// "Editor only." EDITOR_ONLY #else). NOT on the Texture actions:
// TextureHandlers.cpp carries no editor gate, so flagging would newly reject the
// GEditor-less runs the shim served (manage_networking precedent). Two Core
// actions implemented in the ungated texture TU follow it: create_render_target
// (whose texture body is the live create_render_target implementation) carries
// no RequiresEditor. Mutating on everything except pure reads.
//
// MCP_MA_CALL forwards to a 3-arg member; MCP_MA_ACTION_CALL passes the action
// literal to a 4-arg member (self-gating sub-handlers, the shared
// HandleDataTableRowOp, and the shared material OR-branch members that
// sub-dispatch on the action).

#define MCP_MA_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                        \
class FMcpCall_ManageAsset_##ClassSuffix final : public FMcpCall                         \
{                                                                                        \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }       \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_asset"),               \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                            \
		return D;                                                                        \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

#define MCP_MA_ACTION_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                 \
class FMcpCall_ManageAsset_##ClassSuffix final : public FMcpCall                         \
{                                                                                        \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }       \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_asset"),               \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                            \
		return D;                                                                        \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, TEXT(ActionLiteral), Payload, Socket);             \
	}                                                                                    \
};

// Core (AssetWorkflow / AssetQuery members)
MCP_MA_CALL(List, "list", HandleListAssets, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(Import, "import", HandleImportAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Duplicate, "duplicate", HandleDuplicateAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Rename, "rename", HandleRenameAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Move, "move", HandleMoveAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Delete, "delete", HandleDeleteAssets, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateFolder, "create_folder", HandleCreateFolder, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(SearchAssets, "search_assets", HandleSearchAssets, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetDependencies, "get_dependencies", HandleGetDependencies, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetReferencers, "get_referencers", HandleGetReferencers, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetAssetProperties, "get_asset_properties", HandleGetAssetProperties, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(SetAssetProperty, "set_asset_property", HandleSetAssetProperty, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(GetSourceControlState, "get_source_control_state", HandleGetSourceControlState, EMcpCallFlags::RequiresEditor)
MCP_MA_ACTION_CALL(AnalyzeGraph, "analyze_graph", HandleAnalyzeGraph, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetAssetGraph, "get_asset_graph", HandleGetAssetGraph, EMcpCallFlags::RequiresEditor)
MCP_MA_ACTION_CALL(CreateThumbnail, "create_thumbnail", HandleGenerateThumbnail, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetTags, "set_tags", HandleSetTags, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(GetMetadata, "get_metadata", HandleGetMetadata, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(SetMetadata, "set_metadata", HandleSetMetadata, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Validate, "validate", HandleValidateAsset, EMcpCallFlags::RequiresEditor)
MCP_MA_ACTION_CALL(FixupRedirectors, "fixup_redirectors", HandleFixupRedirectors, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(FindByTag, "find_by_tag", HandleFindByTag, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GenerateReport, "generate_report", HandleGenerateReport, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(CreateDataTable, "create_data_table", HandleCreateDataTable, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddDataTableRow, "add_data_table_row", HandleDataTableRowOp, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(EditDataTableRow, "edit_data_table_row", HandleDataTableRowOp, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(RemoveDataTableRow, "remove_data_table_row", HandleDataTableRowOp, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(GetDataTableRows, "get_data_table_rows", HandleDataTableRowOp, EMcpCallFlags::RequiresEditor)
MCP_MA_ACTION_CALL(ImportDataTable, "import_data_table", HandleDataTableRowOp, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateRenderTarget, "create_render_target", HandleTextureCreateRenderTarget, EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(GenerateLods, "generate_lods", HandleGenerateLODs, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddMaterialParameter, "add_material_parameter", HandleAddMaterialParameter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(ListInstances, "list_instances", HandleListMaterialInstances, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(ResetInstanceParameters, "reset_instance_parameters", HandleResetInstanceParameters, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Exists, "exists", HandleDoesAssetExist, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetMaterialStats, "get_material_stats", HandleGetMaterialStats, EMcpCallFlags::RequiresEditor)
MCP_MA_ACTION_CALL(NaniteRebuildMesh, "nanite_rebuild_mesh", HandleNaniteRebuildMesh, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(BulkRename, "bulk_rename", HandleBulkRenameAssets, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(BulkDelete, "bulk_delete", HandleBulkDeleteAssets, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(SourceControlCheckout, "source_control_checkout", HandleSourceControlCheckout, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(SourceControlSubmit, "source_control_submit", HandleSourceControlSubmit, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Save, "save", HandleSaveAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SaveAll, "save_all", HandleControlEditorSaveAll, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Material authoring (MaterialAuthoringHandlers.cpp)
MCP_MA_CALL(CreateMaterial, "create_material", HandleMaterialCreateMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetBlendMode, "set_blend_mode", HandleMaterialSetBlendMode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetShadingModel, "set_shading_model", HandleMaterialSetShadingModel, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetMaterialDomain, "set_material_domain", HandleMaterialSetMaterialDomain, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddTextureSample, "add_texture_sample", HandleMaterialAddTextureSample, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddTextureCoordinate, "add_texture_coordinate", HandleMaterialAddTextureCoordinate, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddScalarParameter, "add_scalar_parameter", HandleMaterialAddScalarParameter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddVectorParameter, "add_vector_parameter", HandleMaterialAddVectorParameter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddStaticSwitchParameter, "add_static_switch_parameter", HandleMaterialAddStaticSwitchParameter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddMathNode, "add_math_node", HandleMaterialAddMathNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddWorldPosition, "add_world_position", HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddVertexNormal, "add_vertex_normal", HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddPixelDepth, "add_pixel_depth", HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddFresnel, "add_fresnel", HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddReflectionVector, "add_reflection_vector", HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddPanner, "add_panner", HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddRotator, "add_rotator", HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddNoise, "add_noise", HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddVoronoi, "add_voronoi", HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddIf, "add_if", HandleMaterialAddConditional, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddSwitch, "add_switch", HandleMaterialAddConditional, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddCustomExpression, "add_custom_expression", HandleMaterialAddCustomExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(ConnectNodes, "connect_nodes", HandleMaterialConnectNodes, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(DisconnectNodes, "disconnect_nodes", HandleMaterialDisconnectNodes, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateMaterialFunction, "create_material_function", HandleMaterialCreateMaterialFunction, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddFunctionInput, "add_function_input", HandleMaterialAddFunctionIO, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddFunctionOutput, "add_function_output", HandleMaterialAddFunctionIO, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(UseMaterialFunction, "use_material_function", HandleMaterialUseMaterialFunction, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(GetMaterialFunctionInfo, "get_material_function_info", HandleMaterialGetMaterialFunctionInfo, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(CreateMaterialInstance, "create_material_instance", HandleMaterialCreateMaterialInstance, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetScalarParameterValue, "set_scalar_parameter_value", HandleMaterialSetScalarParameterValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetVectorParameterValue, "set_vector_parameter_value", HandleMaterialSetVectorParameterValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetTextureParameterValue, "set_texture_parameter_value", HandleMaterialSetTextureParameterValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(CreateLandscapeMaterial, "create_landscape_material", HandleMaterialCreateSpecialMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(CreateDecalMaterial, "create_decal_material", HandleMaterialCreateSpecialMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(CreatePostProcessMaterial, "create_post_process_material", HandleMaterialCreateSpecialMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddLandscapeLayer, "add_landscape_layer", HandleMaterialAddLandscapeLayer, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(ConfigureLayerBlend, "configure_layer_blend", HandleMaterialConfigureLayerBlend, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(CompileMaterial, "compile_material", HandleMaterialCompileMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(GetMaterialInfo, "get_material_info", HandleMaterialGetMaterialInfo, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(FindNode, "find_node", HandleMaterialFindNode, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetNodeConnections, "get_node_connections", HandleMaterialGetNodeConnections, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetNodeProperties, "get_node_properties", HandleMaterialGetNodeProperties, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(SetNodeValue, "set_node_value", HandleMaterialSetNodeValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetStaticSwitchParameterValue, "set_static_switch_parameter_value", HandleMaterialSetStaticSwitchParameterValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(DeleteNode, "delete_node", HandleMaterialDeleteNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(UpdateCustomExpression, "update_custom_expression", HandleMaterialUpdateCustomExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(GetNodeChain, "get_node_chain", HandleMaterialGetNodeChain, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetConnectedSubgraph, "get_connected_subgraph", HandleMaterialGetConnectedSubgraph, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(ArrangeGraph, "arrange_graph", HandleMaterialArrangeGraph, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddMaterialNode, "add_material_node", HandleMaterialAddMaterialNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(RebuildMaterial, "rebuild_material", HandleMaterialCompileMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetMaterialParameter, "set_material_parameter", HandleMaterialSetMaterialParameter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(GetMaterialNodeDetails, "get_material_node_details", HandleMaterialGetMaterialNodeDetails, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(RemoveMaterialNode, "remove_material_node", HandleMaterialRemoveMaterialNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetTwoSided, "set_two_sided", HandleMaterialSetTwoSided, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Texture (TextureHandlers.cpp)
MCP_MA_CALL(CreateNoiseTexture, "create_noise_texture", HandleTextureCreateNoiseTexture, EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateGradientTexture, "create_gradient_texture", HandleTextureCreateGradientTexture, EMcpCallFlags::Mutating)
MCP_MA_CALL(CreatePatternTexture, "create_pattern_texture", HandleTextureCreatePatternTexture, EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateNormalFromHeight, "create_normal_from_height", HandleTextureCreateNormalFromHeight, EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateAoFromMesh, "create_ao_from_mesh", HandleTextureCreateAoFromMesh, EMcpCallFlags::Mutating)
MCP_MA_CALL(ResizeTexture, "resize_texture", HandleTextureResizeTexture, EMcpCallFlags::Mutating)
MCP_MA_CALL(AdjustLevels, "adjust_levels", HandleTextureAdjustLevels, EMcpCallFlags::Mutating)
MCP_MA_CALL(AdjustCurves, "adjust_curves", HandleTextureAdjustCurves, EMcpCallFlags::Mutating)
MCP_MA_CALL(Blur, "blur", HandleTextureBlur, EMcpCallFlags::Mutating)
MCP_MA_CALL(Sharpen, "sharpen", HandleTextureSharpen, EMcpCallFlags::Mutating)
MCP_MA_CALL(Invert, "invert", HandleTextureInvert, EMcpCallFlags::Mutating)
MCP_MA_CALL(Desaturate, "desaturate", HandleTextureDesaturate, EMcpCallFlags::Mutating)
MCP_MA_CALL(ChannelPack, "channel_pack", HandleTextureChannelPack, EMcpCallFlags::Mutating)
MCP_MA_CALL(ChannelExtract, "channel_extract", HandleTextureChannelExtract, EMcpCallFlags::Mutating)
MCP_MA_CALL(CombineTextures, "combine_textures", HandleTextureCombineTextures, EMcpCallFlags::Mutating)
MCP_MA_CALL(SetCompressionSettings, "set_compression_settings", HandleTextureSetCompressionSettings, EMcpCallFlags::Mutating)
MCP_MA_CALL(SetTextureGroup, "set_texture_group", HandleTextureSetTextureGroup, EMcpCallFlags::Mutating)
MCP_MA_CALL(SetLodBias, "set_lod_bias", HandleTextureSetLodBias, EMcpCallFlags::Mutating)
MCP_MA_CALL(ConfigureVirtualTexture, "configure_virtual_texture", HandleTextureConfigureVirtualTexture, EMcpCallFlags::Mutating)
MCP_MA_CALL(SetStreamingPriority, "set_streaming_priority", HandleTextureSetStreamingPriority, EMcpCallFlags::Mutating)
MCP_MA_CALL(GetTextureInfo, "get_texture_info", HandleTextureGetTextureInfo, EMcpCallFlags::None)

#undef MCP_MA_CALL
#undef MCP_MA_ACTION_CALL

} // namespace McpCalls::ManageAsset

void McpRegisterManageAssetCalls()
{
	using namespace McpCalls::ManageAsset;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_List>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Import>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Duplicate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Rename>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Move>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Delete>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateFolder>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SearchAssets>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetDependencies>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetReferencers>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetAssetProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetAssetProperty>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetSourceControlState>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AnalyzeGraph>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetAssetGraph>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateThumbnail>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetTags>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetMetadata>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetMetadata>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Validate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_FixupRedirectors>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_FindByTag>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GenerateReport>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateDataTable>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddDataTableRow>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_EditDataTableRow>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_RemoveDataTableRow>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetDataTableRows>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_ImportDataTable>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateRenderTarget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GenerateLods>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddMaterialParameter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_ListInstances>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_ResetInstanceParameters>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Exists>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetMaterialStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_NaniteRebuildMesh>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_BulkRename>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_BulkDelete>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SourceControlCheckout>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SourceControlSubmit>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Save>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SaveAll>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateMaterial>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetBlendMode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetShadingModel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetMaterialDomain>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddTextureSample>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddTextureCoordinate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddScalarParameter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddVectorParameter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddStaticSwitchParameter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddMathNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddWorldPosition>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddVertexNormal>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddPixelDepth>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddFresnel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddReflectionVector>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddPanner>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddRotator>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddNoise>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddVoronoi>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddIf>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddSwitch>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddCustomExpression>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_ConnectNodes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_DisconnectNodes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateMaterialFunction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddFunctionInput>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddFunctionOutput>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_UseMaterialFunction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetMaterialFunctionInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateMaterialInstance>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetScalarParameterValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetVectorParameterValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetTextureParameterValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateLandscapeMaterial>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateDecalMaterial>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreatePostProcessMaterial>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddLandscapeLayer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_ConfigureLayerBlend>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CompileMaterial>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetMaterialInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_FindNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetNodeConnections>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetNodeProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetNodeValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetStaticSwitchParameterValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_DeleteNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_UpdateCustomExpression>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetNodeChain>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetConnectedSubgraph>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_ArrangeGraph>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AddMaterialNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_RebuildMaterial>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetMaterialParameter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetMaterialNodeDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_RemoveMaterialNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetTwoSided>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateNoiseTexture>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateGradientTexture>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreatePatternTexture>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateNormalFromHeight>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CreateAoFromMesh>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_ResizeTexture>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AdjustLevels>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_AdjustCurves>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Blur>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Sharpen>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Invert>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_Desaturate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_ChannelPack>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_ChannelExtract>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_CombineTextures>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetCompressionSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetTextureGroup>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetLodBias>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_ConfigureVirtualTexture>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_SetStreamingPriority>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_GetTextureInfo>());
}
