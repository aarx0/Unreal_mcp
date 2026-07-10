#pragma once

// manage_sequence handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// Dispatch: MCP/Calls/McpCalls_ManageSequence.cpp.
//
// Only the handlers that touch no private subsystem member are de-membered here.
// The remaining manage_sequence handlers stay UMcpAutomationBridgeSubsystem
// members because their bodies call the private helpers ResolveSequencePath /
// FindActorByName; a sweep that publicizes those can migrate them here.

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

} // namespace McpHandlers::Sequence
