#pragma once

// animation_physics authoring (AnimationAuthoring) handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers reach instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// Shared family namespace McpHandlers::AnimationPhysics spans three handler .cpps
// (Animation/AnimationAuthoring/Skeleton). Dispatch: MCP/Calls/McpCalls_AnimationPhysics.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::AnimationPhysics
{
bool HandleAnimAuthoringCreateAnimationSequence(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetSequenceLength(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddBoneTrack(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetBoneKey(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetCurveKey(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddNotify(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddNotifyState(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddSyncMarker(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetRootMotionSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetAdditiveSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringCreateMontage(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddMontageSection(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddMontageSlot(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetSectionTiming(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddMontageNotify(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetBlendIn(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetBlendOut(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringLinkSections(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringCreateBlendSpace1D(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringCreateBlendSpace2D(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddBlendSample(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringForceRebuildBlendSpace(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetAxisSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetInterpolationSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringCreateAimOffset(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddAimOffsetSample(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringCreateAnimBlueprint(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddStateMachine(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddState(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddTransition(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetTransitionRules(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddBlendNode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddCachedPose(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddSlotNode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringAddLayeredBlendPerBone(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetAnimGraphNodeValue(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringCreateControlRig(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringCreateIkRig(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringCreateIkRetargeter(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringSetRetargetChainMapping(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringGetAnimationInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimAuthoringBindAnimNotify(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
} // namespace McpHandlers::AnimationPhysics
