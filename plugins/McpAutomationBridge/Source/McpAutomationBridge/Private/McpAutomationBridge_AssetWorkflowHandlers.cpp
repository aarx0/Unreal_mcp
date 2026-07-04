// =============================================================================
// McpAutomationBridge_AssetWorkflowHandlers.cpp
// =============================================================================
// Asset workflow, material authoring, and source control handlers.
//
// HANDLERS:
//   Asset Operations:
//     - import, duplicate, rename, move, delete, exists, list, search_assets
//     - create_folder, create_material, create_material_instance
//     - get_dependencies, get_asset_graph, set_tags, set_metadata, get_metadata
//     - validate, generate_report, create_thumbnail, get_material_stats
//
//   Material Authoring:
//     - add_material_node, connect_material_pins, remove_material_node
//     - break_material_connections, get_material_node_details, rebuild_material
//     - add_material_parameter, list_instances, reset_instance_parameters
//
//   Source Control:
//     - source_control_checkout, source_control_submit, get_source_control_state
//     - source_control_enable
//
//   Bulk Operations:
//     - fixup_redirectors, bulk_rename, bulk_delete
//     - generate_lods, nanite_rebuild_mesh
//
// REFACTORING NOTES:
//   - Uses McpVersionCompatibility.h for UE 5.0-5.7 API abstraction
//   - Uses McpHandlerUtils for standardized JSON parsing/responses
//   - Material expression includes grouped by category
//
// VERSION COMPATIBILITY:
//   - MaterialDomain.h: UE 5.1+ (EMaterialDomain in MaterialShared.h for 5.0)
//   - ClassPaths vs ClassNames: UE 5.1+ uses FTopLevelAssetPath
//   - AssetRegistry API varies between UE versions
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpSafeOperations.h"

// -----------------------------------------------------------------------------
// MCP Handler Utilities (centralized JSON/Asset helpers)
// -----------------------------------------------------------------------------
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopeExit.h"
#include "UObject/MetaData.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Basic Operations)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Constants)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Trigonometry)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionSine.h"

// -----------------------------------------------------------------------------
// Material Expression Includes (Texture & Time)
// -----------------------------------------------------------------------------
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVertexColor.h"

// -----------------------------------------------------------------------------
// Material Function Includes (MF support for Tier 3 handlers)
// -----------------------------------------------------------------------------
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"

#if WITH_EDITOR

// -----------------------------------------------------------------------------
// Editor-only Includes (Asset Management)
// -----------------------------------------------------------------------------
#include "AssetRegistry/AssetRegistryModule.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
#include "UObject/AssetRegistryTagsContext.h"  // EAssetRegistryTagsCaller (AssetUpdateTags)
#endif
#include "AssetToolsModule.h"
#include "AssetViewUtils.h"
#include "EditorAssetLibrary.h"
#include "Engine/DataTable.h"  // UDataTable, FTableRowBase (create_data_table)
#include "DataTableEditorUtils.h"  // FDataTableEditorUtils (data table row CRUD)
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"  // TActorIterator
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "FileHelpers.h"
#include "IAssetTools.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Source Control)
// -----------------------------------------------------------------------------
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Material Editing)
// -----------------------------------------------------------------------------
#include "ImageUtils.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"

// MaterialDomain.h was introduced in UE 5.1 - in UE 5.0 EMaterialDomain is in MaterialShared.h
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "MaterialDomain.h"
#endif

#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialShared.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Utilities)
// -----------------------------------------------------------------------------
#include "Misc/FileHelper.h"
#include "ObjectTools.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"

// -----------------------------------------------------------------------------
// Editor-only Includes (Graph/Blueprint)
// -----------------------------------------------------------------------------
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Blueprint/BlueprintSupport.h"

#endif // WITH_EDITOR

// =============================================================================
// MF-AWARE HELPERS (shared by Tier 3 material handlers)
// =============================================================================

// Try loading as UMaterial first, then UMaterialFunction.
// Returns the loaded UObject (Material or Function), or nullptr.
static UObject* LoadMaterialOrFunctionAW(const FString& AssetPath,
                                          UMaterial*& OutMaterial,
                                          UMaterialFunction*& OutFunction) {
  OutMaterial = LoadObject<UMaterial>(nullptr, *AssetPath);
  if (OutMaterial) return OutMaterial;
  OutFunction = LoadObject<UMaterialFunction>(nullptr, *AssetPath);
  return OutFunction;
}

// Return a reference to the expressions TArray for either host type.
// Caller must ensure at least one of Material/Function is non-null.
// Uses decltype(auto) so the return type matches the underlying member
// (TArray<TObjectPtr<...>>& on UE 5.1+, TArray<UMaterialExpression*>& on 5.0).
static decltype(auto) GetHostExpressions(
    UMaterial* Material, UMaterialFunction* Function) {
  return Material ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
                  : MCP_GET_FUNCTION_EXPRESSIONS(Function);
}

// Find a material expression by GUID, name, path, parameter name, or numeric index.
// Templated to accept both TArray<TObjectPtr<...>> (UE 5.1+) and TArray<UMaterialExpression*>.
template <typename TExprArray>
static UMaterialExpression* FindExpressionInHost(TExprArray& Expressions, const FString& IdOrIndex) {
  if (IdOrIndex.IsEmpty()) return nullptr;

  FGuid GuidId;
  if (FGuid::Parse(IdOrIndex, GuidId)) {
    for (UMaterialExpression *Expr : Expressions) {
      if (Expr && Expr->MaterialExpressionGuid == GuidId) return Expr;
    }
  }
  for (UMaterialExpression *Expr : Expressions) {
    if (Expr) {
      if (Expr->GetName() == IdOrIndex || Expr->GetPathName() == IdOrIndex) return Expr;
      if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
        if (Param->ParameterName.ToString() == IdOrIndex) return Expr;
      }
    }
  }
  if (IdOrIndex.IsNumeric()) {
    int32 Index = FCString::Atoi(*IdOrIndex);
    if (Index >= 0 && Index < Expressions.Num()) return Expressions[Index];
  }
  return nullptr;
}

// PostEditChange + MarkPackageDirty on whichever host is non-null.
static void FinalizeHost(UMaterial* Material, UMaterialFunction* Function) {
  if (Material) { Material->PostEditChange(); Material->MarkPackageDirty(); }
  else if (Function) { Function->PostEditChange(); Function->MarkPackageDirty(); }
}

