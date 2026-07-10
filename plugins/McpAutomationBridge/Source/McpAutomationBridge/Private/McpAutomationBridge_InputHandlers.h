#pragma once

// manage_input handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// Dispatch: MCP/Calls/McpCalls_ManageInput.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Input
{
bool HandleInputCreateInputAction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInputCreateInputMappingContext(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInputAddMapping(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInputRemoveMapping(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInputSetInputTrigger(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInputSetInputModifier(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInputGetInputInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Input
