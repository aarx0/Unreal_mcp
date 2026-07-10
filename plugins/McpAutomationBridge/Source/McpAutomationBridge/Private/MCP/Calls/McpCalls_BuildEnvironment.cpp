// LINT-TOOL: build_environment
// LINT-SCHEMA-DERIVED
// build_environment as FMcpCall classes — adopts schema-from-decls
// (docs/action-declarations.md). Each class AUTHORS its schema fragment in a
// S_<Suffix>() function; the published facade schema folds those fragments and
// GetDecl() derives the validation decl from the same fragment via
// McpDeriveDecl(), so schema and decl are one source and cannot drift. Run()
// delegates to the subsystem member handlers — HandleEnvironment*
// (EnvironmentHandlers.cpp) for the 21 core actions, HandleLighting*
// (LightingHandlers.cpp) for the 12 lighting actions, HandleSpline*
// (SplineHandlers.cpp) for the 22 spline actions — until the module split
// de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_EnvironmentHandlers.h"
#include "McpAutomationBridge_LightingHandlers.h"
#include "McpAutomationBridge_SplineHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::BuildEnvironment
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. Actions that accept
// one-of-several spellings author each optional: create_landscape_grass_type's
// meshPath/staticMesh (at least one), modify_heightmap's heightData (required
// only for the 'set' operation), and the location-object / flat-x-y-z pairs
// (create_landscape, create_fog_volume) are all "at least one" requirements the
// decl vocabulary cannot express, so the handler enforces them — only the
// always-required name/actorName/path/etc. stay in Required().

static void S_AddFoliageInstances(FMcpSchemaBuilder& B)
{
	B.String(TEXT("foliageTypePath"),
		TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("foliageType"), TEXT(""))
	 .ArrayOfObjects(TEXT("transforms"), TEXT(""))
	 .ArrayOfObjects(TEXT("locations"), TEXT(""));
}

static void S_GetFoliageInstances(FMcpSchemaBuilder& B)
{
	B.String(TEXT("foliageType"), TEXT(""))
	 .String(TEXT("foliageTypePath"),
		TEXT("Asset path (e.g., /Game/Path/Asset)."));
}

static void S_RemoveFoliage(FMcpSchemaBuilder& B)
{
	B.String(TEXT("foliageType"), TEXT(""))
	 .Bool(TEXT("removeAll"),
		TEXT("remove_foliage: remove all foliage instances of every type."))
	 .String(TEXT("foliageTypePath"),
		TEXT("Asset path (e.g., /Game/Path/Asset)."));
}

static void S_PaintFoliage(FMcpSchemaBuilder& B)
{
	B.String(TEXT("foliageTypePath"),
		TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("foliageType"), TEXT(""))
	 .ArrayOfObjects(TEXT("locations"), TEXT(""))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Object(TEXT("position"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		});
}

static void S_CreateProceduralFoliage(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .Object(TEXT("bounds"), TEXT(""))
	 .ArrayOfObjects(TEXT("foliageTypes"), TEXT(""))
	 .ArrayOfObjects(TEXT("types"),
		TEXT("create_procedural_foliage: alias of 'foliageTypes'."))
	 .Number(TEXT("seed"), TEXT(""))
	 .Number(TEXT("tileSize"), TEXT(""));
}

static void S_CreateProceduralTerrain(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("sizeX"), TEXT(""))
	 .Number(TEXT("sizeY"), TEXT(""))
	 .Number(TEXT("spacing"), TEXT(""))
	 .Number(TEXT("heightScale"), TEXT(""))
	 .Number(TEXT("subdivisions"), TEXT(""))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
		})
	 .String(TEXT("material"), TEXT("Material asset path."));
}

static void S_AddFoliage(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("meshPath"), TEXT("Mesh asset path."))
	 .Number(TEXT("density"), TEXT(""))
	 .Number(TEXT("minScale"), TEXT(""))
	 .Number(TEXT("maxScale"), TEXT(""))
	 .Bool(TEXT("alignToNormal"), TEXT(""))
	 .Bool(TEXT("randomYaw"), TEXT(""))
	 .Number(TEXT("cullDistance"), TEXT(""))
	 .Required({TEXT("name"), TEXT("meshPath")});
}

