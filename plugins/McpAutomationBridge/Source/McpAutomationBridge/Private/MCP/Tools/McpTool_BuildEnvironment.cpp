// McpTool_BuildEnvironment.cpp — build_environment tool definition (22 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_BuildEnvironment : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("build_environment"); }

	FString GetDescription() const override
	{
		return TEXT("Create/sculpt landscapes, paint foliage, and generate procedural "
			"terrain/biomes.");
	}

	FString GetCategory() const override { return TEXT("world"); }


	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
				.StringEnum(TEXT("action"), McpConsolidatedActions::BuildEnvironment(),
					TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("landscapeName"), TEXT(""))
			.Array(TEXT("heightData"), TEXT(""), TEXT("number"))
			.Number(TEXT("minX"), TEXT(""))
			.Number(TEXT("minY"), TEXT(""))
			.Number(TEXT("maxX"), TEXT(""))
			.Number(TEXT("maxY"), TEXT(""))
			.Bool(TEXT("updateNormals"), TEXT(""))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Number(TEXT("sizeX"), TEXT(""))
			.Number(TEXT("sizeY"), TEXT(""))
			.Number(TEXT("sectionSize"), TEXT(""))
			.Number(TEXT("sectionsPerComponent"), TEXT(""))
			.Object(TEXT("componentCount"), TEXT("2D vector."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.String(TEXT("landscapePath"),
				TEXT("sculpt/modify_heightmap/paint_landscape/"
					"set_landscape_material: landscape asset/actor path (alternative "
					"to landscapeName)."))
			.Integer(TEXT("componentsX"),
				TEXT("create_landscape: number of components along X (alternative "
					"to componentCount)."))
			.Integer(TEXT("componentsY"),
				TEXT("create_landscape: number of components along Y (alternative "
					"to componentCount)."))
			.Integer(TEXT("quadsPerComponent"),
				TEXT("create_landscape: quads per component (alias: "
					"quadsPerSection/sectionSize)."))
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
			.Bool(TEXT("skipFlush"),
				TEXT("modify_heightmap/sculpt/paint_landscape: "
					"skip the GPU flush for batched edits."))
			.StringEnum(TEXT("toolMode"), {
				TEXT("Raise"),
				TEXT("Lower"),
				TEXT("Flatten")
			}, TEXT("sculpt: sculpt tool (default: Raise). Alias of "
				"'tool'."))
			.Number(TEXT("brushRadius"),
				TEXT("sculpt: brush radius (alias: radius)."))
			.Number(TEXT("brushFalloff"),
				TEXT("sculpt: brush falloff (alias: falloff)."))
			.String(TEXT("tool"), TEXT(""))
			.Number(TEXT("radius"), TEXT(""))
			.Number(TEXT("strength"), TEXT(""))
			.Number(TEXT("falloff"), TEXT(""))
			.String(TEXT("layerName"), TEXT(""))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("foliageType"), TEXT(""))
			.String(TEXT("foliageTypePath"),
				TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.Bool(TEXT("removeAll"),
				TEXT("remove_foliage: remove all foliage instances of every type."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.Number(TEXT("density"), TEXT(""))
			.Number(TEXT("minScale"), TEXT(""))
			.Number(TEXT("maxScale"), TEXT(""))
			.Number(TEXT("cullDistance"), TEXT(""))
			.Bool(TEXT("alignToNormal"), TEXT(""))
			.Bool(TEXT("randomYaw"), TEXT(""))
			.ArrayOfObjects(TEXT("locations"), TEXT(""))
			.ArrayOfObjects(TEXT("transforms"), TEXT(""))
			.Object(TEXT("position"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.FreeformObject(TEXT("bounds"), TEXT(""))
			.String(TEXT("volumeName"), TEXT(""))
			.Number(TEXT("seed"), TEXT(""))
			.ArrayOfObjects(TEXT("foliageTypes"), TEXT(""))
			.Number(TEXT("quadsPerSection"), TEXT(""))
			.Number(TEXT("count"), TEXT(""))
			.Array(TEXT("assets"), TEXT(""))
			.Number(TEXT("numLODs"), TEXT(""))
			.Number(TEXT("subdivisions"), TEXT(""))
			.Number(TEXT("tileSize"), TEXT(""))
			.String(TEXT("quality"), TEXT(""))
			.String(TEXT("staticMesh"), TEXT("Mesh asset path."))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("filename"), TEXT(""))
			.Array(TEXT("assetPaths"), TEXT(""))
			.Array(TEXT("names"), TEXT(""))
			.Number(TEXT("time"), TEXT(""))
			.Number(TEXT("spacing"), TEXT(""))
			.Number(TEXT("heightScale"), TEXT(""))
			.String(TEXT("material"), TEXT("Material asset path."))
			.Number(TEXT("hour"), TEXT(""))
			.Number(TEXT("intensity"), TEXT(""))
			.String(TEXT("lightClass"),
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
			.FreeformObject(TEXT("properties"),
				TEXT("create_light: light component overrides (intensity, color, "
					"castShadows, useAsAtmosphereSunLight, attenuationRadius, "
					"innerConeAngle, outerConeAngle, sourceWidth, sourceHeight)."))
			.StringEnum(TEXT("sourceType"), {
				TEXT("SpecifiedCubemap"),
				TEXT("CapturedScene")
			}, TEXT("create_sky_light: sky light capture source."))
			.String(TEXT("cubemapPath"),
				TEXT("create_sky_light: cubemap asset path when "
					"sourceType is SpecifiedCubemap."))
			.Bool(TEXT("recapture"),
				TEXT("create_sky_light/ensure_single_sky_light: "
					"recapture the scene into the sky light."))
			.Object(TEXT("size"),
				TEXT("create_lightmass_volume: volume size (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Number(TEXT("viewDistance"),
				TEXT("setup_volumetric_fog: volumetric fog view distance."))
			.StringEnum(TEXT("method"), {
				TEXT("LumenGI"),
				TEXT("ScreenSpace"),
				TEXT("None"),
				TEXT("RayTraced"),
				TEXT("Lightmass")
			}, TEXT("setup_global_illumination: global illumination method."))
			.Bool(TEXT("virtualShadowMaps"),
				TEXT("configure_shadows: enable Virtual Shadow Maps."))
			.Bool(TEXT("rayTracedShadows"),
				TEXT("configure_shadows: alias of virtualShadowMaps."))
			.Number(TEXT("minBrightness"),
				TEXT("set_exposure: auto-exposure minimum brightness."))
			.Number(TEXT("maxBrightness"),
				TEXT("set_exposure: auto-exposure maximum brightness."))
			.Number(TEXT("compensationValue"),
				TEXT("set_exposure: auto-exposure bias/compensation."))
			.Bool(TEXT("enabled"),
				TEXT("set_ambient_occlusion: enable/disable ambient occlusion."))
			.String(TEXT("componentName"),
				TEXT("Spline actions: name of the spline/spline-mesh component."))
			.String(TEXT("blueprintPath"),
				TEXT("create_spline_mesh_component: Blueprint asset path."))
			.StringEnum(TEXT("forwardAxis"), {
				TEXT("X"),
				TEXT("Y"),
				TEXT("Z")
			}, TEXT("create_spline_mesh_component/configure_spline_mesh_axis: spline "
				"mesh forward axis."))
			.Integer(TEXT("materialIndex"),
				TEXT("set_spline_mesh_material: material slot index."))
			.Bool(TEXT("save"),
				TEXT("create_spline_mesh_component: save the Blueprint after the "
					"operation."))
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
			.Object(TEXT("pointRotation"),
				TEXT("set_spline_point_rotation: point rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("pointScale"),
				TEXT("set_spline_point_scale: point scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.StringEnum(TEXT("splineType"), {
				TEXT("Curve"),
				TEXT("Linear"),
				TEXT("Constant"),
				TEXT("CurveClamped"),
				TEXT("CurveCustomTangent")
			}, TEXT("create_spline_actor: default point type. set_spline_type: "
				"type to apply (all points, or just pointIndex if given)."))
			.Bool(TEXT("bClosedLoop"),
				TEXT("create_spline_actor: close the spline into a loop."))
			.ArrayOfObjects(TEXT("points"),
				TEXT("create_spline_actor: initial points, each with a "
					"'location' (x, y, z)."))
			.ArrayOfObjects(TEXT("initialPoints"),
				TEXT("create_spline_actor: alias of 'points'."))
			.Bool(TEXT("alignToSpline"),
				TEXT("scatter_meshes_along_spline: orient meshes to the spline "
					"tangent."))
			.Bool(TEXT("useRandomOffset"),
				TEXT("scatter_meshes_along_spline/configure_mesh_spacing: "
					"randomize mesh spacing along the spline."))
			.Number(TEXT("randomOffsetRange"),
				TEXT("scatter_meshes_along_spline/configure_mesh_spacing: max "
					"random offset distance."))
			.Bool(TEXT("randomizeScale"),
				TEXT("scatter_meshes_along_spline/configure_mesh_randomization: "
					"randomize instance scale between minScale/maxScale."))
			.Bool(TEXT("randomizeRotation"),
				TEXT("scatter_meshes_along_spline/configure_mesh_randomization: "
					"randomize instance yaw."))
			.Number(TEXT("rotationRange"),
				TEXT("scatter_meshes_along_spline/configure_mesh_randomization: "
					"max random yaw offset in degrees."))
			.Number(TEXT("width"),
				TEXT("create_road_spline/create_river_spline/create_fence_spline/"
					"create_wall_spline/create_cable_spline/create_pipe_spline: "
					"template width."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_BuildEnvironment);
