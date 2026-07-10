#pragma once

// manage_level_structure volume handlers as namespaced free functions. De-membered
// off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers reach instance state only
// through its public response API. Declares into the shared family namespace
// McpHandlers::LevelStructure (core handlers: McpAutomationBridge_LevelStructureHandlers.h).
// Dispatch: MCP/Calls/McpCalls_ManageLevelStructure.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::LevelStructure
{
bool HandleVolumeCreateTriggerVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateTriggerBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateTriggerSphere(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateTriggerCapsule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateBlockingVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateKillZVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreatePainCausingVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreatePhysicsVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateAudioVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateReverbVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreatePostProcessVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateCullDistanceVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreatePrecomputedVisibilityVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateLightmassImportanceVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateNavMeshBoundsVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateNavModifierVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeCreateCameraBlockingVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeSetVolumeExtent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeSetVolumeProperties(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeSetVolumeBounds(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeRemoveVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeGetVolumesInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeAddTriggerVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeAddBlockingVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeAddKillZVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeAddPhysicsVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeAddCullDistanceVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleVolumeAddPostProcessVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::LevelStructure