static void S_CreateLandscape(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("x"),
		TEXT("create_landscape/create_fog_volume: flat location X "
			"(alternative to the 'location' object)."))
	 .Number(TEXT("y"),
		TEXT("create_landscape/create_fog_volume: flat location Y "
			"(alternative to the 'location' object)."))
	 .Number(TEXT("z"),
		TEXT("create_landscape/create_fog_volume: flat location Z "
			"(alternative to the 'location' object)."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Integer(TEXT("componentsX"),
		TEXT("create_landscape: number of components along X (alternative "
			"to componentCount)."))
	 .Integer(TEXT("componentsY"),
		TEXT("create_landscape: number of components along Y (alternative "
			"to componentCount)."))
	 .Object(TEXT("componentCount"), TEXT("2D vector."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y"));
		})
	 .Number(TEXT("sizeX"), TEXT(""))
	 .Number(TEXT("sizeY"), TEXT(""))
	 .Integer(TEXT("quadsPerComponent"),
		TEXT("create_landscape: quads per component (alias: "
			"quadsPerSection/sectionSize)."))
	 .Number(TEXT("quadsPerSection"), TEXT(""))
	 .Number(TEXT("sectionSize"), TEXT(""))
	 .Number(TEXT("sectionsPerComponent"), TEXT(""))
	 .String(TEXT("materialPath"), TEXT("Material asset path."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("landscapeName"), TEXT(""));
}

static void S_PaintLandscape(FMcpSchemaBuilder& B)
{
	B.String(TEXT("landscapePath"),
		TEXT("sculpt/modify_heightmap/paint_landscape/"
			"set_landscape_material: landscape asset/actor path (alternative "
			"to landscapeName)."))
	 .String(TEXT("landscapeName"), TEXT(""))
	 .String(TEXT("layerName"), TEXT(""))
	 .Object(TEXT("region"),
		TEXT("modify_heightmap/paint_landscape: sub-region to affect "
			"(minX, minY, maxX, maxY); defaults to top-level minX/minY/"
			"maxX/maxY."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("minX")).Number(TEXT("minY"))
				.Number(TEXT("maxX")).Number(TEXT("maxY"));
		})
	 .Number(TEXT("minX"), TEXT(""))
	 .Number(TEXT("minY"), TEXT(""))
	 .Number(TEXT("maxX"), TEXT(""))
	 .Number(TEXT("maxY"), TEXT(""))
	 .Number(TEXT("strength"), TEXT(""))
	 .Bool(TEXT("skipFlush"),
		TEXT("modify_heightmap/sculpt/paint_landscape: "
			"skip the GPU flush for batched edits."))
	 .Required({TEXT("layerName")});
}

static void S_Sculpt(FMcpSchemaBuilder& B)
{
	B.String(TEXT("landscapePath"),
		TEXT("sculpt/modify_heightmap/paint_landscape/"
			"set_landscape_material: landscape asset/actor path (alternative "
			"to landscapeName)."))
	 .String(TEXT("landscapeName"), TEXT(""))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Object(TEXT("position"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .StringEnum(TEXT("toolMode"), {
		TEXT("Raise"),
		TEXT("Lower"),
		TEXT("Flatten")
	 }, TEXT("sculpt: sculpt tool (default: Raise). Alias of "
		"'tool'."))
	 .String(TEXT("tool"), TEXT(""))
	 .Number(TEXT("brushRadius"),
		TEXT("sculpt: brush radius (alias: radius)."))
	 .Number(TEXT("radius"), TEXT(""))
	 .Number(TEXT("brushFalloff"),
		TEXT("sculpt: brush falloff (alias: falloff)."))
	 .Number(TEXT("falloff"), TEXT(""))
	 .Number(TEXT("strength"), TEXT(""))
	 .Bool(TEXT("skipFlush"),
		TEXT("modify_heightmap/sculpt/paint_landscape: "
			"skip the GPU flush for batched edits."));
}

static void S_ModifyHeightmap(FMcpSchemaBuilder& B)
{
	B.String(TEXT("landscapePath"),
		TEXT("sculpt/modify_heightmap/paint_landscape/"
			"set_landscape_material: landscape asset/actor path (alternative "
			"to landscapeName)."))
	 .String(TEXT("landscapeName"), TEXT(""))
	 .StringEnum(TEXT("operation"), {
		TEXT("raise"),
		TEXT("lower"),
		TEXT("flatten"),
		TEXT("set")
	 }, TEXT("modify_heightmap: how to apply heightData/strength (default: "
		"set). 'add' is accepted as an alias of 'raise'."))
	 .Object(TEXT("region"),
		TEXT("modify_heightmap/paint_landscape: sub-region to affect "
			"(minX, minY, maxX, maxY); defaults to top-level minX/minY/"
			"maxX/maxY."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("minX")).Number(TEXT("minY"))
				.Number(TEXT("maxX")).Number(TEXT("maxY"));
		})
	 .Number(TEXT("minX"), TEXT(""))
	 .Number(TEXT("minY"), TEXT(""))
	 .Number(TEXT("maxX"), TEXT(""))
	 .Number(TEXT("maxY"), TEXT(""))
	 .Array(TEXT("heightData"), TEXT(""), TEXT("number"))
	 .Bool(TEXT("skipFlush"),
		TEXT("modify_heightmap/sculpt/paint_landscape: "
			"skip the GPU flush for batched edits."))
	 .Bool(TEXT("updateNormals"), TEXT(""));
}

static void S_SetLandscapeMaterial(FMcpSchemaBuilder& B)
{
	B.String(TEXT("landscapePath"),
		TEXT("sculpt/modify_heightmap/paint_landscape/"
			"set_landscape_material: landscape asset/actor path (alternative "
			"to landscapeName)."))
	 .String(TEXT("landscapeName"), TEXT(""))
	 .String(TEXT("materialPath"), TEXT("Material asset path."))
	 .Required({TEXT("materialPath")});
}

static void S_CreateLandscapeGrassType(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("meshPath"), TEXT("Mesh asset path."))
	 .String(TEXT("staticMesh"), TEXT("Mesh asset path."))
	 .Number(TEXT("density"), TEXT(""))
	 .Number(TEXT("minScale"), TEXT(""))
	 .Number(TEXT("maxScale"), TEXT(""))
	 .Required({TEXT("name")});
}

static void S_GenerateLods(FMcpSchemaBuilder& B)
{
	B.String(TEXT("landscapePath"),
		TEXT("sculpt/modify_heightmap/paint_landscape/"
			"set_landscape_material: landscape asset/actor path (alternative "
			"to landscapeName)."))
	 .String(TEXT("assetPath"),
		TEXT("generate_lods: single static-mesh asset path (alternative to "
			"assetPaths/assets)."))
	 .Array(TEXT("assetPaths"), TEXT(""))
	 .Array(TEXT("assets"), TEXT(""))
	 .Number(TEXT("lodCount"),
		TEXT("generate_lods: number of LODs to generate (alias: numLODs)."))
	 .Number(TEXT("numLODs"), TEXT(""))
	 .Object(TEXT("reductionSettings"),
		TEXT("generate_lods: per-LOD FMeshReductionSettings overrides "
			"(percentTriangles, percentVertices, maxDeviation, pixelError, "
			"weldingThreshold, hardAngleThreshold, baseLODModel, "
			"recalculateNormals)."));
}

static void S_BakeLightmap(FMcpSchemaBuilder& B)
{
	B.String(TEXT("quality"), TEXT(""));
}

static void S_ExportSnapshot(FMcpSchemaBuilder& B)
{
	B.String(TEXT("path"), TEXT("Path to a directory."))
	 .Required({TEXT("path")});
}

static void S_ImportSnapshot(FMcpSchemaBuilder& B)
{
	B.String(TEXT("path"), TEXT("Path to a directory."))
	 .Required({TEXT("path")});
}

static void S_Delete(FMcpSchemaBuilder& B)
{
	B.Array(TEXT("names"), TEXT(""))
	 .Required({TEXT("names")});
}

static void S_CreateSkySphere(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."));
}

static void S_SetTimeOfDay(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("time"), TEXT(""))
	 .Number(TEXT("hour"), TEXT(""));
}

