#pragma once

// manage_networking GameFramework (game-mode/state/controller/player-start/respawn)
// handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// Declares into the shared manage_networking family namespace (McpHandlers::Networking).
// Dispatch: MCP/Calls/McpCalls_ManageNetworking.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Networking
{
bool HandleGameFrameworkCreateGameMode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkCreateGameState(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkCreatePlayerController(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkCreatePlayerState(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkCreateGameInstance(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkCreateHudClass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkSetDefaultPawnClass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkSetPlayerControllerClass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkSetGameStateClass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkSetPlayerStateClass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkConfigureGameRules(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkConfigurePlayerStart(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkSetRespawnRules(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkConfigureSpectating(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGameFrameworkGetGameFrameworkInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Networking
