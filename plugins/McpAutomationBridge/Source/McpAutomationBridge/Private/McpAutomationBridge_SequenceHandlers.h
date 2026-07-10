#pragma once

// manage_sequence handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public API (S.SendAutomationResponse / S.SendAutomationError /
// S.ResolveSequencePath / S.FindActorByName). Dispatch:
// MCP/Calls/McpCalls_ManageSequence.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Sequence
{
bool HandleSequenceCreate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceList(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceDuplicate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceRename(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceDelete(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceListTrackTypes(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceOpen(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceGetBindings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceGetMetadata(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceGetProperties(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceSetProperties(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceAddCamera(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceAddActor(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceAddActors(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceRemoveActors(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceAddSpawnable(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceAddKeyframe(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceAddSection(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceAddTrack(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceRemoveTrack(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceListTracks(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequencePlay(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequencePause(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceStop(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceSetPlaybackSpeed(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceSetDisplayRate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceSetTickResolution(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceSetViewRange(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceSetWorkRange(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceSetTrackMuted(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceSetTrackSolo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSequenceSetTrackLocked(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Sequence