static void S_CreateFogVolume(FMcpSchemaBuilder& B)
{
	B.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Number(TEXT("x"),
		TEXT("create_landscape/create_fog_volume: flat location X "
			"(alternative to the 'location' object)."))
	 .Number(TEXT("y"),
		TEXT("create_landscape/create_fog_volume: flat location Y "
			"(alternative to the 'location' object)."))
	 .Number(TEXT("z"),
		TEXT("create_landscape/create_fog_volume: flat location Z "
			"(alternative to the 'location' object)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."));
}

static void S_ListLightTypes(FMcpSchemaBuilder&) {}

static void S_CreateLight(FMcpSchemaBuilder& B)
{
	B.String(TEXT("lightClass"),
		TEXT("create_light: light actor "
			"class (e.g. PointLight); alias of lightType/type."))
	 .StringEnum(TEXT("lightType"), {
		TEXT("point"),
		TEXT("directional"),
		TEXT("spot"),
		TEXT("rect"),
		TEXT("sky")
	 }, TEXT("create_light: light type "
		"shorthand."))
	 .String(TEXT("type"),
		TEXT("create_light: alias of "
			"lightType."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
		})
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("properties"),
		TEXT("create_light: light component overrides (intensity, color, "
			"castShadows, useAsAtmosphereSunLight, attenuationRadius, "
			"innerConeAngle, outerConeAngle, sourceWidth, sourceHeight)."));
}

