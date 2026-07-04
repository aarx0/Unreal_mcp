// Action declarations for manage_geometry — the server's contract: which params
// each action reads, and which are required. Fleet-authored from handler
// source (three-witness cross-check, 2026-07-04), hand-maintained since:
// adding an action = adding its declaration here (the boot validation and
// tests/schema/action-decl-lint.ps1 enforce both directions).
// UnverifiedDecl = no reachable read path was attributable; validation skips
// those actions and the lint nags until someone verifies or removes them.
#pragma once

#include "MCP/McpCallRegistry.h"

namespace McpDecls
{
inline const FMcpParamDecl P_ManageGeometry_0[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("count"), EMcpParamKind::Number, false }, { TEXT("offset"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ManageGeometry_1[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("count"), EMcpParamKind::Number, false }, { TEXT("center"), EMcpParamKind::Any, false }, { TEXT("axis"), EMcpParamKind::String, false }, { TEXT("angle"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_2[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageGeometry_3[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("angle"), EMcpParamKind::Number, false }, { TEXT("extent"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_4[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false }, { TEXT("subdivisions"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_5[] = { { TEXT("targetActor"), EMcpParamKind::String, true }, { TEXT("toolActor"), EMcpParamKind::String, true }, { TEXT("keepTool"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageGeometry_6[] = { { TEXT("targetActor"), EMcpParamKind::String, true }, { TEXT("toolActor"), EMcpParamKind::String, true }, { TEXT("keepTool"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageGeometry_7[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("trimActorName"), EMcpParamKind::String, true }, { TEXT("keepInside"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageGeometry_8[] = { { TEXT("targetActor"), EMcpParamKind::String, true }, { TEXT("toolActor"), EMcpParamKind::String, true }, { TEXT("keepTool"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageGeometry_9[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("edgeGroupA"), EMcpParamKind::Number, false }, { TEXT("edgeGroupB"), EMcpParamKind::Number, false }, { TEXT("subdivisions"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_10[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false }, { TEXT("steps"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_11[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageGeometry_12[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageGeometry_13[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("majorRadius"), EMcpParamKind::Number, false }, { TEXT("minorRadius"), EMcpParamKind::Number, false }, { TEXT("angle"), EMcpParamKind::Number, false }, { TEXT("majorSteps"), EMcpParamKind::Number, false }, { TEXT("minorSteps"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_14[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("depth"), EMcpParamKind::Number, false }, { TEXT("dimensions"), EMcpParamKind::Any, false }, { TEXT("widthSegments"), EMcpParamKind::Number, false }, { TEXT("heightSegments"), EMcpParamKind::Number, false }, { TEXT("depthSegments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_15[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("length"), EMcpParamKind::Number, false }, { TEXT("hemisphereSteps"), EMcpParamKind::Number, false }, { TEXT("segments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_16[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("baseRadius"), EMcpParamKind::Number, false }, { TEXT("topRadius"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("segments"), EMcpParamKind::Number, false }, { TEXT("numSides"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_17[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("segments"), EMcpParamKind::Number, false }, { TEXT("numSides"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_18[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("segments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_19[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("outerRadius"), EMcpParamKind::Number, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("radialSteps"), EMcpParamKind::Number, false }, { TEXT("heightSteps"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_20[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("depth"), EMcpParamKind::Number, false }, { TEXT("widthSegments"), EMcpParamKind::Number, false }, { TEXT("widthSubdivisions"), EMcpParamKind::Number, false }, { TEXT("heightSegments"), EMcpParamKind::Number, false }, { TEXT("depthSubdivisions"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_21[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("length"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_22[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("outerRadius"), EMcpParamKind::Number, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("segments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_23[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("subdivisions"), EMcpParamKind::Number, false }, { TEXT("numRings"), EMcpParamKind::Number, false }, { TEXT("radialSegments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_24[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("stepWidth"), EMcpParamKind::Number, false }, { TEXT("stepHeight"), EMcpParamKind::Number, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("curveAngle"), EMcpParamKind::Number, false }, { TEXT("numTurns"), EMcpParamKind::Number, false }, { TEXT("numSteps"), EMcpParamKind::Number, false }, { TEXT("floating"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageGeometry_25[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("stepWidth"), EMcpParamKind::Number, false }, { TEXT("stepHeight"), EMcpParamKind::Number, false }, { TEXT("stepDepth"), EMcpParamKind::Number, false }, { TEXT("numSteps"), EMcpParamKind::Number, false }, { TEXT("floating"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageGeometry_26[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("majorRadius"), EMcpParamKind::Number, false }, { TEXT("minorRadius"), EMcpParamKind::Number, false }, { TEXT("majorSegments"), EMcpParamKind::Number, false }, { TEXT("minorSegments"), EMcpParamKind::Number, false }, { TEXT("numRings"), EMcpParamKind::Number, false }, { TEXT("radialSegments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_27[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("axis"), EMcpParamKind::String, false }, { TEXT("factor"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_28[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("texturePath"), EMcpParamKind::String, true }, { TEXT("heightScale"), EMcpParamKind::Number, false }, { TEXT("strength"), EMcpParamKind::Number, false }, { TEXT("midpoint"), EMcpParamKind::Number, false }, { TEXT("axis"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageGeometry_29[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("splineActorName"), EMcpParamKind::String, true }, { TEXT("count"), EMcpParamKind::Number, false }, { TEXT("alignToSpline"), EMcpParamKind::Bool, false }, { TEXT("scaleVariation"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_30[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("edges"), EMcpParamKind::Array, false }, { TEXT("edgeIndex"), EMcpParamKind::Number, false }, { TEXT("splitFactor"), EMcpParamKind::Number, false }, { TEXT("weldVertices"), EMcpParamKind::Bool, false }, { TEXT("weldTolerance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_31[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false }, { TEXT("direction"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ManageGeometry_32[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("splineActorName"), EMcpParamKind::String, true }, { TEXT("segments"), EMcpParamKind::Number, false }, { TEXT("cap"), EMcpParamKind::Bool, false }, { TEXT("scaleStart"), EMcpParamKind::Number, false }, { TEXT("scaleEnd"), EMcpParamKind::Number, false }, { TEXT("twist"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_33[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageGeometry_34[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageGeometry_35[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("collisionType"), EMcpParamKind::String, false }, { TEXT("hullCount"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_36[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("maxHullCount"), EMcpParamKind::Number, false }, { TEXT("hullPrecision"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_37[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("lodCount"), EMcpParamKind::Number, false }, { TEXT("assetPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageGeometry_38[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageGeometry_39[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_40[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("latticeResolution"), EMcpParamKind::Number, false }, { TEXT("weight"), EMcpParamKind::Number, false }, { TEXT("strength"), EMcpParamKind::Number, false }, { TEXT("position"), EMcpParamKind::Object, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("axis"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageGeometry_41[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("subdivisions"), EMcpParamKind::Number, false }, { TEXT("smooth"), EMcpParamKind::Bool, false }, { TEXT("cap"), EMcpParamKind::Bool, false }, { TEXT("profileActors"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_ManageGeometry_42[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("numCuts"), EMcpParamKind::Number, false }, { TEXT("offset"), EMcpParamKind::Number, false }, { TEXT("axis"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageGeometry_44[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("axis"), EMcpParamKind::String, false }, { TEXT("weld"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageGeometry_45[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("magnitude"), EMcpParamKind::Number, false }, { TEXT("frequency"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_46[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_47[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("distance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_48[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("uvChannel"), EMcpParamKind::Number, false }, { TEXT("textureResolution"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_49[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("offset"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_50[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("projectionType"), EMcpParamKind::String, false }, { TEXT("scale"), EMcpParamKind::Number, false }, { TEXT("uvChannel"), EMcpParamKind::Number, false }, { TEXT("uvScale"), EMcpParamKind::Object, false }, { TEXT("uvOffset"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ManageGeometry_51[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("targetQuadSize"), EMcpParamKind::Number, false }, { TEXT("preserveFeatures"), EMcpParamKind::Bool, false }, { TEXT("featureAngleThreshold"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_52[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("areaWeighted"), EMcpParamKind::Bool, false }, { TEXT("computeWeightedNormals"), EMcpParamKind::Bool, false }, { TEXT("hardEdgeAngle"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_53[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageGeometry_54[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("iterations"), EMcpParamKind::Number, false }, { TEXT("strength"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_55[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("targetTriangleCount"), EMcpParamKind::Number, false }, { TEXT("targetEdgeLength"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_56[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("voxelSize"), EMcpParamKind::Number, false }, { TEXT("surfaceDistance"), EMcpParamKind::Number, false }, { TEXT("fillHoles"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageGeometry_57[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageGeometry_58[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("angle"), EMcpParamKind::Number, false }, { TEXT("steps"), EMcpParamKind::Number, false }, { TEXT("capped"), EMcpParamKind::Bool, false }, { TEXT("profile"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_ManageGeometry_59[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("fillHoles"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageGeometry_60[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("screenSizes"), EMcpParamKind::Array, true } };
inline const FMcpParamDecl P_ManageGeometry_61[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("lodIndex"), EMcpParamKind::Number, false }, { TEXT("trianglePercent"), EMcpParamKind::Number, false }, { TEXT("recomputeNormals"), EMcpParamKind::Bool, false }, { TEXT("recomputeTangents"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageGeometry_62[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("thickness"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_63[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("simplificationFactor"), EMcpParamKind::Number, false }, { TEXT("targetHullCount"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_64[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("targetTriangleCount"), EMcpParamKind::Number, false }, { TEXT("targetPercentage"), EMcpParamKind::Number, false }, { TEXT("reductionPercent"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_65[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("iterations"), EMcpParamKind::Number, false }, { TEXT("alpha"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_66[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("factor"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_67[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("axis"), EMcpParamKind::String, false }, { TEXT("factor"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_68[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("iterations"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_69[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("splineActorName"), EMcpParamKind::String, false }, { TEXT("steps"), EMcpParamKind::Number, false }, { TEXT("twist"), EMcpParamKind::Number, false }, { TEXT("scaleStart"), EMcpParamKind::Number, false }, { TEXT("scaleEnd"), EMcpParamKind::Number, false }, { TEXT("cap"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageGeometry_70[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("flareX"), EMcpParamKind::Number, false }, { TEXT("flareY"), EMcpParamKind::Number, false }, { TEXT("extent"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_71[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("uvChannel"), EMcpParamKind::Number, false }, { TEXT("translateU"), EMcpParamKind::Number, false }, { TEXT("translateV"), EMcpParamKind::Number, false }, { TEXT("scaleU"), EMcpParamKind::Number, false }, { TEXT("scaleV"), EMcpParamKind::Number, false }, { TEXT("rotation"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_72[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageGeometry_73[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("angle"), EMcpParamKind::Number, false }, { TEXT("extent"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_74[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("uvChannel"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageGeometry_75[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("tolerance"), EMcpParamKind::Number, false }, { TEXT("weldDistance"), EMcpParamKind::Number, false } };

inline const FMcpParamDecl P_ManageGeometry_D0[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("tolerance"), EMcpParamKind::Number, false }, { TEXT("compact"), EMcpParamKind::Bool, false } };

inline const FMcpCallDecl GManageGeometry[] =
{
	{ TEXT("manage_geometry"), TEXT("array_linear"), P_ManageGeometry_0, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("array_radial"), P_ManageGeometry_1, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("auto_uv"), P_ManageGeometry_2, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("bend"), P_ManageGeometry_3, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("bevel"), P_ManageGeometry_4, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("boolean_intersection"), P_ManageGeometry_5, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("boolean_subtract"), P_ManageGeometry_6, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("boolean_trim"), P_ManageGeometry_7, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("boolean_union"), P_ManageGeometry_8, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("bridge"), P_ManageGeometry_9, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("chamfer"), P_ManageGeometry_10, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("convert_to_nanite"), P_ManageGeometry_11, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("convert_to_static_mesh"), P_ManageGeometry_12, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_arch"), P_ManageGeometry_13, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_box"), P_ManageGeometry_14, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_capsule"), P_ManageGeometry_15, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_cone"), P_ManageGeometry_16, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_cylinder"), P_ManageGeometry_17, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_disc"), P_ManageGeometry_18, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_pipe"), P_ManageGeometry_19, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_plane"), P_ManageGeometry_20, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_ramp"), P_ManageGeometry_21, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_ring"), P_ManageGeometry_22, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_sphere"), P_ManageGeometry_23, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_spiral_stairs"), P_ManageGeometry_24, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_stairs"), P_ManageGeometry_25, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("create_torus"), P_ManageGeometry_26, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("cylindrify"), P_ManageGeometry_27, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("displace_by_texture"), P_ManageGeometry_28, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("duplicate_along_spline"), P_ManageGeometry_29, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("edge_split"), P_ManageGeometry_30, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("extrude"), P_ManageGeometry_31, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("extrude_along_spline"), P_ManageGeometry_32, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("fill_holes"), P_ManageGeometry_33, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("flip_normals"), P_ManageGeometry_34, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("generate_collision"), P_ManageGeometry_35, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("generate_complex_collision"), P_ManageGeometry_36, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("generate_lods"), P_ManageGeometry_37, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("get_mesh_info"), P_ManageGeometry_38, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("inset"), P_ManageGeometry_39, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("lattice_deform"), P_ManageGeometry_40, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("loft"), P_ManageGeometry_41, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("loop_cut"), P_ManageGeometry_42, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("merge_vertices"), P_ManageGeometry_D0, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("mirror"), P_ManageGeometry_44, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("noise_deform"), P_ManageGeometry_45, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("offset_faces"), P_ManageGeometry_46, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("outset"), P_ManageGeometry_47, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("pack_uv_islands"), P_ManageGeometry_48, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("poke"), P_ManageGeometry_49, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("project_uv"), P_ManageGeometry_50, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("quadrangulate"), P_ManageGeometry_51, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("recalculate_normals"), P_ManageGeometry_52, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("recompute_tangents"), P_ManageGeometry_53, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("relax"), P_ManageGeometry_54, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("remesh_uniform"), P_ManageGeometry_55, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("remesh_voxel"), P_ManageGeometry_56, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("remove_degenerates"), P_ManageGeometry_57, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("revolve"), P_ManageGeometry_58, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("self_union"), P_ManageGeometry_59, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("set_lod_screen_sizes"), P_ManageGeometry_60, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("set_lod_settings"), P_ManageGeometry_61, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("shell"), P_ManageGeometry_62, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("simplify_collision"), P_ManageGeometry_63, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("simplify_mesh"), P_ManageGeometry_64, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("smooth"), P_ManageGeometry_65, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("spherify"), P_ManageGeometry_66, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("stretch"), P_ManageGeometry_67, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("subdivide"), P_ManageGeometry_68, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("sweep"), P_ManageGeometry_69, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("taper"), P_ManageGeometry_70, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("transform_uvs"), P_ManageGeometry_71, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("triangulate"), P_ManageGeometry_72, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("twist"), P_ManageGeometry_73, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("unwrap_uv"), P_ManageGeometry_74, EMcpCallFlags::None },
	{ TEXT("manage_geometry"), TEXT("weld_vertices"), P_ManageGeometry_75, EMcpCallFlags::None },
};
}
