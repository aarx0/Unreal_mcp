#pragma once

// manage_audio authoring handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationError). Dispatch:
// MCP/Calls/McpCalls_ManageAudio.cpp. Audio core actions live in the sibling
// McpAutomationBridge_AudioHandlers.h (same family namespace).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Audio
{
bool HandleAudioAuthoringAddCueNode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringConnectCueNodes(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringSetCueAttenuation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringSetCueConcurrency(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringCreateMetasound(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringAddMetasoundNode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringConnectMetasoundNodes(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringAddMetasoundInput(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringAddMetasoundOutput(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringSetMetasoundDefault(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringSetClassProperties(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringSetClassParent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringAddMixModifier(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringConfigureMixEq(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringCreateAttenuationSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringConfigureDistanceAttenuation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringConfigureSpatialization(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringConfigureOcclusion(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringConfigureReverbSend(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringCreateDialogueVoice(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringCreateDialogueWave(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringSetDialogueContext(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringCreateReverbEffect(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringCreateSourceEffectChain(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringAddSourceEffect(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringCreateSubmixEffect(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioAuthoringGetAudioInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);

} // namespace McpHandlers::Audio