static void S_CreateSkyLight(FMcpSchemaBuilder& B)
{
	B.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
		})
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .StringEnum(TEXT("sourceType"), {
		TEXT("SpecifiedCubemap"),
		TEXT("CapturedScene")
	 }, TEXT("create_sky_light: sky light capture source."))
	 .String(TEXT("cubemapPath"),
		TEXT("create_sky_light: cubemap asset path when "
			"sourceType is SpecifiedCubemap."))
	 .Number(TEXT("intensity"), TEXT(""))
	 .Bool(TEXT("recapture"),
		TEXT("create_sky_light/ensure_single_sky_light: "
			"recapture the scene into the sky light."));
}

static void S_BuildLighting(FMcpSchemaBuilder& B)
{
	B.String(TEXT("quality"), TEXT(""));
}

static void S_EnsureSingleSkyLight(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .Bool(TEXT("recapture"),
		TEXT("create_sky_light/ensure_single_sky_light: "
			"recapture the scene into the sky light."));
}

static void S_CreateLightmassVolume(FMcpSchemaBuilder& B)
{
	B.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Object(TEXT("size"),
		TEXT("create_lightmass_volume: volume size (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .String(TEXT("name"), TEXT("Name identifier."));
}

static void S_SetupVolumetricFog(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("viewDistance"),
		TEXT("setup_volumetric_fog: volumetric fog view distance."));
}

static void S_SetupGlobalIllumination(FMcpSchemaBuilder& B)
{
	B.StringEnum(TEXT("method"), {
		TEXT("LumenGI"),
		TEXT("ScreenSpace"),
		TEXT("None"),
		TEXT("RayTraced"),
		TEXT("Lightmass")
	}, TEXT("setup_global_illumination: global illumination method."))
	 .Required({TEXT("method")});
}

static void S_ConfigureShadows(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("virtualShadowMaps"),
		TEXT("configure_shadows: enable Virtual Shadow Maps."))
	 .Bool(TEXT("rayTracedShadows"),
		TEXT("configure_shadows: alias of virtualShadowMaps."));
}

static void S_SetExposure(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("minBrightness"),
		TEXT("set_exposure: auto-exposure minimum brightness."))
	 .Number(TEXT("maxBrightness"),
		TEXT("set_exposure: auto-exposure maximum brightness."))
	 .Number(TEXT("compensationValue"),
		TEXT("set_exposure: auto-exposure bias/compensation."));
}

static void S_SetAmbientOcclusion(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enabled"),
		TEXT("set_ambient_occlusion: enable/disable ambient occlusion."))
	 .Number(TEXT("intensity"), TEXT(""))
	 .Number(TEXT("radius"), TEXT(""));
}

static void S_CreateLightingEnabledLevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("path"), TEXT("Path to a directory."))
	 .Required({TEXT("path")});
}

static void S_CreateSplineActor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
		})
	 .Bool(TEXT("bClosedLoop"),
		TEXT("create_spline_actor: close the spline into a loop."))
	 .StringEnum(TEXT("splineType"), {
		TEXT("Curve"),
		TEXT("Linear"),
		TEXT("Constant"),
		TEXT("CurveClamped"),
		TEXT("CurveCustomTangent")
	 }, TEXT("create_spline_actor: default point type. set_spline_type: "
		"type to apply (all points, or just pointIndex if given)."))
	 .ArrayOfObjects(TEXT("points"),
		TEXT("create_spline_actor: initial points, each with a "
			"'location' (x, y, z)."))
	 .ArrayOfObjects(TEXT("initialPoints"),
		TEXT("create_spline_actor: alias of 'points'."));
}

