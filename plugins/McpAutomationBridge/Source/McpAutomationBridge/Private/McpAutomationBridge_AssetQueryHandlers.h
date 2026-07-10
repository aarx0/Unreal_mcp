#pragma once

// manage_asset query handlers (asset_query dispatcher + search_assets) as namespaced free functions.
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
bool HandleAssetQueryAction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSearchAssets(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
} // namespace McpHandlers::Asset
