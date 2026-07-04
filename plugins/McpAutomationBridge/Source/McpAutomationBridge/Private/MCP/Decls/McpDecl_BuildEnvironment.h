// Action declarations for build_environment — the server's contract: which params
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
inline const FMcpParamDecl P_BuildEnvironment_1[] = { { TEXT("foliageTypePath"), EMcpParamKind::String, false }, { TEXT("foliageType"), EMcpParamKind::String, false }, { TEXT("transforms"), EMcpParamKind::Array, false }, { TEXT("locations"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_BuildEnvironment_2[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("position"), EMcpParamKind::Object, false }, { TEXT("index"), EMcpParamKind::Number, false }, { TEXT("pointType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_3[] = { { TEXT("quality"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_4[] = { { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("quality"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_5[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("randomizeScale"), EMcpParamKind::Bool, false }, { TEXT("minScale"), EMcpParamKind::Number, false }, { TEXT("maxScale"), EMcpParamKind::Number, false }, { TEXT("randomizeRotation"), EMcpParamKind::Bool, false }, { TEXT("rotationRange"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_6[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("spacing"), EMcpParamKind::Number, false }, { TEXT("useRandomOffset"), EMcpParamKind::Bool, false }, { TEXT("randomOffsetRange"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_7[] = { { TEXT("virtualShadowMaps"), EMcpParamKind::Bool, false }, { TEXT("rayTracedShadows"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BuildEnvironment_8[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("forwardAxis"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_9[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_10[] = { { TEXT("lightClass"), EMcpParamKind::String, false }, { TEXT("lightType"), EMcpParamKind::String, false }, { TEXT("type"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_BuildEnvironment_11[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_12[] = { { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("z"), EMcpParamKind::Number, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_13[] = { { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("z"), EMcpParamKind::Number, false }, { TEXT("location"), EMcpParamKind::Any, false }, { TEXT("componentsX"), EMcpParamKind::Number, false }, { TEXT("componentsY"), EMcpParamKind::Number, false }, { TEXT("componentCount"), EMcpParamKind::Any, false }, { TEXT("sizeX"), EMcpParamKind::Number, false }, { TEXT("sizeY"), EMcpParamKind::Number, false }, { TEXT("quadsPerComponent"), EMcpParamKind::Number, false }, { TEXT("quadsPerSection"), EMcpParamKind::Number, false }, { TEXT("sectionSize"), EMcpParamKind::Number, false }, { TEXT("sectionsPerComponent"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("landscapeName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_14[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("meshPath"), EMcpParamKind::String, true }, { TEXT("staticMesh"), EMcpParamKind::String, true }, { TEXT("density"), EMcpParamKind::Number, false }, { TEXT("minScale"), EMcpParamKind::Number, false }, { TEXT("maxScale"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_15[] = { { TEXT("lightClass"), EMcpParamKind::String, false }, { TEXT("lightType"), EMcpParamKind::String, false }, { TEXT("type"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_BuildEnvironment_16[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_BuildEnvironment_17[] = { { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("size"), EMcpParamKind::Object, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_18[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_19[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("bounds"), EMcpParamKind::Object, false }, { TEXT("foliageTypes"), EMcpParamKind::Array, false }, { TEXT("types"), EMcpParamKind::Array, false }, { TEXT("seed"), EMcpParamKind::Number, false }, { TEXT("tileSize"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_20[] = { { TEXT("sizeX"), EMcpParamKind::Number, false }, { TEXT("sizeY"), EMcpParamKind::Number, false }, { TEXT("spacing"), EMcpParamKind::Number, false }, { TEXT("heightScale"), EMcpParamKind::Number, false }, { TEXT("subdivisions"), EMcpParamKind::Number, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("material"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_21[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_22[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_23[] = { { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("sourceType"), EMcpParamKind::String, false }, { TEXT("cubemapPath"), EMcpParamKind::String, false }, { TEXT("intensity"), EMcpParamKind::Number, false }, { TEXT("recapture"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BuildEnvironment_24[] = { { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_25[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("bClosedLoop"), EMcpParamKind::Bool, false }, { TEXT("splineType"), EMcpParamKind::String, false }, { TEXT("points"), EMcpParamKind::Array, false }, { TEXT("initialPoints"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_BuildEnvironment_26[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("meshPath"), EMcpParamKind::String, false }, { TEXT("forwardAxis"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BuildEnvironment_27[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_28[] = { { TEXT("names"), EMcpParamKind::Array, true } };
inline const FMcpParamDecl P_BuildEnvironment_29[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("recapture"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BuildEnvironment_30[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_BuildEnvironment_32[] = { { TEXT("foliageType"), EMcpParamKind::String, false }, { TEXT("foliageTypePath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_33[] = { { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_34[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_BuildEnvironment_36[] = { { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("landscapeName"), EMcpParamKind::String, false }, { TEXT("operation"), EMcpParamKind::String, false }, { TEXT("region"), EMcpParamKind::Object, false }, { TEXT("minX"), EMcpParamKind::Number, false }, { TEXT("minY"), EMcpParamKind::Number, false }, { TEXT("maxX"), EMcpParamKind::Number, false }, { TEXT("maxY"), EMcpParamKind::Number, false }, { TEXT("heightData"), EMcpParamKind::Array, true }, { TEXT("skipFlush"), EMcpParamKind::Bool, false }, { TEXT("updateNormals"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BuildEnvironment_37[] = { { TEXT("foliageTypePath"), EMcpParamKind::String, false }, { TEXT("foliageType"), EMcpParamKind::String, false }, { TEXT("locations"), EMcpParamKind::Array, false }, { TEXT("location"), EMcpParamKind::Any, false }, { TEXT("position"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_BuildEnvironment_39[] = { { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("landscapeName"), EMcpParamKind::String, false }, { TEXT("layerName"), EMcpParamKind::String, true }, { TEXT("region"), EMcpParamKind::Object, false }, { TEXT("minX"), EMcpParamKind::Number, false }, { TEXT("minY"), EMcpParamKind::Number, false }, { TEXT("maxX"), EMcpParamKind::Number, false }, { TEXT("maxY"), EMcpParamKind::Number, false }, { TEXT("strength"), EMcpParamKind::Number, false }, { TEXT("skipFlush"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BuildEnvironment_40[] = { { TEXT("foliageType"), EMcpParamKind::String, false }, { TEXT("removeAll"), EMcpParamKind::Bool, false }, { TEXT("foliageTypePath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildEnvironment_41[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("pointIndex"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_42[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("meshPath"), EMcpParamKind::String, true }, { TEXT("alignToSpline"), EMcpParamKind::Bool, false }, { TEXT("spacing"), EMcpParamKind::Number, false }, { TEXT("useRandomOffset"), EMcpParamKind::Bool, false }, { TEXT("randomOffsetRange"), EMcpParamKind::Number, false }, { TEXT("randomizeScale"), EMcpParamKind::Bool, false }, { TEXT("minScale"), EMcpParamKind::Number, false }, { TEXT("maxScale"), EMcpParamKind::Number, false }, { TEXT("randomizeRotation"), EMcpParamKind::Bool, false }, { TEXT("rotationRange"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_44[] = { { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("landscapeName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, true }, { TEXT("position"), EMcpParamKind::Object, true }, { TEXT("toolMode"), EMcpParamKind::String, false }, { TEXT("tool"), EMcpParamKind::String, false }, { TEXT("brushRadius"), EMcpParamKind::Number, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("brushFalloff"), EMcpParamKind::Number, false }, { TEXT("falloff"), EMcpParamKind::Number, false }, { TEXT("strength"), EMcpParamKind::Number, false }, { TEXT("skipFlush"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BuildEnvironment_45[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("intensity"), EMcpParamKind::Number, false }, { TEXT("radius"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_46[] = { { TEXT("minBrightness"), EMcpParamKind::Number, false }, { TEXT("maxBrightness"), EMcpParamKind::Number, false }, { TEXT("compensationValue"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_47[] = { { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("landscapeName"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_BuildEnvironment_48[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("meshPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_BuildEnvironment_49[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, true }, { TEXT("materialIndex"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_50[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("pointIndex"), EMcpParamKind::Number, false }, { TEXT("position"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_BuildEnvironment_51[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("pointIndex"), EMcpParamKind::Number, false }, { TEXT("pointRotation"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_BuildEnvironment_52[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("pointIndex"), EMcpParamKind::Number, false }, { TEXT("pointScale"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_BuildEnvironment_53[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("pointIndex"), EMcpParamKind::Number, false }, { TEXT("arriveTangent"), EMcpParamKind::Object, false }, { TEXT("leaveTangent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_BuildEnvironment_54[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("splineType"), EMcpParamKind::String, false }, { TEXT("pointIndex"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_55[] = { { TEXT("time"), EMcpParamKind::Number, false }, { TEXT("hour"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_56[] = { { TEXT("method"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_BuildEnvironment_57[] = { { TEXT("viewDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BuildEnvironment_58[] = { { TEXT("lightType"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("lightClass"), EMcpParamKind::String, false }, { TEXT("type"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_BuildEnvironment_59[] = { { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("sourceType"), EMcpParamKind::String, false }, { TEXT("cubemapPath"), EMcpParamKind::String, false }, { TEXT("intensity"), EMcpParamKind::Number, false }, { TEXT("recapture"), EMcpParamKind::Bool, false } };

inline const FMcpCallDecl GBuildEnvironment[] =
{
	{ TEXT("build_environment"), TEXT("add_foliage"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("add_foliage_instances"), P_BuildEnvironment_1, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("add_spline_point"), P_BuildEnvironment_2, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("bake_lightmap"), P_BuildEnvironment_3, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("build_lighting"), P_BuildEnvironment_4, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("configure_mesh_randomization"), P_BuildEnvironment_5, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("configure_mesh_spacing"), P_BuildEnvironment_6, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("configure_shadows"), P_BuildEnvironment_7, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("configure_spline_mesh_axis"), P_BuildEnvironment_8, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_cable_spline"), P_BuildEnvironment_9, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_dynamic_light"), P_BuildEnvironment_10, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_fence_spline"), P_BuildEnvironment_11, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_fog_volume"), P_BuildEnvironment_12, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_landscape"), P_BuildEnvironment_13, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("create_landscape_grass_type"), P_BuildEnvironment_14, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("create_light"), P_BuildEnvironment_15, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_lighting_enabled_level"), P_BuildEnvironment_16, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_lightmass_volume"), P_BuildEnvironment_17, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_pipe_spline"), P_BuildEnvironment_18, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_procedural_foliage"), P_BuildEnvironment_19, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("create_procedural_terrain"), P_BuildEnvironment_20, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_river_spline"), P_BuildEnvironment_21, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_road_spline"), P_BuildEnvironment_22, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_sky_light"), P_BuildEnvironment_23, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_sky_sphere"), P_BuildEnvironment_24, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_spline_actor"), P_BuildEnvironment_25, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_spline_mesh_component"), P_BuildEnvironment_26, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("create_wall_spline"), P_BuildEnvironment_27, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("delete"), P_BuildEnvironment_28, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("ensure_single_sky_light"), P_BuildEnvironment_29, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("export_snapshot"), P_BuildEnvironment_30, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("generate_lods"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("get_foliage_instances"), P_BuildEnvironment_32, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("get_splines_info"), P_BuildEnvironment_33, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("import_snapshot"), P_BuildEnvironment_34, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("list_light_types"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("modify_heightmap"), P_BuildEnvironment_36, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("paint_foliage"), P_BuildEnvironment_37, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("paint_landscape"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("paint_landscape_layer"), P_BuildEnvironment_39, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("remove_foliage"), P_BuildEnvironment_40, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("remove_spline_point"), P_BuildEnvironment_41, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("scatter_meshes_along_spline"), P_BuildEnvironment_42, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("sculpt"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("sculpt_landscape"), P_BuildEnvironment_44, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("set_ambient_occlusion"), P_BuildEnvironment_45, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("set_exposure"), P_BuildEnvironment_46, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("set_landscape_material"), P_BuildEnvironment_47, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("build_environment"), TEXT("set_spline_mesh_asset"), P_BuildEnvironment_48, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("set_spline_mesh_material"), P_BuildEnvironment_49, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("set_spline_point_position"), P_BuildEnvironment_50, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("set_spline_point_rotation"), P_BuildEnvironment_51, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("set_spline_point_scale"), P_BuildEnvironment_52, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("set_spline_point_tangents"), P_BuildEnvironment_53, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("set_spline_type"), P_BuildEnvironment_54, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("set_time_of_day"), P_BuildEnvironment_55, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("setup_global_illumination"), P_BuildEnvironment_56, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("setup_volumetric_fog"), P_BuildEnvironment_57, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("spawn_light"), P_BuildEnvironment_58, EMcpCallFlags::None },
	{ TEXT("build_environment"), TEXT("spawn_sky_light"), P_BuildEnvironment_59, EMcpCallFlags::None },
};
}
