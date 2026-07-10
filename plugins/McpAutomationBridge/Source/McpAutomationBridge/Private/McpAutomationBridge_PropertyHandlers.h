#pragma once

// inspect property/cdo/diff handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// Dispatch: MCP/Calls/McpCalls_*.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Inspect
{
bool HandleSetObjectProperty(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                             const FString& Action, const TSharedPtr<FJsonObject>& Payload,
                             FMcpResponseHandle Socket);
bool HandleGetObjectProperty(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                             const FString& Action, const TSharedPtr<FJsonObject>& Payload,
                             FMcpResponseHandle Socket);
bool HandleInspectCdoAction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleDiffAssetAction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Inspect
