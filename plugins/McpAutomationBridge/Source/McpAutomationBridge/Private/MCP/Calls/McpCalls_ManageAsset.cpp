// LINT-TOOL: manage_asset
// manage_asset as FMcpCall classes — twentieth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers — Core actions to their existing AssetWorkflow/AssetQuery
// members, MaterialAuthoring actions to HandleMaterial* members
// (MaterialAuthoringHandlers.cpp), Texture actions to HandleTexture* members
// (TextureHandlers.cpp) — until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageAsset
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations (McpDecl_ManageAsset.h)
// and re-verified against the member bodies. 13 rows shipped with decl fixes at
// classing, all with body evidence:
//  • The five own-branch shadowed material actions had rows unioned from the
//    DEAD AssetWorkflow copies the router never let execute (MaterialAuthoring
//    is tested first). Re-derived from the live HandleManageMaterialAuthoring
//    branches: create_material dropped `properties`; create_material_instance
//    dropped `parameters`; add_material_node dropped materialPath/posX/posY/
//    value/color/texturePath; remove_material_node and get_material_node_details
//    dropped materialPath/expressionIndex (all proven absent from the live
//    bodies).
//  • The three material alias rows re-derived from their remap targets (the
//    dispatcher rewrites connect_material_pins→connect_nodes,
//    break_material_connections→disconnect_nodes, rebuild_material→
//    compile_material before the branch chain): connect_material_pins dropped
//    materialPath/fromExpression/toExpression/targetPin; break_material_
//    connections dropped materialPath/expressionIndex/inputName; rebuild_material
//    dropped materialPath.
//  • The five data-table rows declared the assetPath/dataTablePath/tablePath/
//    path alias group all-required although McpLoadDataTableArg resolves them
//    first-present-wins and joint-rejects only when all are absent — flipped
//    optional (rowName stays required on add/edit/remove per the body's own
//    reject). import_data_table's sourceText/csv/json/content source aliases
//    were likewise all-required despite a first-present-wins joint reject —
//    flipped optional.

