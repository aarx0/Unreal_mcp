#pragma once

// animation_physics core (Animation) handlers as namespaced free functions. De-membered off
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
bool HandleAnimPhysCleanup(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysCreateBlendSpace(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysCreateBlendTree(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysCreateProceduralAnim(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysCreateStateMachine(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysSetupIk(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysConfigureVehicle(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysSetupPhysicsSimulation(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysCreateAnimationAsset(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysSetupRetargeting(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysPlayMontage(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysCreatePoseLibrary(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleAnimPhysActivateRagdoll(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandlePlayAnimMontage(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleSetupRagdoll(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
bool HandleActivateRagdoll(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
} // namespace McpHandlers::AnimationPhysics