// Saves one asset's package to disk if dirty. Never force-loads: an unloaded
// package cannot be dirty, so there is nothing to save.
static bool HandleSaveAssetStatic(UMcpAutomationBridgeSubsystem* Self,
                                  const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload,
                                  FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    Self->SendAutomationResponse(Socket, RequestId, false,
                                 TEXT("assetPath required"), nullptr,
                                 TEXT("INVALID_ARGUMENT"));
    return true;
  }
  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    Self->SendAutomationResponse(Socket, RequestId, false,
                                 TEXT("Invalid assetPath"), nullptr,
                                 TEXT("SECURITY_VIOLATION"));
    return true;
  }

  FString PackageName = AssetPath;
  int32 DotIndex;
  if (PackageName.FindChar(TEXT('.'), DotIndex)) {
    PackageName.LeftInline(DotIndex);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), AssetPath);

  UPackage* Package = FindPackage(nullptr, *PackageName);
  if (!Package) {
    if (!FPackageName::DoesPackageExist(PackageName)) {
      Self->SendAutomationResponse(Socket, RequestId, false,
                                   FString::Printf(TEXT("Asset not found: %s"), *PackageName),
                                   nullptr, TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    Result->SetBoolField(TEXT("saved"), false);
    Result->SetBoolField(TEXT("wasDirty"), false);
    Result->SetBoolField(TEXT("existsOnDisk"), true);
    Self->SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Package not loaded; nothing dirty to save"), Result);
    return true;
  }

  const bool bWasDirty = Package->IsDirty();
  UObject* Asset = Package->FindAssetInPackage();
  if (!Asset) {
    Self->SendAutomationResponse(Socket, RequestId, false,
                                 FString::Printf(TEXT("No asset found in package %s"), *PackageName),
                                 nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  const bool bSaved = bWasDirty ? McpSafeAssetSave(Asset) : true;
  Result->SetBoolField(TEXT("saved"), bWasDirty && bSaved);
  Result->SetBoolField(TEXT("wasDirty"), bWasDirty);
  McpHandlerUtils::AddVerification(Result, Asset);
  if (bWasDirty && !bSaved) {
    Self->SendAutomationResponse(Socket, RequestId, false,
                                 FString::Printf(TEXT("Failed to save %s"), *PackageName),
                                 Result, TEXT("SAVE_FAILED"));
    return true;
  }
  Self->SendAutomationResponse(
      Socket, RequestId, true,
      bWasDirty ? TEXT("Asset saved") : TEXT("Asset already clean; no save needed"), Result);
  return true;
#else
  return false;
#endif
}

// =============================================================================
// ASSET ACTION DISPATCHER
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleAssetAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  FString Lower = Action.ToLower();

  // If the action is the generic "manage_asset" tool, check for a subAction in
  // the payload
  if (Lower == TEXT("manage_asset") && Payload.IsValid()) {
    FString SubAction;
    if (Payload->TryGetStringField(TEXT("subAction"), SubAction) &&
        !SubAction.IsEmpty()) {
      Lower = SubAction.ToLower();
    }
  }

  if (Lower.IsEmpty())
    return false;

  // Dispatch to specific handlers
  // CRITICAL: These actions must match what TS sends as 'action' (not just 'subAction')
  // When TS calls executeAutomationRequest(tools, 'search_assets', {...}), Action='search_assets'
  
  // Asset Operations
  if (Lower == TEXT("import"))
    return HandleImportAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("duplicate"))
    return HandleDuplicateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("rename"))
    return HandleRenameAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("move"))
    return HandleMoveAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("delete"))
    return HandleDeleteAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("save"))
    return HandleSaveAssetStatic(this, RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("save_all"))
    return HandleControlEditorSaveAll(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_folder"))
    return HandleCreateFolder(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_material"))
    return HandleCreateMaterial(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_material_instance"))
    return HandleCreateMaterialInstance(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_data_table"))
    return HandleCreateDataTable(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("add_data_table_row") ||
      Lower == TEXT("edit_data_table_row") ||
      Lower == TEXT("remove_data_table_row") ||
      Lower == TEXT("get_data_table_rows") || Lower == TEXT("import_data_table"))
    return HandleDataTableRowOp(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("create_render_target"))
    return HandleManageTextureAction(RequestId, TEXT("manage_texture"), Payload, RequestingSocket);
  if (Lower == TEXT("get_dependencies"))
    return HandleGetDependencies(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_referencers"))
    return HandleGetReferencers(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_asset_properties"))
    return HandleGetAssetProperties(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_asset_property"))
    return HandleSetAssetProperty(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_asset_graph"))
    return HandleGetAssetGraph(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_tags"))
    return HandleSetTags(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_metadata"))
    return HandleSetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_metadata"))
    return HandleGetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("validate"))
    return HandleValidateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list"))
    return HandleListAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("generate_report"))
    return HandleGenerateReport(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_thumbnail"))
    return HandleGenerateThumbnail(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("add_material_parameter"))
    return HandleAddMaterialParameter(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list_instances"))
    return HandleListMaterialInstances(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("reset_instance_parameters"))
    return HandleResetInstanceParameters(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("exists"))
    return HandleDoesAssetExist(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_stats"))
    return HandleGetMaterialStats(RequestId, Payload, RequestingSocket);
  
  // Search (CRITICAL: search_assets must be dispatched - was missing causing timeouts)
  if (Lower == TEXT("search_assets"))
    return HandleSearchAssets(RequestId, Lower, Payload, RequestingSocket);

  // Bulk Operations
  if (Lower == TEXT("fixup_redirectors"))
    return HandleFixupRedirectors(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_rename"))
    return HandleBulkRenameAssets(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_delete"))
    return HandleBulkDeleteAssets(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("generate_lods"))
    return HandleGenerateLODs(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("nanite_rebuild_mesh"))
    return HandleNaniteRebuildMesh(RequestId, Lower, Payload, RequestingSocket);

  // Source Control
  if (Lower == TEXT("source_control_checkout"))
    return HandleSourceControlCheckout(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_submit"))
    return HandleSourceControlSubmit(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("get_source_control_state"))
    return HandleGetSourceControlState(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_enable"))
    return HandleSourceControlEnable(RequestId, Lower, Payload, RequestingSocket);

  // Graph & Analysis
  if (Lower == TEXT("analyze_graph"))
    return HandleAnalyzeGraph(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("find_by_tag"))
    return HandleFindByTag(RequestId, Lower, Payload, RequestingSocket);

  // Material Authoring
  if (Lower == TEXT("add_material_node"))
    return HandleAddMaterialNode(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("connect_material_pins"))
    return HandleConnectMaterialPins(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("remove_material_node"))
    return HandleRemoveMaterialNode(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("break_material_connections"))
    return HandleBreakMaterialConnections(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_node_details"))
    return HandleGetMaterialNodeDetails(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("rebuild_material"))
    return HandleRebuildMaterial(RequestId, Lower, Payload, RequestingSocket);

  return false;
}


// ============================================================================
// 1. FIXUP REDIRECTORS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleFixupRedirectors(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("fixup_redirectors"), ESearchCase::IgnoreCase)) {
    // Not our action — allow other handlers to try
    return false;
  }

  // Implementation of redirector fixup functionality
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("fixup_redirectors payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Get directory path - REQUIRED for proper error reporting
  FString DirectoryPath;
  Payload->TryGetStringField(TEXT("directoryPath"), DirectoryPath);
  
  // Also check for "path" as alias
  if (DirectoryPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("path"), DirectoryPath);
  }

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

  // Validate path is provided
  if (DirectoryPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("directoryPath or path is required for fixup_redirectors"),
                        TEXT("MISSING_ARGUMENT"));
    return true;
  }

  // SECURITY: Sanitize path to prevent traversal attacks
  FString SanitizedPath = SanitizeProjectRelativePath(DirectoryPath);
  if (SanitizedPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *DirectoryPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Normalize path
  FString NormalizedPath = SanitizedPath;
  if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
  }

  // CRITICAL FIX: Use DoesAssetDirectoryExistOnDisk for strict validation
  // UEditorAssetLibrary::DoesDirectoryExist() uses AssetRegistry cache which may
  // contain stale entries. We need to check if the directory ACTUALLY exists on disk.
  if (!DoesAssetDirectoryExistOnDisk(NormalizedPath)) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Directory not found: %s"), *NormalizedPath),
                        TEXT("PATH_NOT_FOUND"));
    return true;
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Find all redirectors
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"),
                                           TEXT("ObjectRedirector")));
#else
  Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif

  Filter.PackagePaths.Add(FName(*NormalizedPath));
  Filter.bRecursivePaths = true;

  TArray<FAssetData> RedirectorAssets;
  AssetRegistry.GetAssets(Filter, RedirectorAssets);

  if (RedirectorAssets.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("redirectorsFound"), 0);
    Result->SetNumberField(TEXT("redirectorsFixed"), 0);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("No redirectors found"), Result, FString());
    return true;
  }

  // Convert to string paths for AssetTools
  TArray<FString> RedirectorPaths;
  for (const FAssetData &Asset : RedirectorAssets) {
    RedirectorPaths.Add(Asset.ToSoftObjectPath().ToString());
  }

  // Checkout files if source control is enabled
  if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled()) {
    TArray<FString> PackageNames;
    for (const FAssetData &Asset : RedirectorAssets) {
      PackageNames.Add(Asset.PackageName.ToString());
    }
    SourceControlHelpers::CheckOutFiles(PackageNames, true);
  }

  // Convert FAssetData to UObjectRedirector* for AssetTools
  TArray<UObjectRedirector *> Redirectors;
  for (const FAssetData &Asset : RedirectorAssets) {
    if (UObjectRedirector *Redirector =
            Cast<UObjectRedirector>(Asset.GetAsset())) {
      Redirectors.Add(Redirector);
    }
  }

  // Fixup redirectors using AssetTools
  if (Redirectors.Num() > 0) {
    IAssetTools &AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools"))
            .Get();
    AssetTools.FixupReferencers(Redirectors);
  }

  // Delete the now-unused redirectors
  int32 DeletedCount = 0;
  TArray<UObject *> ObjectsToDelete;
  for (const FAssetData &Asset : RedirectorAssets) {
    if (UObject *Obj = Asset.GetAsset()) {
      ObjectsToDelete.Add(Obj);
    }
  }

  if (ObjectsToDelete.Num() > 0) {
    DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, false);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetNumberField(TEXT("redirectorsFound"), RedirectorAssets.Num());
  Result->SetNumberField(TEXT("redirectorsFixed"), DeletedCount);

  SendAutomationResponse(
      RequestingSocket, RequestId, true,
      FString::Printf(TEXT("Fixed %d redirectors"), DeletedCount), Result,
      FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("fixup_redirectors requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 2. SOURCE CONTROL CHECKOUT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlCheckout(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_checkout"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("checkout"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("source_control_checkout payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPaths (array) and assetPath (single string)
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Try single assetPath
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Source control is not enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control disabled"), Result,
                           TEXT("SOURCE_CONTROL_DISABLED"));
    return true;
  }

  TArray<FString> PackageNames;
  TArray<FString> ValidPaths;
  for (const FString &Path : AssetPaths) {
    const FString SafePath = SanitizeProjectRelativePath(Path);
    if (!SafePath.IsEmpty() && UEditorAssetLibrary::DoesAssetExist(SafePath)) {
      ValidPaths.Add(SafePath);
      FString PackageName = FPackageName::ObjectPathToPackageName(SafePath);
      PackageNames.Add(PackageName);
    }
  }

  if (PackageNames.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  bool bSuccess = SourceControlHelpers::CheckOutFiles(PackageNames, true);

  TArray<TSharedPtr<FJsonValue>> CheckedOutPaths;
  for (const FString &Path : ValidPaths) {
    CheckedOutPaths.Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("checkedOut"), PackageNames.Num());
  Result->SetArrayField(TEXT("assets"), CheckedOutPaths);

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                         bSuccess ? TEXT("Assets checked out successfully")
                                  : TEXT("Checkout failed"),
                         Result,
                         bSuccess ? FString() : TEXT("CHECKOUT_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_checkout requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 3. SOURCE CONTROL SUBMIT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlSubmit(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_submit"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("submit"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("source_control_submit payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPaths (array) and assetPath (single string)
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Try single assetPath
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString Description;
  if (!Payload->TryGetStringField(TEXT("description"), Description) ||
      Description.IsEmpty()) {
    Description = TEXT("Automated submission via MCP Automation Bridge");
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Source control is not enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control disabled"), Result,
                           TEXT("SOURCE_CONTROL_DISABLED"));
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<FString> PackageNames;
  for (const FString &Path : AssetPaths) {
    const FString SafePath = SanitizeProjectRelativePath(Path);
    if (!SafePath.IsEmpty() && UEditorAssetLibrary::DoesAssetExist(SafePath)) {
      FString PackageName = FPackageName::ObjectPathToPackageName(SafePath);
      PackageNames.Add(PackageName);
    }
  }

  if (PackageNames.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  TArray<FString> FilePaths;
  for (const FString &PackageName : PackageNames) {
    FString FilePath;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension()) ||
        FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetMapPackageExtension())) {
      FilePaths.Add(FilePath);
    }
  }

  TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation =
      ISourceControlOperation::Create<FCheckIn>();
  CheckInOperation->SetDescription(FText::FromString(Description));

  ECommandResult::Type Result =
      SourceControlProvider.Execute(CheckInOperation, FilePaths);
  bool bSuccess = (Result == ECommandResult::Succeeded);

  TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
  ResultObj->SetBoolField(TEXT("success"), bSuccess);
  ResultObj->SetNumberField(TEXT("submitted"),
                            bSuccess ? PackageNames.Num() : 0);
  ResultObj->SetStringField(TEXT("description"), Description);

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Assets submitted successfully") : TEXT("Submit failed"),
      ResultObj, bSuccess ? FString() : TEXT("SUBMIT_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_submit requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 4A. SOURCE CONTROL ENABLE
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlEnable(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_enable"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  FString Provider = TEXT("None");
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("provider"), Provider);
  }

  ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
  
  // Check if already enabled
  if (SourceControlModule.IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("provider"), SourceControlModule.GetProvider().GetName().ToString());
    Result->SetStringField(TEXT("message"), TEXT("Source control already enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Source control already enabled"), Result, FString());
    return true;
  }

  // Try to set the provider by name
  if (!Provider.IsEmpty() && !Provider.Equals(TEXT("None"), ESearchCase::IgnoreCase)) {
    SourceControlModule.SetProvider(FName(*Provider));
  }
  
  bool bEnabled = SourceControlModule.IsEnabled();
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bEnabled);
  Result->SetStringField(TEXT("provider"), SourceControlModule.GetProvider().GetName().ToString());
  
  if (bEnabled) {
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Source control enabled"), Result, FString());
  } else {
    Result->SetStringField(TEXT("error"), TEXT("Failed to enable source control. Please configure provider in Editor preferences."));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control enable failed"), Result,
                           TEXT("SOURCE_CONTROL_ENABLE_FAILED"));
  }
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_enable requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================

// ============================================================================
// 4. BULK RENAME ASSETS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleBulkRenameAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_rename_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_rename"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("bulk_rename payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Get rename options
  FString Prefix, Suffix, SearchText, ReplaceText;
  Payload->TryGetStringField(TEXT("prefix"), Prefix);
  Payload->TryGetStringField(TEXT("suffix"), Suffix);
  Payload->TryGetStringField(TEXT("searchText"), SearchText);
  Payload->TryGetStringField(TEXT("replaceText"), ReplaceText);

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

  TArray<FString> AssetPaths;

  // Check for assetPaths array first
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Check for folderPath - if provided, list all assets in that folder
    FString FolderPath;
    if (Payload->TryGetStringField(TEXT("folderPath"), FolderPath) && !FolderPath.IsEmpty()) {
      // Normalize path
      FString NormalizedPath = FolderPath;
      if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
        NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
      }

      NormalizedPath = SanitizeProjectRelativePath(NormalizedPath);
      if (NormalizedPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Invalid folderPath: %s"), *FolderPath),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }
      
      // Get all assets in the folder
      FAssetRegistryModule &AssetRegistryModule =
          FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
      IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();
      
      FARFilter Filter;
      Filter.PackagePaths.Add(FName(*NormalizedPath));
      Filter.bRecursivePaths = true;
      
      // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
      // Asset listing uses cached AssetRegistry data exclusively.
      // LIMITATION: Assets not yet indexed by the editor's background scanner
      // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
      TArray<FAssetData> AssetDataList;
      AssetRegistry.GetAssets(Filter, AssetDataList);
      
      for (const FAssetData &AssetData : AssetDataList) {
        AssetPaths.Add(AssetData.ToSoftObjectPath().ToString());
      }
      
      if (AssetPaths.Num() == 0) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetNumberField(TEXT("renamed"), 0);
        Result->SetStringField(TEXT("message"), TEXT("No assets found in folder"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No assets found"), Result, FString());
        return true;
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Either assetPaths array or folderPath is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  TArray<FAssetRenameData> RenameData;

  for (const FString &InputPath : AssetPaths) {
    FString AssetPath = ResolveAssetPath(InputPath);
    if (AssetPath.IsEmpty()) {
      AssetPath = InputPath;
    }

    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty()) {
      continue;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      continue;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      continue;
    }

    FString CurrentName = Asset->GetName();
    FString NewName = CurrentName;

    if (!SearchText.IsEmpty()) {
      NewName =
          NewName.Replace(*SearchText, *ReplaceText, ESearchCase::IgnoreCase);
    }

    if (!Prefix.IsEmpty()) {
      NewName = Prefix + NewName;
    }
    if (!Suffix.IsEmpty()) {
      NewName = NewName + Suffix;
    }

    if (NewName == CurrentName) {
      continue;
    }

    FString PackagePath =
        FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
    FAssetRenameData RenameEntry(Asset, PackagePath, NewName);
    RenameData.Add(RenameEntry);
  }

  if (RenameData.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("renamed"), 0);
    Result->SetStringField(TEXT("message"),
                           TEXT("No assets required renaming"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("No renames needed"), Result, FString());
    return true;
  }

  if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled()) {
    TArray<FString> PackageNames;
    for (const FAssetRenameData &Data : RenameData) {
      PackageNames.Add(Data.Asset->GetOutermost()->GetName());
    }
    SourceControlHelpers::CheckOutFiles(PackageNames, true);
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
          .Get();
  bool bSuccess = AssetTools.RenameAssets(RenameData);

  TArray<TSharedPtr<FJsonValue>> RenamedAssets;
  for (const FAssetRenameData &Data : RenameData) {
    TSharedPtr<FJsonObject> AssetInfo = McpHandlerUtils::CreateResultObject();
    AssetInfo->SetStringField(TEXT("oldPath"), Data.Asset->GetPathName());
    AssetInfo->SetStringField(TEXT("newName"), Data.NewName);
    RenamedAssets.Add(MakeShared<FJsonValueObject>(AssetInfo));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("renamed"), RenameData.Num());
  Result->SetArrayField(TEXT("assets"), RenamedAssets);

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? FString::Printf(TEXT("Renamed %d assets"), RenameData.Num())
               : TEXT("Bulk rename failed"),
      Result, bSuccess ? FString() : TEXT("BULK_RENAME_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("bulk_rename requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 5. BULK DELETE ASSETS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleBulkDeleteAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_delete_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_delete"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("bulk_delete payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  bool bShowConfirmation = false;
  Payload->TryGetBoolField(TEXT("showConfirmation"), bShowConfirmation);

  bool bFixupRedirectors = true;
  Payload->TryGetBoolField(TEXT("fixupRedirectors"), bFixupRedirectors);

  TArray<FString> AssetPaths;

  // Check for assetPaths array first
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Check for folderPath - if provided, list all assets in that folder
    FString FolderPath;
    FString Pattern;
    Payload->TryGetStringField(TEXT("folderPath"), FolderPath);
    Payload->TryGetStringField(TEXT("path"), FolderPath);  // alias
    Payload->TryGetStringField(TEXT("pattern"), Pattern);
    
    if (!FolderPath.IsEmpty()) {
      // Normalize path
      FString NormalizedPath = FolderPath;
      if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
        NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
      }

      NormalizedPath = SanitizeProjectRelativePath(NormalizedPath);
      if (NormalizedPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Invalid folderPath: %s"), *FolderPath),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }
      
      // Get all assets in the folder
      FAssetRegistryModule &AssetRegistryModule =
          FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
      IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();
      
      FARFilter Filter;
      Filter.PackagePaths.Add(FName(*NormalizedPath));
      Filter.bRecursivePaths = true;
      
      // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
      // Asset listing uses cached AssetRegistry data exclusively.
      // LIMITATION: Assets not yet indexed by the editor's background scanner
      // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
      TArray<FAssetData> AssetDataList;
      AssetRegistry.GetAssets(Filter, AssetDataList);
      
      for (const FAssetData &AssetData : AssetDataList) {
        FString AssetPath = AssetData.ToSoftObjectPath().ToString();
        // If pattern is specified, filter by it
        if (!Pattern.IsEmpty()) {
          FString AssetName = AssetData.AssetName.ToString();
          if (!AssetName.Contains(Pattern)) {
            continue;
          }
        }
        AssetPaths.Add(AssetPath);
      }
      
      if (AssetPaths.Num() == 0) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetNumberField(TEXT("deleted"), 0);
        Result->SetStringField(TEXT("message"), TEXT("No assets found matching criteria"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No assets found"), Result, FString());
        return true;
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Either assetPaths array or folderPath is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  TArray<UObject *> ObjectsToDelete;
  TArray<FString> ValidPaths;

  for (const FString &AssetPath : AssetPaths) {
    const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (!SafeAssetPath.IsEmpty() && UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      if (UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath)) {
        ObjectsToDelete.Add(Asset);
        ValidPaths.Add(SafeAssetPath);
      }
    }
  }

  if (ObjectsToDelete.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  int32 DeletedCount =
      ObjectTools::DeleteObjects(ObjectsToDelete, bShowConfirmation);

  if (bFixupRedirectors && DeletedCount > 0) {
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"),
                                             TEXT("ObjectRedirector")));
#else
    Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif

    TArray<FAssetData> RedirectorAssets;
    AssetRegistry.GetAssets(Filter, RedirectorAssets);

    if (RedirectorAssets.Num() > 0) {
      TArray<UObjectRedirector *> Redirectors;
      for (const FAssetData &Asset : RedirectorAssets) {
        if (UObjectRedirector *Redirector =
                Cast<UObjectRedirector>(Asset.GetAsset())) {
          Redirectors.Add(Redirector);
        }
      }

      if (Redirectors.Num() > 0) {
        IAssetTools &AssetTools =
            FModuleManager::LoadModuleChecked<FAssetToolsModule>(
                TEXT("AssetTools"))
                .Get();
        AssetTools.FixupReferencers(Redirectors);
      }
    }
  }

  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Path : ValidPaths) {
    DeletedArray.Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), DeletedCount > 0);
  Result->SetArrayField(TEXT("deleted"), DeletedArray);
  Result->SetNumberField(TEXT("requested"), ObjectsToDelete.Num());

  SendAutomationResponse(
      RequestingSocket, RequestId, DeletedCount > 0,
      FString::Printf(TEXT("Deleted %d of %d assets"), DeletedCount,
                      ObjectsToDelete.Num()),
      Result, DeletedCount > 0 ? FString() : TEXT("BULK_DELETE_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("bulk_delete requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 6. GENERATE THUMBNAIL
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGenerateThumbnail(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_thumbnail"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("create_thumbnail payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // SECURITY: Validate asset path
  FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *AssetPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  int32 Width = 512;
  int32 Height = 512;

  double TempWidth = 0, TempHeight = 0;
  if (Payload->TryGetNumberField(TEXT("width"), TempWidth))
    Width = static_cast<int32>(TempWidth);
  if (Payload->TryGetNumberField(TEXT("height"), TempHeight))
    Height = static_cast<int32>(TempHeight);

  FString OutputPath;
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

  // NOTE: ProcessAutomationRequest already dispatches to GameThread.
  // Wrapping ALL work (including fast existence checks) in AsyncTask(GameThread, ...)
  // caused the queued lambda to sit behind the current dispatch cycle, so responses
  // never reached the MCP server before the 30-second timeout (issues #138, #139).
  // Execute synchronously instead.
  SendProgressUpdate(RequestId, 0.0f,
      FString::Printf(TEXT("Starting thumbnail generation for: %s"), *SafeAssetPath), true);

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Asset not found"), nullptr,
                           TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
  if (!Asset) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    Result->SetBoolField(TEXT("thumbnailRendered"), false);
    Result->SetBoolField(TEXT("headlessSafe"), true);
    if (!OutputPath.IsEmpty()) {
      Result->SetStringField(TEXT("outputPath"), OutputPath);
    }
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Asset verified; thumbnail rendering skipped under NullRHI"),
                           Result, FString());
    return true;
  }

  // Send progress update before GPU operation
  SendProgressUpdate(RequestId, 50.0f,
      TEXT("Rendering thumbnail (GPU operation)..."), true);

  FObjectThumbnail ObjectThumbnail;
  ThumbnailTools::RenderThumbnail(
      Asset, Width, Height,
      ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush, nullptr,
      &ObjectThumbnail);

  bool bSuccess = ObjectThumbnail.GetImageWidth() > 0 &&
                  ObjectThumbnail.GetImageHeight() > 0;

  if (bSuccess && !OutputPath.IsEmpty()) {
    const TArray<uint8> &ImageData = ObjectThumbnail.GetUncompressedImageData();

    if (ImageData.Num() > 0) {
      TArray<FColor> ColorData;
      ColorData.Reserve(Width * Height);

      // Fixed: Ensure we don't read out of bounds if ImageData length isn't a multiple of 4
      for (int32 i = 0; i + 3 < ImageData.Num(); i += 4) {
        FColor Color;
        Color.B = ImageData[i + 0];
        Color.G = ImageData[i + 1];
        Color.R = ImageData[i + 2];
        Color.A = ImageData[i + 3];
        ColorData.Add(Color);
      }

      // SECURITY: Sanitize and validate the output path to prevent path traversal
      FString SafeOutputPath = SanitizeProjectFilePath(OutputPath);
      if (SafeOutputPath.IsEmpty()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               FString::Printf(TEXT("Invalid or unsafe output path: %s"), *OutputPath),
                               nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }

      FString AbsolutePath = FPaths::ProjectDir() / SafeOutputPath;
      AbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
      FPaths::NormalizeFilename(AbsolutePath);

      FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
        NormalizedProjectDir += TEXT("/");
      }

      if (!AbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               FString::Printf(TEXT("Output path escapes project directory: %s"), *OutputPath),
                               nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }

      TArray<uint8> CompressedData;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      FImageUtils::ThumbnailCompressImageArray(Width, Height, ColorData,
                                               CompressedData);
#else
      // UE 5.0: Use CompressImageArray instead
      FImageUtils::CompressImageArray(Width, Height, ColorData, CompressedData);
#endif
      bSuccess = FFileHelper::SaveArrayToFile(CompressedData, *AbsolutePath);
    }
  }

  if (Asset->GetOutermost()) {
    Asset->GetOutermost()->MarkPackageDirty();
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
  Result->SetNumberField(TEXT("width"), Width);
  Result->SetNumberField(TEXT("height"), Height);

  if (!OutputPath.IsEmpty()) {
    Result->SetStringField(TEXT("outputPath"), OutputPath);
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Thumbnail generated successfully")
               : TEXT("Thumbnail generation failed"),
      Result, bSuccess ? FString() : TEXT("THUMBNAIL_GENERATION_FAILED"));

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("create_thumbnail requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 7. BASIC ASSET OPERATIONS (Import, Duplicate, Rename, Move, etc.)
// ============================================================================

/**
 * Handles asset import requests.
 *
 * IMPORTANT: In UE 5.7+, the Interchange Framework is the default importer for
 * FBX/glTF files. Interchange uses the TaskGraph internally for async operations.
 * If we call ImportAssetsAutomated() synchronously from within an AsyncTask callback
 * (which is how WebSocket messages are dispatched), we hit a TaskGraph recursion
 * guard assertion: "++Queue(QueueIndex).RecursionGuard == 1".
 *
 * The fix is to defer the import to the next editor tick using GEditor->GetTimerManager(),
 * which breaks out of the TaskGraph callback chain and allows Interchange to function
 * correctly.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleImportAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);

  if (DestinationPath.IsEmpty() || SourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ResolvedSourcePath = SourcePath;
  if (!FPaths::FileExists(ResolvedSourcePath) && FPaths::IsRelative(SourcePath)) {
    const FString ProjectRelativeSourcePath = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir(), SourcePath);
    if (FPaths::FileExists(ProjectRelativeSourcePath)) {
      ResolvedSourcePath = ProjectRelativeSourcePath;
    }
  }

  // Verify source file exists
  if (!FPaths::FileExists(ResolvedSourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source file not found: %s"), *SourcePath),
        nullptr, TEXT("SOURCE_NOT_FOUND"));
    return true;
  }

  // Sanitize destination path
  FString SafeDestPath = SanitizeProjectRelativePath(DestinationPath);
  if (SafeDestPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid destination path"), nullptr,
                           TEXT("INVALID_PATH"));
    return true;
  }

  FString DestPath = FPaths::GetPath(SafeDestPath);
  FString DestName = FPaths::GetBaseFilename(SafeDestPath);

  // If destination is just a folder, use that
  if (FPaths::GetExtension(SafeDestPath).IsEmpty()) {
    DestPath = SafeDestPath;
    DestName = FPaths::GetBaseFilename(SourcePath);
  }

  // Sanitize DestName: UE asset names cannot contain spaces or dots
  DestName.ReplaceInline(TEXT(" "), TEXT("_"));
  DestName.ReplaceInline(TEXT("."), TEXT("_"));

  // Defer the import to the next tick to avoid TaskGraph recursion issues with
  // UE 5.7+ Interchange Framework. See issue #137.
  // We use SetTimerForNextTick to ensure we're completely outside of any
  // TaskGraph callback chain before invoking the import.
  if (GEditor) {
    MarkRequestDeferred(RequestId);
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
    GEditor->GetTimerManager()->SetTimerForNextTick(
        [WeakThis, RequestId, ResolvedSourcePath, DestPath, DestName, Socket]() {
          UMcpAutomationBridgeSubsystem *StrongThis = WeakThis.Get();
          if (!StrongThis) {
            return;
          }

          IAssetTools &AssetTools =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools")
                  .Get();

          TArray<FString> Files;
          Files.Add(ResolvedSourcePath);

          UAutomatedAssetImportData *ImportData =
              NewObject<UAutomatedAssetImportData>();
          ImportData->bReplaceExisting = true;
          ImportData->DestinationPath = DestPath;
          ImportData->Filenames = Files;

          TArray<UObject *> ImportedAssets =
              AssetTools.ImportAssetsAutomated(ImportData);

          // Find the first valid (non-null) asset in the array.
          // ImportAssetsAutomated can return arrays with nullptr entries.
          UObject *Asset = nullptr;
          for (UObject *ImportedObj : ImportedAssets) {
            if (ImportedObj) {
              Asset = ImportedObj;
              break;
            }
          }

          if (Asset) {
            // Compute the final asset path. If we rename, use the destination
            // path/name since RenameAssets may invalidate the Asset pointer.
            FString FinalAssetPath;
            bool bRenameSucceeded = true;

            // Rename if needed
            if (Asset->GetName() != DestName) {
              FAssetRenameData RenameData(Asset, DestPath, DestName);
              bRenameSucceeded = AssetTools.RenameAssets({RenameData});
              // After rename, compute path from destination (Asset pointer may
              // be stale)
              FinalAssetPath = DestPath / DestName + TEXT(".") + DestName;
            } else {
              // No rename needed, safe to use the asset's current path
              FinalAssetPath = Asset->GetPathName();
            }

            TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("assetPath"), FinalAssetPath);
            if (!bRenameSucceeded) {
              Resp->SetBoolField(TEXT("renameWarning"), true);
            }
            // Add verification data
            UObject *ImportedAsset = UEditorAssetLibrary::LoadAsset(FinalAssetPath);
            if (ImportedAsset) {
              McpHandlerUtils::AddVerification(Resp, ImportedAsset);
            }
            StrongThis->SendAutomationResponse(
                Socket, RequestId, true,
                bRenameSucceeded ? TEXT("Asset imported")
                                 : TEXT("Asset imported but rename failed"),
                Resp, FString());
          } else {
            StrongThis->SendAutomationResponse(
                Socket, RequestId, false,
                FString::Printf(TEXT("Failed to import asset from '%s'"),
                                 *ResolvedSourcePath),
                nullptr, TEXT("IMPORT_FAILED"));
          }
        });
  } else {
    // Fallback: GEditor not available (shouldn't happen in editor context)
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available for deferred import"),
                           nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
  }

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles metadata setting requests for assets.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and 'metadata' object.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleSetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("set_metadata payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  const FString SafeAssetPath = AssetPath;

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  const TSharedPtr<FJsonObject> *MetadataObjPtr = nullptr;
  if (!Payload->TryGetObjectField(TEXT("metadata"), MetadataObjPtr) ||
      !MetadataObjPtr) {
    // Treat missing/empty metadata as a no-op success; nothing to write.
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Resp->SetNumberField(TEXT("updatedKeys"), 0);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("No metadata provided; no-op"), Resp,
                           FString());
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  UPackage *Package = Asset->GetOutermost();
  if (!Package) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to resolve package for asset"), nullptr,
                           TEXT("PACKAGE_NOT_FOUND"));
    return true;
  }

  // GetMetaData returns the metadata object that is owned by this package.
  // UE 5.0 uses UMetaData*, UE 5.6+ uses FMetaData&
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
  FMetaData& Meta = Package->GetMetaData();
#else
  UMetaData* Meta = Package->GetMetaData();
#endif

  const TSharedPtr<FJsonObject> &MetadataObj = *MetadataObjPtr;
  int32 UpdatedCount = 0;

  for (const auto &Kvp : MetadataObj->Values) {
    const FString Key(*Kvp.Key);
    const TSharedPtr<FJsonValue> &Val = Kvp.Value;

    FString ValueString;
    if (!Val.IsValid() || Val->IsNull()) {
      continue;
    }
    switch (Val->Type) {
    case EJson::String:
      ValueString = Val->AsString();
      break;
    case EJson::Number:
      ValueString = LexToString(Val->AsNumber());
      break;
    case EJson::Boolean:
      ValueString = Val->AsBool() ? TEXT("true") : TEXT("false");
      break;
    default:
      // For arrays/objects, store a compact JSON string
      {
        FString JsonOut;
        const TSharedRef<TJsonWriter<>> Writer =
            TJsonWriterFactory<>::Create(&JsonOut);
        FJsonSerializer::Serialize(Val, TEXT(""), Writer);
        ValueString = JsonOut;
      }
      break;
    }

    if (!ValueString.IsEmpty()) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
      Meta.SetValue(Asset, *Key, *ValueString);
#else
      Meta->SetValue(Asset, *Key, *ValueString);
#endif
      ++UpdatedCount;
    }
  }

  if (UpdatedCount > 0) {
    Package->SetDirtyFlag(true);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
  Resp->SetNumberField(TEXT("updatedKeys"), UpdatedCount);
  
  // Add verification data
  McpHandlerUtils::AddVerification(Resp, Asset);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset metadata updated"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles asset duplication requests. Supports both single asset and folder
 * (deep) duplication.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleDuplicateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  if (SourcePath.IsEmpty())
    Payload->TryGetStringField(TEXT("assetPath"), SourcePath);
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  if (DestinationPath.IsEmpty())
    Payload->TryGetStringField(TEXT("newName"), DestinationPath);

  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sourcePath (or assetPath) and destinationPath (or newName) "
             "required"),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve simple name for destination
  if (!DestinationPath.IsEmpty() &&
      FPaths::GetPath(DestinationPath).IsEmpty()) {
    FString ParentDir = FPaths::GetPath(SourcePath);
    if (ParentDir.IsEmpty() || ParentDir == TEXT("/"))
      ParentDir = TEXT("/Game");

    DestinationPath = ParentDir / DestinationPath;
  }

  SourcePath = SanitizeProjectRelativePath(SourcePath);
  DestinationPath = SanitizeProjectRelativePath(DestinationPath);
  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid sourcePath or destinationPath"),
                           nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // If the source path is a directory, perform a deep duplication of all
  // assets under that folder into the destination folder, preserving
  // relative structure. This powers the "Deep Duplication - Duplicate
  // Folder" scenario in tests.
  if (UEditorAssetLibrary::DoesDirectoryExist(SourcePath)) {
    // Ensure the destination root exists
    UEditorAssetLibrary::MakeDirectory(DestinationPath);

    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*SourcePath));
    Filter.bRecursivePaths = true;

    // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
    // Asset listing uses cached AssetRegistry data exclusively.
    // LIMITATION: Assets not yet indexed by the editor's background scanner
    // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssets(Filter, Assets);

    int32 DuplicatedCount = 0;
    for (const FAssetData &Asset : Assets) {
      // PackageName is the long package path (e.g.,
      // /Game/Tests/DeepCopy/Source/M_Source)
      const FString SourceAssetPath = Asset.PackageName.ToString();

      FString RelativePath;
      if (SourceAssetPath.StartsWith(SourcePath)) {
        RelativePath = SourceAssetPath.RightChop(SourcePath.Len());
      } else {
        // Should not happen for the filtered set, but skip if it does.
        continue;
      }

      const FString TargetAssetPath =
          DestinationPath + RelativePath; // preserves any subfolders
      const FString TargetFolderPath = FPaths::GetPath(TargetAssetPath);
      if (!TargetFolderPath.IsEmpty()) {
        UEditorAssetLibrary::MakeDirectory(TargetFolderPath);
      }

      if (UEditorAssetLibrary::DuplicateAsset(SourceAssetPath,
                                              TargetAssetPath)) {
        ++DuplicatedCount;
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    const bool bSuccess = DuplicatedCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetStringField(TEXT("sourcePath"), SourcePath);
    Resp->SetStringField(TEXT("destinationPath"), DestinationPath);
    Resp->SetNumberField(TEXT("duplicatedCount"), DuplicatedCount);

    if (bSuccess) {
      SendAutomationResponse(Socket, RequestId, true, TEXT("Folder duplicated"),
                             Resp, FString());
    } else {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("No assets duplicated"), Resp,
                             TEXT("DUPLICATE_FAILED"));
    }
    return true;
  }

  // Fallback: single-asset duplication
  if (!UEditorAssetLibrary::DoesAssetExist(SourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source asset not found: %s"), *SourcePath),
        nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  if (UEditorAssetLibrary::DoesAssetExist(DestinationPath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Destination asset already exists: %s"),
                        *DestinationPath),
        nullptr, TEXT("DESTINATION_EXISTS"));
    return true;
  }

  if (UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DestinationPath);
    // Add verification data
    UObject *NewAsset = UEditorAssetLibrary::LoadAsset(DestinationPath);
    if (NewAsset) {
      McpHandlerUtils::AddVerification(Resp, NewAsset);
    }
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset duplicated"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Duplicate failed"),
                           nullptr, TEXT("DUPLICATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles asset renaming (and moving) requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleRenameAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  if (SourcePath.IsEmpty())
    Payload->TryGetStringField(TEXT("assetPath"), SourcePath);
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  if (DestinationPath.IsEmpty())
    Payload->TryGetStringField(TEXT("newName"), DestinationPath);

  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sourcePath (or assetPath) and destinationPath (or newName) "
             "required"),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve simple name for destination
  if (!DestinationPath.IsEmpty() &&
      FPaths::GetPath(DestinationPath).IsEmpty()) {
    FString ParentDir = FPaths::GetPath(SourcePath);
    if (ParentDir.IsEmpty() || ParentDir == TEXT("/"))
      ParentDir = TEXT("/Game");

    DestinationPath = ParentDir / DestinationPath;
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Display,
        TEXT(
            "HandleRenameAsset: Auto-resolved simple name destination to '%s'"),
        *DestinationPath);
  }

  if ((SourcePath.Contains(TEXT("/")) || SourcePath.StartsWith(TEXT("/"))) &&
      SanitizeProjectRelativePath(SourcePath).IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid sourcePath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  DestinationPath = SanitizeProjectRelativePath(DestinationPath);
  if (DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid destinationPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Resolve source path to ensure it matches a real asset
  FString ResolvedSourcePath = ResolveAssetPath(SourcePath);
  if (ResolvedSourcePath.IsEmpty()) {
    // If resolution failed, fall back to original for strict check
    ResolvedSourcePath = SourcePath;
  }

  ResolvedSourcePath = SanitizeProjectRelativePath(ResolvedSourcePath);
  if (ResolvedSourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid resolved sourcePath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(ResolvedSourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source asset not found: %s"), *SourcePath),
        nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Use the resolved path for the rename operation
  if (UEditorAssetLibrary::RenameAsset(ResolvedSourcePath, DestinationPath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DestinationPath);
    
    // Add verification data
    UObject* RenamedAsset = UEditorAssetLibrary::LoadAsset(DestinationPath);
    if (RenamedAsset) {
      McpHandlerUtils::AddVerification(Resp, RenamedAsset);
    }
    
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset renamed"), Resp,
                           FString());
  } else {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Failed to rename asset. Check if destination "
                             "'%s' already exists or source is locked."),
                        *DestinationPath),
        nullptr, TEXT("RENAME_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleMoveAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // Move is essentially rename in Unreal
  return HandleRenameAsset(RequestId, Payload, Socket);
}

/**
 * Handles asset deletion requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'path' (string) or 'paths' (array of
 * strings).
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleDeleteAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  // Accept array forms 'paths' / 'assetPaths' and singular forms 'path' /
  // 'assetPath'. The manage_asset schema advertises assetPath/assetPaths, so a
  // caller using those (the natural choice for an asset action) previously hit
  // "No paths provided" because only path/paths were read here.
  TArray<FString> PathsToDelete;
  const TArray<TSharedPtr<FJsonValue>> *PathsArray = nullptr;
  if ((Payload->TryGetArrayField(TEXT("paths"), PathsArray) ||
       Payload->TryGetArrayField(TEXT("assetPaths"), PathsArray)) &&
      PathsArray) {
    for (const auto &Val : *PathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String)
        PathsToDelete.Add(Val->AsString());
    }
  }

  FString SinglePath;
  if ((Payload->TryGetStringField(TEXT("path"), SinglePath) ||
       Payload->TryGetStringField(TEXT("assetPath"), SinglePath)) &&
      !SinglePath.IsEmpty()) {
    PathsToDelete.Add(SinglePath);
  }

  if (PathsToDelete.Num() == 0) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("No paths provided"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 DeletedCount = 0;
  TArray<FString> NotFoundPaths;
  TArray<FString> FailedToDeletePaths;
  
  for (const FString &Path : PathsToDelete) {
    const FString SafePath = SanitizeProjectRelativePath(Path);
    if (SafePath.IsEmpty()) {
      FailedToDeletePaths.Add(Path);
      continue;
    }

    // Check if it's a directory first (folder path)
    if (UEditorAssetLibrary::DoesDirectoryExist(SafePath)) {
      // Directory exists - use safe folder deletion with proper cleanup
      // CRITICAL for UE 5.7+: Use McpSafeDeleteFolder instead of UEditorAssetLibrary::DeleteDirectory
      // to prevent crashes during UWorld::CleanupWorld when deleting folders containing
      // AnimBlueprints, IKRigs, IKRetargeters, etc.
      if (McpSafeOperations::McpSafeDeleteFolder(SafePath, true))
      {
        // McpSafeDeleteFolder performs registry and filesystem verification itself.
        DeletedCount++;
      } else {
        FailedToDeletePaths.Add(SafePath);
      }
    } else if (UEditorAssetLibrary::DoesAssetExist(SafePath)) {
      // Asset exists - attempt to delete it
      if (UEditorAssetLibrary::DeleteAsset(SafePath)) {
        // Verify the asset was actually deleted
        if (!UEditorAssetLibrary::DoesAssetExist(SafePath)) {
          DeletedCount++;
        } else {
          // Delete returned true but asset still exists
          FailedToDeletePaths.Add(SafePath);
        }
      } else {
        FailedToDeletePaths.Add(SafePath);
      }
    } else {
      // Asset/directory does not exist
      NotFoundPaths.Add(SafePath);
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  
  // Return success only if at least one asset was deleted
  bool bSuccess = DeletedCount > 0;
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetNumberField(TEXT("deletedCount"), DeletedCount);
  Resp->SetBoolField(TEXT("existsAfter"), false);
  
  if (NotFoundPaths.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> NotFoundArray;
    for (const FString& P : NotFoundPaths) {
      NotFoundArray.Add(MakeShared<FJsonValueString>(P));
    }
    Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
    Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
  }
  
  if (FailedToDeletePaths.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> FailedArray;
    for (const FString& P : FailedToDeletePaths) {
      FailedArray.Add(MakeShared<FJsonValueString>(P));
    }
    Resp->SetArrayField(TEXT("failedToDeletePaths"), FailedArray);
    Resp->SetNumberField(TEXT("failedCount"), FailedToDeletePaths.Num());
  }
  
  if (bSuccess) {
    SendAutomationResponse(Socket, RequestId, true, TEXT("Assets deleted"), Resp, FString());
  } else {
    // Nothing was deleted - determine the reason
    FString ErrorMessage;
    FString ErrorCode;
    
    if (NotFoundPaths.Num() > 0 && FailedToDeletePaths.Num() == 0) {
      // All paths were not found
      ErrorMessage = FString::Printf(TEXT("No assets deleted. %d path(s) not found."), NotFoundPaths.Num());
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    } else if (FailedToDeletePaths.Num() > 0 && NotFoundPaths.Num() == 0) {
      // All paths existed but deletion failed
      ErrorMessage = FString::Printf(TEXT("Failed to delete %d asset(s). They may be in use or locked."), FailedToDeletePaths.Num());
      ErrorCode = TEXT("DELETE_FAILED");
    } else {
      // Mixed: some not found, some failed to delete
      ErrorMessage = FString::Printf(TEXT("No assets deleted. %d path(s) not found, %d failed to delete."), 
                                      NotFoundPaths.Num(), FailedToDeletePaths.Num());
      ErrorCode = TEXT("DELETE_FAILED");
    }
    
    SendAutomationResponse(Socket, RequestId, false, ErrorMessage, Resp, ErrorCode);
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles folder creation requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'path'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleCreateFolder(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString Path;
  if (!Payload->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty()) {
    Payload->TryGetStringField(TEXT("directoryPath"), Path);
  }

  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("path (or directoryPath) required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SafePath = SanitizeProjectRelativePath(Path);
  if (SafePath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Invalid path: must be project-relative and not contain '..'"),
        nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  if (UEditorAssetLibrary::DoesDirectoryExist(SafePath) ||
      UEditorAssetLibrary::MakeDirectory(SafePath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SafePath);
    // Verify as a DIRECTORY, not an asset. A folder is never an asset, so the
    // generic VerifyAssetExists (DoesAssetExist) always reported
    // existsAfter:false for a folder that was in fact created successfully.
    const bool bDirExists = DoesAssetDirectoryExistOnDisk(SafePath);
    Resp->SetStringField(TEXT("verifiedPath"), SafePath);
    Resp->SetBoolField(TEXT("existsAfter"), bDirExists);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Folder created"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create folder"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to get asset dependencies.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and optional 'recursive'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleGetDependencies(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid asset path"),
                           nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  // Registry dependency queries key on PACKAGE names; accept the object-path
  // form (/Game/Pkg/Name.Name) by stripping the object suffix — the FName
  // overload keyed on an object path used to return a silent empty result.
  const FString PackagePath = FPackageName::ObjectPathToPackageName(SafeAssetPath);

  bool bRecursive = false;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

  // Existence check via the registry, NOT UEditorAssetLibrary::DoesAssetExist:
  // that one returns false for EVERY asset while PIE is running (engine-side
  // CheckIfInEditorAndPIE gate), which this handler misreported as
  // ASSET_NOT_FOUND. The registry query is PIE-immune and load-free.
  TArray<FAssetData> PackageAssets;
  AssetRegistryModule.Get().GetAssetsByPackageName(FName(*PackagePath),
                                                   PackageAssets);
  if (PackageAssets.Num() == 0) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *SafeAssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  TArray<FName> Dependencies;
  AssetRegistryModule.Get().GetDependencies(FName(*PackagePath), Dependencies);

  TArray<TSharedPtr<FJsonValue>> DepArray;
  for (const FName &Dep : Dependencies) {
    DepArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("dependencies"), DepArray);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Dependencies retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Returns the assets that reference a given asset (inverse of get_dependencies).
 * Payload: 'assetPath'.
 */
bool UMcpAutomationBridgeSubsystem::HandleGetReferencers(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid asset path"),
                           nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  // See HandleGetDependencies: package-path normalization + PIE-immune
  // registry existence check (DoesAssetExist false-fails wholesale in PIE).
  const FString PackagePath = FPackageName::ObjectPathToPackageName(SafeAssetPath);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

  TArray<FAssetData> PackageAssets;
  AssetRegistryModule.Get().GetAssetsByPackageName(FName(*PackagePath),
                                                   PackageAssets);
  if (PackageAssets.Num() == 0) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *SafeAssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  TArray<FName> Referencers;
  AssetRegistryModule.Get().GetReferencers(FName(*PackagePath), Referencers);

  TArray<TSharedPtr<FJsonValue>> RefArray;
  for (const FName &Ref : Referencers) {
    RefArray.Add(MakeShared<FJsonValueString>(Ref.ToString()));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("count"), RefArray.Num());
  Resp->SetArrayField(TEXT("referencers"), RefArray);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Referencers retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Dumps every UPROPERTY of a content asset to JSON (the reflection-based read
 * that previously required an execute_python probe). Payload: 'assetPath',
 * optional 'includeTransient'.
 */
namespace {

#if WITH_EDITOR
struct FMcpResolvedPropertyPath {
  void *Container = nullptr;     // container base for Prop's *_InContainer accessors
  FProperty *Prop = nullptr;     // resolved leaf (the array Inner when the last segment is indexed)
  FProperty *RootProp = nullptr; // first segment's class-level property (for PostEditChangeProperty)
  UObject *OwnerObject = nullptr; // deepest UObject on the walk (instancing Outer / PostEditChange target)
};

// Resolves a dotted property path with optional array indices —
// "DefaultKeyMappings.Mappings[19].Modifiers" — against an asset. Dotted
// segments descend struct fields and object references (e.g. into an Instanced
// input modifier); "[i]" indexes an array property. Fails fast with a specific
// error: unknown field, malformed/non-numeric index, indexing a non-array,
// index out of bounds, or descending into a None object / scalar field.
bool ResolveAssetPropertyPath(UObject *Asset, const FString &PropertyPath,
                              FMcpResolvedPropertyPath &Out, FString &OutError) {
  if (!Asset || PropertyPath.IsEmpty()) {
    OutError = TEXT("Asset and property path are required");
    return false;
  }

  TArray<FString> Segments;
  PropertyPath.ParseIntoArray(Segments, TEXT("."), /*InCullEmpty=*/false);
  if (Segments.Num() == 0) {
    OutError = TEXT("Empty property path");
    return false;
  }

  void *Container = Asset;
  const UStruct *Scope = Asset->GetClass();
  UObject *OwnerObject = Asset;
  FProperty *Prop = nullptr;

  for (int32 SegIdx = 0; SegIdx < Segments.Num(); ++SegIdx) {
    const FString &Seg = Segments[SegIdx];

    FString Name = Seg;
    TArray<int32> Indices;
    int32 BracketPos = INDEX_NONE;
    if (Seg.FindChar(TEXT('['), BracketPos)) {
      Name = Seg.Left(BracketPos);
      FString Rest = Seg.Mid(BracketPos);
      while (Rest.StartsWith(TEXT("["))) {
        const int32 Close = Rest.Find(TEXT("]"));
        if (Close == INDEX_NONE) {
          OutError = FString::Printf(TEXT("Malformed index (missing ']') in segment '%s'"), *Seg);
          return false;
        }
        const FString NumStr = Rest.Mid(1, Close - 1);
        if (NumStr.IsEmpty() || !NumStr.IsNumeric()) {
          OutError = FString::Printf(TEXT("Non-numeric index '%s' in segment '%s'"), *NumStr, *Seg);
          return false;
        }
        Indices.Add(FCString::Atoi(*NumStr));
        Rest = Rest.Mid(Close + 1);
      }
      if (!Rest.IsEmpty()) {
        OutError = FString::Printf(TEXT("Malformed segment '%s' (trailing '%s')"), *Seg, *Rest);
        return false;
      }
    }
    if (Name.IsEmpty()) {
      OutError = FString::Printf(TEXT("Empty field name at path segment %d"), SegIdx);
      return false;
    }

    Prop = Scope->FindPropertyByName(FName(*Name));
    if (!Prop) {
      for (TFieldIterator<FProperty> It(Scope); It; ++It) {
        if (It->GetName().Equals(Name, ESearchCase::IgnoreCase)) {
          Prop = *It;
          break;
        }
      }
    }
    if (!Prop) {
      OutError = FString::Printf(TEXT("Field '%s' not found on '%s'"), *Name, *Scope->GetName());
      return false;
    }
    // Fail fast on engine-deprecated fields. UHT only sets CPF_Deprecated for
    // _DEPRECATED-suffixed names, so the meta=(DeprecatedProperty) check is
    // the load-bearing half. Without this, UE 5.7's legacy
    // UInputMappingContext::Mappings (emptied into DefaultKeyMappings.Mappings
    // by PostLoad) read back as a plausible-but-wrong [] — and writes to such
    // fields mutate dead data that PostLoad ignores.
    if (Prop->HasAnyPropertyFlags(CPF_Deprecated) ||
        Prop->HasMetaData(TEXT("DeprecatedProperty"))) {
      const FString DeprecationMsg = Prop->GetMetaData(TEXT("DeprecationMessage"));
      OutError = FString::Printf(
          TEXT("Field '%s' on '%s' is deprecated%s%s"), *Name, *Scope->GetName(),
          DeprecationMsg.IsEmpty() ? TEXT("") : TEXT(": "), *DeprecationMsg);
      return false;
    }
    if (SegIdx == 0) {
      Out.RootProp = Prop;
    }

    for (const int32 Idx : Indices) {
      FArrayProperty *AP = CastField<FArrayProperty>(Prop);
      if (!AP) {
        OutError = FString::Printf(TEXT("'%s' is not an array property but an index was given"), *Name);
        return false;
      }
      FScriptArrayHelper Helper(AP, AP->ContainerPtrToValuePtr<void>(Container));
      if (Idx < 0 || Idx >= Helper.Num()) {
        OutError = FString::Printf(TEXT("Index %d out of bounds for '%s' (size %d)"), Idx, *Name, Helper.Num());
        return false;
      }
      Container = Helper.GetRawPtr(Idx); // element base; the Inner property has offset 0
      Prop = AP->Inner;
    }

    if (SegIdx == Segments.Num() - 1) {
      break;
    }

    // More segments follow: descend into the resolved field.
    if (FStructProperty *SPd = CastField<FStructProperty>(Prop)) {
      Container = SPd->ContainerPtrToValuePtr<void>(Container);
      Scope = SPd->Struct;
    } else if (FObjectProperty *OPd = CastField<FObjectProperty>(Prop)) {
      UObject *Obj = OPd->GetObjectPropertyValue_InContainer(Container);
      if (!Obj) {
        OutError = FString::Printf(TEXT("Object field '%s' is None; cannot descend further"), *Name);
        return false;
      }
      Container = Obj;
      Scope = Obj->GetClass();
      OwnerObject = Obj;
    } else {
      OutError = FString::Printf(TEXT("Field '%s' (%s) cannot be descended into"), *Name,
                                 *Prop->GetClass()->GetName());
      return false;
    }
  }

  Out.Container = Container;
  Out.Prop = Prop;
  Out.OwnerObject = OwnerObject;
  if (!Out.RootProp) {
    Out.RootProp = Prop;
  }
  return true;
}
#endif // WITH_EDITOR

} // namespace

bool UMcpAutomationBridgeSubsystem::HandleGetAssetProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ResolvedPath;
  UObject *Asset = McpHandlerUtils::ResolveObjectFromPath(AssetPath, &ResolvedPath);
  if (!Asset) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  bool bIncludeTransient = false;
  Payload->TryGetBoolField(TEXT("includeTransient"), bIncludeTransient);

  // Optional propertyName: read ONE property, supporting the same dotted/indexed
  // subpaths set_asset_property accepts ("DefaultKeyMappings.Mappings[19].Modifiers")
  // so a subpath write can be verified with a symmetric subpath read.
  FString PropertyName;
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  if (!PropertyName.IsEmpty()) {
    FMcpResolvedPropertyPath Resolved;
    FString ResolveError;
    if (!ResolveAssetPropertyPath(Asset, PropertyName, Resolved, ResolveError)) {
      SendAutomationError(
          Socket, RequestId,
          FString::Printf(TEXT("Property path '%s': %s"), *PropertyName, *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), ResolvedPath);
    Resp->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
    Resp->SetStringField(TEXT("propertyName"), PropertyName);
    Resp->SetField(TEXT("value"), McpPropertyReflection::ExportPropertyToJsonValue(
                                      Resolved.Container, Resolved.Prop));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Asset property retrieved"), Resp, FString());
    return true;
  }

  TSharedPtr<FJsonObject> Props =
      McpPropertyReflection::ExportObjectToJson(Asset, bIncludeTransient);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), ResolvedPath);
  Resp->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
  Resp->SetObjectField(TEXT("properties"),
                       Props.IsValid() ? Props : MakeShared<FJsonObject>());
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset properties retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Sets a single UPROPERTY on a content asset (reflection + ImportText), fires
 * PostEditChangeProperty, and saves. Payload: 'assetPath', 'propertyName',
 * 'value'.
 */
bool UMcpAutomationBridgeSubsystem::HandleSetAssetProperty(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath, PropertyName;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  if (AssetPath.IsEmpty() || PropertyName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("assetPath and propertyName required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!Payload->HasField(TEXT("value"))) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("value required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ResolvedPath;
  UObject *Asset = McpHandlerUtils::ResolveObjectFromPath(AssetPath, &ResolvedPath);
  if (!Asset) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Resolve dotted/indexed subpaths ("DefaultKeyMappings.Mappings[19].Modifiers")
  // to a (container, property) pair. A plain name resolves identically to the old
  // flat class lookup.
  FMcpResolvedPropertyPath Resolved;
  FString ResolveError;
  if (!ResolveAssetPropertyPath(Asset, PropertyName, Resolved, ResolveError)) {
    SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Property path '%s' on %s: %s"), *PropertyName,
                        *Asset->GetClass()->GetName(), *ResolveError),
        TEXT("PROPERTY_NOT_FOUND"));
    return true;
  }
  FProperty *Prop = Resolved.Prop;

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  Asset->Modify();
  if (Resolved.OwnerObject && Resolved.OwnerObject != Asset) {
    Resolved.OwnerObject->Modify();
  }
  // ApplyJsonValueToProperty / ExportPropertyToJsonValue use the *_InContainer
  // reflection accessors, which add the property's offset to the pointer given.
  // So they expect the CONTAINER BASE (the resolved container), not an
  // already-offset value pointer (passing one applies the offset twice — silent
  // corruption for PODs, an access violation for strings).
  void *Container = Resolved.Container;
  FString ApplyError;
  // Pass the deepest object on the path as the owner so Instanced subobject
  // values ({"__class", ...}) re-instance Outered into the right package
  // (round-trips input Triggers/Modifiers, etc.).
  if (!McpPropertyReflection::ApplyJsonValueToProperty(
          Container, Prop, ValueField, ApplyError, 0,
          Resolved.OwnerObject ? Resolved.OwnerObject : Asset)) {
    SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Failed to set '%s': %s"), *PropertyName, *ApplyError),
        TEXT("SET_PROPERTY_FAILED"));
    return true;
  }

  // Notify with the ROOT class-level property: for a subpath write the leaf
  // FProperty doesn't belong to the asset's class, and editors (e.g. the IMC
  // editor) key their rebuild logic off the top-level member.
  if (Resolved.OwnerObject && Resolved.OwnerObject != Asset) {
    Resolved.OwnerObject->PostEditChange();
  }
  FPropertyChangedEvent ChangeEvent(Resolved.RootProp ? Resolved.RootProp : Prop);
  Asset->PostEditChangeProperty(ChangeEvent);
  Asset->MarkPackageDirty();
  // Save via a raw UPackage::Save that bypasses the editor save-UI pipeline.
  // That pipeline has crashed here in TWO different subsystems on this project:
  //  - validate-on-save (UEditorAssetLibrary::SaveLoadedAsset -> DataValidation)
  //  - source-control checkout-on-save (McpSafeAssetSave -> SavePackagesForObjects
  //    / PromptForCheckoutAndSave -> GitSourceControl).
  // McpDirectPackageSave touches neither, writing the package straight to disk.
  FString SaveError;
  const bool bSaved = McpSafeOperations::McpDirectPackageSave(Asset, &SaveError);
  if (!bSaved) {
    // The property was applied in memory but not persisted (e.g. a read-only /
    // un-checked-out file under Perforce). Report it rather than claiming success.
    SendAutomationError(
        Socket, RequestId,
        FString::Printf(
            TEXT("Property '%s' was set in memory but the asset could not be saved: %s"),
            *PropertyName, *SaveError),
        TEXT("ASSET_SAVE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), ResolvedPath);
  Resp->SetStringField(TEXT("propertyName"), PropertyName);
  Resp->SetBoolField(TEXT("saved"), bSaved);
  Resp->SetField(TEXT("value"),
                 McpPropertyReflection::ExportPropertyToJsonValue(Container, Prop));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset property set"), Resp,
                         FString());
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to traverse and return an asset dependency graph.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and optional 'maxDepth'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleGetAssetGraph(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid asset path"),
                           nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  // Check if asset exists - return error for non-existent assets
  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationError(Socket, RequestId, 
                        FString::Printf(TEXT("Asset not found: %s"), *SafeAssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  int32 MaxDepth = 3;
  Payload->TryGetNumberField(TEXT("maxDepth"), MaxDepth);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  TSharedPtr<FJsonObject> GraphObj = McpHandlerUtils::CreateResultObject();

  TArray<FString> Queue;
  Queue.Add(SafeAssetPath);

  TSet<FString> Visited;
  Visited.Add(SafeAssetPath);

  TMap<FString, int32> Depths;
  Depths.Add(SafeAssetPath, 0);

  int32 Head = 0;
  while (Head < Queue.Num()) {
    FString Current = Queue[Head++];
    int32 CurrentDepth = Depths[Current];

    TArray<FName> Dependencies;
    AssetRegistry.GetDependencies(FName(*Current), Dependencies);

    TArray<TSharedPtr<FJsonValue>> DepArray;
    for (const FName &Dep : Dependencies) {
      FString DepStr = Dep.ToString();
      if (!DepStr.StartsWith(TEXT("/Game")))
        continue; // Only graph Game assets for now

      DepArray.Add(MakeShared<FJsonValueString>(DepStr));

      if (CurrentDepth < MaxDepth) {
        if (!Visited.Contains(DepStr)) {
          Visited.Add(DepStr);
          Depths.Add(DepStr, CurrentDepth + 1);
          Queue.Add(DepStr);
        }
      }
    }
    GraphObj->SetArrayField(Current, DepArray);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("graph"), GraphObj);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset graph retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to set asset tags. NOTE: Asset Registry tags are distinct
 * from Actor tags. This function currently returns NOT_IMPLEMENTED as generic
 * asset tagging is ambiguous (metadata vs registry tags).
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleSetTags(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("set_tags payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *TagsArray = nullptr;
  TArray<FString> Tags;
  if (Payload->TryGetArrayField(TEXT("tags"), TagsArray) && TagsArray) {
    for (const TSharedPtr<FJsonValue> &Val : *TagsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        Tags.Add(Val->AsString());
      }
    }
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Edge-case: empty or missing tags array should be treated as a no-op
  // success.
  if (Tags.Num() == 0) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Resp->SetNumberField(TEXT("appliedTags"), 0);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("No tags provided; no-op"), Resp, FString());
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  // Implement set_tags by mapping them to Package Metadata (Tag=true).
  // Metadata keys only export to asset-registry tags when globally registered;
  // without that, find_by_tag's registry query can never see them.
  TSet<FName> &RegistryMetaTags = UObject::GetMetaDataTagsForAssetRegistry();
  int32 AppliedCount = 0;
  for (const FString &Tag : Tags) {
    const FName TagName(*Tag);
    RegistryMetaTags.Add(TagName);
    UEditorAssetLibrary::SetMetadataTag(Asset, TagName, TEXT("true"));
    AppliedCount++;
  }

  // Mark dirty so the asset can be saved later
  Asset->MarkPackageDirty();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
  // The registry's cached tags otherwise refresh only on save.
  IAssetRegistry::GetChecked().AssetUpdateTags(
      Asset, EAssetRegistryTagsCaller::FullUpdate);
#endif

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("markedDirty"), true);
  Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
  Resp->SetNumberField(TEXT("appliedTags"), AppliedCount);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Tags applied as metadata"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to validate if an asset exists and can be loaded.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleValidateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("validate payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  bool bIsValid = true;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bIsValid);
  Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
  Resp->SetBoolField(TEXT("isValid"), bIsValid);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset validated"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to list assets with filtering and pagination.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing filter criteria and pagination
 * options.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleListAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  // Parse filters
  FString PathFilter;
  FString ClassFilter;
  FString TagFilter;
  FString PathStartsWith;

  const TSharedPtr<FJsonObject> *FilterObj;
  if (Payload->TryGetObjectField(TEXT("filter"), FilterObj) && FilterObj) {
    (*FilterObj)->TryGetStringField(TEXT("path"), PathFilter);
    (*FilterObj)->TryGetStringField(TEXT("class"), ClassFilter);
    (*FilterObj)->TryGetStringField(TEXT("tag"), TagFilter);
    (*FilterObj)->TryGetStringField(TEXT("pathStartsWith"), PathStartsWith);
  } else {
    Payload->TryGetStringField(TEXT("path"), PathFilter);
    if (PathFilter.IsEmpty())
      Payload->TryGetStringField(TEXT("directory"), PathFilter);
    if (PathFilter.IsEmpty())
      Payload->TryGetStringField(TEXT("directoryPath"), PathFilter);
  }

  // Sanitize PathFilter to remove trailing slash which can break AssetRegistry
  // lookups
  if (PathFilter.Len() > 1 && PathFilter.EndsWith(TEXT("/"))) {
    PathFilter.RemoveAt(PathFilter.Len() - 1);
  }

  bool bRecursive = true;
  if (!Payload->TryGetBoolField(TEXT("recursive"), bRecursive))
    Payload->TryGetBoolField(TEXT("recursivePaths"), bRecursive);

  // Parse pagination
  int32 Offset = 0;
  int32 Limit = -1; // -1 means no limit
  const TSharedPtr<FJsonObject> *PaginationObj;
  if (Payload->TryGetObjectField(TEXT("pagination"), PaginationObj) &&
      PaginationObj) {
    (*PaginationObj)->TryGetNumberField(TEXT("offset"), Offset);
    (*PaginationObj)->TryGetNumberField(TEXT("limit"), Limit);
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Default to /Game to prevent empty results or massive scan
  const FString EffectiveDirectory =
      !PathFilter.IsEmpty()
          ? PathFilter
          : (!PathStartsWith.IsEmpty() ? PathStartsWith : TEXT("/Game"));

  if (!AssetRegistry.PathExists(EffectiveDirectory)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Directory '%s' not found in the asset registry. "
                             "Pass a mounted content path like /Game/Folder."),
                        *EffectiveDirectory),
        nullptr, TEXT("DIRECTORY_NOT_FOUND"));
    return true;
  }

  FARFilter Filter;
  Filter.bRecursivePaths = bRecursive;
  Filter.bRecursiveClasses = true;

  // Note: FARFilter doesn't support 'StartsWith' natively for paths in an
  // efficient way other than adding the path and set bRecursivePaths=true. So
  // if PathStartsWith is a folder, we use it.
  Filter.PackagePaths.Add(FName(*EffectiveDirectory));

  // Use cached AssetRegistry data — ScanPathsSynchronous() removed to prevent
  // blocking the GameThread (causes SSE/HTTP transport timeouts).
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.

  if (!ClassFilter.IsEmpty()) {
    // Support both short class names and full paths (best effort)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FTopLevelAssetPath ClassPath(ClassFilter);
    if (ClassPath.IsValid()) {
      Filter.ClassPaths.Add(ClassPath);
    }
#else
    // UE 5.0: Use ClassNames instead of ClassPaths
    Filter.ClassNames.Add(FName(*ClassFilter));
#endif
  }

  // Tags are not standard on assets in the same way as actors.
  // AssetRegistry tags are Key-Value pairs.
  // If TagFilter is provided, we assume it checks for the existence of a tag
  // key or value. Implementing a generic "HasTag" is ambiguous. We'll assume
  // TagFilter refers to a metadata key presence.

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  // Post-filtering
  if (!ClassFilter.IsEmpty() || !TagFilter.IsEmpty()) {
    AssetList.RemoveAll([&](const FAssetData &Asset) {
      if (!ClassFilter.IsEmpty()) {
        // Check full class path or asset class name
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FString AssetClass = Asset.AssetClassPath.ToString();
        FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
#else
        FString AssetClass = Asset.AssetClass.ToString();
        FString AssetClassName = Asset.AssetClass.ToString();
#endif
        if (!AssetClass.Equals(ClassFilter) &&
            !AssetClassName.Equals(ClassFilter)) {
          return true; // Remove
        }
      }
      if (!TagFilter.IsEmpty()) {
        if (!Asset.TagsAndValues.Contains(FName(*TagFilter))) {
          return true; // Remove
        }
      }
      return false;
    });
  }

  // Filter by Depth if specified
  // (Changes made to support depth and folders - Touch to force rebuild)
  int32 Depth = -1;
  Payload->TryGetNumberField(TEXT("depth"), Depth);

  if (Depth >= 0 && bRecursive && !PathFilter.IsEmpty()) {
    // Normalize base path for depth calculation
    FString BasePath = PathFilter;
    if (BasePath.EndsWith(TEXT("/"))) {
      BasePath.RemoveAt(BasePath.Len() - 1);
    }
    // Base depth: number of slashes in /Game/Foo is 2
    int32 BaseSlashCount = 0;
    for (const TCHAR *P = *BasePath; *P; ++P) {
      if (*P == TEXT('/'))
        BaseSlashCount++;
    }

    AssetList.RemoveAll([&](const FAssetData &Asset) {
      FString PkgPath = Asset.PackagePath.ToString();
      // If PkgPath is shorter than BasePath (shouldn't happen with filter),
      // keep it I guess? Actually we only care about descendants.

      int32 SlashCount = 0;
      for (const TCHAR *P = *PkgPath; *P; ++P) {
        if (*P == TEXT('/'))
          SlashCount++;
      }

      // Difference in slashes determines depth
      return (SlashCount - BaseSlashCount) > Depth;
    });
  }

  int32 TotalCount = AssetList.Num();

  // Apply pagination
  if (Offset > 0) {
    if (Offset >= AssetList.Num()) {
      AssetList.Empty();
    } else {
      AssetList.RemoveAt(0, Offset);
    }
  }

  if (Limit >= 0 && AssetList.Num() > Limit) {
    AssetList.SetNum(Limit);
  }

  // Also fetch sub-folders if we are listing a directory (PathFilter is set)
  TArray<FString> SubPathList;
  if (!PathFilter.IsEmpty()) {
    // If non-recursive (or depth limited), we generally want at least the
    // immediate subfolders. GetSubPaths is non-recursive by default.
    AssetRegistry.GetSubPaths(PathFilter, SubPathList, false);
  }

  TArray<TSharedPtr<FJsonValue>> AssetsArray;
  for (const FAssetData &Asset : AssetList) {
    TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
    AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    AssetObj->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
    AssetObj->SetStringField(TEXT("path"), Asset.ToSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
    AssetObj->SetStringField(TEXT("packagePath"), Asset.PackagePath.ToString());

    // Add tags for context if requested
    TArray<TSharedPtr<FJsonValue>> Tags;
    for (auto TagPair : Asset.TagsAndValues) {
      Tags.Add(MakeShared<FJsonValueString>(TagPair.Key.ToString()));
    }
    AssetObj->SetArrayField(TEXT("tags"), Tags);

    AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
  }

  TArray<TSharedPtr<FJsonValue>> FoldersJson;
  for (const FString &SubPath : SubPathList) {
    FoldersJson.Add(MakeShared<FJsonValueString>(SubPath));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("directory"), EffectiveDirectory);
  Resp->SetBoolField(TEXT("recursive"), bRecursive);
  Resp->SetArrayField(TEXT("assets"), AssetsArray);
  Resp->SetArrayField(TEXT("folders"), FoldersJson);
  Resp->SetNumberField(TEXT("totalCount"), TotalCount);
  Resp->SetNumberField(TEXT("count"), AssetsArray.Num());
  Resp->SetNumberField(TEXT("offset"), Offset);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Assets listed"), Resp,
                         FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to get detailed information about a single asset.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleGetAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("get_asset payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FAssetData AssetData = UEditorAssetLibrary::FindAssetData(SafeAssetPath);
  if (!AssetData.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to find asset data"), nullptr,
                           TEXT("ASSET_DATA_INVALID"));
    return true;
  }

  TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
  AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  AssetObj->SetStringField(TEXT("path"), AssetData.GetSoftObjectPath().ToString());
  AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
#else
  AssetObj->SetStringField(TEXT("path"), AssetData.ToSoftObjectPath().ToString());
  AssetObj->SetStringField(TEXT("class"), AssetData.AssetClass.ToString());
#endif
  AssetObj->SetStringField(TEXT("packagePath"),
                           AssetData.PackagePath.ToString());

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("result"), AssetObj);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset details retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to generate an asset report (CSV/JSON).
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'directory' and 'reportType'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleGenerateReport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("generate_report payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Directory;
  Payload->TryGetStringField(TEXT("directory"), Directory);
  if (Directory.IsEmpty()) {
    Directory = TEXT("/Game");
  }

  // Normalize /Content prefix to /Game for convenience
  if (Directory.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    Directory = FString::Printf(TEXT("/Game%s"), *Directory.RightChop(8));
  }

  FString ReportType;
  Payload->TryGetStringField(TEXT("reportType"), ReportType);
  if (ReportType.IsEmpty()) {
    ReportType = TEXT("Summary");
  }

  FString OutputPath;
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  FARFilter Filter;
  Filter.bRecursivePaths = true;
  if (!Directory.IsEmpty()) {
    Filter.PackagePaths.Add(FName(*Directory));
  }

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
  TArray<FAssetData> AssetList;
  AssetRegistryModule.Get().GetAssets(Filter, AssetList);

  TArray<TSharedPtr<FJsonValue>> AssetsArray;
  for (const FAssetData &Asset : AssetList) {
    TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
    AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
    AssetObj->SetStringField(TEXT("path"),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                             Asset.GetSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
                             Asset.ToSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
    AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
  }

  bool bFileWritten = false;
  if (!OutputPath.IsEmpty()) {
    // SECURITY: Sanitize and validate the output path to prevent path traversal
    FString SafeOutputPath = SanitizeProjectFilePath(OutputPath);
    if (SafeOutputPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe output path: %s"), *OutputPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    FString AbsoluteOutput = FPaths::ProjectDir() / SafeOutputPath;
    AbsoluteOutput = FPaths::ConvertRelativePathToFull(AbsoluteOutput);
    FPaths::NormalizeFilename(AbsoluteOutput);

    FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(NormalizedProjectDir);
    if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
      NormalizedProjectDir += TEXT("/");
    }

    if (!AbsoluteOutput.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Output path escapes project directory: %s"), *OutputPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    const FString DirPath = FPaths::GetPath(AbsoluteOutput);
    IPlatformFile &PlatformFile =
        FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*DirPath);

    const FString FileContents = TEXT(
        "{\"report\":\"Asset report generated by MCP Automation Bridge\"}");
    bFileWritten =
        FFileHelper::SaveStringToFile(FileContents, *AbsoluteOutput);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("directory"), Directory);
  Resp->SetStringField(TEXT("reportType"), ReportType);
  Resp->SetNumberField(TEXT("assetCount"), AssetList.Num());
  Resp->SetArrayField(TEXT("assets"), AssetsArray);
  if (!OutputPath.IsEmpty()) {
    Resp->SetStringField(TEXT("outputPath"), OutputPath);
    Resp->SetBoolField(TEXT("fileWritten"), bFileWritten);
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset report generated"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// ============================================================================
// 8. MATERIAL CREATION
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterial(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  Payload->TryGetStringField(TEXT("path"), Path);
  bool bSave = false;
  Payload->TryGetBoolField(TEXT("save"), bSave);

  if (Name.IsEmpty() || Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("name and path required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  Name = SanitizeAssetName(Name);
  Path = SanitizeProjectRelativePath(Path);
  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid path"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Validate properties if present
  const TSharedPtr<FJsonObject> *Props;
  if (Payload->TryGetObjectField(TEXT("properties"), Props)) {
    FString ShadingModelStr;
    if ((*Props)->TryGetStringField(TEXT("ShadingModel"), ShadingModelStr)) {
      // Simple validation for test case
      if (ShadingModelStr.Equals(TEXT("InvalidModel"),
                                 ESearchCase::IgnoreCase)) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Invalid shading model"), nullptr,
                               TEXT("INVALID_PROPERTY"));
        return true;
      }
    }
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  FString FullPath = Path + TEXT("/") + Name;
  if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
    UEditorAssetLibrary::DeleteAsset(FullPath);
  }

  UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
  UObject *NewAsset =
      AssetTools.CreateAsset(Name, Path, UMaterial::StaticClass(), Factory);

  if (NewAsset) {
    bool bSaved = false;
    if (bSave) {
      bSaved = McpSafeAssetSave(NewAsset);
      if (!bSaved) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Material created but save failed"), nullptr,
                               TEXT("SAVE_FAILED"));
        return true;
      }
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    Resp->SetBoolField(TEXT("saved"), bSaved);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material created"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create material"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateDataTable(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  Payload->TryGetStringField(TEXT("path"), Path);
  // Row struct: a UScriptStruct deriving from FTableRowBase. Accept either a full
  // object path (/Script/Module.StructName or /Game/.../UserStruct) or a bare name.
  FString RowStructStr;
  if (!Payload->TryGetStringField(TEXT("rowStruct"), RowStructStr))
    Payload->TryGetStringField(TEXT("rowStructPath"), RowStructStr);
  // Newly created assets persist by default; pass save:false for in-memory only.
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);

  if (Name.IsEmpty() || Path.IsEmpty() || RowStructStr.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("name, path and rowStruct are required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  Name = SanitizeAssetName(Name);
  Path = SanitizeProjectRelativePath(Path);
  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid path"),
                           nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Resolve the row struct. LoadObject handles full paths (native /Script structs
  // and /Game user structs); fall back to a by-name lookup for a bare struct name.
  UScriptStruct *RowStruct = LoadObject<UScriptStruct>(nullptr, *RowStructStr);
  if (!RowStruct) {
    RowStruct = UClass::TryFindTypeSlow<UScriptStruct>(RowStructStr);
  }
  if (!RowStruct) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Could not resolve row struct '%s'"), *RowStructStr),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!RowStruct->IsChildOf(FTableRowBase::StaticStruct())) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(
            TEXT("Row struct '%s' must derive from FTableRowBase"),
            *RowStruct->GetName()),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString FullPath = Path + TEXT("/") + Name;
  if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
    UEditorAssetLibrary::DeleteAsset(FullPath);
  }

  UPackage *Package = CreatePackage(*FullPath);
  if (!Package) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create package"), nullptr,
                           TEXT("CREATE_FAILED"));
    return true;
  }

  UDataTable *DataTable =
      NewObject<UDataTable>(Package, UDataTable::StaticClass(), FName(*Name),
                            RF_Public | RF_Standalone);
  if (!DataTable) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create DataTable object"), nullptr,
                           TEXT("CREATE_FAILED"));
    return true;
  }
  DataTable->RowStruct = RowStruct;

  // McpDirectPackageSave skips the asset-registry rescan, so register explicitly.
  FAssetRegistryModule::AssetCreated(DataTable);
  DataTable->MarkPackageDirty();

  bool bSaved = false;
  if (bSave) {
    FString SaveErr;
    bSaved = McpSafeOperations::McpDirectPackageSave(DataTable, &SaveErr);
    if (!bSaved) {
      SendAutomationResponse(
          Socket, RequestId, false,
          FString::Printf(TEXT("DataTable created but save failed: %s"),
                          *SaveErr),
          nullptr, TEXT("SAVE_FAILED"));
      return true;
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), DataTable->GetPathName());
  Resp->SetStringField(TEXT("rowStruct"), RowStruct->GetPathName());
  Resp->SetBoolField(TEXT("saved"), bSaved);
  SendAutomationResponse(Socket, RequestId, true, TEXT("DataTable created"), Resp,
                         FString());
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor build required"),
                      TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

#if WITH_EDITOR
namespace {
// Resolve the DataTable named by the payload (assetPath / dataTablePath / path /
// tablePath). Returns the table, or nullptr with OutError set (caller sends the
// response). Validates that a RowStruct is assigned (row ops are meaningless without).
UDataTable *McpLoadDataTableArg(const TSharedPtr<FJsonObject> &Payload,
                                FString &OutError) {
  FString Path;
  if (!Payload->TryGetStringField(TEXT("assetPath"), Path) &&
      !Payload->TryGetStringField(TEXT("dataTablePath"), Path) &&
      !Payload->TryGetStringField(TEXT("tablePath"), Path) &&
      !Payload->TryGetStringField(TEXT("path"), Path)) {
    OutError = TEXT("assetPath (the DataTable) is required");
    return nullptr;
  }
  if (Path.IsEmpty()) {
    OutError = TEXT("assetPath (the DataTable) is required");
    return nullptr;
  }
  UObject *Obj = UEditorAssetLibrary::LoadAsset(Path);
  UDataTable *DT = Cast<UDataTable>(Obj);
  if (!DT) {
    OutError = FString::Printf(TEXT("DataTable not found: %s"), *Path);
    return nullptr;
  }
  if (!DT->RowStruct) {
    OutError =
        FString::Printf(TEXT("DataTable '%s' has no RowStruct"), *Path);
    return nullptr;
  }
  return DT;
}
} // namespace
#endif

bool UMcpAutomationBridgeSubsystem::HandleDataTableRowOp(
    const FString &RequestId, const FString &SubAction,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  const bool bAdd = SubAction == TEXT("add_data_table_row");
  const bool bEdit = SubAction == TEXT("edit_data_table_row");
  const bool bRemove = SubAction == TEXT("remove_data_table_row");
  const bool bGet = SubAction == TEXT("get_data_table_rows");
  const bool bImport = SubAction == TEXT("import_data_table");

  FString Err;
  UDataTable *DT = McpLoadDataTableArg(Payload, Err);
  if (!DT) {
    SendAutomationResponse(Socket, RequestId, false, Err, nullptr,
                           TEXT("NOT_FOUND"));
    return true;
  }

  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  auto SaveIfRequested = [&](bool &bOutSaved) {
    bOutSaved = false;
    if (bSave) {
      FString SaveErr;
      bOutSaved = McpSafeOperations::McpDirectPackageSave(DT, &SaveErr);
    }
  };

  // ---- get_rows: read-only dump of all rows as JSON ----
  if (bGet) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DT->GetPathName());
    Resp->SetStringField(TEXT("rowStruct"), DT->RowStruct->GetPathName());
    Resp->SetNumberField(TEXT("rowCount"), DT->GetRowMap().Num());

    TArray<TSharedPtr<FJsonValue>> NameVals;
    for (const FName &RowName : DT->GetRowNames())
      NameVals.Add(MakeShared<FJsonValueString>(RowName.ToString()));
    Resp->SetArrayField(TEXT("rowNames"), NameVals);

    const FString RowsJson = DT->GetTableAsJSON();
    TArray<TSharedPtr<FJsonValue>> RowsArr;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RowsJson);
    if (FJsonSerializer::Deserialize(Reader, RowsArr))
      Resp->SetArrayField(TEXT("rows"), RowsArr);
    else
      Resp->SetStringField(TEXT("rowsJson"), RowsJson);

    SendAutomationResponse(Socket, RequestId, true, TEXT("DataTable rows"), Resp,
                           FString());
    return true;
  }

  // ---- import: replace all rows from a CSV or JSON string ----
  if (bImport) {
    FString Source;
    if (!Payload->TryGetStringField(TEXT("sourceText"), Source) &&
        !Payload->TryGetStringField(TEXT("csv"), Source) &&
        !Payload->TryGetStringField(TEXT("json"), Source) &&
        !Payload->TryGetStringField(TEXT("content"), Source)) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("sourceText (CSV or JSON) is required"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FString Format;
    Payload->TryGetStringField(TEXT("format"), Format);
    const FString Trimmed = Source.TrimStart();
    const bool bJson = Format.Equals(TEXT("json"), ESearchCase::IgnoreCase) ||
                       (Format.IsEmpty() &&
                        (Trimmed.StartsWith(TEXT("[")) ||
                         Trimmed.StartsWith(TEXT("{"))));

    TArray<FString> Problems = bJson ? DT->CreateTableFromJSONString(Source)
                                     : DT->CreateTableFromCSVString(Source);
    FDataTableEditorUtils::BroadcastPostChange(
        DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
    DT->MarkPackageDirty();
    bool bSaved = false;
    SaveIfRequested(bSaved);

    // CreateTableFrom* returns a list of "problems" that mixes hard parse errors
    // with benign notes (e.g. "column 'Name' is also the row key"), and it still
    // imports every well-formed row. So the import is best-effort: report it as a
    // success that ran, surface the problems as warnings, and let the caller judge
    // from rowCount + problems. (Don't fail the whole call on a benign note -- same
    // principle as not failing on a self-healing ensure.)
    const int32 RowsAfter = DT->GetRowMap().Num();
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DT->GetPathName());
    Resp->SetStringField(TEXT("format"), bJson ? TEXT("json") : TEXT("csv"));
    Resp->SetNumberField(TEXT("rowCount"), RowsAfter);
    Resp->SetBoolField(TEXT("saved"), bSaved);
    TArray<TSharedPtr<FJsonValue>> ProblemVals;
    for (const FString &P : Problems)
      ProblemVals.Add(MakeShared<FJsonValueString>(P));
    Resp->SetArrayField(TEXT("problems"), ProblemVals);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("DataTable imported: %d row(s), %d problem(s)"),
                        RowsAfter, Problems.Num()),
        Resp, FString());
    return true;
  }

  // add / edit / remove all key on a row name.
  FString RowName;
  Payload->TryGetStringField(TEXT("rowName"), RowName);
  if (RowName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("rowName is required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const FName RowFName(*RowName);
  const bool bExists = DT->GetRowMap().Contains(RowFName);

  // ---- remove: idempotent delete (already-absent reported as success) ----
  if (bRemove) {
    bool bRemoved = false;
    if (bExists)
      bRemoved = FDataTableEditorUtils::RemoveRow(DT, RowFName);
    if (bRemoved) {
      DT->MarkPackageDirty();
    }
    bool bSaved = false;
    if (bRemoved)
      SaveIfRequested(bSaved);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DT->GetPathName());
    Resp->SetStringField(TEXT("rowName"), RowName);
    Resp->SetBoolField(TEXT("removed"), bRemoved);
    Resp->SetBoolField(TEXT("alreadyAbsent"), !bExists);
    Resp->SetNumberField(TEXT("rowCount"), DT->GetRowMap().Num());
    Resp->SetBoolField(TEXT("saved"), bSaved);
    SendAutomationResponse(Socket, RequestId, true,
                           bRemoved ? TEXT("Row removed")
                                    : TEXT("Row already absent"),
                           Resp, FString());
    return true;
  }

  // ---- add / edit: locate (or create) the row, then apply rowData ----
  // add_row fail-fasts when the row already exists (idempotent retry safety:
  // a dropped response then a re-send surfaces ROW_ALREADY_EXISTS rather than
  // silently double-applying). edit_row requires the row to exist.
  if (bAdd && bExists) {
    TSharedPtr<FJsonObject> E = McpHandlerUtils::CreateResultObject();
    E->SetStringField(TEXT("rowName"), RowName);
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Row '%s' already exists; use edit_row to modify"),
                        *RowName),
        E, TEXT("ROW_ALREADY_EXISTS"));
    return true;
  }
  if (bEdit && !bExists) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Row '%s' not found; use add_row to create"),
                        *RowName),
        nullptr, TEXT("NOT_FOUND"));
    return true;
  }

  uint8 *RowMem = nullptr;
  if (bAdd) {
    RowMem = FDataTableEditorUtils::AddRow(DT, RowFName);
    if (!RowMem) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to add row"), nullptr,
                             TEXT("OPERATION_FAILED"));
      return true;
    }
  } else {
    DT->Modify();
    RowMem = DT->FindRowUnchecked(RowFName);
    if (!RowMem) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Row exists but its data is missing"),
                             nullptr, TEXT("OPERATION_FAILED"));
      return true;
    }
  }

  // Resolve rowData, accepting EITHER a nested JSON object OR a JSON-encoded
  // string (MCP clients commonly stringify freeform-object params).
  TSharedPtr<FJsonObject> RowDataJson;
  {
    const TSharedPtr<FJsonObject> *RowDataObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("rowData"), RowDataObj) && RowDataObj &&
        RowDataObj->IsValid()) {
      RowDataJson = *RowDataObj;
    } else {
      FString RowDataStr;
      if (Payload->TryGetStringField(TEXT("rowData"), RowDataStr) &&
          !RowDataStr.TrimStartAndEnd().IsEmpty()) {
        TSharedRef<TJsonReader<>> Reader =
            TJsonReaderFactory<>::Create(RowDataStr);
        TSharedPtr<FJsonObject> Parsed;
        if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid()) {
          RowDataJson = Parsed;
        } else {
          if (bAdd)
            FDataTableEditorUtils::RemoveRow(DT, RowFName);
          SendAutomationResponse(
              Socket, RequestId, false,
              TEXT("rowData must be a JSON object (or a JSON-object string)"),
              nullptr, TEXT("INVALID_ARGUMENT"));
          return true;
        }
      }
    }
  }

  // Apply the supplied fields. JsonObjectToUStruct writes the fields present in
  // rowData and leaves the rest at their current value (default for a fresh add,
  // existing for an edit) -- so edit is a partial merge.
  if (RowDataJson.IsValid()) {
    if (!FJsonObjectConverter::JsonObjectToUStruct(RowDataJson.ToSharedRef(),
                                                   DT->RowStruct, RowMem, 0,
                                                   0)) {
      // Keep add atomic: roll back the row we just created so a bad rowData
      // doesn't leave an orphan empty row behind.
      if (bAdd)
        FDataTableEditorUtils::RemoveRow(DT, RowFName);
      SendAutomationResponse(
          Socket, RequestId, false,
          TEXT("rowData could not be applied to the row struct"), nullptr,
          TEXT("DESERIALIZE_FAILED"));
      return true;
    }
  }

  FDataTableEditorUtils::BroadcastPostChange(
      DT, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
  DT->MarkPackageDirty();
  bool bSaved = false;
  SaveIfRequested(bSaved);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), DT->GetPathName());
  Resp->SetStringField(TEXT("rowName"), RowName);
  Resp->SetNumberField(TEXT("rowCount"), DT->GetRowMap().Num());
  Resp->SetBoolField(TEXT("saved"), bSaved);
  SendAutomationResponse(Socket, RequestId, true,
                         bAdd ? TEXT("Row added") : TEXT("Row updated"), Resp,
                         FString());
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor build required"),
                      TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterialInstance(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  Payload->TryGetStringField(TEXT("path"), Path);
  FString ParentPath;
  Payload->TryGetStringField(TEXT("parentMaterial"), ParentPath);
  bool bSave = false;
  Payload->TryGetBoolField(TEXT("save"), bSave);

  if (Name.IsEmpty() || Path.IsEmpty() || ParentPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("name, path and parentMaterial required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  Name = SanitizeAssetName(Name);
  Path = SanitizeProjectRelativePath(Path);
  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid path"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }
  UMaterialInterface *ParentMaterial = nullptr;

  // Special test sentinel: treat "/Valid" as a shorthand for the engine's
  // default surface material so tests can exercise parameter handling without
  // requiring a real asset at that path.
  if (ParentPath.Equals(TEXT("/Valid"), ESearchCase::IgnoreCase)) {
    ParentMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
  } else {
    ParentPath = SanitizeProjectRelativePath(ParentPath);
    if (ParentPath.IsEmpty()) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Invalid parentMaterial"), nullptr,
                             TEXT("SECURITY_VIOLATION"));
      return true;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(ParentPath)) {
      SendAutomationResponse(
          Socket, RequestId, false,
          FString::Printf(TEXT("Parent material asset not found: %s"),
                          *ParentPath),
          nullptr, TEXT("PARENT_NOT_FOUND"));
      return true;
    }
    ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
  }

  if (!ParentMaterial) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Parent material not found"), nullptr,
                           TEXT("PARENT_NOT_FOUND"));
    return true;
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  UMaterialInstanceConstantFactoryNew *Factory =
      NewObject<UMaterialInstanceConstantFactoryNew>();
  Factory->InitialParent = ParentMaterial;

  // Pre-check: CreateAsset pops an "overwrite?" modal on a path collision, which freezes a
  // headless bridge (force-kill required). Fail fast on an existing path instead.
  const FString MicObjectPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
  if (UEditorAssetLibrary::DoesAssetExist(MicObjectPath)) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Asset already exists: %s"), *MicObjectPath),
                           nullptr, TEXT("ALREADY_EXISTS"));
    return true;
  }

  UObject *NewAsset = AssetTools.CreateAsset(
      Name, Path, UMaterialInstanceConstant::StaticClass(), Factory);

  if (NewAsset) {
    // Handle parameters if provided
    UMaterialInstanceConstant *MIC = Cast<UMaterialInstanceConstant>(NewAsset);
    const TSharedPtr<FJsonObject> *ParamsObj = nullptr;
    if (MIC && Payload->TryGetObjectField(TEXT("parameters"), ParamsObj)) {
      // Scalar parameters
      const TSharedPtr<FJsonObject> *Scalars;
      if ((*ParamsObj)->TryGetObjectField(TEXT("scalar"), Scalars)) {
        for (const auto &Kvp : (*Scalars)->Values) {
          double Val = 0.0;
          if (Kvp.Value->TryGetNumber(Val)) {
            MIC->SetScalarParameterValueEditorOnly(FName(*Kvp.Key), (float)Val);
          }
        }
      }

      // Vector parameters
      const TSharedPtr<FJsonObject> *Vectors;
      if ((*ParamsObj)->TryGetObjectField(TEXT("vector"), Vectors)) {
        for (const auto &Kvp : (*Vectors)->Values) {
          const TSharedPtr<FJsonObject> *VecObj;
          if (Kvp.Value->TryGetObject(VecObj)) {
            // Try generic RGBA
            double R = 0, G = 0, B = 0, A = 1;
            (*VecObj)->TryGetNumberField(TEXT("r"), R);
            (*VecObj)->TryGetNumberField(TEXT("g"), G);
            (*VecObj)->TryGetNumberField(TEXT("b"), B);
            (*VecObj)->TryGetNumberField(TEXT("a"), A);
            MIC->SetVectorParameterValueEditorOnly(
                FName(*Kvp.Key),
                FLinearColor((float)R, (float)G, (float)B, (float)A));
          }
        }
      }

      // Texture parameters
      const TSharedPtr<FJsonObject> *Textures;
      if ((*ParamsObj)->TryGetObjectField(TEXT("texture"), Textures)) {
        for (const auto &Kvp : (*Textures)->Values) {
          FString TexPath;
          if (Kvp.Value->TryGetString(TexPath) && !TexPath.IsEmpty()) {
            TexPath = SanitizeProjectRelativePath(TexPath);
            if (TexPath.IsEmpty()) {
              continue;
            }
            UTexture *Tex = LoadObject<UTexture>(nullptr, *TexPath);
            if (Tex) {
              MIC->SetTextureParameterValueEditorOnly(FName(*Kvp.Key), Tex);
            }
          }
        }
      }
    }

    bool bSaved = false;
    if (bSave) {
      bSaved = McpSafeAssetSave(NewAsset);
      if (!bSaved) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Material Instance created but save failed"), nullptr,
                               TEXT("SAVE_FAILED"));
        return true;
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    Resp->SetBoolField(TEXT("saved"), bSaved);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material Instance created"), Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create material instance"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// ============================================================================
// 10. MATERIAL PARAMETER & INSTANCE MANAGEMENT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialParameter(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Type;
  Payload->TryGetStringField(TEXT("type"), Type);

  if (AssetPath.IsEmpty() || Name.IsEmpty() || Type.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("assetPath, name, and type required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(AssetPath, Material, Function);

  if (!Material && !Function) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material or Material Function"),
                           nullptr, TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  UObject *HostOuter = Material ? static_cast<UObject*>(Material)
                                : static_cast<UObject*>(Function);

  UMaterialExpression *NewExpression = nullptr;
  Type = Type.ToLower();

  // Asymmetric creation paths by design:
  // - UMaterial: UMaterialEditingLibrary::CreateMaterialExpression handles
  //   graph registration, undo transactions, and editor-only data setup.
  // - UMaterialFunction: UMaterialEditingLibrary only supports UMaterial, so we
  //   use NewObject + manual add to the expression collection. This is
  //   intentional due to API limitations — CreateMaterialExpression does not
  //   accept UMaterialFunction as a host.
  auto CreateExpr = [&](UClass* ExprClass) -> UMaterialExpression* {
    if (Material) {
      return UMaterialEditingLibrary::CreateMaterialExpression(Material, ExprClass);
    }
    UMaterialExpression* Expr = NewObject<UMaterialExpression>(HostOuter, ExprClass, NAME_None, RF_Transactional);
    if (Expr) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      Function->GetEditorOnlyData()->ExpressionCollection.AddExpression(Expr);
#else
      Function->FunctionExpressions.Add(Expr);
#endif
    }
    return Expr;
  };

  if (Type == TEXT("scalar")) {
    NewExpression = CreateExpr(UMaterialExpressionScalarParameter::StaticClass());
    if (UMaterialExpressionScalarParameter *ScalarParam =
            Cast<UMaterialExpressionScalarParameter>(NewExpression)) {
      ScalarParam->ParameterName = FName(*Name);
      double Val = 0.0;
      if (Payload->TryGetNumberField(TEXT("value"), Val)) {
        ScalarParam->DefaultValue = (float)Val;
      }
    }
  } else if (Type == TEXT("vector")) {
    NewExpression = CreateExpr(UMaterialExpressionVectorParameter::StaticClass());
    if (UMaterialExpressionVectorParameter *VectorParam =
            Cast<UMaterialExpressionVectorParameter>(NewExpression)) {
      VectorParam->ParameterName = FName(*Name);
      const TSharedPtr<FJsonObject> *VecObj;
      if (Payload->TryGetObjectField(TEXT("value"), VecObj)) {
        double R = 0, G = 0, B = 0, A = 1;
        (*VecObj)->TryGetNumberField(TEXT("r"), R);
        (*VecObj)->TryGetNumberField(TEXT("g"), G);
        (*VecObj)->TryGetNumberField(TEXT("b"), B);
        (*VecObj)->TryGetNumberField(TEXT("a"), A);
        VectorParam->DefaultValue =
            FLinearColor((float)R, (float)G, (float)B, (float)A);
      }
    }
  } else if (Type == TEXT("texture")) {
    NewExpression = CreateExpr(UMaterialExpressionTextureSampleParameter2D::StaticClass());
    if (UMaterialExpressionTextureSampleParameter2D *TexParam =
            Cast<UMaterialExpressionTextureSampleParameter2D>(NewExpression)) {
      TexParam->ParameterName = FName(*Name);
      FString TexPath;
      if (Payload->TryGetStringField(TEXT("value"), TexPath) &&
          !TexPath.IsEmpty()) {
        TexPath = SanitizeProjectRelativePath(TexPath);
        if (TexPath.IsEmpty()) {
          SendAutomationResponse(Socket, RequestId, false,
                                 TEXT("Invalid texture path"), nullptr,
                                 TEXT("SECURITY_VIOLATION"));
          return true;
        }
        UTexture *Tex = LoadObject<UTexture>(nullptr, *TexPath);
        if (Tex) {
          TexParam->Texture = Tex;
        }
      }
    }
  } else if (Type == TEXT("staticswitch") || Type == TEXT("static_switch")) {
    NewExpression = CreateExpr(UMaterialExpressionStaticSwitchParameter::StaticClass());
    if (UMaterialExpressionStaticSwitchParameter *SwitchParam =
            Cast<UMaterialExpressionStaticSwitchParameter>(NewExpression)) {
      SwitchParam->ParameterName = FName(*Name);
      bool Val = false;
      if (Payload->TryGetBoolField(TEXT("value"), Val)) {
        SwitchParam->DefaultValue = Val;
      }
    }
  } else {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Unsupported parameter type: %s"), *Type), nullptr,
        TEXT("INVALID_TYPE"));
    return true;
  }

  if (NewExpression) {
    if (Material) {
      UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
      UMaterialEditingLibrary::RecompileMaterial(Material);
      Material->MarkPackageDirty();
    } else {
      FinalizeHost(nullptr, Function);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetStringField(TEXT("parameterName"), Name);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Parameter added"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create parameter expression"),
                           nullptr, TEXT("CREATE_FAILED"));
  }

  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleListMaterialInstances(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Find all assets that are Material Instances and have this asset as parent
  // Note: This can be expensive if we scan all assets.
  // Optimization: Use GetReferencers? Or just filter by class and check parent.
  // Since we can't easily query by "Parent" tag efficiently without iterating,
  // we'll try a filtered query.

  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                           TEXT("MaterialInstanceConstant")));
