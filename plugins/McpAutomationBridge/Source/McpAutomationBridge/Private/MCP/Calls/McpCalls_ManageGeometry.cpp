// LINT-TOOL: manage_geometry
// manage_geometry as FMcpCall classes — eighteenth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers — HandleGeometry* (GeometryHandlers.cpp) — until the
// module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageGeometry
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations
// (McpDecl_ManageGeometry.h) and re-verified against the static free-function
// bodies the retired dispatcher delegated to — zero decl fixes needed, the
// first fully clean port of a large family: the read-sets reconcile on all
// 76 rows (create_plane's height/widthSegments/heightSegments are the body's
// first-choice reads with depth/widthSubdivisions/depthSubdivisions as
// fallbacks), and every required flag matches a body reject. The joint
// rejects require every named spelling (targetActor AND toolActor on the
// three booleans, actorName AND trimActorName on boolean_trim, actorName AND
// splineActorName on duplicate_along_spline; extrude_along_spline rejects
// each separately), so those rows keep both spellings required — these are
// AND requirements, not one-of alias pairs. get_mesh_info and generate_lods
// joint-reject only when BOTH actorName and assetPath are absent, so both
// stay optional (at-least-one handler-enforced), and sweep's splineActorName
// stays optional — an absent spelling falls back to a straight-path sweep.
inline const FMcpParamDecl P_CreateBox[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("depth"), EMcpParamKind::Number, false }, { TEXT("dimensions"), EMcpParamKind::Any, false }, { TEXT("widthSegments"), EMcpParamKind::Number, false }, { TEXT("heightSegments"), EMcpParamKind::Number, false }, { TEXT("depthSegments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateSphere[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("subdivisions"), EMcpParamKind::Number, false }, { TEXT("numRings"), EMcpParamKind::Number, false }, { TEXT("radialSegments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateCylinder[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("segments"), EMcpParamKind::Number, false }, { TEXT("numSides"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateCone[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("baseRadius"), EMcpParamKind::Number, false }, { TEXT("topRadius"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("segments"), EMcpParamKind::Number, false }, { TEXT("numSides"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateCapsule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("length"), EMcpParamKind::Number, false }, { TEXT("hemisphereSteps"), EMcpParamKind::Number, false }, { TEXT("segments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateTorus[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("majorRadius"), EMcpParamKind::Number, false }, { TEXT("minorRadius"), EMcpParamKind::Number, false }, { TEXT("majorSegments"), EMcpParamKind::Number, false }, { TEXT("minorSegments"), EMcpParamKind::Number, false }, { TEXT("numRings"), EMcpParamKind::Number, false }, { TEXT("radialSegments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreatePlane[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("depth"), EMcpParamKind::Number, false }, { TEXT("widthSegments"), EMcpParamKind::Number, false }, { TEXT("widthSubdivisions"), EMcpParamKind::Number, false }, { TEXT("heightSegments"), EMcpParamKind::Number, false }, { TEXT("depthSubdivisions"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateDisc[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("segments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateStairs[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("stepWidth"), EMcpParamKind::Number, false }, { TEXT("stepHeight"), EMcpParamKind::Number, false }, { TEXT("stepDepth"), EMcpParamKind::Number, false }, { TEXT("numSteps"), EMcpParamKind::Number, false }, { TEXT("floating"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateSpiralStairs[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("stepWidth"), EMcpParamKind::Number, false }, { TEXT("stepHeight"), EMcpParamKind::Number, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("curveAngle"), EMcpParamKind::Number, false }, { TEXT("numTurns"), EMcpParamKind::Number, false }, { TEXT("numSteps"), EMcpParamKind::Number, false }, { TEXT("floating"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateRing[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("outerRadius"), EMcpParamKind::Number, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("segments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateArch[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("majorRadius"), EMcpParamKind::Number, false }, { TEXT("minorRadius"), EMcpParamKind::Number, false }, { TEXT("angle"), EMcpParamKind::Number, false }, { TEXT("majorSteps"), EMcpParamKind::Number, false }, { TEXT("minorSteps"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreatePipe[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("outerRadius"), EMcpParamKind::Number, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("radialSteps"), EMcpParamKind::Number, false }, { TEXT("heightSteps"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateRamp[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("length"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Revolve[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("angle"), EMcpParamKind::Number, false }, { TEXT("steps"), EMcpParamKind::Number, false }, { TEXT("capped"), EMcpParamKind::Bool, false }, { TEXT("profile"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_BooleanUnion[] = { { TEXT("targetActor"), EMcpParamKind::String, true }, { TEXT("toolActor"), EMcpParamKind::String, true }, { TEXT("keepTool"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BooleanSubtract[] = { { TEXT("targetActor"), EMcpParamKind::String, true }, { TEXT("toolActor"), EMcpParamKind::String, true }, { TEXT("keepTool"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BooleanIntersection[] = { { TEXT("targetActor"), EMcpParamKind::String, true }, { TEXT("toolActor"), EMcpParamKind::String, true }, { TEXT("keepTool"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BooleanTrim[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("trimActorName"), EMcpParamKind::String, true }, { TEXT("keepInside"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SelfUnion[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("fillHoles"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetMeshInfo[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_RecalculateNormals[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("areaWeighted"), EMcpParamKind::Bool, false }, { TEXT("computeWeightedNormals"), EMcpParamKind::Bool, false }, { TEXT("hardEdgeAngle"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_FlipNormals[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SimplifyMesh[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("targetTriangleCount"), EMcpParamKind::Number, false }, { TEXT("targetPercentage"), EMcpParamKind::Number, false }, { TEXT("reductionPercent"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Subdivide[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("iterations"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AutoUV[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConvertToStaticMesh[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Extrude[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false }, { TEXT("direction"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_Inset[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Outset[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Bevel[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false }, { TEXT("subdivisions"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_OffsetFaces[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Shell[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("thickness"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Chamfer[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false }, { TEXT("steps"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Bend[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("angle"), EMcpParamKind::Number, false }, { TEXT("extent"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Twist[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("angle"), EMcpParamKind::Number, false }, { TEXT("extent"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Taper[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("flareX"), EMcpParamKind::Number, false }, { TEXT("flareY"), EMcpParamKind::Number, false }, { TEXT("extent"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_NoiseDeform[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("magnitude"), EMcpParamKind::Number, false }, { TEXT("frequency"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Smooth[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("iterations"), EMcpParamKind::Number, false }, { TEXT("alpha"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Relax[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("iterations"), EMcpParamKind::Number, false }, { TEXT("strength"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Stretch[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("axis"), EMcpParamKind::String, false }, { TEXT("factor"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Spherify[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("factor"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Cylindrify[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("axis"), EMcpParamKind::String, false }, { TEXT("factor"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_LatticeDeform[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("latticeResolution"), EMcpParamKind::Number, false }, { TEXT("weight"), EMcpParamKind::Number, false }, { TEXT("strength"), EMcpParamKind::Number, false }, { TEXT("position"), EMcpParamKind::Object, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("axis"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_DisplaceByTexture[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("texturePath"), EMcpParamKind::String, true }, { TEXT("heightScale"), EMcpParamKind::Number, false }, { TEXT("strength"), EMcpParamKind::Number, false }, { TEXT("midpoint"), EMcpParamKind::Number, false }, { TEXT("axis"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_WeldVertices[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("tolerance"), EMcpParamKind::Number, false }, { TEXT("weldDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_FillHoles[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_RemoveDegenerates[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_RemeshUniform[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("targetTriangleCount"), EMcpParamKind::Number, false }, { TEXT("targetEdgeLength"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_MergeVertices[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("tolerance"), EMcpParamKind::Number, false }, { TEXT("compact"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GenerateCollision[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("collisionType"), EMcpParamKind::String, false }, { TEXT("hullCount"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Mirror[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("axis"), EMcpParamKind::String, false }, { TEXT("weld"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ArrayLinear[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("count"), EMcpParamKind::Number, false }, { TEXT("offset"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ArrayRadial[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("count"), EMcpParamKind::Number, false }, { TEXT("center"), EMcpParamKind::Any, false }, { TEXT("axis"), EMcpParamKind::String, false }, { TEXT("angle"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Triangulate[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Poke[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("offset"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ProjectUV[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("projectionType"), EMcpParamKind::String, false }, { TEXT("scale"), EMcpParamKind::Number, false }, { TEXT("uvChannel"), EMcpParamKind::Number, false }, { TEXT("uvScale"), EMcpParamKind::Object, false }, { TEXT("uvOffset"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_TransformUVs[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("uvChannel"), EMcpParamKind::Number, false }, { TEXT("translateU"), EMcpParamKind::Number, false }, { TEXT("translateV"), EMcpParamKind::Number, false }, { TEXT("scaleU"), EMcpParamKind::Number, false }, { TEXT("scaleV"), EMcpParamKind::Number, false }, { TEXT("rotation"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_RecomputeTangents[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Bridge[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("edgeGroupA"), EMcpParamKind::Number, false }, { TEXT("edgeGroupB"), EMcpParamKind::Number, false }, { TEXT("subdivisions"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Loft[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("subdivisions"), EMcpParamKind::Number, false }, { TEXT("smooth"), EMcpParamKind::Bool, false }, { TEXT("cap"), EMcpParamKind::Bool, false }, { TEXT("profileActors"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_Sweep[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("splineActorName"), EMcpParamKind::String, false }, { TEXT("steps"), EMcpParamKind::Number, false }, { TEXT("twist"), EMcpParamKind::Number, false }, { TEXT("scaleStart"), EMcpParamKind::Number, false }, { TEXT("scaleEnd"), EMcpParamKind::Number, false }, { TEXT("cap"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_LoopCut[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("numCuts"), EMcpParamKind::Number, false }, { TEXT("offset"), EMcpParamKind::Number, false }, { TEXT("axis"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_DuplicateAlongSpline[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("splineActorName"), EMcpParamKind::String, true }, { TEXT("count"), EMcpParamKind::Number, false }, { TEXT("alignToSpline"), EMcpParamKind::Bool, false }, { TEXT("scaleVariation"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_UnwrapUV[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("uvChannel"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_PackUVIslands[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("uvChannel"), EMcpParamKind::Number, false }, { TEXT("textureResolution"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConvertToNanite[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ExtrudeAlongSpline[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("splineActorName"), EMcpParamKind::String, true }, { TEXT("segments"), EMcpParamKind::Number, false }, { TEXT("cap"), EMcpParamKind::Bool, false }, { TEXT("scaleStart"), EMcpParamKind::Number, false }, { TEXT("scaleEnd"), EMcpParamKind::Number, false }, { TEXT("twist"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_EdgeSplit[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("edges"), EMcpParamKind::Array, false }, { TEXT("edgeIndex"), EMcpParamKind::Number, false }, { TEXT("splitFactor"), EMcpParamKind::Number, false }, { TEXT("weldVertices"), EMcpParamKind::Bool, false }, { TEXT("weldTolerance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Quadrangulate[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("targetQuadSize"), EMcpParamKind::Number, false }, { TEXT("preserveFeatures"), EMcpParamKind::Bool, false }, { TEXT("featureAngleThreshold"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_RemeshVoxel[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("voxelSize"), EMcpParamKind::Number, false }, { TEXT("surfaceDistance"), EMcpParamKind::Number, false }, { TEXT("fillHoles"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GenerateComplexCollision[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("maxHullCount"), EMcpParamKind::Number, false }, { TEXT("hullPrecision"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SimplifyCollision[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("simplificationFactor"), EMcpParamKind::Number, false }, { TEXT("targetHullCount"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_GenerateLODs[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("lodCount"), EMcpParamKind::Number, false }, { TEXT("assetPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetLODSettings[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("lodIndex"), EMcpParamKind::Number, false }, { TEXT("trianglePercent"), EMcpParamKind::Number, false }, { TEXT("recomputeNormals"), EMcpParamKind::Bool, false }, { TEXT("recomputeTangents"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetLODScreenSizes[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("screenSizes"), EMcpParamKind::Array, true } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row: the implementation TU is compiled
// only WITH_EDITOR (dispatcher included), so the members answer exactly as
// the retired chain did in every build flavor — absent in non-editor builds
// with the flag answering at Execute(), and each member's
// MCP_HAS_FULL_GEOMETRY_SCRIPT stub answering the chain's NOT_SUPPORTED on
// pre-5.1 engines. Mutating on everything except the one reader,
// get_mesh_info.

#define MCP_GE_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)      \
class FMcpCall_ManageGeometry_##ClassSuffix final : public FMcpCall                      \
{                                                                                        \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl Decl{ TEXT("manage_geometry"), TEXT(ActionLiteral),    \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                 \
		return Decl;                                                                     \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

// Primitives
MCP_GE_CALL(CreateBox, "create_box", P_CreateBox, HandleGeometryCreateBox, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateSphere, "create_sphere", P_CreateSphere, HandleGeometryCreateSphere, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateCylinder, "create_cylinder", P_CreateCylinder, HandleGeometryCreateCylinder, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateCone, "create_cone", P_CreateCone, HandleGeometryCreateCone, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateCapsule, "create_capsule", P_CreateCapsule, HandleGeometryCreateCapsule, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateTorus, "create_torus", P_CreateTorus, HandleGeometryCreateTorus, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreatePlane, "create_plane", P_CreatePlane, HandleGeometryCreatePlane, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateDisc, "create_disc", P_CreateDisc, HandleGeometryCreateDisc, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateStairs, "create_stairs", P_CreateStairs, HandleGeometryCreateStairs, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateSpiralStairs, "create_spiral_stairs", P_CreateSpiralStairs, HandleGeometryCreateSpiralStairs, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateRing, "create_ring", P_CreateRing, HandleGeometryCreateRing, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateArch, "create_arch", P_CreateArch, HandleGeometryCreateArch, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreatePipe, "create_pipe", P_CreatePipe, HandleGeometryCreatePipe, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateRamp, "create_ramp", P_CreateRamp, HandleGeometryCreateRamp, EMcpCallFlags::Mutating)
MCP_GE_CALL(Revolve, "revolve", P_Revolve, HandleGeometryRevolve, EMcpCallFlags::Mutating)

// Booleans
MCP_GE_CALL(BooleanUnion, "boolean_union", P_BooleanUnion, HandleGeometryBooleanUnion, EMcpCallFlags::Mutating)
MCP_GE_CALL(BooleanSubtract, "boolean_subtract", P_BooleanSubtract, HandleGeometryBooleanSubtract, EMcpCallFlags::Mutating)
MCP_GE_CALL(BooleanIntersection, "boolean_intersection", P_BooleanIntersection, HandleGeometryBooleanIntersection, EMcpCallFlags::Mutating)
MCP_GE_CALL(BooleanTrim, "boolean_trim", P_BooleanTrim, HandleGeometryBooleanTrim, EMcpCallFlags::Mutating)
MCP_GE_CALL(SelfUnion, "self_union", P_SelfUnion, HandleGeometrySelfUnion, EMcpCallFlags::Mutating)

// Mesh Utils
MCP_GE_CALL(GetMeshInfo, "get_mesh_info", P_GetMeshInfo, HandleGeometryGetMeshInfo, EMcpCallFlags::None)
MCP_GE_CALL(RecalculateNormals, "recalculate_normals", P_RecalculateNormals, HandleGeometryRecalculateNormals, EMcpCallFlags::Mutating)
MCP_GE_CALL(FlipNormals, "flip_normals", P_FlipNormals, HandleGeometryFlipNormals, EMcpCallFlags::Mutating)
MCP_GE_CALL(SimplifyMesh, "simplify_mesh", P_SimplifyMesh, HandleGeometrySimplifyMesh, EMcpCallFlags::Mutating)
MCP_GE_CALL(Subdivide, "subdivide", P_Subdivide, HandleGeometrySubdivide, EMcpCallFlags::Mutating)
MCP_GE_CALL(AutoUV, "auto_uv", P_AutoUV, HandleGeometryAutoUV, EMcpCallFlags::Mutating)
MCP_GE_CALL(ConvertToStaticMesh, "convert_to_static_mesh", P_ConvertToStaticMesh, HandleGeometryConvertToStaticMesh, EMcpCallFlags::Mutating)

// Modeling Operations
MCP_GE_CALL(Extrude, "extrude", P_Extrude, HandleGeometryExtrude, EMcpCallFlags::Mutating)
MCP_GE_CALL(Inset, "inset", P_Inset, HandleGeometryInset, EMcpCallFlags::Mutating)
MCP_GE_CALL(Outset, "outset", P_Outset, HandleGeometryOutset, EMcpCallFlags::Mutating)
MCP_GE_CALL(Bevel, "bevel", P_Bevel, HandleGeometryBevel, EMcpCallFlags::Mutating)
MCP_GE_CALL(OffsetFaces, "offset_faces", P_OffsetFaces, HandleGeometryOffsetFaces, EMcpCallFlags::Mutating)
MCP_GE_CALL(Shell, "shell", P_Shell, HandleGeometryShell, EMcpCallFlags::Mutating)
MCP_GE_CALL(Chamfer, "chamfer", P_Chamfer, HandleGeometryChamfer, EMcpCallFlags::Mutating)

// Deformers
MCP_GE_CALL(Bend, "bend", P_Bend, HandleGeometryBend, EMcpCallFlags::Mutating)
MCP_GE_CALL(Twist, "twist", P_Twist, HandleGeometryTwist, EMcpCallFlags::Mutating)
MCP_GE_CALL(Taper, "taper", P_Taper, HandleGeometryTaper, EMcpCallFlags::Mutating)
MCP_GE_CALL(NoiseDeform, "noise_deform", P_NoiseDeform, HandleGeometryNoiseDeform, EMcpCallFlags::Mutating)
MCP_GE_CALL(Smooth, "smooth", P_Smooth, HandleGeometrySmooth, EMcpCallFlags::Mutating)
MCP_GE_CALL(Relax, "relax", P_Relax, HandleGeometryRelax, EMcpCallFlags::Mutating)
MCP_GE_CALL(Stretch, "stretch", P_Stretch, HandleGeometryStretch, EMcpCallFlags::Mutating)
MCP_GE_CALL(Spherify, "spherify", P_Spherify, HandleGeometrySpherify, EMcpCallFlags::Mutating)
MCP_GE_CALL(Cylindrify, "cylindrify", P_Cylindrify, HandleGeometryCylindrify, EMcpCallFlags::Mutating)
MCP_GE_CALL(LatticeDeform, "lattice_deform", P_LatticeDeform, HandleGeometryLatticeDeform, EMcpCallFlags::Mutating)
MCP_GE_CALL(DisplaceByTexture, "displace_by_texture", P_DisplaceByTexture, HandleGeometryDisplaceByTexture, EMcpCallFlags::Mutating)

// Mesh Repair
MCP_GE_CALL(WeldVertices, "weld_vertices", P_WeldVertices, HandleGeometryWeldVertices, EMcpCallFlags::Mutating)
MCP_GE_CALL(FillHoles, "fill_holes", P_FillHoles, HandleGeometryFillHoles, EMcpCallFlags::Mutating)
MCP_GE_CALL(RemoveDegenerates, "remove_degenerates", P_RemoveDegenerates, HandleGeometryRemoveDegenerates, EMcpCallFlags::Mutating)
MCP_GE_CALL(RemeshUniform, "remesh_uniform", P_RemeshUniform, HandleGeometryRemeshUniform, EMcpCallFlags::Mutating)
MCP_GE_CALL(MergeVertices, "merge_vertices", P_MergeVertices, HandleGeometryMergeVertices, EMcpCallFlags::Mutating)

// Collision Generation
MCP_GE_CALL(GenerateCollision, "generate_collision", P_GenerateCollision, HandleGeometryGenerateCollision, EMcpCallFlags::Mutating)

// Transform Operations
MCP_GE_CALL(Mirror, "mirror", P_Mirror, HandleGeometryMirror, EMcpCallFlags::Mutating)
MCP_GE_CALL(ArrayLinear, "array_linear", P_ArrayLinear, HandleGeometryArrayLinear, EMcpCallFlags::Mutating)
MCP_GE_CALL(ArrayRadial, "array_radial", P_ArrayRadial, HandleGeometryArrayRadial, EMcpCallFlags::Mutating)

// Mesh Topology Operations
MCP_GE_CALL(Triangulate, "triangulate", P_Triangulate, HandleGeometryTriangulate, EMcpCallFlags::Mutating)
MCP_GE_CALL(Poke, "poke", P_Poke, HandleGeometryPoke, EMcpCallFlags::Mutating)

// UV Operations
MCP_GE_CALL(ProjectUV, "project_uv", P_ProjectUV, HandleGeometryProjectUV, EMcpCallFlags::Mutating)
MCP_GE_CALL(TransformUVs, "transform_uvs", P_TransformUVs, HandleGeometryTransformUVs, EMcpCallFlags::Mutating)

// Tangent Operations
MCP_GE_CALL(RecomputeTangents, "recompute_tangents", P_RecomputeTangents, HandleGeometryRecomputeTangents, EMcpCallFlags::Mutating)

// Advanced Operations (Bridge, Loft, Sweep)
MCP_GE_CALL(Bridge, "bridge", P_Bridge, HandleGeometryBridge, EMcpCallFlags::Mutating)
MCP_GE_CALL(Loft, "loft", P_Loft, HandleGeometryLoft, EMcpCallFlags::Mutating)
MCP_GE_CALL(Sweep, "sweep", P_Sweep, HandleGeometrySweep, EMcpCallFlags::Mutating)
MCP_GE_CALL(LoopCut, "loop_cut", P_LoopCut, HandleGeometryLoopCut, EMcpCallFlags::Mutating)
MCP_GE_CALL(DuplicateAlongSpline, "duplicate_along_spline", P_DuplicateAlongSpline, HandleGeometryDuplicateAlongSpline, EMcpCallFlags::Mutating)

// Additional UV Operations
MCP_GE_CALL(UnwrapUV, "unwrap_uv", P_UnwrapUV, HandleGeometryUnwrapUV, EMcpCallFlags::Mutating)
MCP_GE_CALL(PackUVIslands, "pack_uv_islands", P_PackUVIslands, HandleGeometryPackUVIslands, EMcpCallFlags::Mutating)

// Nanite Conversion
MCP_GE_CALL(ConvertToNanite, "convert_to_nanite", P_ConvertToNanite, HandleGeometryConvertToNanite, EMcpCallFlags::Mutating)

// Spline-based Operations
MCP_GE_CALL(ExtrudeAlongSpline, "extrude_along_spline", P_ExtrudeAlongSpline, HandleGeometryExtrudeAlongSpline, EMcpCallFlags::Mutating)

// Edge Operations
MCP_GE_CALL(EdgeSplit, "edge_split", P_EdgeSplit, HandleGeometryEdgeSplit, EMcpCallFlags::Mutating)

// Topology Operations
MCP_GE_CALL(Quadrangulate, "quadrangulate", P_Quadrangulate, HandleGeometryQuadrangulate, EMcpCallFlags::Mutating)

// Remesh Operations
MCP_GE_CALL(RemeshVoxel, "remesh_voxel", P_RemeshVoxel, HandleGeometryRemeshVoxel, EMcpCallFlags::Mutating)

// Complex Collision
MCP_GE_CALL(GenerateComplexCollision, "generate_complex_collision", P_GenerateComplexCollision, HandleGeometryGenerateComplexCollision, EMcpCallFlags::Mutating)

// Collision Simplification
MCP_GE_CALL(SimplifyCollision, "simplify_collision", P_SimplifyCollision, HandleGeometrySimplifyCollision, EMcpCallFlags::Mutating)

// LOD Operations (Geometry-specific)
MCP_GE_CALL(GenerateLODs, "generate_lods", P_GenerateLODs, HandleGeometryGenerateLODs, EMcpCallFlags::Mutating)
MCP_GE_CALL(SetLODSettings, "set_lod_settings", P_SetLODSettings, HandleGeometrySetLODSettings, EMcpCallFlags::Mutating)
MCP_GE_CALL(SetLODScreenSizes, "set_lod_screen_sizes", P_SetLODScreenSizes, HandleGeometrySetLODScreenSizes, EMcpCallFlags::Mutating)

#undef MCP_GE_CALL

} // namespace McpCalls::ManageGeometry

void McpRegisterManageGeometryCalls()
{
	using namespace McpCalls::ManageGeometry;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateSphere>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateCylinder>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateCone>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateCapsule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateTorus>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreatePlane>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateDisc>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateStairs>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateSpiralStairs>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateRing>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateArch>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreatePipe>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_CreateRamp>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Revolve>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_BooleanUnion>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_BooleanSubtract>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_BooleanIntersection>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_BooleanTrim>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_SelfUnion>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_GetMeshInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_RecalculateNormals>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_FlipNormals>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_SimplifyMesh>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Subdivide>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_AutoUV>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_ConvertToStaticMesh>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Extrude>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Inset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Outset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Bevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_OffsetFaces>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Shell>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Chamfer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Bend>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Twist>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Taper>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_NoiseDeform>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Smooth>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Relax>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Stretch>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Spherify>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Cylindrify>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_LatticeDeform>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_DisplaceByTexture>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_WeldVertices>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_FillHoles>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_RemoveDegenerates>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_RemeshUniform>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_MergeVertices>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_GenerateCollision>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Mirror>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_ArrayLinear>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_ArrayRadial>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Triangulate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Poke>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_ProjectUV>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_TransformUVs>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_RecomputeTangents>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Bridge>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Loft>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Sweep>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_LoopCut>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_DuplicateAlongSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_UnwrapUV>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_PackUVIslands>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_ConvertToNanite>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_ExtrudeAlongSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_EdgeSplit>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_Quadrangulate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_RemeshVoxel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_GenerateComplexCollision>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_SimplifyCollision>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_GenerateLODs>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_SetLODSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGeometry_SetLODScreenSizes>());
}
