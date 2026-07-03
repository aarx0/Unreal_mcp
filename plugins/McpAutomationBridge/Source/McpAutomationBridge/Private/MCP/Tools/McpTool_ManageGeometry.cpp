// McpTool_ManageGeometry.cpp — manage_geometry tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageGeometry : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_geometry"); }

	FString GetDescription() const override
	{
		return TEXT("Create procedural meshes using Geometry Script: booleans, "
			"deformers, UVs, collision, and LOD generation.");
	}

	FString GetCategory() const override { return TEXT("world"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ManageGeometry(), TEXT("Geometry action to perform"))
			.String(TEXT("assetPath"), TEXT("Destination asset path for convert_to_static_mesh/convert_to_nanite (e.g. /Game/Meshes/SM_Rock)."))
			.String(TEXT("outputPath"), TEXT("Alias of assetPath."))
			.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
			.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
			.String(TEXT("targetActor"), TEXT("Actor receiving the result of a boolean operation."))
			.String(TEXT("toolActor"), TEXT("Actor used as the cutting/combining tool in a boolean operation."))
			.Bool(TEXT("keepTool"), TEXT("Keep the tool actor after a boolean operation (default true)."))
			.String(TEXT("trimActorName"), TEXT("boolean_trim: actor used as the trimming surface."))
			.Bool(TEXT("keepInside"), TEXT("boolean_trim: keep the portion inside the trim surface."))
			.Bool(TEXT("fillHoles"), TEXT("self_union/remesh_voxel: fill holes left by the operation."))
			.Number(TEXT("width"), TEXT("Width value."))
			.Number(TEXT("height"), TEXT("Height value."))
			.Number(TEXT("depth"), TEXT("Depth value."))
			.Object(TEXT("dimensions"), TEXT("Box dimensions (width, height, depth)."),
				[](FMcpSchemaBuilder& S) {
					S.Number(TEXT("width")).Number(TEXT("height")).Number(TEXT("depth"));
				})
			.Number(TEXT("radius"), TEXT("Radius value."))
			.Number(TEXT("innerRadius"), TEXT("Inner radius for torus."))
			.Integer(TEXT("subdivisions"), TEXT("create_sphere: icosphere subdivision count (default topology; alternative to numRings/radialSegments)."))
			.Number(TEXT("baseRadius"), TEXT("create_cone: radius at the base."))
			.Number(TEXT("topRadius"), TEXT("create_cone: radius at the top (0 for a point)."))
			.Number(TEXT("length"), TEXT("create_capsule/create_ramp: cylinder/ramp length."))
			.Integer(TEXT("hemisphereSteps"), TEXT("create_capsule: hemisphere cap subdivision count."))
			.Number(TEXT("majorRadius"), TEXT("create_torus/create_arch: major (ring) radius."))
			.Number(TEXT("minorRadius"), TEXT("create_torus/create_arch: minor (tube) radius."))
			.Integer(TEXT("majorSegments"), TEXT("create_torus: major ring segment count (alias of numRings)."))
			.Integer(TEXT("minorSegments"), TEXT("create_torus: minor tube segment count (alias of radialSegments)."))
			.Integer(TEXT("majorSteps"), TEXT("create_arch: major ring step count."))
			.Integer(TEXT("minorSteps"), TEXT("create_arch: minor tube step count."))
			.Number(TEXT("outerRadius"), TEXT("create_ring/create_pipe: outer radius."))
			.Integer(TEXT("radialSteps"), TEXT("create_pipe: radial step count."))
			.Integer(TEXT("heightSteps"), TEXT("create_pipe: height step count."))
			.Integer(TEXT("widthSubdivisions"), TEXT("create_plane: subdivision count along width."))
			.Integer(TEXT("depthSubdivisions"), TEXT("create_plane: subdivision count along depth."))
			.Number(TEXT("numSides"),
				TEXT("Radial side count for cylinder and cone (alias of segments)."))
			.Number(TEXT("numRings"),
				TEXT("Ring count: sphere latitude rings (switches to lat-long topology) or torus major segments."))
			.Number(TEXT("numSteps"), TEXT("Number of steps for stairs."))
			.Number(TEXT("stepWidth"), TEXT("Width of each stair step."))
			.Number(TEXT("stepHeight"), TEXT("Height of each stair step."))
			.Number(TEXT("stepDepth"), TEXT("Depth of each stair step."))
			.Bool(TEXT("floating"), TEXT("create_stairs/create_spiral_stairs: omit the support structure beneath the steps."))
			.Number(TEXT("numTurns"), TEXT("Number of full revolutions for create_spiral_stairs (mutually exclusive with curveAngle)."))
			.Number(TEXT("curveAngle"), TEXT("Total sweep angle in degrees for create_spiral_stairs (mutually exclusive with numTurns; default 90)."))
			.Number(TEXT("widthSegments"), TEXT("Segments along width."))
			.Number(TEXT("heightSegments"), TEXT("Segments along height."))
			.Number(TEXT("depthSegments"), TEXT("Segments along depth."))
			.Number(TEXT("radialSegments"),
				TEXT("Radial segments: sphere longitude steps (switches to lat-long topology) or torus minor segments."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Number(TEXT("distance"), TEXT("Distance value."))
			.Number(TEXT("offset"), TEXT("offset_faces/loop_cut: offset distance."))
			.Number(TEXT("amount"),
				TEXT("Generic amount for operations (bevel size, inset distance, etc.)."))
			.Number(TEXT("segments"),
				TEXT("Number of segments for bevel, subdivide."))
			.Integer(TEXT("steps"), TEXT("revolve/sweep/chamfer: step count (distinct from numSteps, which is stairs-specific)."))
			.Bool(TEXT("capped"), TEXT("revolve: cap the ends of the revolved profile."))
			.ArrayOfObjects(TEXT("profile"), TEXT("revolve: 2D profile points to revolve, each {x, y}."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Number(TEXT("angle"), TEXT("Angle in degrees."))
			.Number(TEXT("extent"), TEXT("bend/twist: warp extent along the axis."))
			.Number(TEXT("flareX"), TEXT("taper: flare percentage along X."))
			.Number(TEXT("flareY"), TEXT("taper: flare percentage along Y."))
			.Number(TEXT("magnitude"), TEXT("noise_deform: displacement magnitude."))
			.Number(TEXT("frequency"), TEXT("noise_deform: noise frequency."))
			.StringEnum(TEXT("axis"), {
				TEXT("X"),
				TEXT("Y"),
				TEXT("Z")
			}, TEXT("Axis for deformation operations."))
			.Bool(TEXT("weld"), TEXT("mirror: weld vertices along the mirror plane."))
			.Integer(TEXT("count"), TEXT("array_linear/array_radial/duplicate_along_spline: number of copies."))
			.Number(TEXT("strength"), TEXT("Strength or weight."))
			.Number(TEXT("thickness"), TEXT("shell: wall thickness."))
			.Number(TEXT("alpha"), TEXT("smooth: per-iteration smoothing factor."))
			.Number(TEXT("weight"), TEXT("Weight for lattice deformation."))
			.Number(TEXT("latticeResolution"), TEXT("Control lattice resolution for lattice deformation."))
			.Object(TEXT("position"), TEXT("lattice_deform: deformation center (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Number(TEXT("heightScale"), TEXT("Texture displacement height scale."))
			.Number(TEXT("midpoint"), TEXT("Texture luminance midpoint for displacement."))
			.String(TEXT("texturePath"), TEXT("Texture asset path for displacement."))
			.Number(TEXT("iterations"),
				TEXT("Number of iterations for smooth, remesh."))
			.Number(TEXT("targetTriangleCount"),
				TEXT("Target triangle count for simplify_mesh and remesh_uniform."))
			.Number(TEXT("targetPercentage"),
				TEXT("Percent of triangles to keep for simplify_mesh when targetTriangleCount is absent (default 50)."))
			.Number(TEXT("targetEdgeLength"),
				TEXT("Target edge length for remesh_uniform (mutually exclusive with targetTriangleCount)."))
			.Number(TEXT("weldDistance"),
				TEXT("Distance threshold for weld_vertices (alias of tolerance)."))
			.Number(TEXT("tolerance"), TEXT("weld_vertices/merge_vertices: distance threshold for merging coincident vertices."))
			.Bool(TEXT("compact"), TEXT("merge_vertices: compact the mesh buffers after merging."))
			.Number(TEXT("uvChannel"), TEXT("UV channel index (0-7)."))
			.Object(TEXT("uvScale"), TEXT("UV scale."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("u")).Number(TEXT("v"));
			})
			.Object(TEXT("uvOffset"), TEXT("UV offset."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("u")).Number(TEXT("v"));
			})
			.String(TEXT("projectionType"), TEXT("project_uv: projection mode (e.g. box, planar, cylindrical)."))
			.Number(TEXT("translateU"), TEXT("transform_uvs: U-axis translation."))
			.Number(TEXT("translateV"), TEXT("transform_uvs: V-axis translation."))
			.Number(TEXT("scaleU"), TEXT("transform_uvs: U-axis scale."))
			.Number(TEXT("scaleV"), TEXT("transform_uvs: V-axis scale."))
			.Integer(TEXT("textureResolution"), TEXT("pack_uv_islands: target texture resolution."))
			.Number(TEXT("hardEdgeAngle"),
				TEXT("Opening angle in degrees above which recalculate_normals splits hard edges."))
			.Bool(TEXT("computeWeightedNormals"),
				TEXT("Use area-weighted normals for recalculate_normals."))
			.Integer(TEXT("edgeGroupA"), TEXT("bridge: first polygroup edge to bridge from."))
			.Integer(TEXT("edgeGroupB"), TEXT("bridge: second polygroup edge to bridge to."))
			.Bool(TEXT("smooth"), TEXT("loft: smooth the interpolated cross-sections."))
			.Bool(TEXT("cap"), TEXT("loft/sweep/extrude_along_spline: cap the open ends."))
			.Array(TEXT("profileActors"), TEXT("loft: actor names providing the cross-section profiles."), TEXT("string"))
			.String(TEXT("splineActorName"), TEXT("sweep/duplicate_along_spline/extrude_along_spline: spline actor to follow."))
			.Number(TEXT("twist"), TEXT("sweep/extrude_along_spline: twist in degrees along the path."))
			.Number(TEXT("scaleStart"), TEXT("sweep/extrude_along_spline: cross-section scale at the path start."))
			.Number(TEXT("scaleEnd"), TEXT("sweep/extrude_along_spline: cross-section scale at the path end."))
			.Bool(TEXT("alignToSpline"), TEXT("duplicate_along_spline: orient copies to the spline tangent."))
			.Number(TEXT("scaleVariation"), TEXT("duplicate_along_spline: random per-copy scale variation."))
			.Integer(TEXT("numCuts"), TEXT("loop_cut: number of cuts to insert."))
			.Array(TEXT("edges"), TEXT("edge_split: edge indices to split."), TEXT("number"))
			.Integer(TEXT("edgeIndex"), TEXT("edge_split: single edge index to split (alternative to edges)."))
			.Number(TEXT("splitFactor"), TEXT("edge_split: split position along the edge in [0,1]."))
			.Bool(TEXT("weldVertices"), TEXT("edge_split: weld coincident vertices after splitting."))
			.Number(TEXT("weldTolerance"), TEXT("edge_split: distance tolerance for post-split welding."))
			.Number(TEXT("targetQuadSize"), TEXT("quadrangulate: target quad edge length."))
			.Bool(TEXT("preserveFeatures"), TEXT("quadrangulate: preserve sharp features."))
			.Number(TEXT("featureAngleThreshold"), TEXT("quadrangulate: angle in degrees above which an edge is treated as a feature."))
			.Number(TEXT("voxelSize"), TEXT("remesh_voxel: voxel grid cell size."))
			.Number(TEXT("surfaceDistance"), TEXT("remesh_voxel: surface extraction distance/offset."))
			.StringEnum(TEXT("collisionType"), {
				TEXT("box"),
				TEXT("sphere"),
				TEXT("capsule"),
				TEXT("convex"),
				TEXT("convex_decomposition")
			}, TEXT("generate_collision: shape/method to generate."))
			.Number(TEXT("hullCount"),
				TEXT("Max convex hulls for generate_collision (convex/convex_decomposition only)."))
			.Number(TEXT("hullPrecision"),
				TEXT("generate_complex_collision: convex decomposition search factor in [0,1] (default 0.5; higher searches harder)."))
			.Integer(TEXT("maxHullCount"), TEXT("generate_complex_collision: max convex hulls to generate."))
			.Number(TEXT("simplificationFactor"), TEXT("simplify_collision: simplification strength in [0,1]."))
			.Integer(TEXT("targetHullCount"), TEXT("simplify_collision: target convex hull count."))
			.Number(TEXT("lodCount"),
				TEXT("Number of LOD levels to generate."))
			.Number(TEXT("lodIndex"),
				TEXT("Specific LOD index to configure."))
			.Number(TEXT("reductionPercent"),
				TEXT("Percent of triangles to remove for simplify_mesh (mutually exclusive with targetTriangleCount/targetPercentage)."))
			.Number(TEXT("trianglePercent"), TEXT("set_lod_settings: percent of triangles to keep at this LOD."))
			.Bool(TEXT("recomputeNormals"), TEXT("set_lod_settings: recompute normals for this LOD."))
			.Bool(TEXT("recomputeTangents"), TEXT("set_lod_settings: recompute tangents for this LOD."))
			.Array(TEXT("screenSizes"),
				TEXT("Array of screen sizes for each LOD."), TEXT("number"))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageGeometry);
