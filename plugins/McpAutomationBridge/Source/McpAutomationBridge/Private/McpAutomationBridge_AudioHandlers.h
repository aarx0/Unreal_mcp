#pragma once

// manage_audio core handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// Dispatch: MCP/Calls/McpCalls_ManageAudio.cpp. Audio authoring actions live in
// the sibling McpAutomationBridge_AudioAuthoringHandlers.h (same family namespace).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Audio
{
bool HandleAudioCreateSoundCue(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioCreateSoundClass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioCreateSoundMix(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioPlaySoundAtLocation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioPlaySound2D(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioPlaySoundAttached(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioCreateAmbientSound(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioSpawnSoundAtLocation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioPushSoundMix(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioPopSoundMix(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioSetSoundMixClassOverride(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioClearSoundMixClassOverride(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioSetBaseSoundMix(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
// fade_sound_out/fade_sound_in shared implementation.
bool HandleAudioFadeSoundInternal(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket,
                                        bool bFadeIn);
bool HandleAudioFadeSoundOut(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioFadeSoundIn(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioPrimeSound(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioCreateAudioComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioEnableAudioAnalysis(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioSetDopplerEffect(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioSetAudioOcclusion(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioSetSoundAttenuation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioFadeSound(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAudioCreateReverbZone(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);

} // namespace McpHandlers::Audio
