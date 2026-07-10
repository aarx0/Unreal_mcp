#pragma once

// Foliage handlers as namespaced free functions. De-membered off
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
bool HandlePaintFoliage(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleRemoveFoliage(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGetFoliageInstances(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleAddFoliageType(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleAddFoliageInstances(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCreateProceduralFoliage(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::BuildEnvironment
