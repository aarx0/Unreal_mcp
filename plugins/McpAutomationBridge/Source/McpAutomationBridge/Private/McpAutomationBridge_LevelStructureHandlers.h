#pragma once

// manage_level_structure core handlers as namespaced free functions. De-membered
// off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers reach instance state only
// through its public response API. The family namespace McpHandlers::LevelStructure
// is shared with the volume handlers (McpAutomationBridge_VolumeHandlers.h).
// Dispatch: MCP/Calls/McpCalls_ManageLevelStructure.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::LevelStructure
{
bool HandleLevelStructureCreateLevel(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureCreateSublevel(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureConfigureLevelStreaming(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureSetStreamingDistance(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureEnableWorldPartition(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureConfigureGridSize(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureCreateDataLayer(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureAssignActorToDataLayer(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureConfigureHlodLayer(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureCreateMinimapVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureOpenLevelBlueprint(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureAddLevelBlueprintNode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureConnectLevelBlueprintNodes(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureCreateLevelInstance(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureCreatePackedLevelActor(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStructureGetInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::LevelStructure
