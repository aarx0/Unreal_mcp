#pragma once

// build_environment spline handlers as namespaced free functions. De-membered
// off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S). Each wrapper forwards to the file-scope
// editor-only implementation and replicates the retired chain's editor-build stub.
// Dispatch: MCP/Calls/McpCalls_BuildEnvironment.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::BuildEnvironment
{
bool HandleSplineCreateSplineActor(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineAddSplinePoint(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineRemoveSplinePoint(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineSetSplinePointPosition(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineSetSplinePointTangents(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineSetSplinePointRotation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineSetSplinePointScale(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineSetSplineType(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineCreateSplineMeshComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineSetSplineMeshAsset(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineConfigureSplineMeshAxis(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineSetSplineMeshMaterial(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineScatterMeshesAlongSpline(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineCreateRoadSpline(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineCreateRiverSpline(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineCreateFenceSpline(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineCreateWallSpline(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineCreateCableSpline(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineCreatePipeSpline(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleSplineGetSplinesInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::BuildEnvironment