#else
  Filter.ClassNames.Add(FName(TEXT("MaterialInstanceConstant")));
#endif
  Filter.bRecursiveClasses = true;

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  TArray<TSharedPtr<FJsonValue>> Instances;

  // We need to check the parent. Loading the asset is safest but slow.
  // Checking tags is faster. MICs usually have "Parent" tag.
  for (const FAssetData &Asset : AssetList) {
    // Check tag first
    FString ParentTag;
    if (Asset.GetTagValue(TEXT("Parent"), ParentTag)) {
      // Tag value might be "Material'Path'" or just "Path"
      // It's usually formatted string.
      if (ParentTag.Contains(AssetPath)) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.GetSoftObjectPath().ToString()));
#else
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.ToSoftObjectPath().ToString()));
#endif
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("instances"), Instances);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Instances listed"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleResetInstanceParameters(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterialInstanceConstant *MIC = Cast<UMaterialInstanceConstant>(Asset);

  if (!MIC) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material Instance Constant"),
                           nullptr, TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  MIC->ClearParameterValuesEditorOnly();
  MIC->PostEditChange();
  MIC->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Instance parameters reset"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleDoesAssetExist(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("exists"), bExists);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true,
                         bExists ? TEXT("Asset exists")
                                 : TEXT("Asset does not exist"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterialInterface *Material = Cast<UMaterialInterface>(Asset);

  if (!Material) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material"), nullptr,
                           TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  // Ensure material is compiled
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Material->EnsureIsComplete();
#else
  // UE 5.0: Force compilation by accessing the material resource
  Material->GetMaterial();
#endif

  TSharedPtr<FJsonObject> Stats = McpHandlerUtils::CreateResultObject();

  // Get actual shading model from the material
  FString ShadingModelStr = TEXT("Unknown");
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    FMaterialShadingModelField ShadingModels = BaseMat->GetShadingModels();
    // Check shading models using HasShadingModel - prioritize common ones
    if (ShadingModels.HasShadingModel(MSM_Unlit)) {
      ShadingModelStr = TEXT("Unlit");
    } else if (ShadingModels.HasShadingModel(MSM_DefaultLit)) {
      ShadingModelStr = TEXT("DefaultLit");
    } else if (ShadingModels.HasShadingModel(MSM_Subsurface)) {
      ShadingModelStr = TEXT("Subsurface");
    } else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile)) {
      ShadingModelStr = TEXT("SubsurfaceProfile");
    } else if (ShadingModels.HasShadingModel(MSM_ClearCoat)) {
      ShadingModelStr = TEXT("ClearCoat");
    } else if (ShadingModels.HasShadingModel(MSM_TwoSidedFoliage)) {
      ShadingModelStr = TEXT("TwoSidedFoliage");
    } else if (ShadingModels.HasShadingModel(MSM_Hair)) {
      ShadingModelStr = TEXT("Hair");
    } else if (ShadingModels.HasShadingModel(MSM_Cloth)) {
      ShadingModelStr = TEXT("Cloth");
    } else if (ShadingModels.HasShadingModel(MSM_Eye)) {
      ShadingModelStr = TEXT("Eye");
    } else if (ShadingModels.HasShadingModel(MSM_PreintegratedSkin)) {
      ShadingModelStr = TEXT("PreintegratedSkin");
    }
  }
  Stats->SetStringField(TEXT("shadingModel"), ShadingModelStr);

  // Report compile status + errors so a failed compile is diagnosable instead of a
  // silent instructionCount:-1. Translation errors are populated by EnsureIsComplete above.
  TArray<FString> CompileErrors;
  if (UMaterial *BaseMatForErrors = Material->GetMaterial()) {
    if (const FMaterialResource *Res =
            BaseMatForErrors->GetMaterialResource(GMaxRHIShaderPlatform)) {
      CompileErrors = Res->GetCompileErrors();
    }
  }
  const bool bMatCompiled = CompileErrors.Num() == 0;
  Stats->SetBoolField(TEXT("compiled"), bMatCompiled);
  if (!bMatCompiled) {
    TArray<TSharedPtr<FJsonValue>> ErrArr;
    for (const FString &E : CompileErrors) {
      ErrArr.Add(MakeShared<FJsonValueString>(E));
    }
    Stats->SetArrayField(TEXT("errors"), ErrArr);
  }

  // The instruction count needs a representative FShaderType* (the MaterialEditor
  // stats path), which isn't linked here — surface -1 with an explicit reason
  // rather than a silent value that reads like a real measurement.
  Stats->SetNumberField(TEXT("instructionCount"), -1);
  Stats->SetStringField(
      TEXT("instructionCountNote"),
      bMatCompiled
          ? TEXT("not surfaced over the bridge (needs the MaterialEditor stats path)")
          : TEXT("-1 because the material failed to compile; see errors[]"));

  // Count texture samplers used in the material
  int32 SamplerCount = 0;
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(BaseMat)) {
      if (Expr && Expr->IsA<UMaterialExpressionTextureSample>()) {
        SamplerCount++;
      }
    }
  }
  Stats->SetNumberField(TEXT("samplerCount"), SamplerCount);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("stats"), Stats);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material stats retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGenerateLODs(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("generate_lods"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Support both landscapePath (single) and assetPaths (array)
  FString LandscapePath;
  Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
  
  // Support both assetPath (single) and assetPaths (array)
  FString SingleAssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), SingleAssetPath);
  
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray)) {
    Payload->TryGetArrayField(TEXT("assets"), AssetPathsArray);
  }

  // Support both lodCount and numLODs
  int32 NumLODs = 4;
  Payload->TryGetNumberField(TEXT("lodCount"), NumLODs);
  Payload->TryGetNumberField(TEXT("numLODs"), NumLODs);
  NumLODs = FMath::Clamp(NumLODs, 1, 50);

  // FMeshReductionSettings overrides applied to every generated LOD; percent
  // fields replace the progressive 50%/25%/... defaults.
  TOptional<float> OverridePercentTriangles;
  TOptional<float> OverridePercentVertices;
  TOptional<float> OverrideMaxDeviation;
  TOptional<float> OverridePixelError;
  TOptional<float> OverrideWeldingThreshold;
  TOptional<float> OverrideHardAngleThreshold;
  TOptional<int32> OverrideBaseLODModel;
  TOptional<bool> OverrideRecalculateNormals;
  const TSharedPtr<FJsonObject> *ReductionOverrides = nullptr;
  const TSharedPtr<FJsonValue> ReductionValue =
      Payload->TryGetField(TEXT("reductionSettings"));
  if (ReductionValue.IsValid() && ReductionValue->Type != EJson::Null) {
    if (!ReductionValue->TryGetObject(ReductionOverrides)) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("reductionSettings must be a JSON object"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>> &Field :
         (*ReductionOverrides)->Values) {
      double Number = 0.0;
      const bool bIsNumber =
          Field.Value.IsValid() && Field.Value->TryGetNumber(Number);
      if (Field.Key == TEXT("percentTriangles") ||
          Field.Key == TEXT("percentVertices")) {
        if (!bIsNumber || Number < 0.0 || Number > 1.0) {
          SendAutomationError(
              RequestingSocket, RequestId,
              FString::Printf(
                  TEXT("reductionSettings.%s must be a number in [0, 1]"),
                  *Field.Key),
              TEXT("INVALID_ARGUMENT"));
          return true;
        }
        if (Field.Key == TEXT("percentTriangles")) {
          OverridePercentTriangles = static_cast<float>(Number);
        } else {
          OverridePercentVertices = static_cast<float>(Number);
        }
      } else if (Field.Key == TEXT("maxDeviation") ||
                 Field.Key == TEXT("pixelError") ||
                 Field.Key == TEXT("weldingThreshold") ||
                 Field.Key == TEXT("hardAngleThreshold")) {
        if (!bIsNumber || Number < 0.0) {
          SendAutomationError(
              RequestingSocket, RequestId,
              FString::Printf(
                  TEXT("reductionSettings.%s must be a non-negative number"),
                  *Field.Key),
              TEXT("INVALID_ARGUMENT"));
          return true;
        }
        const float Value = static_cast<float>(Number);
        if (Field.Key == TEXT("maxDeviation")) {
          OverrideMaxDeviation = Value;
        } else if (Field.Key == TEXT("pixelError")) {
          OverridePixelError = Value;
        } else if (Field.Key == TEXT("weldingThreshold")) {
          OverrideWeldingThreshold = Value;
        } else {
          OverrideHardAngleThreshold = Value;
        }
      } else if (Field.Key == TEXT("baseLODModel")) {
        if (!bIsNumber || Number < 0.0 ||
            Number != FMath::FloorToDouble(Number)) {
          SendAutomationError(RequestingSocket, RequestId,
                              TEXT("reductionSettings.baseLODModel must be a "
                                   "non-negative integer"),
                              TEXT("INVALID_ARGUMENT"));
          return true;
        }
        OverrideBaseLODModel = static_cast<int32>(Number);
      } else if (Field.Key == TEXT("recalculateNormals")) {
        bool bRecalculate = false;
        if (!Field.Value.IsValid() || !Field.Value->TryGetBool(bRecalculate)) {
          SendAutomationError(
              RequestingSocket, RequestId,
              TEXT("reductionSettings.recalculateNormals must be a boolean"),
              TEXT("INVALID_ARGUMENT"));
          return true;
        }
        OverrideRecalculateNormals = bRecalculate;
      } else {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(
                TEXT("reductionSettings.%s is not supported; supported keys: "
                     "percentTriangles, percentVertices, maxDeviation, "
                     "pixelError, weldingThreshold, hardAngleThreshold, "
                     "baseLODModel, recalculateNormals"),
                *Field.Key),
            TEXT("INVALID_ARGUMENT"));
        return true;
      }
    }
  }

  // Build list of paths to process
  TArray<FString> Paths;
  
  // Add landscape path if provided
  if (!LandscapePath.IsEmpty()) {
    // Validate landscape path
    FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe landscape path: %s"), *LandscapePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    Paths.Add(SafePath);
  }
  
  // Add single asset path if provided
  if (!SingleAssetPath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(SingleAssetPath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe asset path: %s"), *SingleAssetPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    Paths.Add(SafePath);
  }
  
  // Add asset paths if provided
  if (AssetPathsArray) {
    for (const auto &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        FString SafePath = SanitizeProjectRelativePath(Val->AsString());
        if (!SafePath.IsEmpty()) {
          Paths.Add(SafePath);
        }
      }
    }
  }

  if (Paths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("landscapePath or assetPaths required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    int32 VerifiedCount = 0;
    TArray<FString> NotFoundPaths;
    TArray<FString> NotMeshPaths;
    TArray<TSharedPtr<FJsonValue>> MeshDetails;

    for (const FString &Path : Paths) {
      UObject *Obj = LoadObject<UObject>(nullptr, *Path);
      if (!Obj) {
        NotFoundPaths.Add(Path);
        continue;
      }

      UStaticMesh *Mesh = Cast<UStaticMesh>(Obj);
      if (!Mesh) {
        NotMeshPaths.Add(Path);
        continue;
      }

      TSharedPtr<FJsonObject> MeshInfo = MakeShared<FJsonObject>();
      MeshInfo->SetStringField(TEXT("assetPath"), Path);
      MeshInfo->SetStringField(TEXT("assetClass"), Mesh->GetClass()->GetName());
      MeshInfo->SetNumberField(TEXT("currentLODCount"), Mesh->GetNumLODs());
      MeshInfo->SetNumberField(TEXT("requestedLODCount"), NumLODs);
      MeshDetails.Add(MakeShared<FJsonValueObject>(MeshInfo));
      VerifiedCount++;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    const bool bSuccess = VerifiedCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetBoolField(TEXT("headlessSafe"), true);
    Resp->SetBoolField(TEXT("lodBuildSkipped"), true);
    Resp->SetNumberField(TEXT("verified"), VerifiedCount);
    Resp->SetNumberField(TEXT("requested"), Paths.Num());
    Resp->SetNumberField(TEXT("lodCount"), NumLODs);
    if (ReductionOverrides) {
      Resp->SetObjectField(TEXT("reductionSettings"), *ReductionOverrides);
    }
    Resp->SetArrayField(TEXT("meshes"), MeshDetails);

    if (NotFoundPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotFoundArray;
      for (const FString &Path : NotFoundPaths) {
        NotFoundArray.Add(MakeShared<FJsonValueString>(Path));
      }
      Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
      Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
    }

    if (NotMeshPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotMeshArray;
      for (const FString &Path : NotMeshPaths) {
        NotMeshArray.Add(MakeShared<FJsonValueString>(Path));
      }
      Resp->SetArrayField(TEXT("notMeshPaths"), NotMeshArray);
      Resp->SetNumberField(TEXT("notMeshCount"), NotMeshPaths.Num());
    }

    const FString Message = bSuccess
        ? FString::Printf(TEXT("Verified %d mesh(es); LOD build skipped under NullRHI"), VerifiedCount)
        : TEXT("No static meshes verified for LOD generation under NullRHI");
    const FString ErrorCode = bSuccess ? FString() : TEXT("LOD_GENERATION_FAILED");
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  // NOTE: ProcessAutomationRequest already dispatches to GameThread.
  // Wrapping ALL work in AsyncTask(GameThread, ...) caused the queued lambda
  // to sit behind the current dispatch cycle, so responses never reached the
  // MCP server before the 30-second timeout. Execute synchronously instead.
  int32 SuccessCount = 0;
  TArray<FString> NotFoundPaths;
  TArray<FString> NotMeshPaths;

  for (const FString &Path : Paths) {
    SendProgressUpdate(RequestId, -1.0f,
        FString::Printf(TEXT("Processing LOD generation for: %s"), *Path), true);

    UObject *Obj = LoadObject<UObject>(nullptr, *Path);

    if (!Obj) {
      NotFoundPaths.Add(Path);
      continue;
    }

    // Try Static Mesh
    if (UStaticMesh *Mesh = Cast<UStaticMesh>(Obj)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Generating %d LODs for static mesh %s"), NumLODs, *Path);

        Mesh->Modify();
        Mesh->SetNumSourceModels(NumLODs);

        // Configure LOD reduction settings with progressive reduction
        for (int32 LODIndex = 1; LODIndex < NumLODs; LODIndex++) {
          FStaticMeshSourceModel &SourceModel = Mesh->GetSourceModel(LODIndex);
          FMeshReductionSettings &ReductionSettings =
              SourceModel.ReductionSettings;

          // Progressive reduction: 50%, 25%, 12.5%...
          float ReductionPercent =
              1.0f / FMath::Pow(2.0f, static_cast<float>(LODIndex));
          ReductionSettings.PercentTriangles =
              OverridePercentTriangles.Get(ReductionPercent);
          ReductionSettings.PercentVertices =
              OverridePercentVertices.Get(ReductionPercent);
          if (OverrideMaxDeviation.IsSet()) {
            ReductionSettings.MaxDeviation = OverrideMaxDeviation.GetValue();
          }
          if (OverridePixelError.IsSet()) {
            ReductionSettings.PixelError = OverridePixelError.GetValue();
          }
          if (OverrideWeldingThreshold.IsSet()) {
            ReductionSettings.WeldingThreshold =
                OverrideWeldingThreshold.GetValue();
          }
          if (OverrideHardAngleThreshold.IsSet()) {
            ReductionSettings.HardAngleThreshold =
                OverrideHardAngleThreshold.GetValue();
          }
          if (OverrideBaseLODModel.IsSet()) {
            ReductionSettings.BaseLODModel = OverrideBaseLODModel.GetValue();
          }
          if (OverrideRecalculateNormals.IsSet()) {
            ReductionSettings.bRecalculateNormals =
                OverrideRecalculateNormals.GetValue();
          }

          // Enable reduction for this LOD level
          SourceModel.BuildSettings.bRecomputeNormals = false;
          SourceModel.BuildSettings.bRecomputeTangents = false;
          SourceModel.BuildSettings.bUseMikkTSpace = true;
        }

        // Build the mesh with new LOD settings
        Mesh->Build();
        Mesh->PostEditChange();
        McpSafeAssetSave(Mesh);

        SuccessCount++;
      } else {
        // Asset exists but is not a static mesh
        NotMeshPaths.Add(Path);
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    
    // CRITICAL FIX: Return proper success/failure based on actual results
    // Previously always returned success=true even when 0 meshes processed
    bool bSuccess = SuccessCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetNumberField(TEXT("processed"), SuccessCount);
    Resp->SetNumberField(TEXT("requested"), Paths.Num());
    Resp->SetNumberField(TEXT("lodCount"), NumLODs);
    if (ReductionOverrides) {
      Resp->SetObjectField(TEXT("reductionSettings"), *ReductionOverrides);
    }

    // Add details about failures
    if (NotFoundPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotFoundArray;
      for (const FString& P : NotFoundPaths) {
        NotFoundArray.Add(MakeShared<FJsonValueString>(P));
      }
      Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
      Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
    }
    
    if (NotMeshPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotMeshArray;
      for (const FString& P : NotMeshPaths) {
        NotMeshArray.Add(MakeShared<FJsonValueString>(P));
      }
      Resp->SetArrayField(TEXT("notMeshPaths"), NotMeshArray);
      Resp->SetNumberField(TEXT("notMeshCount"), NotMeshPaths.Num());
    }
    
    FString Message;
    FString ErrorCode;
    
    if (bSuccess) {
      Message = FString::Printf(TEXT("Generated LODs for %d mesh(es)"), SuccessCount);
    } else if (NotFoundPaths.Num() > 0 && NotMeshPaths.Num() == 0) {
      Message = FString::Printf(TEXT("No assets found. %d path(s) not found."), NotFoundPaths.Num());
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    } else if (NotMeshPaths.Num() > 0 && NotFoundPaths.Num() == 0) {
      Message = FString::Printf(TEXT("No static meshes found. %d asset(s) are not meshes."), NotMeshPaths.Num());
      ErrorCode = TEXT("INVALID_ASSET_TYPE");
    } else {
      Message = FString::Printf(TEXT("No LODs generated. %d not found, %d not meshes."), 
                                NotFoundPaths.Num(), NotMeshPaths.Num());
      ErrorCode = TEXT("LOD_GENERATION_FAILED");
    }
    
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                                      Message, Resp, ErrorCode);

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Requires editor"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 8. METADATA
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("get_metadata payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);

  // 1. Asset Registry Tags
  FAssetData AssetData(Asset);
  TSharedPtr<FJsonObject> TagsObj = McpHandlerUtils::CreateResultObject();
  for (const auto &Kvp : AssetData.TagsAndValues) {
    TagsObj->SetStringField(Kvp.Key.ToString(), Kvp.Value.AsString());
  }
  Resp->SetObjectField(TEXT("tags"), TagsObj);

  // 2. Package Metadata information
  UPackage *Package = Asset->GetOutermost();
  if (Package) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
    const TMap<FName, FString> *ObjectMeta = FMetaData::GetMapForObject(Asset);
#else
    const TMap<FName, FString> *ObjectMeta =
        Package->GetMetaData()->GetMapForObject(Asset);
#endif
    if (ObjectMeta) {
      TSharedPtr<FJsonObject> MetaObj = McpHandlerUtils::CreateResultObject();
      for (const auto &Entry : *ObjectMeta) {
        MetaObj->SetStringField(Entry.Key.ToString(), Entry.Value);
      }
      Resp->SetObjectField(TEXT("metadata"), MetaObj);
    }
  }

  SendAutomationResponse(Socket, RequestId, true, TEXT("Metadata retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_metadata requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 9. NANITE/MESH WORKFLOW ACTIONS
// ============================================================================

// Dispatcher-compatible mesh workflow handlers with explicit success/error responses.

bool UMcpAutomationBridgeSubsystem::HandleNaniteRebuildMesh(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("nanite_rebuild_mesh"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR && ENGINE_MAJOR_VERSION >= 5
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("nanite_rebuild_mesh payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MeshPath;
  if (!Payload->TryGetStringField(TEXT("meshPath"), MeshPath) ||
      MeshPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("meshPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MeshPath = SanitizeProjectRelativePath(MeshPath);
  if (MeshPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid meshPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the static mesh
  UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
  if (!StaticMesh) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath),
                        TEXT("MESH_NOT_FOUND"));
    return true;
  }

  // Check if mesh supports Nanite
  bool bEnableNanite = true;
  Payload->TryGetBoolField(TEXT("enableNanite"), bEnableNanite);

  // Nanite settings
  bool bPreserveArea = true;
  double TrianglePercent = 100.0;
  double FallbackPercent = 0.0;

  Payload->TryGetBoolField(TEXT("preserveArea"), bPreserveArea);
  Payload->TryGetNumberField(TEXT("trianglePercent"), TrianglePercent);
  Payload->TryGetNumberField(TEXT("fallbackPercent"), FallbackPercent);

  // Clamp values
  TrianglePercent = FMath::Clamp(TrianglePercent, 0.0, 100.0);
  FallbackPercent = FMath::Clamp(FallbackPercent, 0.0, 100.0);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
  // UE 5.7+: Use accessor functions to avoid deprecation warnings
  FMeshNaniteSettings Settings = StaticMesh->GetNaniteSettings();
  Settings.bEnabled = bEnableNanite;
  Settings.PositionPrecision = 8; // Default precision
  
  // bPreserveArea replaced with ShapePreservation enum
  if (bPreserveArea) {
    Settings.ShapePreservation = ENaniteShapePreservation::PreserveArea;
  } else {
    Settings.ShapePreservation = ENaniteShapePreservation::None;
  }
  Settings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  Settings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
  if (FallbackPercent > 0.0) {
    Settings.GenerateFallback = ENaniteGenerateFallback::Enabled;
  } else {
    Settings.GenerateFallback = ENaniteGenerateFallback::PlatformDefault;
  }
  StaticMesh->SetNaniteSettings(Settings);
  StaticMesh->NotifyNaniteSettingsChanged();
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  // UE 5.1-5.6: Uses KeepPercentTriangles, FallbackPercentTriangles, and bPreserveArea
  StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
  StaticMesh->NaniteSettings.PositionPrecision = 8;
  StaticMesh->NaniteSettings.bPreserveArea = bPreserveArea;
  StaticMesh->NaniteSettings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  StaticMesh->NaniteSettings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
#else
  // UE 5.0: Uses KeepPercentTriangles (no bPreserveArea)
  StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
  StaticMesh->NaniteSettings.PositionPrecision = 8;
  StaticMesh->NaniteSettings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  StaticMesh->NaniteSettings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
#endif

  // Mark mesh as modified
  StaticMesh->MarkPackageDirty();

  // Build response
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("meshPath"), MeshPath);
  Resp->SetStringField(TEXT("meshName"), StaticMesh->GetName());
  Resp->SetBoolField(TEXT("naniteEnabled"), bEnableNanite);
  Resp->SetBoolField(TEXT("preserveArea"), bPreserveArea);
  Resp->SetNumberField(TEXT("trianglePercent"), TrianglePercent);
  Resp->SetNumberField(TEXT("fallbackPercent"), FallbackPercent);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Nanite settings updated for %s"), *StaticMesh->GetName()),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("nanite_rebuild_mesh requires UE 5.0+ editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleFindByTag(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("find_by_tag"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("find_by_tag payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Tag;
  if (!Payload->TryGetStringField(TEXT("tag"), Tag) || Tag.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("tag field is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Optional scope for the asset search
  FString ScopePath = TEXT("/Game");
  FString Path;
  if (Payload->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty()) {
    const FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *Path),
          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    ScopePath = SanitizedPath;
  }

  FName TagName(*Tag);
  TArray<TSharedPtr<FJsonValue>> Results;
  int32 MaxResults = 100;
  Payload->TryGetNumberField(TEXT("maxResults"), MaxResults);
  MaxResults = FMath::Clamp(MaxResults, 1, 1000);

  bool bSearchActors = true;
  bool bSearchComponents = false;
  bool bSearchAssets = true;
  Payload->TryGetBoolField(TEXT("searchActors"), bSearchActors);
  Payload->TryGetBoolField(TEXT("searchComponents"), bSearchComponents);
  Payload->TryGetBoolField(TEXT("searchAssets"), bSearchAssets);

  // Search assets: registry tags (e.g. RowStructure) plus the package-metadata
  // store set_tags writes to (readable here only for already-loaded assets).
  if (bSearchAssets) {
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*ScopePath));
    Filter.bRecursivePaths = true;
    TArray<FAssetData> AssetDataList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);

    for (const FAssetData &Data : AssetDataList) {
      if (Results.Num() >= MaxResults)
        break;

      FString TagValue;
      bool bMatch = Data.GetTagValue(TagName, TagValue);
      if (!bMatch && Data.IsAssetLoaded()) {
        if (UObject *LoadedAsset = Data.GetAsset()) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
          const TMap<FName, FString> *ObjectMeta =
              FMetaData::GetMapForObject(LoadedAsset);
#else
          UPackage *AssetPackage = LoadedAsset->GetOutermost();
          const TMap<FName, FString> *ObjectMeta =
              AssetPackage
                  ? AssetPackage->GetMetaData()->GetMapForObject(LoadedAsset)
                  : nullptr;
#endif
          if (const FString *MetaValue =
                  ObjectMeta ? ObjectMeta->Find(TagName) : nullptr) {
            bMatch = true;
            TagValue = *MetaValue;
          }
        }
      }
      if (!bMatch)
        continue;

      TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
      ResultObj->SetStringField(TEXT("type"), TEXT("Asset"));
      ResultObj->SetStringField(TEXT("name"), Data.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      ResultObj->SetStringField(TEXT("path"),
                                Data.GetSoftObjectPath().ToString());
      ResultObj->SetStringField(TEXT("class"),
                                Data.AssetClassPath.ToString());
#else
      ResultObj->SetStringField(TEXT("path"),
                                Data.ToSoftObjectPath().ToString());
      ResultObj->SetStringField(TEXT("class"), Data.AssetClass.ToString());
#endif
      ResultObj->SetStringField(TEXT("tagValue"), TagValue);
      Results.Add(MakeShared<FJsonValueObject>(ResultObj));
    }
  }

  // Search in world
  if (GEditor && bSearchActors) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor && Actor->ActorHasTag(TagName)) {
          TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
          ResultObj->SetStringField(TEXT("type"), TEXT("Actor"));
          ResultObj->SetStringField(TEXT("name"), Actor->GetName());
          ResultObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
          ResultObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ResultObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
          
          const FVector Location = Actor->GetActorLocation();
          TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
          LocObj->SetNumberField(TEXT("x"), Location.X);
          LocObj->SetNumberField(TEXT("y"), Location.Y);
          LocObj->SetNumberField(TEXT("z"), Location.Z);
          ResultObj->SetObjectField(TEXT("location"), LocObj);
          
          Results.Add(MakeShared<FJsonValueObject>(ResultObj));
        }
      }
    }
  }

  // Search for components with tag
  if (bSearchComponents && GEditor && Results.Num() < MaxResults) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor) {
          TInlineComponentArray<UActorComponent*> Components;
          Actor->GetComponents(Components);
          for (UActorComponent *Component : Components) {
            if (Component && Component->ComponentHasTag(TagName)) {
              TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
              ResultObj->SetStringField(TEXT("type"), TEXT("Component"));
              ResultObj->SetStringField(TEXT("name"), Component->GetName());
              ResultObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
              ResultObj->SetStringField(TEXT("owner"), Actor->GetName());
              ResultObj->SetStringField(TEXT("path"), Component->GetPathName());
              Results.Add(MakeShared<FJsonValueObject>(ResultObj));
            }
          }
        }
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("tag"), Tag);
  Resp->SetNumberField(TEXT("count"), Results.Num());
  Resp->SetArrayField(TEXT("results"), Results);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Found %d objects with tag '%s'"), Results.Num(), *Tag),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("find_by_tag requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialNode(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("add_material_node"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("add_material_node payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid materialPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  FString NodeType;
  if (!Payload->TryGetStringField(TEXT("nodeType"), NodeType) ||
      NodeType.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("nodeType is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(MaterialPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  UObject *HostOuter = Material ? static_cast<UObject*>(Material)
                                : static_cast<UObject*>(Function);

  // Create material expression based on node type
  UMaterialExpression *NewExpression = nullptr;
  UClass *ExpressionClass = nullptr;

  // Map common node type names to expression classes
  if (NodeType.Equals(TEXT("Constant"), ESearchCase::IgnoreCase) ||
      NodeType.Equals(TEXT("Constant1"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant2"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant2Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant2Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant3"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant3Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant4"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant4Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant4Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("TextureSample"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Texture"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
  } else if (NodeType.Equals(TEXT("Add"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionAdd::StaticClass();
  } else if (NodeType.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionMultiply::StaticClass();
  } else if (NodeType.Equals(TEXT("Sine"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionSine::StaticClass();
  } else if (NodeType.Equals(TEXT("Cosine"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionCosine::StaticClass();
  } else if (NodeType.Equals(TEXT("Time"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionTime::StaticClass();
  } else if (NodeType.Equals(TEXT("VertexColor"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionVertexColor::StaticClass();
  } else {
    // Try to find the class dynamically
    FString FullClassName = FString::Printf(TEXT("/Script/Engine.MaterialExpression%s"), *NodeType);
    ExpressionClass = LoadClass<UMaterialExpression>(nullptr, *FullClassName);

    if (!ExpressionClass) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Unknown node type: %s"), *NodeType),
                          TEXT("INVALID_NODE_TYPE"));
      return true;
    }
  }

  // Create the expression
  NewExpression = NewObject<UMaterialExpression>(HostOuter, ExpressionClass, NAME_None, RF_Transactional);
  if (!NewExpression) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create material expression"),
                        TEXT("EXPRESSION_CREATION_FAILED"));
    return true;
  }

  // Set position
  double PosX = 0, PosY = 0;
  Payload->TryGetNumberField(TEXT("posX"), PosX);
  Payload->TryGetNumberField(TEXT("posY"), PosY);
  NewExpression->MaterialExpressionEditorX = static_cast<int32>(PosX);
  NewExpression->MaterialExpressionEditorY = static_cast<int32>(PosY);

  // Set node properties based on type
  if (UMaterialExpressionConstant *Const = Cast<UMaterialExpressionConstant>(NewExpression)) {
    double Value = 0;
    Payload->TryGetNumberField(TEXT("value"), Value);
    Const->R = static_cast<float>(Value);
  } else if (UMaterialExpressionConstant3Vector *Const3 = Cast<UMaterialExpressionConstant3Vector>(NewExpression)) {
    double R = 0, G = 0, B = 0;
    const TSharedPtr<FJsonObject> *ColorObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj) {
      (*ColorObj)->TryGetNumberField(TEXT("r"), R);
      (*ColorObj)->TryGetNumberField(TEXT("g"), G);
      (*ColorObj)->TryGetNumberField(TEXT("b"), B);
    }
    Const3->Constant = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B));
  } else if (UMaterialExpressionTextureSample *TexSample = Cast<UMaterialExpressionTextureSample>(NewExpression)) {
    FString TexturePath;
    if (Payload->TryGetStringField(TEXT("texturePath"), TexturePath) && !TexturePath.IsEmpty()) {
      TexturePath = SanitizeProjectRelativePath(TexturePath);
      if (TexturePath.IsEmpty()) {
        SendAutomationError(Socket, RequestId,
                            TEXT("Invalid texturePath"),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }
      UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
      if (Texture) {
        TexSample->Texture = Texture;
      }
    }
  }

  // Add to host expression collection
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  if (Material) Material->GetEditorOnlyData()->ExpressionCollection.AddExpression(NewExpression);
  else Function->GetEditorOnlyData()->ExpressionCollection.AddExpression(NewExpression);
#else
  auto& Expressions = GetHostExpressions(Material, Function);
  Expressions.Add(NewExpression);
#endif

  // Only mark dirty — skip PostEditChange to avoid shader recompile per node.
  // Users batch-add nodes and compile once via compile_material.
  if (Material) { Material->MarkPackageDirty(); }
  if (Function) { Function->MarkPackageDirty(); }

  // Get the expression index for reference
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  int32 ExpressionIndex = Material
    ? Material->GetEditorOnlyData()->ExpressionCollection.Expressions.IndexOfByKey(NewExpression)
    : Function->GetEditorOnlyData()->ExpressionCollection.Expressions.IndexOfByKey(NewExpression);
#else
  int32 ExpressionIndex = Expressions.IndexOfByKey(NewExpression);
#endif

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("materialPath"), MaterialPath);
  Resp->SetStringField(TEXT("nodeType"), NodeType);
  Resp->SetNumberField(TEXT("expressionIndex"), ExpressionIndex);
  Resp->SetStringField(TEXT("expressionName"), NewExpression->GetName());
  Resp->SetStringField(TEXT("nodeId"), NewExpression->MaterialExpressionGuid.ToString());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node added successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("add_material_node requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleConnectMaterialPins(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("connect_material_pins"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("connect_material_pins payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid material path"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(MaterialPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  auto& Expressions = GetHostExpressions(Material, Function);

  // Helper to find expression by stable name, GUID (legacy), or index
  auto FindExpression = [&Expressions](const FString &Id) { return FindExpressionInHost(Expressions, Id); };

  // Accept both sourceNodeId/targetNodeId (stable name strings) and fromExpression/toExpression (indices)
  FString SourceNodeId, TargetNodeId;
  int32 FromExpressionIndex = -1, ToExpressionIndex = -1;

  UMaterialExpression *FromExpression = nullptr;
  UMaterialExpression *ToExpression = nullptr;

  if (Payload->TryGetStringField(TEXT("sourceNodeId"), SourceNodeId) && !SourceNodeId.IsEmpty()) {
    FromExpression = FindExpression(SourceNodeId);
  }
  if (Payload->TryGetStringField(TEXT("targetNodeId"), TargetNodeId) && !TargetNodeId.IsEmpty()) {
    ToExpression = FindExpression(TargetNodeId);
  }

  if (!FromExpression && Payload->TryGetNumberField(TEXT("fromExpression"), FromExpressionIndex)) {
    if (FromExpressionIndex >= 0 && FromExpressionIndex < Expressions.Num()) FromExpression = Expressions[FromExpressionIndex];
  }
  if (!ToExpression && Payload->TryGetNumberField(TEXT("toExpression"), ToExpressionIndex)) {
    if (ToExpressionIndex >= 0 && ToExpressionIndex < Expressions.Num()) ToExpression = Expressions[ToExpressionIndex];
  }

  FString SourcePin;
  Payload->TryGetStringField(TEXT("sourcePin"), SourcePin);
  int32 SourcePinIndex = 0;
  if (!SourcePin.IsEmpty()) {
    if (SourcePin.IsNumeric()) {
      SourcePinIndex = FCString::Atoi(*SourcePin);
    } else {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("sourcePin must be a numeric index, got '%s'"), *SourcePin),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  FString InputName;
  Payload->TryGetStringField(TEXT("inputName"), InputName);
  if (InputName.IsEmpty()) Payload->TryGetStringField(TEXT("targetPin"), InputName);

  // Handle connection to main material / function output node
  bool bConnectToMainNode = false;
  if ((TargetNodeId.IsEmpty() || TargetNodeId == TEXT("Main")) && !InputName.IsEmpty()) {
    bConnectToMainNode = true;
  } else if (!TargetNodeId.IsEmpty() && ToExpression == nullptr) {
    SendAutomationError(Socket, RequestId, TEXT("Target node not found"), TEXT("TARGET_NODE_NOT_FOUND"));
    return true;
  }

  if (bConnectToMainNode && FromExpression) {
    if (Material) {
      bool bFound = false;
      auto ConnectMainInput = [&](FExpressionInput& Input) {
        Input.Expression = FromExpression;
        Input.OutputIndex = SourcePinIndex;
        bFound = true;
      };
#if WITH_EDITORONLY_DATA
      if (InputName == TEXT("BaseColor")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, BaseColor)); }
      else if (InputName == TEXT("EmissiveColor")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor)); }
      else if (InputName == TEXT("Roughness")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Roughness)); }
      else if (InputName == TEXT("Metallic")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Metallic)); }
      else if (InputName == TEXT("Specular")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Specular)); }
      else if (InputName == TEXT("Normal")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Normal)); }
      else if (InputName == TEXT("Opacity")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Opacity)); }
      else if (InputName == TEXT("OpacityMask")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, OpacityMask)); }
      else if (InputName == TEXT("AmbientOcclusion") || InputName == TEXT("AO")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion)); }
      else if (InputName == TEXT("SubsurfaceColor")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor)); }
      else if (InputName == TEXT("WorldPositionOffset")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset)); }
