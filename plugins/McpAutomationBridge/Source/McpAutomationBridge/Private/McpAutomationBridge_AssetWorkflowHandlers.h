#pragma once

// manage_asset core asset-workflow / source-control / bulk / datatable / material-instance handlers as namespaced free functions.
// De-membered off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem
// is passed by reference as the first parameter (S), and handlers touch instance
// state only through its public response API (S.SendAutomationResponse /
// S.SendAutomationError). Dispatch: MCP/Calls/McpCalls_ManageAsset.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Asset
{
bool HandleSaveAsset(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleFixupRedirectors(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleSourceControlCheckout(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleSourceControlSubmit(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleBulkRenameAssets(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleBulkDeleteAssets(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleGenerateThumbnail(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleImportAsset(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleSetMetadata(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleDuplicateAsset(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleRenameAsset(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleDeleteAssets(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleCreateFolder(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleGetDependencies(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleGetReferencers(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleGetAssetProperties(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleSetAssetProperty(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleGetAssetGraph(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleSetTags(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleValidateAsset(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleListAssets(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleGenerateReport(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleCreateDataTable(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleDataTableRowOp(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &SubAction, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleAddMaterialParameter(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleListMaterialInstances(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleResetInstanceParameters(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleDoesAssetExist(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleGetMaterialStats(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleGenerateLODs(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleGetMetadata(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleNaniteRebuildMesh(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleFindByTag(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleGetSourceControlState(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleAnalyzeGraph(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
} // namespace McpHandlers::Asset
