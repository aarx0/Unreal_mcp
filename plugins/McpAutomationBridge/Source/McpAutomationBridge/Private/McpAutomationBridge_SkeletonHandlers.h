#pragma once

// animation_physics skeleton (Skeleton) handlers as namespaced free functions. De-membered off
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
bool HandleGetSkeletonInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleListBones(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleListSockets(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleCreateSocket(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleConfigureSocket(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleCreateVirtualBone(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleCreatePhysicsAsset(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleListPhysicsBodies(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAddPhysicsBody(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleConfigurePhysicsBody(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAddPhysicsConstraint(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleConfigureConstraintLimits(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleRenameBone(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSetBoneTransform(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleCreateMorphTarget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSetMorphTargetDeltas(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleImportMorphTargets(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNormalizeWeights(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandlePruneWeights(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleBindClothToSkeletalMesh(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleAssignClothAssetToMesh(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonGetSkeletonInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonListBones(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonListSockets(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonCreateSocket(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonConfigureSocket(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonCreateVirtualBone(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonCreatePhysicsAsset(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonListPhysicsBodies(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonAddPhysicsBody(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonConfigurePhysicsBody(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonAddPhysicsConstraint(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonConfigureConstraintLimits(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonRenameBone(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonSetBoneTransform(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonCreateMorphTarget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonSetMorphTargetDeltas(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonImportMorphTargets(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonNormalizeWeights(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonPruneWeights(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonBindClothToSkeletalMesh(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonAssignClothAssetToMesh(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonCreateSkeleton(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonAddBone(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonRemoveBone(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonSetBoneParent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonSetVertexWeights(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonAutoSkinWeights(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonCopyWeights(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleSkeletonMirrorWeights(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
} // namespace McpHandlers::AnimationPhysics