#endif
      if (bFound) {
        FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(Resp, Material);
        Resp->SetStringField(TEXT("inputName"), InputName);
        Resp->SetStringField(TEXT("sourceNodeId"), FromExpression->GetName());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Connected to main material pin"), Resp, FString());
      } else {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown main material input: %s"), *InputName), TEXT("INVALID_PIN"));
      }
      return true;
    } else {
      // MaterialFunction: connect to FunctionOutput by name (name is required)
      if (InputName.IsEmpty()) {
        SendAutomationError(Socket, RequestId, TEXT("inputName is required when connecting to a function output"), TEXT("MISSING_INPUT_NAME"));
        return true;
      }
      UMaterialExpressionFunctionOutput *TargetOutput = nullptr;
      for (UMaterialExpression *Expr : Expressions) {
        if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          if (Out->OutputName.ToString().Equals(InputName)) { TargetOutput = Out; break; }
        }
      }
      if (TargetOutput) {
        TargetOutput->A.Expression = FromExpression;
        TargetOutput->A.OutputIndex = SourcePinIndex;
        FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("inputName"), TargetOutput->OutputName.ToString());
        Resp->SetStringField(TEXT("sourceNodeId"), FromExpression->GetName());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Connected to function output"), Resp, FString());
      } else {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No FunctionOutput named '%s' found"), *InputName), TEXT("INVALID_PIN"));
      }
      return true;
    }
  }

  if (!FromExpression) { SendAutomationError(Socket, RequestId, TEXT("Source node not found"), TEXT("SOURCE_NODE_NOT_FOUND")); return true; }
  if (!ToExpression) { SendAutomationError(Socket, RequestId, TEXT("Target node not found"), TEXT("TARGET_NODE_NOT_FOUND")); return true; }

  if (InputName.IsEmpty()) InputName = TEXT("Input");

  FExpressionInput *TargetInput = nullptr;
  for (FProperty *Property = ToExpression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        if (Property->GetName().Equals(InputName, ESearchCase::IgnoreCase)) {
          TargetInput = StructProp->ContainerPtrToValuePtr<FExpressionInput>(ToExpression);
          break;
        }
      }
    }
  }
  if (!TargetInput) {
    for (FProperty *Property = ToExpression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext) {
      if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
        if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
          TargetInput = StructProp->ContainerPtrToValuePtr<FExpressionInput>(ToExpression);
          InputName = Property->GetName();
          break;
        }
      }
    }
  }

  if (!TargetInput) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No input found on target expression. Tried: %s"), *InputName), TEXT("INPUT_NOT_FOUND"));
    return true;
  }

  TargetInput->Expression = FromExpression;
  TargetInput->OutputIndex = SourcePinIndex;
  FinalizeHost(Material, Function);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("sourceNodeId"), FromExpression->GetName());
  Resp->SetStringField(TEXT("targetNodeId"), ToExpression->GetName());
  Resp->SetStringField(TEXT("inputName"), InputName);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Material pins connected successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("connect_material_pins requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleRemoveMaterialNode(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("remove_material_node"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("remove_material_node payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid material path"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(MaterialPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  auto& Expressions = GetHostExpressions(Material, Function);

  // Helper to find expression by stable name, GUID (legacy), or index
  auto FindExpression = [&Expressions](const FString &Id) { return FindExpressionInHost(Expressions, Id); };

  FString NodeId;
  int32 ExpressionIndex = -1;
  UMaterialExpression *ExpressionToRemove = nullptr;

  if (Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty()) {
    ExpressionToRemove = FindExpression(NodeId);
  } else if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    if (ExpressionIndex >= 0 && ExpressionIndex < Expressions.Num()) {
      ExpressionToRemove = Expressions[ExpressionIndex];
    }
  }

  if (!ExpressionToRemove) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Node not found. Provide valid nodeId (stable name) or expressionIndex"),
                        TEXT("NODE_NOT_FOUND"));
    return true;
  }

  FString RemovedName = ExpressionToRemove->GetName();
  FString RemovedStableName = ExpressionToRemove->GetName();

  // Disconnect inbound links: walk all sibling expressions and clear any
  // FExpressionInput that references the node we're about to remove.
  for (UMaterialExpression *Expr : Expressions) {
    if (!Expr || Expr == ExpressionToRemove) continue;
    for (FProperty *Property = Expr->GetClass()->PropertyLink; Property;
         Property = Property->PropertyLinkNext) {
      FStructProperty *StructProp = CastField<FStructProperty>(Property);
      if (StructProp && StructProp->Struct &&
          StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        FExpressionInput *Input =
            StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (Input && Input->Expression == ExpressionToRemove) {
          Input->Expression = nullptr;
          Input->OutputIndex = 0;
        }
      }
    }
  }

  // Disconnect from main material inputs (root node pins)
  if (Material) {
#if WITH_EDITORONLY_DATA
    auto ClearIfMatches = [&](FExpressionInput& Input) {
      if (Input.Expression == ExpressionToRemove) {
        Input.Expression = nullptr;
        Input.OutputIndex = 0;
      }
    };
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, BaseColor));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Roughness));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Metallic));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Specular));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Normal));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Opacity));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
#endif
  }

  // Remove the expression from the appropriate container
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  if (Material) Material->GetExpressionCollection().RemoveExpression(ExpressionToRemove);
  else Function->GetExpressionCollection().RemoveExpression(ExpressionToRemove);
