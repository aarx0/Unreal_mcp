#pragma once

// manage_ai (Behavior Tree graph actions) handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError
// / S.ResolveBlueprintOrError). Dispatch: MCP/Calls/McpCalls_ManageAi.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Ai
{
bool HandleBehaviorTreeCreate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleBehaviorTreeAddNode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleBehaviorTreeConnectNodes(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleBehaviorTreeRemoveNode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleBehaviorTreeBreakConnections(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleBehaviorTreeSetNodeProperties(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleBehaviorTreeAddSubnode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);

} // namespace McpHandlers::Ai