static void S_AddSplinePoint(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("position"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Integer(TEXT("index"),
		TEXT("add_spline_point: insertion index (-1/omitted appends at "
			"the end)."))
	 .StringEnum(TEXT("pointType"), {
		TEXT("Curve"),
		TEXT("Linear"),
		TEXT("Constant"),
		TEXT("CurveClamped"),
		TEXT("CurveCustomTangent")
	 }, TEXT("add_spline_point: spline point interpolation type."))
	 .Required({TEXT("actorName")});
}

static void S_RemoveSplinePoint(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Integer(TEXT("pointIndex"),
		TEXT("remove_spline_point/set_spline_point_position/"
			"set_spline_point_tangents/set_spline_point_rotation/"
			"set_spline_point_scale/set_spline_type: index of the point "
			"to modify."))
	 .Required({TEXT("actorName")});
}

static void S_SetSplinePointPosition(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Integer(TEXT("pointIndex"),
		TEXT("remove_spline_point/set_spline_point_position/"
			"set_spline_point_tangents/set_spline_point_rotation/"
			"set_spline_point_scale/set_spline_type: index of the point "
			"to modify."))
	 .Object(TEXT("position"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Required({TEXT("actorName")});
}

static void S_SetSplinePointTangents(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Integer(TEXT("pointIndex"),
		TEXT("remove_spline_point/set_spline_point_position/"
			"set_spline_point_tangents/set_spline_point_rotation/"
			"set_spline_point_scale/set_spline_type: index of the point "
			"to modify."))
	 .Object(TEXT("arriveTangent"),
		TEXT("set_spline_point_tangents: incoming tangent (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Object(TEXT("leaveTangent"),
		TEXT("set_spline_point_tangents: outgoing tangent (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Required({TEXT("actorName")});
}

static void S_SetSplinePointRotation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Integer(TEXT("pointIndex"),
		TEXT("remove_spline_point/set_spline_point_position/"
			"set_spline_point_tangents/set_spline_point_rotation/"
			"set_spline_point_scale/set_spline_type: index of the point "
			"to modify."))
	 .Object(TEXT("pointRotation"),
		TEXT("set_spline_point_rotation: point rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
		})
	 .Required({TEXT("actorName")});
}

static void S_SetSplinePointScale(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Integer(TEXT("pointIndex"),
		TEXT("remove_spline_point/set_spline_point_position/"
			"set_spline_point_tangents/set_spline_point_rotation/"
			"set_spline_point_scale/set_spline_type: index of the point "
			"to modify."))
	 .Object(TEXT("pointScale"),
		TEXT("set_spline_point_scale: point scale (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Required({TEXT("actorName")});
}

static void S_SetSplineType(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .StringEnum(TEXT("splineType"), {
		TEXT("Curve"),
		TEXT("Linear"),
		TEXT("Constant"),
		TEXT("CurveClamped"),
		TEXT("CurveCustomTangent")
	 }, TEXT("create_spline_actor: default point type. set_spline_type: "
		"type to apply (all points, or just pointIndex if given)."))
	 .Integer(TEXT("pointIndex"),
		TEXT("remove_spline_point/set_spline_point_position/"
			"set_spline_point_tangents/set_spline_point_rotation/"
			"set_spline_point_scale/set_spline_type: index of the point "
			"to modify."))
	 .Required({TEXT("actorName")});
}

static void S_CreateSplineMeshComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"),
		TEXT("create_spline_mesh_component: Blueprint asset path."))
	 .String(TEXT("componentName"),
		TEXT("Spline actions: name of the spline/spline-mesh component."))
	 .String(TEXT("meshPath"), TEXT("Mesh asset path."))
	 .StringEnum(TEXT("forwardAxis"), {
		TEXT("X"),
		TEXT("Y"),
		TEXT("Z")
	 }, TEXT("create_spline_mesh_component/configure_spline_mesh_axis: spline "
		"mesh forward axis."))
	 .Bool(TEXT("save"),
		TEXT("create_spline_mesh_component: save the Blueprint after the "
			"operation."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetSplineMeshAsset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("componentName"),
		TEXT("Spline actions: name of the spline/spline-mesh component."))
	 .String(TEXT("meshPath"), TEXT("Mesh asset path."))
	 .Required({TEXT("actorName"), TEXT("meshPath")});
}

static void S_ConfigureSplineMeshAxis(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("componentName"),
		TEXT("Spline actions: name of the spline/spline-mesh component."))
	 .StringEnum(TEXT("forwardAxis"), {
		TEXT("X"),
		TEXT("Y"),
		TEXT("Z")
	 }, TEXT("create_spline_mesh_component/configure_spline_mesh_axis: spline "
		"mesh forward axis."))
	 .Required({TEXT("actorName")});
}

static void S_SetSplineMeshMaterial(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("componentName"),
		TEXT("Spline actions: name of the spline/spline-mesh component."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."))
	 .Integer(TEXT("materialIndex"),
		TEXT("set_spline_mesh_material: material slot index."))
	 .Required({TEXT("actorName"), TEXT("materialPath")});
}

static void S_ScatterMeshesAlongSpline(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("meshPath"), TEXT("Mesh asset path."))
	 .Bool(TEXT("alignToSpline"),
		TEXT("scatter_meshes_along_spline: orient meshes to the spline "
			"tangent."))
	 .Number(TEXT("spacing"), TEXT(""))
	 .Bool(TEXT("useRandomOffset"),
		TEXT("scatter_meshes_along_spline/configure_mesh_spacing: "
			"randomize mesh spacing along the spline."))
	 .Number(TEXT("randomOffsetRange"),
		TEXT("scatter_meshes_along_spline/configure_mesh_spacing: max "
			"random offset distance."))
	 .Bool(TEXT("randomizeScale"),
		TEXT("scatter_meshes_along_spline/configure_mesh_randomization: "
			"randomize instance scale between minScale/maxScale."))
	 .Number(TEXT("minScale"), TEXT(""))
	 .Number(TEXT("maxScale"), TEXT(""))
	 .Bool(TEXT("randomizeRotation"),
		TEXT("scatter_meshes_along_spline/configure_mesh_randomization: "
			"randomize instance yaw."))
	 .Number(TEXT("rotationRange"),
		TEXT("scatter_meshes_along_spline/configure_mesh_randomization: "
			"max random yaw offset in degrees."))
	 .Required({TEXT("actorName"), TEXT("meshPath")});
}



static void S_CreateRoadSpline(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Number(TEXT("width"),
		TEXT("create_road_spline/create_river_spline/create_fence_spline/"
			"create_wall_spline/create_cable_spline/create_pipe_spline: "
			"template width."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."));
}

static void S_CreateRiverSpline(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Number(TEXT("width"),
		TEXT("create_road_spline/create_river_spline/create_fence_spline/"
			"create_wall_spline/create_cable_spline/create_pipe_spline: "
			"template width."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."));
}

static void S_CreateFenceSpline(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Number(TEXT("width"),
		TEXT("create_road_spline/create_river_spline/create_fence_spline/"
			"create_wall_spline/create_cable_spline/create_pipe_spline: "
			"template width."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."));
}

static void S_CreateWallSpline(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Number(TEXT("width"),
		TEXT("create_road_spline/create_river_spline/create_fence_spline/"
			"create_wall_spline/create_cable_spline/create_pipe_spline: "
			"template width."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."));
}

static void S_CreateCableSpline(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Number(TEXT("width"),
		TEXT("create_road_spline/create_river_spline/create_fence_spline/"
			"create_wall_spline/create_cable_spline/create_pipe_spline: "
			"template width."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."));
}

static void S_CreatePipeSpline(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Number(TEXT("width"),
		TEXT("create_road_spline/create_river_spline/create_fence_spline/"
			"create_wall_spline/create_cable_spline/create_pipe_spline: "
			"template width."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."));
}

static void S_GetSplinesInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."));
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are authored per action and deliberately mixed. RequiresEditor on
// the 12 Lighting and 22 Spline actions (both chains were whole-editor-gated
// and the members answer their editor-build stubs in non-editor builds) and
// on the six core inline extractions (their bodies sat inside the retired
// dispatcher's editor-gated region and answer its stub); NOT on the 15
// delegating core actions — the retired dispatcher forwarded them outside
// any editor gate, and each 4-arg member self-gates with its own stub.
// Mutating on the writers only — bake_lightmap and build_lighting both kick
// a lighting build; the readers are get_foliage_instances, import_snapshot
// (reads a snapshot file back, writes nothing), list_light_types, and
// get_splines_info.

#define MCP_BE_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                        \
class FMcpCall_BuildEnvironment_##ClassSuffix final : public FMcpCall                    \
{                                                                                        \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }       \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("build_environment"),          \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                            \
		return D;                                                                        \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return HandlerFn(S, RequestId, Payload, Socket);                                 \
	}                                                                                    \
};

// The foliage/landscape wrappers + bake_lightmap forward to still-private
// Foliage/Landscape/EditorFunction members, so they stay subsystem members
// (F1 rule 4) and dispatch through S.; this variant keeps that member call.
#define MCP_BE_MEMBER_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                 \
class FMcpCall_BuildEnvironment_##ClassSuffix final : public FMcpCall                    \
{                                                                                        \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }       \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("build_environment"),          \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                            \
		return D;                                                                        \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

// Foliage (delegating wrappers @ EnvironmentHandlers.cpp) — still members
MCP_BE_MEMBER_CALL(AddFoliageInstances, "add_foliage_instances", HandleEnvironmentAddFoliageInstances, EMcpCallFlags::Mutating)
MCP_BE_MEMBER_CALL(GetFoliageInstances, "get_foliage_instances", HandleEnvironmentGetFoliageInstances, EMcpCallFlags::None)
MCP_BE_MEMBER_CALL(RemoveFoliage, "remove_foliage", HandleEnvironmentRemoveFoliage, EMcpCallFlags::Mutating)
MCP_BE_MEMBER_CALL(PaintFoliage, "paint_foliage", HandleEnvironmentPaintFoliage, EMcpCallFlags::Mutating)
MCP_BE_MEMBER_CALL(CreateProceduralFoliage, "create_procedural_foliage", HandleEnvironmentCreateProceduralFoliage, EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateProceduralTerrain, "create_procedural_terrain", McpHandlers::BuildEnvironment::HandleEnvironmentCreateProceduralTerrain, EMcpCallFlags::Mutating)
MCP_BE_MEMBER_CALL(AddFoliage, "add_foliage", HandleEnvironmentAddFoliage, EMcpCallFlags::Mutating)
// Landscape — still members
MCP_BE_MEMBER_CALL(CreateLandscape, "create_landscape", HandleEnvironmentCreateLandscape, EMcpCallFlags::Mutating)
MCP_BE_MEMBER_CALL(PaintLandscape, "paint_landscape", HandleEnvironmentPaintLandscape, EMcpCallFlags::Mutating)
MCP_BE_MEMBER_CALL(Sculpt, "sculpt", HandleEnvironmentSculpt, EMcpCallFlags::Mutating)
MCP_BE_MEMBER_CALL(ModifyHeightmap, "modify_heightmap", HandleEnvironmentModifyHeightmap, EMcpCallFlags::Mutating)
MCP_BE_MEMBER_CALL(SetLandscapeMaterial, "set_landscape_material", HandleEnvironmentSetLandscapeMaterial, EMcpCallFlags::Mutating)
MCP_BE_MEMBER_CALL(CreateLandscapeGrassType, "create_landscape_grass_type", HandleEnvironmentCreateLandscapeGrassType, EMcpCallFlags::Mutating)
MCP_BE_CALL(GenerateLods, "generate_lods", McpHandlers::BuildEnvironment::HandleEnvironmentGenerateLODs, EMcpCallFlags::Mutating)
MCP_BE_MEMBER_CALL(BakeLightmap, "bake_lightmap", HandleEnvironmentBakeLightmap, EMcpCallFlags::Mutating)

// Environment inline extractions (EnvironmentHandlers.cpp)
MCP_BE_CALL(ExportSnapshot, "export_snapshot", McpHandlers::BuildEnvironment::HandleEnvironmentExportSnapshot, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(ImportSnapshot, "import_snapshot", McpHandlers::BuildEnvironment::HandleEnvironmentImportSnapshot, EMcpCallFlags::RequiresEditor)
MCP_BE_CALL(Delete, "delete", McpHandlers::BuildEnvironment::HandleEnvironmentDelete, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateSkySphere, "create_sky_sphere", McpHandlers::BuildEnvironment::HandleEnvironmentCreateSkySphere, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetTimeOfDay, "set_time_of_day", McpHandlers::BuildEnvironment::HandleEnvironmentSetTimeOfDay, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateFogVolume, "create_fog_volume", McpHandlers::BuildEnvironment::HandleEnvironmentCreateFogVolume, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Lighting (LightingHandlers.cpp)
MCP_BE_CALL(ListLightTypes, "list_light_types", McpHandlers::BuildEnvironment::HandleLightingListLightTypes, EMcpCallFlags::RequiresEditor)
MCP_BE_CALL(CreateLight, "create_light", McpHandlers::BuildEnvironment::HandleLightingCreateLight, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateSkyLight, "create_sky_light", McpHandlers::BuildEnvironment::HandleLightingCreateSkyLight, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(BuildLighting, "build_lighting", McpHandlers::BuildEnvironment::HandleLightingBuildLighting, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(EnsureSingleSkyLight, "ensure_single_sky_light", McpHandlers::BuildEnvironment::HandleLightingEnsureSingleSkyLight, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateLightmassVolume, "create_lightmass_volume", McpHandlers::BuildEnvironment::HandleLightingCreateLightmassVolume, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetupVolumetricFog, "setup_volumetric_fog", McpHandlers::BuildEnvironment::HandleLightingSetupVolumetricFog, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetupGlobalIllumination, "setup_global_illumination", McpHandlers::BuildEnvironment::HandleLightingSetupGlobalIllumination, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(ConfigureShadows, "configure_shadows", McpHandlers::BuildEnvironment::HandleLightingConfigureShadows, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetExposure, "set_exposure", McpHandlers::BuildEnvironment::HandleLightingSetExposure, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetAmbientOcclusion, "set_ambient_occlusion", McpHandlers::BuildEnvironment::HandleLightingSetAmbientOcclusion, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateLightingEnabledLevel, "create_lighting_enabled_level", McpHandlers::BuildEnvironment::HandleLightingCreateLightingEnabledLevel, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Spline creation & points (SplineHandlers.cpp)
MCP_BE_CALL(CreateSplineActor, "create_spline_actor", McpHandlers::BuildEnvironment::HandleSplineCreateSplineActor, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(AddSplinePoint, "add_spline_point", McpHandlers::BuildEnvironment::HandleSplineAddSplinePoint, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(RemoveSplinePoint, "remove_spline_point", McpHandlers::BuildEnvironment::HandleSplineRemoveSplinePoint, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplinePointPosition, "set_spline_point_position", McpHandlers::BuildEnvironment::HandleSplineSetSplinePointPosition, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplinePointTangents, "set_spline_point_tangents", McpHandlers::BuildEnvironment::HandleSplineSetSplinePointTangents, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplinePointRotation, "set_spline_point_rotation", McpHandlers::BuildEnvironment::HandleSplineSetSplinePointRotation, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplinePointScale, "set_spline_point_scale", McpHandlers::BuildEnvironment::HandleSplineSetSplinePointScale, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplineType, "set_spline_type", McpHandlers::BuildEnvironment::HandleSplineSetSplineType, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Spline mesh
MCP_BE_CALL(CreateSplineMeshComponent, "create_spline_mesh_component", McpHandlers::BuildEnvironment::HandleSplineCreateSplineMeshComponent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplineMeshAsset, "set_spline_mesh_asset", McpHandlers::BuildEnvironment::HandleSplineSetSplineMeshAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(ConfigureSplineMeshAxis, "configure_spline_mesh_axis", McpHandlers::BuildEnvironment::HandleSplineConfigureSplineMeshAxis, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplineMeshMaterial, "set_spline_mesh_material", McpHandlers::BuildEnvironment::HandleSplineSetSplineMeshMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Mesh scattering
MCP_BE_CALL(ScatterMeshesAlongSpline, "scatter_meshes_along_spline", McpHandlers::BuildEnvironment::HandleSplineScatterMeshesAlongSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Quick templates
MCP_BE_CALL(CreateRoadSpline, "create_road_spline", McpHandlers::BuildEnvironment::HandleSplineCreateRoadSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateRiverSpline, "create_river_spline", McpHandlers::BuildEnvironment::HandleSplineCreateRiverSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateFenceSpline, "create_fence_spline", McpHandlers::BuildEnvironment::HandleSplineCreateFenceSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateWallSpline, "create_wall_spline", McpHandlers::BuildEnvironment::HandleSplineCreateWallSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateCableSpline, "create_cable_spline", McpHandlers::BuildEnvironment::HandleSplineCreateCableSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreatePipeSpline, "create_pipe_spline", McpHandlers::BuildEnvironment::HandleSplineCreatePipeSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Utility
MCP_BE_CALL(GetSplinesInfo, "get_splines_info", McpHandlers::BuildEnvironment::HandleSplineGetSplinesInfo, EMcpCallFlags::RequiresEditor)

#undef MCP_BE_CALL
#undef MCP_BE_MEMBER_CALL

} // namespace McpCalls::BuildEnvironment

void McpRegisterBuildEnvironmentCalls()
{
	using namespace McpCalls::BuildEnvironment;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_AddFoliageInstances>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_GetFoliageInstances>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_RemoveFoliage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_PaintFoliage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateProceduralFoliage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateProceduralTerrain>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_AddFoliage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateLandscape>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_PaintLandscape>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_Sculpt>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ModifyHeightmap>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetLandscapeMaterial>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateLandscapeGrassType>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_GenerateLods>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_BakeLightmap>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ExportSnapshot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ImportSnapshot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_Delete>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateSkySphere>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetTimeOfDay>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateFogVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ListLightTypes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateLight>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateSkyLight>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_BuildLighting>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_EnsureSingleSkyLight>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateLightmassVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetupVolumetricFog>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetupGlobalIllumination>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ConfigureShadows>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetExposure>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetAmbientOcclusion>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateLightingEnabledLevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateSplineActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_AddSplinePoint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_RemoveSplinePoint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplinePointPosition>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplinePointTangents>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplinePointRotation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplinePointScale>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplineType>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateSplineMeshComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplineMeshAsset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ConfigureSplineMeshAxis>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplineMeshMaterial>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ScatterMeshesAlongSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateRoadSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateRiverSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateFenceSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateWallSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateCableSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreatePipeSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_GetSplinesInfo>());
}