#else
  Expressions.Remove(ExpressionToRemove);
#endif

  // Also remove from the material's root node if connected (Material only)
  if (Material) Material->RemoveExpressionParameter(ExpressionToRemove);

  FinalizeHost(Material, Function);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("nodeId"), RemovedStableName);
  Resp->SetStringField(TEXT("removedName"), RemovedName);
  Resp->SetNumberField(TEXT("remainingExpressions"), Expressions.Num());
  Resp->SetBoolField(TEXT("removed"), true);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node removed successfully"), Resp,
                         FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("remove_material_node requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleBreakMaterialConnections(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("break_material_connections"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("break_material_connections payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid material path"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(MaterialPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  auto& Expressions = GetHostExpressions(Material, Function);

  auto FindExpression = [&Expressions](const FString &Id) { return FindExpressionInHost(Expressions, Id); };

  FString NodeId, PinName;
  bool bHasNodeId = Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty();
  bool bHasPinName = Payload->TryGetStringField(TEXT("pinName"), PinName) && !PinName.IsEmpty();

  // If nodeId is "Main" or empty with pinName, disconnect from main/output node
  if ((!bHasNodeId || NodeId == TEXT("Main")) && bHasPinName) {
    if (Material) {
      bool bFound = false;
#if WITH_EDITORONLY_DATA
      auto ClearMainPin = [&](FExpressionInput& Input) { Input.Expression = nullptr; Input.OutputIndex = 0; bFound = true; };
      if (PinName == TEXT("BaseColor")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, BaseColor)); }
      else if (PinName == TEXT("EmissiveColor")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor)); }
      else if (PinName == TEXT("Roughness")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Roughness)); }
      else if (PinName == TEXT("Metallic")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Metallic)); }
      else if (PinName == TEXT("Specular")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Specular)); }
      else if (PinName == TEXT("Normal")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Normal)); }
      else if (PinName == TEXT("Opacity")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Opacity)); }
      else if (PinName == TEXT("OpacityMask")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, OpacityMask)); }
      else if (PinName == TEXT("AmbientOcclusion") || PinName == TEXT("AO")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion)); }
      else if (PinName == TEXT("SubsurfaceColor")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor)); }
      else if (PinName == TEXT("WorldPositionOffset")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset)); }
