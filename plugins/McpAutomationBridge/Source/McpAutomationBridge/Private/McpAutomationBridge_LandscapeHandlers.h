#pragma once

// Landscape handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// The build_environment wrappers in EnvironmentHandlers.cpp forward here.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::BuildEnvironment
{
bool HandleCreateLandscape(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleModifyHeightmap(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSculptLandscape(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePaintLandscapeLayer(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSetLandscapeMaterial(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCreateLandscapeGrassType(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::BuildEnvironment
