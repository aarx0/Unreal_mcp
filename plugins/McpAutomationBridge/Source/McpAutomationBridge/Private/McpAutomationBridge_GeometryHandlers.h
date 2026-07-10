#pragma once

// manage_geometry handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S). Each handler is a thin wrapper over the
// TU-local static free functions in McpAutomationBridge_GeometryHandlers.cpp and
// reaches instance state only through the public response API
// (S.SendAutomationError). Dispatch: MCP/Calls/McpCalls_ManageGeometry.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Geometry
{
bool HandleGeometryCreateBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreateSphere(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreateCylinder(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreateCone(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreateCapsule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreateTorus(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreatePlane(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreateDisc(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreateStairs(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreateSpiralStairs(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreateRing(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreateArch(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreatePipe(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCreateRamp(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryRevolve(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryBooleanUnion(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryBooleanSubtract(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryBooleanIntersection(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryBooleanTrim(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometrySelfUnion(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryGetMeshInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryRecalculateNormals(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryFlipNormals(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometrySimplifyMesh(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometrySubdivide(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryAutoUV(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryConvertToStaticMesh(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryExtrude(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryInset(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryOutset(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryBevel(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryOffsetFaces(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryShell(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryChamfer(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryBend(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryTwist(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryTaper(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryNoiseDeform(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometrySmooth(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryRelax(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryStretch(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometrySpherify(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryCylindrify(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryLatticeDeform(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryDisplaceByTexture(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryWeldVertices(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryFillHoles(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryRemoveDegenerates(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryRemeshUniform(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryMergeVertices(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryGenerateCollision(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryMirror(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryArrayLinear(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryArrayRadial(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryTriangulate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryPoke(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryProjectUV(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryTransformUVs(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryRecomputeTangents(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryBridge(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryLoft(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometrySweep(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryLoopCut(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryDuplicateAlongSpline(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryUnwrapUV(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryPackUVIslands(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryConvertToNanite(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryExtrudeAlongSpline(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryEdgeSplit(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryQuadrangulate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryRemeshVoxel(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryGenerateComplexCollision(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometrySimplifyCollision(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometryGenerateLODs(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometrySetLODSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleGeometrySetLODScreenSizes(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Geometry