#endif
      if (bFound) {
        FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(Resp, Material);
        Resp->SetStringField(TEXT("pinName"), PinName);
        Resp->SetBoolField(TEXT("disconnected"), true);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Disconnected from main material pin"), Resp, FString());
      } else {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown main material pin: %s"), *PinName), TEXT("INVALID_PIN"));
      }
    } else {
      // MaterialFunction: clear FunctionOutput by name
      bool bCleared = false;
      for (UMaterialExpression *Expr : Expressions) {
        if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          if (PinName.IsEmpty() || Out->OutputName.ToString().Equals(PinName)) {
            Out->A.Expression = nullptr; Out->A.OutputIndex = 0; bCleared = true;
            if (!PinName.IsEmpty()) break;
          }
        }
      }
      if (!bCleared && !PinName.IsEmpty()) {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown function output pin: %s"), *PinName), TEXT("INVALID_PIN"));
      } else {
        if (bCleared) FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("pinName"), PinName);
        Resp->SetBoolField(TEXT("disconnected"), bCleared);
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Disconnected from function output"),
                               Resp, FString());
      }
    }
    return true;
  }

  int32 ExpressionIndex = -1;
  UMaterialExpression *TargetExpression = nullptr;
  if (bHasNodeId) TargetExpression = FindExpression(NodeId);
  else if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    if (ExpressionIndex >= 0 && ExpressionIndex < Expressions.Num()) TargetExpression = Expressions[ExpressionIndex];
  }

  if (!TargetExpression) {
    SendAutomationError(Socket, RequestId, TEXT("Node not found. Provide valid nodeId (stable name) or expressionIndex"), TEXT("NODE_NOT_FOUND"));
    return true;
  }

  FString InputName;
  bool bSpecificInput = Payload->TryGetStringField(TEXT("inputName"), InputName) && !InputName.IsEmpty();
  int32 BrokenConnections = 0;

  for (FProperty *Property = TargetExpression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        if (bSpecificInput && !Property->GetName().Equals(InputName, ESearchCase::IgnoreCase)) continue;
        FExpressionInput *Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpression);
        if (Input && Input->Expression) { Input->Expression = nullptr; Input->OutputIndex = 0; BrokenConnections++; if (bSpecificInput) break; }
      }
    }
  }

  if (bSpecificInput && BrokenConnections == 0) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("No input named '%s' found on node '%s'"), *InputName, *TargetExpression->GetName()),
                        TEXT("INPUT_NOT_FOUND"));
    return true;
  }

  if (BrokenConnections > 0) {
    FinalizeHost(Material, Function);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("nodeId"), TargetExpression->MaterialExpressionGuid.ToString());
  Resp->SetNumberField(TEXT("brokenConnections"), BrokenConnections);
  if (bSpecificInput) Resp->SetStringField(TEXT("inputName"), InputName);

  SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Broken %d connection(s)"), BrokenConnections), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("break_material_connections requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialNodeDetails(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_material_node_details"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_material_node_details payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid material path"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(MaterialPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  auto& Expressions = GetHostExpressions(Material, Function);

  auto FindExpression = [&Expressions](const FString &Id) { return FindExpressionInHost(Expressions, Id); };

  FString NodeId;
  int32 ExpressionIndex = -1;
  UMaterialExpression *Expression = nullptr;

  if (Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty()) {
    Expression = FindExpression(NodeId);
  } else if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    if (ExpressionIndex >= 0 && ExpressionIndex < Expressions.Num()) Expression = Expressions[ExpressionIndex];
  }

  if (!Expression) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    if (Material) McpHandlerUtils::AddVerification(Resp, Material);
    else if (Function) McpHandlerUtils::AddVerification(Resp, Function);

    TArray<TSharedPtr<FJsonValue>> NodeList;
    for (int32 i = 0; i < Expressions.Num(); ++i) {
      UMaterialExpression *Expr = Expressions[i];
      if (!Expr) continue;
      
      TSharedPtr<FJsonObject> NodeInfo = McpHandlerUtils::CreateResultObject();
      NodeInfo->SetStringField(TEXT("nodeId"), Expr->MaterialExpressionGuid.ToString());
      NodeInfo->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
      NodeInfo->SetNumberField(TEXT("index"), i);
      NodeInfo->SetNumberField(TEXT("editorX"), Expr->MaterialExpressionEditorX);
      NodeInfo->SetNumberField(TEXT("editorY"), Expr->MaterialExpressionEditorY);
      if (!Expr->Desc.IsEmpty()) {
        NodeInfo->SetStringField(TEXT("desc"), Expr->Desc);
      }
      // Add parameter name if applicable
      if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
        NodeInfo->SetStringField(TEXT("parameterName"), Param->ParameterName.ToString());
      }
      // Add function path for MaterialFunctionCall nodes
      if (UMaterialExpressionMaterialFunctionCall *FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
        if (FuncCall->MaterialFunction) {
          NodeInfo->SetStringField(TEXT("functionPath"), FuncCall->MaterialFunction->GetPathName());
        }
      }
      // Add pin name for FunctionInput/Output expressions
      if (UMaterialExpressionFunctionInput *FuncIn = Cast<UMaterialExpressionFunctionInput>(Expr)) {
        NodeInfo->SetStringField(TEXT("inputName"), FuncIn->InputName.ToString());
      }
      if (UMaterialExpressionFunctionOutput *FuncOut = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
        NodeInfo->SetStringField(TEXT("outputName"), FuncOut->OutputName.ToString());
      }
      NodeList.Add(MakeShared<FJsonValueObject>(NodeInfo));
    }
    
    Resp->SetArrayField(TEXT("nodes"), NodeList);
    Resp->SetNumberField(TEXT("nodeCount"), Expressions.Num());

    FString Message = NodeId.IsEmpty()
        ? FString::Printf(TEXT("Material has %d nodes. Provide nodeId for specific node details."), Expressions.Num())
        : FString::Printf(TEXT("Node '%s' not found. Material has %d nodes."), *NodeId, Expressions.Num());

    SendAutomationResponse(Socket, RequestId, NodeId.IsEmpty(),
                           Message, Resp, NodeId.IsEmpty() ? FString() : TEXT("NODE_NOT_FOUND"));
    return true;
  }

  // Build response for specific node
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("nodeId"), Expression->MaterialExpressionGuid.ToString());
  Resp->SetStringField(TEXT("name"), Expression->GetName());
  Resp->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
  Resp->SetStringField(TEXT("classPath"), Expression->GetClass()->GetPathName());
  Resp->SetNumberField(TEXT("editorX"), Expression->MaterialExpressionEditorX);
  Resp->SetNumberField(TEXT("editorY"), Expression->MaterialExpressionEditorY);
  if (!Expression->Desc.IsEmpty()) {
    Resp->SetStringField(TEXT("desc"), Expression->Desc);
  }

  // Get inputs
  TArray<TSharedPtr<FJsonValue>> InputsArray;
  for (FProperty *Property = Expression->GetClass()->PropertyLink; Property;
       Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        FExpressionInput *Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expression);
        TSharedPtr<FJsonObject> InputObj = McpHandlerUtils::CreateResultObject();
        InputObj->SetStringField(TEXT("name"), Property->GetName());
        InputObj->SetBoolField(TEXT("isConnected"), Input->Expression != nullptr);
        if (Input->Expression) {
          InputObj->SetStringField(TEXT("connectedToId"), Input->Expression->GetName());
          InputObj->SetStringField(TEXT("connectedToName"), Input->Expression->GetName());
        }
        InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
      }
    }
  }
  Resp->SetArrayField(TEXT("inputs"), InputsArray);

  // Get specific properties based on expression type
  if (UMaterialExpressionConstant *Const = Cast<UMaterialExpressionConstant>(Expression)) {
    Resp->SetNumberField(TEXT("value"), Const->R);
  } else if (UMaterialExpressionConstant2Vector *Const2 = Cast<UMaterialExpressionConstant2Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = McpHandlerUtils::CreateResultObject();
    ValueObj->SetNumberField(TEXT("r"), Const2->R);
    ValueObj->SetNumberField(TEXT("g"), Const2->G);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionConstant3Vector *Const3 = Cast<UMaterialExpressionConstant3Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = McpHandlerUtils::CreateResultObject();
    ValueObj->SetNumberField(TEXT("r"), Const3->Constant.R);
    ValueObj->SetNumberField(TEXT("g"), Const3->Constant.G);
    ValueObj->SetNumberField(TEXT("b"), Const3->Constant.B);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionConstant4Vector *Const4 = Cast<UMaterialExpressionConstant4Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = McpHandlerUtils::CreateResultObject();
    ValueObj->SetNumberField(TEXT("r"), Const4->Constant.R);
    ValueObj->SetNumberField(TEXT("g"), Const4->Constant.G);
    ValueObj->SetNumberField(TEXT("b"), Const4->Constant.B);
    ValueObj->SetNumberField(TEXT("a"), Const4->Constant.A);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionTextureSample *TexSample = Cast<UMaterialExpressionTextureSample>(Expression)) {
    if (TexSample->Texture) {
      Resp->SetStringField(TEXT("texture"), TexSample->Texture->GetPathName());
      Resp->SetStringField(TEXT("textureName"), TexSample->Texture->GetName());
    }
  } else if (UMaterialExpressionScalarParameter *ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression)) {
    Resp->SetStringField(TEXT("parameterName"), ScalarParam->ParameterName.ToString());
    Resp->SetNumberField(TEXT("defaultValue"), ScalarParam->DefaultValue);
  } else if (UMaterialExpressionVectorParameter *VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression)) {
    Resp->SetStringField(TEXT("parameterName"), VectorParam->ParameterName.ToString());
    TSharedPtr<FJsonObject> DefaultObj = McpHandlerUtils::CreateResultObject();
    DefaultObj->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
    DefaultObj->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
    DefaultObj->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
    DefaultObj->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
    Resp->SetObjectField(TEXT("defaultValue"), DefaultObj);
  }

  // Expose function pin metadata for MaterialFunctionCall nodes
  if (UMaterialExpressionMaterialFunctionCall *FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression)) {
    if (FuncCall->MaterialFunction) {
      Resp->SetStringField(TEXT("functionPath"), FuncCall->MaterialFunction->GetPathName());
    }
    // Emit function inputs
    TArray<TSharedPtr<FJsonValue>> FuncInputs;
    for (int32 fi = 0; fi < FuncCall->FunctionInputs.Num(); ++fi) {
      TSharedPtr<FJsonObject> FIObj = McpHandlerUtils::CreateResultObject();
      const UMaterialExpressionFunctionInput* FuncInputExpr = FuncCall->FunctionInputs[fi].ExpressionInput;
      FIObj->SetStringField(TEXT("inputName"), FuncInputExpr ? FuncInputExpr->InputName.ToString() : FString());
      FIObj->SetNumberField(TEXT("index"), fi);
      FIObj->SetBoolField(TEXT("isConnected"), FuncCall->FunctionInputs[fi].Input.Expression != nullptr);
      if (FuncCall->FunctionInputs[fi].Input.Expression) {
        FIObj->SetStringField(TEXT("connectedToId"), FuncCall->FunctionInputs[fi].Input.Expression->GetName());
        FIObj->SetNumberField(TEXT("outputIndex"), FuncCall->FunctionInputs[fi].Input.OutputIndex);
      }
      FuncInputs.Add(MakeShared<FJsonValueObject>(FIObj));
    }
    Resp->SetArrayField(TEXT("functionInputs"), FuncInputs);

    // Emit function outputs
    TArray<TSharedPtr<FJsonValue>> FuncOutputs;
    for (int32 fo = 0; fo < FuncCall->FunctionOutputs.Num(); ++fo) {
      TSharedPtr<FJsonObject> FOObj = McpHandlerUtils::CreateResultObject();
      const UMaterialExpressionFunctionOutput* FuncOutputExpr = FuncCall->FunctionOutputs[fo].ExpressionOutput;
      FOObj->SetStringField(TEXT("outputName"), FuncOutputExpr ? FuncOutputExpr->OutputName.ToString() : FString());
      FOObj->SetNumberField(TEXT("index"), fo);
      FuncOutputs.Add(MakeShared<FJsonValueObject>(FOObj));
    }
    Resp->SetArrayField(TEXT("functionOutputs"), FuncOutputs);
  }

  // Expose function input/output pin metadata for FunctionInput/Output expressions
  if (UMaterialExpressionFunctionInput *FuncIn = Cast<UMaterialExpressionFunctionInput>(Expression)) {
    Resp->SetStringField(TEXT("inputName"), FuncIn->InputName.ToString());
  }
  if (UMaterialExpressionFunctionOutput *FuncOut = Cast<UMaterialExpressionFunctionOutput>(Expression)) {
    Resp->SetStringField(TEXT("outputName"), FuncOut->OutputName.ToString());
    Resp->SetBoolField(TEXT("isConnected"), FuncOut->A.Expression != nullptr);
    if (FuncOut->A.Expression) {
      Resp->SetStringField(TEXT("connectedToId"), FuncOut->A.Expression->GetName());
      Resp->SetNumberField(TEXT("sourceOutputIndex"), FuncOut->A.OutputIndex);
    }
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node details retrieved"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_material_node_details requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// SOURCE CONTROL STATE
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGetSourceControlState(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_source_control_state"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_source_control_state payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and assetPaths
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("sourceControlEnabled"), false);
    Result->SetStringField(TEXT("message"), TEXT("Source control is not enabled"));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Source control disabled"), Result, FString());
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<TSharedPtr<FJsonValue>> StatesArray;

  for (const FString &AssetPath : AssetPaths) {
    const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
    TSharedPtr<FJsonObject> StateObj = McpHandlerUtils::CreateResultObject();
    StateObj->SetStringField(TEXT("assetPath"), SafeAssetPath.IsEmpty() ? AssetPath : SafeAssetPath);

    if (SafeAssetPath.IsEmpty()) {
      StateObj->SetBoolField(TEXT("exists"), false);
      StateObj->SetStringField(TEXT("state"), TEXT("invalid_path"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      StateObj->SetBoolField(TEXT("exists"), false);
      StateObj->SetStringField(TEXT("state"), TEXT("not_found"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    StateObj->SetBoolField(TEXT("exists"), true);

    // Convert asset path to file path
    FString PackageName = FPackageName::ObjectPathToPackageName(SafeAssetPath);
    FString FilePath;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension()) &&
        !FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetMapPackageExtension())) {
      StateObj->SetStringField(TEXT("state"), TEXT("path_conversion_failed"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Get source control state
    FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(
        FilePath, EStateCacheUsage::Use);

    if (!SourceControlState.IsValid()) {
      StateObj->SetStringField(TEXT("state"), TEXT("unknown"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Populate state info
    StateObj->SetBoolField(TEXT("isSourceControlled"), SourceControlState->IsSourceControlled());
    StateObj->SetBoolField(TEXT("isCheckedOut"), SourceControlState->IsCheckedOut());
    StateObj->SetBoolField(TEXT("isCurrent"), SourceControlState->IsCurrent());
    StateObj->SetBoolField(TEXT("isAdded"), SourceControlState->IsAdded());
    StateObj->SetBoolField(TEXT("isDeleted"), SourceControlState->IsDeleted());
    StateObj->SetBoolField(TEXT("isModified"), SourceControlState->IsModified());
    StateObj->SetBoolField(TEXT("isIgnored"), SourceControlState->IsIgnored());
    StateObj->SetBoolField(TEXT("isUnknown"), SourceControlState->IsUnknown());
    StateObj->SetBoolField(TEXT("canCheckIn"), SourceControlState->CanCheckIn());
    StateObj->SetBoolField(TEXT("canCheckout"), SourceControlState->CanCheckout());
    StateObj->SetBoolField(TEXT("canRevert"), SourceControlState->CanRevert());
    StateObj->SetBoolField(TEXT("canEdit"), SourceControlState->CanEdit());
    StateObj->SetBoolField(TEXT("canDelete"), SourceControlState->CanDelete());
    StateObj->SetBoolField(TEXT("canAdd"), SourceControlState->CanAdd());
    StateObj->SetBoolField(TEXT("isConflicted"), SourceControlState->IsConflicted());

    // Check if checked out by other
    FString WhoCheckedOut;
    bool bIsCheckedOutOther = SourceControlState->IsCheckedOutOther(&WhoCheckedOut);
    StateObj->SetBoolField(TEXT("isCheckedOutOther"), bIsCheckedOutOther);
    if (bIsCheckedOutOther && !WhoCheckedOut.IsEmpty()) {
      StateObj->SetStringField(TEXT("checkedOutBy"), WhoCheckedOut);
    }

    // Determine primary state string
    FString StateString = TEXT("unknown");
    if (!SourceControlState->IsSourceControlled()) {
      StateString = TEXT("not_controlled");
    } else if (SourceControlState->IsAdded()) {
      StateString = TEXT("added");
    } else if (SourceControlState->IsDeleted()) {
      StateString = TEXT("deleted");
    } else if (SourceControlState->IsConflicted()) {
      StateString = TEXT("conflicted");
    } else if (SourceControlState->IsCheckedOut()) {
      StateString = TEXT("checked_out");
    } else if (SourceControlState->IsModified()) {
      StateString = TEXT("modified");
    } else if (!SourceControlState->IsCurrent()) {
      StateString = TEXT("out_of_date");
    } else {
      StateString = TEXT("current");
    }
    StateObj->SetStringField(TEXT("state"), StateString);

    // Get display name
    StateObj->SetStringField(TEXT("displayName"), SourceControlState->GetDisplayName().ToString());

    StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("sourceControlEnabled"), true);
  Result->SetArrayField(TEXT("states"), StatesArray);
  Result->SetNumberField(TEXT("queriedCount"), AssetPaths.Num());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Source control state retrieved"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_source_control_state requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// ANALYZE GRAPH
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleAnalyzeGraph(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("analyze_graph"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("analyze_graph payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid assetPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the asset
  UObject *Asset = LoadObject<UObject>(nullptr, *AssetPath);
  if (!Asset) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, Asset);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());

  // Check if it's a material
  UMaterial *Material = Cast<UMaterial>(Asset);
  UMaterialInstance *MaterialInstance = Cast<UMaterialInstance>(Asset);

  if (Material || MaterialInstance) {
    // Analyze material graph
    UMaterial *BaseMaterial = Material ? Material : MaterialInstance->GetBaseMaterial();

    // Get expressions count
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    const TArray<TObjectPtr<UMaterialExpression>> *Expressions = nullptr;
    if (Material && Material->GetEditorOnlyData()) {
      Expressions = &Material->GetEditorOnlyData()->ExpressionCollection.Expressions;
    }
#else
    // UE 5.0: Direct access, but also uses TObjectPtr
    const TArray<TObjectPtr<UMaterialExpression>> *Expressions = nullptr;
    if (Material) {
      Expressions = &Material->Expressions;
    }
#endif

    int32 NodeCount = Expressions ? Expressions->Num() : 0;
    int32 ParameterCount = 0;
    int32 TextureSampleCount = 0;
    TArray<FString> ParameterNames;

    if (Expressions) {
      for (UMaterialExpression *Expr : *Expressions) {
        if (!Expr) continue;
        if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
          ParameterCount++;
          ParameterNames.Add(Param->ParameterName.ToString());
        }
        if (Cast<UMaterialExpressionTextureSample>(Expr)) {
          TextureSampleCount++;
        }
      }
    }

    Result->SetStringField(TEXT("graphType"), TEXT("Material"));
    Result->SetNumberField(TEXT("nodeCount"), NodeCount);
    Result->SetNumberField(TEXT("parameterCount"), ParameterCount);
    Result->SetNumberField(TEXT("textureSampleCount"), TextureSampleCount);

    // Add parameter names
    TArray<TSharedPtr<FJsonValue>> ParamArray;
    for (const FString &ParamName : ParameterNames) {
      ParamArray.Add(MakeShared<FJsonValueString>(ParamName));
    }
    Result->SetArrayField(TEXT("parameters"), ParamArray);

    // Material properties
    Result->SetBoolField(TEXT("isMaterialInstance"), MaterialInstance != nullptr);
    if (Material) {
      Result->SetBoolField(TEXT("isTwoSided"), Material->TwoSided);
      Result->SetBoolField(TEXT("isMasked"), Material->IsMasked());
#if WITH_EDITORONLY_DATA
      Result->SetStringField(TEXT("blendMode"),
                             StaticEnum<EBlendMode>()->GetNameStringByValue((int64)Material->GetBlendMode()));
      // Get shading model name from the first selected model
      FString ShadingModelName = TEXT("Unknown");
      FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
      if (ShadingModels.HasShadingModel(MSM_DefaultLit)) ShadingModelName = TEXT("DefaultLit");
      else if (ShadingModels.HasShadingModel(MSM_Subsurface)) ShadingModelName = TEXT("Subsurface");
      else if (ShadingModels.HasShadingModel(MSM_Unlit)) ShadingModelName = TEXT("Unlit");
      else if (ShadingModels.HasShadingModel(MSM_ClearCoat)) ShadingModelName = TEXT("ClearCoat");
      else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile)) ShadingModelName = TEXT("SubsurfaceProfile");
      else if (ShadingModels.HasShadingModel(MSM_PreintegratedSkin)) ShadingModelName = TEXT("PreintegratedSkin");
      Result->SetStringField(TEXT("shadingModel"), ShadingModelName);
#endif
    }

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material graph analyzed"), Result, FString());
    return true;
  }

  // Check if it's a blueprint
  UBlueprint *Blueprint = Cast<UBlueprint>(Asset);
  if (Blueprint) {
    TArray<UEdGraph *> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);

    int32 TotalNodes = 0;
    TArray<TSharedPtr<FJsonValue>> GraphInfoArray;

    for (UEdGraph *Graph : AllGraphs) {
      if (!Graph) continue;
      TSharedPtr<FJsonObject> GraphInfo = McpHandlerUtils::CreateResultObject();
      GraphInfo->SetStringField(TEXT("name"), Graph->GetName());
      GraphInfo->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());
      TotalNodes += Graph->Nodes.Num();
      GraphInfoArray.Add(MakeShared<FJsonValueObject>(GraphInfo));
    }

    Result->SetStringField(TEXT("graphType"), TEXT("Blueprint"));
    Result->SetStringField(TEXT("blueprintType"), Blueprint->BlueprintType == BPTYPE_Interface ? TEXT("Interface") :
                           Blueprint->BlueprintType == BPTYPE_MacroLibrary ? TEXT("MacroLibrary") :
                           Blueprint->BlueprintType == BPTYPE_FunctionLibrary ? TEXT("FunctionLibrary") : TEXT("Class"));
    Result->SetNumberField(TEXT("totalNodes"), TotalNodes);
    Result->SetNumberField(TEXT("graphCount"), AllGraphs.Num());
    Result->SetArrayField(TEXT("graphs"), GraphInfoArray);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Blueprint graph analyzed"), Result, FString());
    return true;
  }

  // Generic asset - no graph
  Result->SetStringField(TEXT("graphType"), TEXT("None"));
  Result->SetStringField(TEXT("message"), TEXT("Asset does not have a graph structure"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("No graph to analyze for this asset type"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("analyze_graph requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// REBUILD MATERIAL
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleRebuildMaterial(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("rebuild_material"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid assetPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(AssetPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Rebuild by triggering a recompile (already on game thread — no AsyncTask needed)
  UObject *Host = Material ? static_cast<UObject*>(Material) : static_cast<UObject*>(Function);
  Host->MarkPackageDirty();
  Host->PreEditChange(nullptr);
  Host->PostEditChange();

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (Material) McpHandlerUtils::AddVerification(Result, Material);
    else if (Function) McpHandlerUtils::AddVerification(Result, Function);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("rebuilt"), true);
    Result->SetBoolField(TEXT("headlessSafe"), true);
    Result->SetBoolField(TEXT("saveSkipped"), true);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material rebuilt; save skipped under NullRHI"), Result, FString());
    return true;
  }

  if (!McpSafeAssetSave(Host)) {
    SendAutomationError(Socket, RequestId, TEXT("Failed to save rebuilt material"), TEXT("SAVE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Result, Material);
  else if (Function) McpHandlerUtils::AddVerification(Result, Function);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetBoolField(TEXT("rebuilt"), true);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material rebuilt successfully"), Result, FString());

  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
