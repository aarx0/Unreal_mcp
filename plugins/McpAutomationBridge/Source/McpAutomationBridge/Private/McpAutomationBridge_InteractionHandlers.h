#pragma once

// manage_interaction handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// Dispatch: MCP/Calls/McpCalls_ManageInteraction.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Interaction
{
bool HandleInteractionCreateComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionConfigureTrace(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionConfigureWidget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionAddEvents(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionCreateInteractableInterface(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionCreateDoorActor(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionConfigureDoorProperties(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionCreateSwitchActor(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionConfigureSwitchProperties(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionCreateChestActor(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionConfigureChestProperties(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionCreateLeverActor(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionAddDestructionComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionCreateTriggerActor(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInteractionGetInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Interaction
