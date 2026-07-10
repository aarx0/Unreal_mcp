#pragma once

// manage_networking Sessions (split-screen/local-players/LAN/voice) handlers as
// namespaced free functions. De-membered off UMcpAutomationBridgeSubsystem
// (F1 module split); the subsystem is passed by reference as the first parameter
// (S). Each wrapper delegates to a file-local editor-only worker or answers the
// non-editor stub through S.SendAutomationResponse. Declares into the shared
// manage_networking family namespace (McpHandlers::Networking).
// Dispatch: MCP/Calls/McpCalls_ManageNetworking.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Networking
{
bool HandleSessionsAddLocalPlayer(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSessionsRemoveLocalPlayer(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSessionsHostLanServer(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSessionsEnableVoiceChat(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSessionsMutePlayer(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSessionsGetSessionsInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Networking
