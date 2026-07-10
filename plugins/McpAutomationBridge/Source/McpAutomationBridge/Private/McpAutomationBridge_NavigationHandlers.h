#pragma once

// manage_ai (navigation actions) handlers as namespaced free functions. De-membered off
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
bool HandleNavigationConfigureNavMeshSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNavigationSetNavAgentProperties(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNavigationRebuildNavigation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNavigationCreateNavModifierComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNavigationSetNavAreaClass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNavigationConfigureNavAreaCost(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNavigationCreateNavLinkProxy(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNavigationConfigureNavLink(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNavigationSetNavLinkType(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNavigationCreateSmartLink(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNavigationConfigureSmartLinkBehavior(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNavigationGetNavigationInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Ai