// Core (AssetWorkflow / AssetQuery members)
inline const FMcpParamDecl P_List[] = { { TEXT("filter"), EMcpParamKind::Object, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("directory"), EMcpParamKind::String, false }, { TEXT("directoryPath"), EMcpParamKind::String, false }, { TEXT("recursive"), EMcpParamKind::Bool, false }, { TEXT("recursivePaths"), EMcpParamKind::Bool, false }, { TEXT("pagination"), EMcpParamKind::Object, false }, { TEXT("depth"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Import[] = { { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("sourcePath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Duplicate[] = { { TEXT("sourcePath"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Rename[] = { { TEXT("levelPath"), EMcpParamKind::String, true }, { TEXT("sourcePath"), EMcpParamKind::String, false }, { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, false }, { TEXT("overwrite"), EMcpParamKind::Bool, false }, { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Move[] = { { TEXT("sourcePath"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Delete[] = { { TEXT("paths"), EMcpParamKind::Array, true }, { TEXT("assetPaths"), EMcpParamKind::Array, true }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateFolder[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("directoryPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SearchAssets[] = { { TEXT("classNames"), EMcpParamKind::Array, false }, { TEXT("packagePaths"), EMcpParamKind::Array, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("searchText"), EMcpParamKind::String, false }, { TEXT("recursivePaths"), EMcpParamKind::Bool, false }, { TEXT("recursiveClasses"), EMcpParamKind::Bool, false }, { TEXT("offset"), EMcpParamKind::Number, false }, { TEXT("limit"), EMcpParamKind::Number, false }, { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("sourcePath"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, true }, { TEXT("paths"), EMcpParamKind::Array, true }, { TEXT("assetPaths"), EMcpParamKind::Array, true }, { TEXT("directoryPath"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("parentMaterial"), EMcpParamKind::String, true }, { TEXT("parameters"), EMcpParamKind::Object, false }, { TEXT("rowStruct"), EMcpParamKind::String, true }, { TEXT("rowStructPath"), EMcpParamKind::String, true }, { TEXT("dataTablePath"), EMcpParamKind::String, true }, { TEXT("tablePath"), EMcpParamKind::String, true }, { TEXT("rowName"), EMcpParamKind::String, true }, { TEXT("rowData"), EMcpParamKind::Any, false }, { TEXT("sourceText"), EMcpParamKind::String, true }, { TEXT("csv"), EMcpParamKind::String, true }, { TEXT("json"), EMcpParamKind::String, true }, { TEXT("content"), EMcpParamKind::String, true }, { TEXT("format"), EMcpParamKind::String, false }, { TEXT("recursive"), EMcpParamKind::Bool, false }, { TEXT("includeTransient"), EMcpParamKind::Bool, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, true }, { TEXT("maxDepth"), EMcpParamKind::Number, false }, { TEXT("tags"), EMcpParamKind::Array, false }, { TEXT("metadata"), EMcpParamKind::Object, false }, { TEXT("filter"), EMcpParamKind::Object, false }, { TEXT("directory"), EMcpParamKind::String, false }, { TEXT("pagination"), EMcpParamKind::Object, false }, { TEXT("depth"), EMcpParamKind::Number, false }, { TEXT("reportType"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("type"), EMcpParamKind::String, true }, { TEXT("checkoutFiles"), EMcpParamKind::Bool, false }, { TEXT("prefix"), EMcpParamKind::String, false }, { TEXT("suffix"), EMcpParamKind::String, false }, { TEXT("replaceText"), EMcpParamKind::String, false }, { TEXT("folderPath"), EMcpParamKind::String, true }, { TEXT("showConfirmation"), EMcpParamKind::Bool, false }, { TEXT("fixupRedirectors"), EMcpParamKind::Bool, false }, { TEXT("pattern"), EMcpParamKind::String, false }, { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("assets"), EMcpParamKind::Array, false }, { TEXT("lodCount"), EMcpParamKind::Number, false }, { TEXT("numLODs"), EMcpParamKind::Number, false }, { TEXT("reductionSettings"), EMcpParamKind::Object, false }, { TEXT("meshPath"), EMcpParamKind::String, true }, { TEXT("enableNanite"), EMcpParamKind::Bool, false }, { TEXT("preserveArea"), EMcpParamKind::Bool, false }, { TEXT("trianglePercent"), EMcpParamKind::Number, false }, { TEXT("fallbackPercent"), EMcpParamKind::Number, false }, { TEXT("description"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("maxResults"), EMcpParamKind::Number, false }, { TEXT("searchActors"), EMcpParamKind::Bool, false }, { TEXT("searchComponents"), EMcpParamKind::Bool, false }, { TEXT("searchAssets"), EMcpParamKind::Bool, false }, { TEXT("nodeType"), EMcpParamKind::String, true }, { TEXT("posX"), EMcpParamKind::Number, false }, { TEXT("posY"), EMcpParamKind::Number, false }, { TEXT("color"), EMcpParamKind::Object, false }, { TEXT("texturePath"), EMcpParamKind::String, false }, { TEXT("sourceNodeId"), EMcpParamKind::String, false }, { TEXT("targetNodeId"), EMcpParamKind::String, false }, { TEXT("fromExpression"), EMcpParamKind::Number, false }, { TEXT("toExpression"), EMcpParamKind::Number, false }, { TEXT("sourcePin"), EMcpParamKind::String, false }, { TEXT("inputName"), EMcpParamKind::String, false }, { TEXT("targetPin"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("expressionIndex"), EMcpParamKind::Number, false }, { TEXT("pinName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetDependencies[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("includeSoftDependencies"), EMcpParamKind::Bool, false }, { TEXT("recursive"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetReferencers[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_GetAssetProperties[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("includeTransient"), EMcpParamKind::Bool, false }, { TEXT("propertyName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetAssetProperty[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::Any, true } };
inline const FMcpParamDecl P_GetSourceControlState[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("assetPaths"), EMcpParamKind::Array, true } };
inline const FMcpParamDecl P_AnalyzeGraph[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("materialPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_GetAssetGraph[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("maxDepth"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateThumbnail[] = { { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("sourcePath"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, true }, { TEXT("paths"), EMcpParamKind::Array, true }, { TEXT("assetPaths"), EMcpParamKind::Array, true }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("directoryPath"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("parentMaterial"), EMcpParamKind::String, true }, { TEXT("parameters"), EMcpParamKind::Object, false }, { TEXT("rowStruct"), EMcpParamKind::String, true }, { TEXT("rowStructPath"), EMcpParamKind::String, true }, { TEXT("dataTablePath"), EMcpParamKind::String, true }, { TEXT("tablePath"), EMcpParamKind::String, true }, { TEXT("rowName"), EMcpParamKind::String, true }, { TEXT("rowData"), EMcpParamKind::Any, false }, { TEXT("sourceText"), EMcpParamKind::String, true }, { TEXT("csv"), EMcpParamKind::String, true }, { TEXT("json"), EMcpParamKind::String, true }, { TEXT("content"), EMcpParamKind::String, true }, { TEXT("format"), EMcpParamKind::String, false }, { TEXT("recursive"), EMcpParamKind::Bool, false }, { TEXT("includeTransient"), EMcpParamKind::Bool, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, true }, { TEXT("maxDepth"), EMcpParamKind::Number, false }, { TEXT("tags"), EMcpParamKind::Array, false }, { TEXT("metadata"), EMcpParamKind::Object, false }, { TEXT("filter"), EMcpParamKind::Object, false }, { TEXT("directory"), EMcpParamKind::String, false }, { TEXT("recursivePaths"), EMcpParamKind::Bool, false }, { TEXT("pagination"), EMcpParamKind::Object, false }, { TEXT("depth"), EMcpParamKind::Number, false }, { TEXT("reportType"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("type"), EMcpParamKind::String, true }, { TEXT("checkoutFiles"), EMcpParamKind::Bool, false }, { TEXT("prefix"), EMcpParamKind::String, false }, { TEXT("suffix"), EMcpParamKind::String, false }, { TEXT("searchText"), EMcpParamKind::String, false }, { TEXT("replaceText"), EMcpParamKind::String, false }, { TEXT("folderPath"), EMcpParamKind::String, true }, { TEXT("showConfirmation"), EMcpParamKind::Bool, false }, { TEXT("fixupRedirectors"), EMcpParamKind::Bool, false }, { TEXT("pattern"), EMcpParamKind::String, false }, { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("assets"), EMcpParamKind::Array, false }, { TEXT("lodCount"), EMcpParamKind::Number, false }, { TEXT("numLODs"), EMcpParamKind::Number, false }, { TEXT("reductionSettings"), EMcpParamKind::Object, false }, { TEXT("meshPath"), EMcpParamKind::String, true }, { TEXT("enableNanite"), EMcpParamKind::Bool, false }, { TEXT("preserveArea"), EMcpParamKind::Bool, false }, { TEXT("trianglePercent"), EMcpParamKind::Number, false }, { TEXT("fallbackPercent"), EMcpParamKind::Number, false }, { TEXT("description"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("maxResults"), EMcpParamKind::Number, false }, { TEXT("searchActors"), EMcpParamKind::Bool, false }, { TEXT("searchComponents"), EMcpParamKind::Bool, false }, { TEXT("searchAssets"), EMcpParamKind::Bool, false }, { TEXT("nodeType"), EMcpParamKind::String, true }, { TEXT("posX"), EMcpParamKind::Number, false }, { TEXT("posY"), EMcpParamKind::Number, false }, { TEXT("color"), EMcpParamKind::Object, false }, { TEXT("texturePath"), EMcpParamKind::String, false }, { TEXT("sourceNodeId"), EMcpParamKind::String, false }, { TEXT("targetNodeId"), EMcpParamKind::String, false }, { TEXT("fromExpression"), EMcpParamKind::Number, false }, { TEXT("toExpression"), EMcpParamKind::Number, false }, { TEXT("sourcePin"), EMcpParamKind::String, false }, { TEXT("inputName"), EMcpParamKind::String, false }, { TEXT("targetPin"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("expressionIndex"), EMcpParamKind::Number, false }, { TEXT("pinName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetTags[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("tags"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_GetMetadata[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetMetadata[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("metadata"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_Validate[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_FixupRedirectors[] = { { TEXT("directoryPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("checkoutFiles"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_FindByTag[] = { { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("maxResults"), EMcpParamKind::Number, false }, { TEXT("searchActors"), EMcpParamKind::Bool, false }, { TEXT("searchComponents"), EMcpParamKind::Bool, false }, { TEXT("searchAssets"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GenerateReport[] = { { TEXT("directory"), EMcpParamKind::String, false }, { TEXT("reportType"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateDataTable[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("rowStruct"), EMcpParamKind::String, true }, { TEXT("rowStructPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddDataTableRow[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("dataTablePath"), EMcpParamKind::String, false }, { TEXT("tablePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("rowName"), EMcpParamKind::String, true }, { TEXT("rowData"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_EditDataTableRow[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("dataTablePath"), EMcpParamKind::String, false }, { TEXT("tablePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("rowName"), EMcpParamKind::String, true }, { TEXT("rowData"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_RemoveDataTableRow[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("dataTablePath"), EMcpParamKind::String, false }, { TEXT("tablePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("rowName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_GetDataTableRows[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("dataTablePath"), EMcpParamKind::String, false }, { TEXT("tablePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ImportDataTable[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("dataTablePath"), EMcpParamKind::String, false }, { TEXT("tablePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("sourceText"), EMcpParamKind::String, false }, { TEXT("csv"), EMcpParamKind::String, false }, { TEXT("json"), EMcpParamKind::String, false }, { TEXT("content"), EMcpParamKind::String, false }, { TEXT("format"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateRenderTarget[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("format"), EMcpParamKind::String, false }, { TEXT("packagePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("sourcePath"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, true }, { TEXT("paths"), EMcpParamKind::Array, true }, { TEXT("assetPaths"), EMcpParamKind::Array, true }, { TEXT("directoryPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("parentMaterial"), EMcpParamKind::String, true }, { TEXT("parameters"), EMcpParamKind::Object, false }, { TEXT("rowStruct"), EMcpParamKind::String, true }, { TEXT("rowStructPath"), EMcpParamKind::String, true }, { TEXT("dataTablePath"), EMcpParamKind::String, true }, { TEXT("tablePath"), EMcpParamKind::String, true }, { TEXT("rowName"), EMcpParamKind::String, true }, { TEXT("rowData"), EMcpParamKind::Any, false }, { TEXT("sourceText"), EMcpParamKind::String, true }, { TEXT("csv"), EMcpParamKind::String, true }, { TEXT("json"), EMcpParamKind::String, true }, { TEXT("content"), EMcpParamKind::String, true }, { TEXT("recursive"), EMcpParamKind::Bool, false }, { TEXT("includeTransient"), EMcpParamKind::Bool, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, true }, { TEXT("maxDepth"), EMcpParamKind::Number, false }, { TEXT("tags"), EMcpParamKind::Array, false }, { TEXT("metadata"), EMcpParamKind::Object, false }, { TEXT("filter"), EMcpParamKind::Object, false }, { TEXT("directory"), EMcpParamKind::String, false }, { TEXT("recursivePaths"), EMcpParamKind::Bool, false }, { TEXT("pagination"), EMcpParamKind::Object, false }, { TEXT("depth"), EMcpParamKind::Number, false }, { TEXT("reportType"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("type"), EMcpParamKind::String, true }, { TEXT("checkoutFiles"), EMcpParamKind::Bool, false }, { TEXT("prefix"), EMcpParamKind::String, false }, { TEXT("suffix"), EMcpParamKind::String, false }, { TEXT("searchText"), EMcpParamKind::String, false }, { TEXT("replaceText"), EMcpParamKind::String, false }, { TEXT("folderPath"), EMcpParamKind::String, true }, { TEXT("showConfirmation"), EMcpParamKind::Bool, false }, { TEXT("fixupRedirectors"), EMcpParamKind::Bool, false }, { TEXT("pattern"), EMcpParamKind::String, false }, { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("assets"), EMcpParamKind::Array, false }, { TEXT("lodCount"), EMcpParamKind::Number, false }, { TEXT("numLODs"), EMcpParamKind::Number, false }, { TEXT("reductionSettings"), EMcpParamKind::Object, false }, { TEXT("meshPath"), EMcpParamKind::String, true }, { TEXT("enableNanite"), EMcpParamKind::Bool, false }, { TEXT("preserveArea"), EMcpParamKind::Bool, false }, { TEXT("trianglePercent"), EMcpParamKind::Number, false }, { TEXT("fallbackPercent"), EMcpParamKind::Number, false }, { TEXT("description"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("maxResults"), EMcpParamKind::Number, false }, { TEXT("searchActors"), EMcpParamKind::Bool, false }, { TEXT("searchComponents"), EMcpParamKind::Bool, false }, { TEXT("searchAssets"), EMcpParamKind::Bool, false }, { TEXT("nodeType"), EMcpParamKind::String, true }, { TEXT("posX"), EMcpParamKind::Number, false }, { TEXT("posY"), EMcpParamKind::Number, false }, { TEXT("color"), EMcpParamKind::Object, false }, { TEXT("texturePath"), EMcpParamKind::String, false }, { TEXT("sourceNodeId"), EMcpParamKind::String, false }, { TEXT("targetNodeId"), EMcpParamKind::String, false }, { TEXT("fromExpression"), EMcpParamKind::Number, false }, { TEXT("toExpression"), EMcpParamKind::Number, false }, { TEXT("sourcePin"), EMcpParamKind::String, false }, { TEXT("inputName"), EMcpParamKind::String, false }, { TEXT("targetPin"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("expressionIndex"), EMcpParamKind::Number, false }, { TEXT("pinName"), EMcpParamKind::String, false }, { TEXT("renderTargetPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GenerateLods[] = { { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("assetPaths"), EMcpParamKind::Array, false }, { TEXT("assets"), EMcpParamKind::Array, false }, { TEXT("lodCount"), EMcpParamKind::Number, false }, { TEXT("numLODs"), EMcpParamKind::Number, false }, { TEXT("reductionSettings"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_AddMaterialParameter[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("type"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ListInstances[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ResetInstanceParameters[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Exists[] = { { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("params"), EMcpParamKind::Object, true }, { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_GetMaterialStats[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_NaniteRebuildMesh[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("meshPath"), EMcpParamKind::String, true }, { TEXT("enableNanite"), EMcpParamKind::Bool, false }, { TEXT("preserveArea"), EMcpParamKind::Bool, false }, { TEXT("trianglePercent"), EMcpParamKind::Number, false }, { TEXT("fallbackPercent"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BulkRename[] = { { TEXT("prefix"), EMcpParamKind::String, false }, { TEXT("suffix"), EMcpParamKind::String, false }, { TEXT("searchText"), EMcpParamKind::String, false }, { TEXT("replaceText"), EMcpParamKind::String, false }, { TEXT("checkoutFiles"), EMcpParamKind::Bool, false }, { TEXT("assetPaths"), EMcpParamKind::Array, true }, { TEXT("folderPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_BulkDelete[] = { { TEXT("showConfirmation"), EMcpParamKind::Bool, false }, { TEXT("fixupRedirectors"), EMcpParamKind::Bool, false }, { TEXT("assetPaths"), EMcpParamKind::Array, true }, { TEXT("folderPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("pattern"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SourceControlCheckout[] = { { TEXT("assetPaths"), EMcpParamKind::Array, true }, { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SourceControlSubmit[] = { { TEXT("assetPaths"), EMcpParamKind::Array, true }, { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("description"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Save[] = { { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("sourcePath"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, true }, { TEXT("paths"), EMcpParamKind::Array, true }, { TEXT("assetPaths"), EMcpParamKind::Array, true }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("directoryPath"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("parentMaterial"), EMcpParamKind::String, true }, { TEXT("parameters"), EMcpParamKind::Object, false }, { TEXT("rowStruct"), EMcpParamKind::String, true }, { TEXT("rowStructPath"), EMcpParamKind::String, true }, { TEXT("dataTablePath"), EMcpParamKind::String, true }, { TEXT("tablePath"), EMcpParamKind::String, true }, { TEXT("rowName"), EMcpParamKind::String, true }, { TEXT("rowData"), EMcpParamKind::Any, false }, { TEXT("sourceText"), EMcpParamKind::String, true }, { TEXT("csv"), EMcpParamKind::String, true }, { TEXT("json"), EMcpParamKind::String, true }, { TEXT("content"), EMcpParamKind::String, true }, { TEXT("format"), EMcpParamKind::String, false }, { TEXT("recursive"), EMcpParamKind::Bool, false }, { TEXT("includeTransient"), EMcpParamKind::Bool, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, true }, { TEXT("maxDepth"), EMcpParamKind::Number, false }, { TEXT("tags"), EMcpParamKind::Array, false }, { TEXT("metadata"), EMcpParamKind::Object, false }, { TEXT("filter"), EMcpParamKind::Object, false }, { TEXT("directory"), EMcpParamKind::String, false }, { TEXT("recursivePaths"), EMcpParamKind::Bool, false }, { TEXT("pagination"), EMcpParamKind::Object, false }, { TEXT("depth"), EMcpParamKind::Number, false }, { TEXT("reportType"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("type"), EMcpParamKind::String, true }, { TEXT("checkoutFiles"), EMcpParamKind::Bool, false }, { TEXT("prefix"), EMcpParamKind::String, false }, { TEXT("suffix"), EMcpParamKind::String, false }, { TEXT("searchText"), EMcpParamKind::String, false }, { TEXT("replaceText"), EMcpParamKind::String, false }, { TEXT("folderPath"), EMcpParamKind::String, true }, { TEXT("showConfirmation"), EMcpParamKind::Bool, false }, { TEXT("fixupRedirectors"), EMcpParamKind::Bool, false }, { TEXT("pattern"), EMcpParamKind::String, false }, { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("assets"), EMcpParamKind::Array, false }, { TEXT("lodCount"), EMcpParamKind::Number, false }, { TEXT("numLODs"), EMcpParamKind::Number, false }, { TEXT("reductionSettings"), EMcpParamKind::Object, false }, { TEXT("meshPath"), EMcpParamKind::String, true }, { TEXT("enableNanite"), EMcpParamKind::Bool, false }, { TEXT("preserveArea"), EMcpParamKind::Bool, false }, { TEXT("trianglePercent"), EMcpParamKind::Number, false }, { TEXT("fallbackPercent"), EMcpParamKind::Number, false }, { TEXT("description"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("maxResults"), EMcpParamKind::Number, false }, { TEXT("searchActors"), EMcpParamKind::Bool, false }, { TEXT("searchComponents"), EMcpParamKind::Bool, false }, { TEXT("searchAssets"), EMcpParamKind::Bool, false }, { TEXT("nodeType"), EMcpParamKind::String, true }, { TEXT("posX"), EMcpParamKind::Number, false }, { TEXT("posY"), EMcpParamKind::Number, false }, { TEXT("color"), EMcpParamKind::Object, false }, { TEXT("texturePath"), EMcpParamKind::String, false }, { TEXT("sourceNodeId"), EMcpParamKind::String, false }, { TEXT("targetNodeId"), EMcpParamKind::String, false }, { TEXT("fromExpression"), EMcpParamKind::Number, false }, { TEXT("toExpression"), EMcpParamKind::Number, false }, { TEXT("sourcePin"), EMcpParamKind::String, false }, { TEXT("inputName"), EMcpParamKind::String, false }, { TEXT("targetPin"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("expressionIndex"), EMcpParamKind::Number, false }, { TEXT("pinName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SaveAll[] = { { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("sourcePath"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, true }, { TEXT("paths"), EMcpParamKind::Array, true }, { TEXT("assetPaths"), EMcpParamKind::Array, true }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("directoryPath"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("parentMaterial"), EMcpParamKind::String, true }, { TEXT("parameters"), EMcpParamKind::Object, false }, { TEXT("rowStruct"), EMcpParamKind::String, true }, { TEXT("rowStructPath"), EMcpParamKind::String, true }, { TEXT("dataTablePath"), EMcpParamKind::String, true }, { TEXT("tablePath"), EMcpParamKind::String, true }, { TEXT("rowName"), EMcpParamKind::String, true }, { TEXT("rowData"), EMcpParamKind::Any, false }, { TEXT("sourceText"), EMcpParamKind::String, true }, { TEXT("csv"), EMcpParamKind::String, true }, { TEXT("json"), EMcpParamKind::String, true }, { TEXT("content"), EMcpParamKind::String, true }, { TEXT("format"), EMcpParamKind::String, false }, { TEXT("recursive"), EMcpParamKind::Bool, false }, { TEXT("includeTransient"), EMcpParamKind::Bool, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, true }, { TEXT("maxDepth"), EMcpParamKind::Number, false }, { TEXT("tags"), EMcpParamKind::Array, false }, { TEXT("metadata"), EMcpParamKind::Object, false }, { TEXT("filter"), EMcpParamKind::Object, false }, { TEXT("directory"), EMcpParamKind::String, false }, { TEXT("recursivePaths"), EMcpParamKind::Bool, false }, { TEXT("pagination"), EMcpParamKind::Object, false }, { TEXT("depth"), EMcpParamKind::Number, false }, { TEXT("reportType"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("type"), EMcpParamKind::String, true }, { TEXT("checkoutFiles"), EMcpParamKind::Bool, false }, { TEXT("prefix"), EMcpParamKind::String, false }, { TEXT("suffix"), EMcpParamKind::String, false }, { TEXT("searchText"), EMcpParamKind::String, false }, { TEXT("replaceText"), EMcpParamKind::String, false }, { TEXT("folderPath"), EMcpParamKind::String, true }, { TEXT("showConfirmation"), EMcpParamKind::Bool, false }, { TEXT("fixupRedirectors"), EMcpParamKind::Bool, false }, { TEXT("pattern"), EMcpParamKind::String, false }, { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("assets"), EMcpParamKind::Array, false }, { TEXT("lodCount"), EMcpParamKind::Number, false }, { TEXT("numLODs"), EMcpParamKind::Number, false }, { TEXT("reductionSettings"), EMcpParamKind::Object, false }, { TEXT("meshPath"), EMcpParamKind::String, true }, { TEXT("enableNanite"), EMcpParamKind::Bool, false }, { TEXT("preserveArea"), EMcpParamKind::Bool, false }, { TEXT("trianglePercent"), EMcpParamKind::Number, false }, { TEXT("fallbackPercent"), EMcpParamKind::Number, false }, { TEXT("description"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("maxResults"), EMcpParamKind::Number, false }, { TEXT("searchActors"), EMcpParamKind::Bool, false }, { TEXT("searchComponents"), EMcpParamKind::Bool, false }, { TEXT("searchAssets"), EMcpParamKind::Bool, false }, { TEXT("nodeType"), EMcpParamKind::String, true }, { TEXT("posX"), EMcpParamKind::Number, false }, { TEXT("posY"), EMcpParamKind::Number, false }, { TEXT("color"), EMcpParamKind::Object, false }, { TEXT("texturePath"), EMcpParamKind::String, false }, { TEXT("sourceNodeId"), EMcpParamKind::String, false }, { TEXT("targetNodeId"), EMcpParamKind::String, false }, { TEXT("fromExpression"), EMcpParamKind::Number, false }, { TEXT("toExpression"), EMcpParamKind::Number, false }, { TEXT("sourcePin"), EMcpParamKind::String, false }, { TEXT("inputName"), EMcpParamKind::String, false }, { TEXT("targetPin"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("expressionIndex"), EMcpParamKind::Number, false }, { TEXT("pinName"), EMcpParamKind::String, false } };

// Material authoring (MaterialAuthoringHandlers.cpp)
inline const FMcpParamDecl P_CreateMaterial[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("materialDomain"), EMcpParamKind::String, false }, { TEXT("blendMode"), EMcpParamKind::String, false }, { TEXT("shadingModel"), EMcpParamKind::String, false }, { TEXT("twoSided"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetBlendMode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blendMode"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetShadingModel[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("shadingModel"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetMaterialDomain[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("materialDomain"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddTextureSample[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("texturePath"), EMcpParamKind::String, false }, { TEXT("parameterName"), EMcpParamKind::String, false }, { TEXT("samplerType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddTextureCoordinate[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("coordinateIndex"), EMcpParamKind::Number, false }, { TEXT("uTiling"), EMcpParamKind::Number, false }, { TEXT("vTiling"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddScalarParameter[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("defaultValue"), EMcpParamKind::Number, false }, { TEXT("group"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddVectorParameter[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("group"), EMcpParamKind::String, false }, { TEXT("defaultValue"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddStaticSwitchParameter[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("defaultValue"), EMcpParamKind::Bool, false }, { TEXT("group"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddMathNode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("operation"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddWorldPosition[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddVertexNormal[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddPixelDepth[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddFresnel[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddReflectionVector[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddPanner[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddRotator[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddNoise[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddVoronoi[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddIf[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddSwitch[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddCustomExpression[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("code"), EMcpParamKind::String, true }, { TEXT("outputType"), EMcpParamKind::String, false }, { TEXT("description"), EMcpParamKind::String, false }, { TEXT("inputs"), EMcpParamKind::Array, false }, { TEXT("additionalOutputs"), EMcpParamKind::Array, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConnectNodes[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("sourceNodeId"), EMcpParamKind::String, true }, { TEXT("targetNodeId"), EMcpParamKind::String, false }, { TEXT("inputName"), EMcpParamKind::String, false }, { TEXT("sourcePin"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConnectMaterialPins[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("sourceNodeId"), EMcpParamKind::String, true }, { TEXT("targetNodeId"), EMcpParamKind::String, false }, { TEXT("inputName"), EMcpParamKind::String, false }, { TEXT("sourcePin"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_DisconnectNodes[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("pinName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BreakMaterialConnections[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("pinName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateMaterialFunction[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("description"), EMcpParamKind::String, false }, { TEXT("exposeToLibrary"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddFunctionInput[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("inputName"), EMcpParamKind::String, true }, { TEXT("inputType"), EMcpParamKind::String, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("functionPath"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_AddFunctionOutput[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("inputName"), EMcpParamKind::String, true }, { TEXT("inputType"), EMcpParamKind::String, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_UseMaterialFunction[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("functionPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetMaterialFunctionInfo[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateMaterialInstance[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("parentMaterial"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetScalarParameterValue[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetVectorParameterValue[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetTextureParameterValue[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("texturePath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateLandscapeMaterial[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateDecalMaterial[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreatePostProcessMaterial[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddLandscapeLayer[] = { { TEXT("layerName"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("hardness"), EMcpParamKind::Number, false }, { TEXT("physicalMaterialPath"), EMcpParamKind::String, false }, { TEXT("noWeightBlend"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureLayerBlend[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, false }, { TEXT("layers"), EMcpParamKind::Array, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CompileMaterial[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("waitForShaders"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetMaterialInfo[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("nodeIds"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_FindNode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeType"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetNodeConnections[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeId"), EMcpParamKind::String, true }, { TEXT("direction"), EMcpParamKind::String, false }, { TEXT("depth"), EMcpParamKind::Number, false }, { TEXT("upstream"), EMcpParamKind::Bool, false }, { TEXT("downstream"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetNodeProperties[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeId"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetNodeValue[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeId"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::Number, false }, { TEXT("r"), EMcpParamKind::Number, false }, { TEXT("g"), EMcpParamKind::Number, false }, { TEXT("b"), EMcpParamKind::Number, false }, { TEXT("a"), EMcpParamKind::Number, false }, { TEXT("constA"), EMcpParamKind::Number, false }, { TEXT("constB"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetStaticSwitchParameterValue[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_DeleteNode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("nodeIds"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_UpdateCustomExpression[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeId"), EMcpParamKind::String, true }, { TEXT("code"), EMcpParamKind::String, false }, { TEXT("description"), EMcpParamKind::String, false }, { TEXT("outputType"), EMcpParamKind::String, false }, { TEXT("inputs"), EMcpParamKind::Array, false }, { TEXT("additionalOutputs"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_GetNodeChain[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("startNodeId"), EMcpParamKind::String, true }, { TEXT("endNodeId"), EMcpParamKind::String, false }, { TEXT("endPin"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetConnectedSubgraph[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("orphansOnly"), EMcpParamKind::Bool, false }, { TEXT("nodeId"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ArrangeGraph[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddMaterialNode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("nodeType"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_RebuildMaterial[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("waitForShaders"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetMaterialParameter[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("parameterType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetMaterialNodeDetails[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("nodeId"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_RemoveMaterialNode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("nodeId"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetTwoSided[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("twoSided"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };

// Texture (TextureHandlers.cpp)
inline const FMcpParamDecl P_CreateNoiseTexture[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("noiseType"), EMcpParamKind::String, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("scale"), EMcpParamKind::Number, false }, { TEXT("octaves"), EMcpParamKind::Number, false }, { TEXT("persistence"), EMcpParamKind::Number, false }, { TEXT("lacunarity"), EMcpParamKind::Number, false }, { TEXT("seed"), EMcpParamKind::Number, false }, { TEXT("seamless"), EMcpParamKind::Bool, false }, { TEXT("hdr"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateGradientTexture[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("gradientType"), EMcpParamKind::String, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("angle"), EMcpParamKind::Number, false }, { TEXT("centerX"), EMcpParamKind::Number, false }, { TEXT("centerY"), EMcpParamKind::Number, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("hdr"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("startColor"), EMcpParamKind::Object, false }, { TEXT("endColor"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreatePatternTexture[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("patternType"), EMcpParamKind::String, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("tilesX"), EMcpParamKind::Number, false }, { TEXT("tilesY"), EMcpParamKind::Number, false }, { TEXT("lineWidth"), EMcpParamKind::Number, false }, { TEXT("brickRatio"), EMcpParamKind::Number, false }, { TEXT("offset"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("primaryColor"), EMcpParamKind::Object, false }, { TEXT("secondaryColor"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateNormalFromHeight[] = { { TEXT("sourceTexture"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("strength"), EMcpParamKind::Number, false }, { TEXT("algorithm"), EMcpParamKind::String, false }, { TEXT("flipY"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("channelMode"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateAoFromMesh[] = { { TEXT("meshPath"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("sampleCount"), EMcpParamKind::Number, false }, { TEXT("rayDistance"), EMcpParamKind::Number, false }, { TEXT("bias"), EMcpParamKind::Number, false }, { TEXT("uvChannel"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ResizeTexture[] = { { TEXT("sourcePath"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("newWidth"), EMcpParamKind::Number, false }, { TEXT("newHeight"), EMcpParamKind::Number, false }, { TEXT("filterMethod"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AdjustLevels[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("inBlack"), EMcpParamKind::Number, false }, { TEXT("inWhite"), EMcpParamKind::Number, false }, { TEXT("gamma"), EMcpParamKind::Number, false }, { TEXT("outBlack"), EMcpParamKind::Number, false }, { TEXT("outWhite"), EMcpParamKind::Number, false }, { TEXT("inPlace"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AdjustCurves[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("inPlace"), EMcpParamKind::Bool, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("inputR"), EMcpParamKind::Array, false }, { TEXT("outputR"), EMcpParamKind::Array, false }, { TEXT("inputG"), EMcpParamKind::Array, false }, { TEXT("outputG"), EMcpParamKind::Array, false }, { TEXT("inputB"), EMcpParamKind::Array, false }, { TEXT("outputB"), EMcpParamKind::Array, false }, { TEXT("input"), EMcpParamKind::Array, false }, { TEXT("output"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_Blur[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_Sharpen[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("amount"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_Invert[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("inPlace"), EMcpParamKind::Bool, false }, { TEXT("invertAlpha"), EMcpParamKind::Bool, false }, { TEXT("channel"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_Desaturate[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("amount"), EMcpParamKind::Number, false }, { TEXT("inPlace"), EMcpParamKind::Bool, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ChannelPack[] = { { TEXT("redTexture"), EMcpParamKind::String, false }, { TEXT("greenTexture"), EMcpParamKind::String, false }, { TEXT("blueTexture"), EMcpParamKind::String, false }, { TEXT("alphaTexture"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ChannelExtract[] = { { TEXT("texturePath"), EMcpParamKind::String, true }, { TEXT("channel"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CombineTextures[] = { { TEXT("baseTexture"), EMcpParamKind::String, true }, { TEXT("overlayTexture"), EMcpParamKind::String, false }, { TEXT("blendTexture"), EMcpParamKind::String, false }, { TEXT("blendMode"), EMcpParamKind::String, false }, { TEXT("opacity"), EMcpParamKind::Number, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetCompressionSettings[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("compressionSettings"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetTextureGroup[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("textureGroup"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetLodBias[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("lodBias"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureVirtualTexture[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("virtualTextureStreaming"), EMcpParamKind::Bool, false }, { TEXT("tileSize"), EMcpParamKind::Number, false }, { TEXT("tileBorderSize"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetStreamingPriority[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("neverStream"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetTextureInfo[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };

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
// MCP_MA_CALL forwards to a 3-arg member; MCP_MA_ACTION_CALL passes the action literal
// to a 4-arg member (self-gating sub-handlers, the shared HandleDataTableRowOp,
// and the shared material OR-branch members that sub-dispatch on the action).

#define MCP_MA_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, Flags)           \
class FMcpCall_ManageAsset_##ClassSuffix final : public FMcpCall                         \
{                                                                                        \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl Decl{ TEXT("manage_asset"), TEXT(ActionLiteral),       \
			ParamsArray, (Flags) };                                                      \
		return Decl;                                                                     \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

#define MCP_MA_ACTION_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, Flags)          \
class FMcpCall_ManageAsset_##ClassSuffix final : public FMcpCall                         \
{                                                                                        \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl Decl{ TEXT("manage_asset"), TEXT(ActionLiteral),       \
			ParamsArray, (Flags) };                                                      \
		return Decl;                                                                     \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, TEXT(ActionLiteral), Payload, Socket);             \
	}                                                                                    \
};

// Core (AssetWorkflow / AssetQuery members)
MCP_MA_CALL(List, "list", P_List, HandleListAssets, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(Import, "import", P_Import, HandleImportAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Duplicate, "duplicate", P_Duplicate, HandleDuplicateAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Rename, "rename", P_Rename, HandleRenameAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Move, "move", P_Move, HandleMoveAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Delete, "delete", P_Delete, HandleDeleteAssets, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateFolder, "create_folder", P_CreateFolder, HandleCreateFolder, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(SearchAssets, "search_assets", P_SearchAssets, HandleSearchAssets, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetDependencies, "get_dependencies", P_GetDependencies, HandleGetDependencies, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetReferencers, "get_referencers", P_GetReferencers, HandleGetReferencers, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetAssetProperties, "get_asset_properties", P_GetAssetProperties, HandleGetAssetProperties, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(SetAssetProperty, "set_asset_property", P_SetAssetProperty, HandleSetAssetProperty, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(GetSourceControlState, "get_source_control_state", P_GetSourceControlState, HandleGetSourceControlState, EMcpCallFlags::RequiresEditor)
MCP_MA_ACTION_CALL(AnalyzeGraph, "analyze_graph", P_AnalyzeGraph, HandleAnalyzeGraph, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetAssetGraph, "get_asset_graph", P_GetAssetGraph, HandleGetAssetGraph, EMcpCallFlags::RequiresEditor)
MCP_MA_ACTION_CALL(CreateThumbnail, "create_thumbnail", P_CreateThumbnail, HandleGenerateThumbnail, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetTags, "set_tags", P_SetTags, HandleSetTags, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(GetMetadata, "get_metadata", P_GetMetadata, HandleGetMetadata, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(SetMetadata, "set_metadata", P_SetMetadata, HandleSetMetadata, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Validate, "validate", P_Validate, HandleValidateAsset, EMcpCallFlags::RequiresEditor)
MCP_MA_ACTION_CALL(FixupRedirectors, "fixup_redirectors", P_FixupRedirectors, HandleFixupRedirectors, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(FindByTag, "find_by_tag", P_FindByTag, HandleFindByTag, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GenerateReport, "generate_report", P_GenerateReport, HandleGenerateReport, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(CreateDataTable, "create_data_table", P_CreateDataTable, HandleCreateDataTable, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddDataTableRow, "add_data_table_row", P_AddDataTableRow, HandleDataTableRowOp, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(EditDataTableRow, "edit_data_table_row", P_EditDataTableRow, HandleDataTableRowOp, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(RemoveDataTableRow, "remove_data_table_row", P_RemoveDataTableRow, HandleDataTableRowOp, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(GetDataTableRows, "get_data_table_rows", P_GetDataTableRows, HandleDataTableRowOp, EMcpCallFlags::RequiresEditor)
MCP_MA_ACTION_CALL(ImportDataTable, "import_data_table", P_ImportDataTable, HandleDataTableRowOp, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateRenderTarget, "create_render_target", P_CreateRenderTarget, HandleTextureCreateRenderTarget, EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(GenerateLods, "generate_lods", P_GenerateLods, HandleGenerateLODs, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddMaterialParameter, "add_material_parameter", P_AddMaterialParameter, HandleAddMaterialParameter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(ListInstances, "list_instances", P_ListInstances, HandleListMaterialInstances, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(ResetInstanceParameters, "reset_instance_parameters", P_ResetInstanceParameters, HandleResetInstanceParameters, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Exists, "exists", P_Exists, HandleDoesAssetExist, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetMaterialStats, "get_material_stats", P_GetMaterialStats, HandleGetMaterialStats, EMcpCallFlags::RequiresEditor)
MCP_MA_ACTION_CALL(NaniteRebuildMesh, "nanite_rebuild_mesh", P_NaniteRebuildMesh, HandleNaniteRebuildMesh, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(BulkRename, "bulk_rename", P_BulkRename, HandleBulkRenameAssets, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(BulkDelete, "bulk_delete", P_BulkDelete, HandleBulkDeleteAssets, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(SourceControlCheckout, "source_control_checkout", P_SourceControlCheckout, HandleSourceControlCheckout, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(SourceControlSubmit, "source_control_submit", P_SourceControlSubmit, HandleSourceControlSubmit, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(Save, "save", P_Save, HandleSaveAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SaveAll, "save_all", P_SaveAll, HandleControlEditorSaveAll, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Material authoring (MaterialAuthoringHandlers.cpp)
MCP_MA_CALL(CreateMaterial, "create_material", P_CreateMaterial, HandleMaterialCreateMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetBlendMode, "set_blend_mode", P_SetBlendMode, HandleMaterialSetBlendMode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetShadingModel, "set_shading_model", P_SetShadingModel, HandleMaterialSetShadingModel, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetMaterialDomain, "set_material_domain", P_SetMaterialDomain, HandleMaterialSetMaterialDomain, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddTextureSample, "add_texture_sample", P_AddTextureSample, HandleMaterialAddTextureSample, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddTextureCoordinate, "add_texture_coordinate", P_AddTextureCoordinate, HandleMaterialAddTextureCoordinate, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddScalarParameter, "add_scalar_parameter", P_AddScalarParameter, HandleMaterialAddScalarParameter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddVectorParameter, "add_vector_parameter", P_AddVectorParameter, HandleMaterialAddVectorParameter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddStaticSwitchParameter, "add_static_switch_parameter", P_AddStaticSwitchParameter, HandleMaterialAddStaticSwitchParameter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddMathNode, "add_math_node", P_AddMathNode, HandleMaterialAddMathNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddWorldPosition, "add_world_position", P_AddWorldPosition, HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddVertexNormal, "add_vertex_normal", P_AddVertexNormal, HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddPixelDepth, "add_pixel_depth", P_AddPixelDepth, HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddFresnel, "add_fresnel", P_AddFresnel, HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddReflectionVector, "add_reflection_vector", P_AddReflectionVector, HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddPanner, "add_panner", P_AddPanner, HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddRotator, "add_rotator", P_AddRotator, HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddNoise, "add_noise", P_AddNoise, HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddVoronoi, "add_voronoi", P_AddVoronoi, HandleMaterialAddSimpleExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddIf, "add_if", P_AddIf, HandleMaterialAddConditional, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddSwitch, "add_switch", P_AddSwitch, HandleMaterialAddConditional, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddCustomExpression, "add_custom_expression", P_AddCustomExpression, HandleMaterialAddCustomExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(ConnectNodes, "connect_nodes", P_ConnectNodes, HandleMaterialConnectNodes, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(ConnectMaterialPins, "connect_material_pins", P_ConnectMaterialPins, HandleMaterialConnectNodes, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(DisconnectNodes, "disconnect_nodes", P_DisconnectNodes, HandleMaterialDisconnectNodes, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(BreakMaterialConnections, "break_material_connections", P_BreakMaterialConnections, HandleMaterialDisconnectNodes, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateMaterialFunction, "create_material_function", P_CreateMaterialFunction, HandleMaterialCreateMaterialFunction, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddFunctionInput, "add_function_input", P_AddFunctionInput, HandleMaterialAddFunctionIO, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(AddFunctionOutput, "add_function_output", P_AddFunctionOutput, HandleMaterialAddFunctionIO, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(UseMaterialFunction, "use_material_function", P_UseMaterialFunction, HandleMaterialUseMaterialFunction, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(GetMaterialFunctionInfo, "get_material_function_info", P_GetMaterialFunctionInfo, HandleMaterialGetMaterialFunctionInfo, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(CreateMaterialInstance, "create_material_instance", P_CreateMaterialInstance, HandleMaterialCreateMaterialInstance, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetScalarParameterValue, "set_scalar_parameter_value", P_SetScalarParameterValue, HandleMaterialSetScalarParameterValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetVectorParameterValue, "set_vector_parameter_value", P_SetVectorParameterValue, HandleMaterialSetVectorParameterValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetTextureParameterValue, "set_texture_parameter_value", P_SetTextureParameterValue, HandleMaterialSetTextureParameterValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(CreateLandscapeMaterial, "create_landscape_material", P_CreateLandscapeMaterial, HandleMaterialCreateSpecialMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(CreateDecalMaterial, "create_decal_material", P_CreateDecalMaterial, HandleMaterialCreateSpecialMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_ACTION_CALL(CreatePostProcessMaterial, "create_post_process_material", P_CreatePostProcessMaterial, HandleMaterialCreateSpecialMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddLandscapeLayer, "add_landscape_layer", P_AddLandscapeLayer, HandleMaterialAddLandscapeLayer, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(ConfigureLayerBlend, "configure_layer_blend", P_ConfigureLayerBlend, HandleMaterialConfigureLayerBlend, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(CompileMaterial, "compile_material", P_CompileMaterial, HandleMaterialCompileMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(GetMaterialInfo, "get_material_info", P_GetMaterialInfo, HandleMaterialGetMaterialInfo, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(FindNode, "find_node", P_FindNode, HandleMaterialFindNode, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetNodeConnections, "get_node_connections", P_GetNodeConnections, HandleMaterialGetNodeConnections, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetNodeProperties, "get_node_properties", P_GetNodeProperties, HandleMaterialGetNodeProperties, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(SetNodeValue, "set_node_value", P_SetNodeValue, HandleMaterialSetNodeValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetStaticSwitchParameterValue, "set_static_switch_parameter_value", P_SetStaticSwitchParameterValue, HandleMaterialSetStaticSwitchParameterValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(DeleteNode, "delete_node", P_DeleteNode, HandleMaterialDeleteNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(UpdateCustomExpression, "update_custom_expression", P_UpdateCustomExpression, HandleMaterialUpdateCustomExpression, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(GetNodeChain, "get_node_chain", P_GetNodeChain, HandleMaterialGetNodeChain, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(GetConnectedSubgraph, "get_connected_subgraph", P_GetConnectedSubgraph, HandleMaterialGetConnectedSubgraph, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(ArrangeGraph, "arrange_graph", P_ArrangeGraph, HandleMaterialArrangeGraph, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(AddMaterialNode, "add_material_node", P_AddMaterialNode, HandleMaterialAddMaterialNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(RebuildMaterial, "rebuild_material", P_RebuildMaterial, HandleMaterialCompileMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetMaterialParameter, "set_material_parameter", P_SetMaterialParameter, HandleMaterialSetMaterialParameter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(GetMaterialNodeDetails, "get_material_node_details", P_GetMaterialNodeDetails, HandleMaterialGetMaterialNodeDetails, EMcpCallFlags::RequiresEditor)
MCP_MA_CALL(RemoveMaterialNode, "remove_material_node", P_RemoveMaterialNode, HandleMaterialRemoveMaterialNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MA_CALL(SetTwoSided, "set_two_sided", P_SetTwoSided, HandleMaterialSetTwoSided, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Texture (TextureHandlers.cpp)
MCP_MA_CALL(CreateNoiseTexture, "create_noise_texture", P_CreateNoiseTexture, HandleTextureCreateNoiseTexture, EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateGradientTexture, "create_gradient_texture", P_CreateGradientTexture, HandleTextureCreateGradientTexture, EMcpCallFlags::Mutating)
MCP_MA_CALL(CreatePatternTexture, "create_pattern_texture", P_CreatePatternTexture, HandleTextureCreatePatternTexture, EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateNormalFromHeight, "create_normal_from_height", P_CreateNormalFromHeight, HandleTextureCreateNormalFromHeight, EMcpCallFlags::Mutating)
MCP_MA_CALL(CreateAoFromMesh, "create_ao_from_mesh", P_CreateAoFromMesh, HandleTextureCreateAoFromMesh, EMcpCallFlags::Mutating)
MCP_MA_CALL(ResizeTexture, "resize_texture", P_ResizeTexture, HandleTextureResizeTexture, EMcpCallFlags::Mutating)
MCP_MA_CALL(AdjustLevels, "adjust_levels", P_AdjustLevels, HandleTextureAdjustLevels, EMcpCallFlags::Mutating)
MCP_MA_CALL(AdjustCurves, "adjust_curves", P_AdjustCurves, HandleTextureAdjustCurves, EMcpCallFlags::Mutating)
MCP_MA_CALL(Blur, "blur", P_Blur, HandleTextureBlur, EMcpCallFlags::Mutating)
MCP_MA_CALL(Sharpen, "sharpen", P_Sharpen, HandleTextureSharpen, EMcpCallFlags::Mutating)
MCP_MA_CALL(Invert, "invert", P_Invert, HandleTextureInvert, EMcpCallFlags::Mutating)
MCP_MA_CALL(Desaturate, "desaturate", P_Desaturate, HandleTextureDesaturate, EMcpCallFlags::Mutating)
MCP_MA_CALL(ChannelPack, "channel_pack", P_ChannelPack, HandleTextureChannelPack, EMcpCallFlags::Mutating)
MCP_MA_CALL(ChannelExtract, "channel_extract", P_ChannelExtract, HandleTextureChannelExtract, EMcpCallFlags::Mutating)
MCP_MA_CALL(CombineTextures, "combine_textures", P_CombineTextures, HandleTextureCombineTextures, EMcpCallFlags::Mutating)
MCP_MA_CALL(SetCompressionSettings, "set_compression_settings", P_SetCompressionSettings, HandleTextureSetCompressionSettings, EMcpCallFlags::Mutating)
MCP_MA_CALL(SetTextureGroup, "set_texture_group", P_SetTextureGroup, HandleTextureSetTextureGroup, EMcpCallFlags::Mutating)
MCP_MA_CALL(SetLodBias, "set_lod_bias", P_SetLodBias, HandleTextureSetLodBias, EMcpCallFlags::Mutating)
MCP_MA_CALL(ConfigureVirtualTexture, "configure_virtual_texture", P_ConfigureVirtualTexture, HandleTextureConfigureVirtualTexture, EMcpCallFlags::Mutating)
MCP_MA_CALL(SetStreamingPriority, "set_streaming_priority", P_SetStreamingPriority, HandleTextureSetStreamingPriority, EMcpCallFlags::Mutating)
MCP_MA_CALL(GetTextureInfo, "get_texture_info", P_GetTextureInfo, HandleTextureGetTextureInfo, EMcpCallFlags::None)

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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_ConnectMaterialPins>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_DisconnectNodes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAsset_BreakMaterialConnections>());
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
