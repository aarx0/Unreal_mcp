#pragma once

// build_environment lighting handlers as namespaced free functions. De-membered
// off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// Dispatch: MCP/Calls/McpCalls_BuildEnvironment.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::BuildEnvironment
{
bool HandleLightingListLightTypes(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLightingCreateLight(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLightingCreateSkyLight(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLightingBuildLighting(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLightingEnsureSingleSkyLight(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLightingCreateLightmassVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLightingSetupVolumetricFog(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLightingSetupGlobalIllumination(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLightingConfigureShadows(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLightingSetExposure(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLightingSetAmbientOcclusion(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLightingCreateLightingEnabledLevel(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::BuildEnvironment
