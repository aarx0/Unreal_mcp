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
			.Number(TEXT("width"), TEXT("Width value."))
			.Number(TEXT("height"), TEXT("Height value."))
			.Number(TEXT("depth"), TEXT("Depth value."))
			.Object(TEXT("dimensions"), TEXT("Box dimensions (width, height, depth)."),
				[](FMcpSchemaBuilder& S) {
					S.Number(TEXT("width")).Number(TEXT("height")).Number(TEXT("depth"));
				})
			.Number(TEXT("radius"), TEXT("Radius value."))
			.Number(TEXT("innerRadius"), TEXT("Inner radius for torus."))
			.Number(TEXT("numSides"),
				TEXT("Number of sides for cylinder, cone, etc."))
			.Number(TEXT("numRings"),
				TEXT("Number of rings for sphere, torus."))
			.Number(TEXT("numSteps"), TEXT("Number of steps for stairs."))
			.Number(TEXT("stepWidth"), TEXT("Width of each stair step."))
			.Number(TEXT("stepHeight"), TEXT("Height of each stair step."))
			.Number(TEXT("stepDepth"), TEXT("Depth of each stair step."))
			.Number(TEXT("numTurns"), TEXT("Number of full revolutions for create_spiral_stairs (mutually exclusive with curveAngle)."))
			.Number(TEXT("curveAngle"), TEXT("Total sweep angle in degrees for create_spiral_stairs (mutually exclusive with numTurns; default 90)."))
			.Number(TEXT("widthSegments"), TEXT("Segments along width."))
			.Number(TEXT("heightSegments"), TEXT("Segments along height."))
			.Number(TEXT("depthSegments"), TEXT("Segments along depth."))
			.Number(TEXT("radialSegments"),
				TEXT("Radial segments for circular shapes."))
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
			.Number(TEXT("amount"),
				TEXT("Generic amount for operations (bevel size, inset distance, etc.)."))
			.Number(TEXT("segments"),
				TEXT("Number of segments for bevel, subdivide."))
			.Number(TEXT("angle"), TEXT("Angle in degrees."))
			.StringEnum(TEXT("axis"), {
				TEXT("X"),
				TEXT("Y"),
				TEXT("Z")
			}, TEXT("Axis for deformation operations."))
			.Number(TEXT("strength"), TEXT("Strength or weight."))
			.Number(TEXT("weight"), TEXT("Weight for lattice deformation."))
			.Number(TEXT("latticeResolution"), TEXT("Control lattice resolution for lattice deformation."))
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
				TEXT("Target edge length for remeshing."))
			.Number(TEXT("weldDistance"),
				TEXT("Distance threshold for vertex welding."))
			.Number(TEXT("uvChannel"), TEXT("UV channel index (0-7)."))
			.Object(TEXT("uvScale"), TEXT("UV scale."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("u")).Number(TEXT("v"));
			})
			.Object(TEXT("uvOffset"), TEXT("UV offset."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("u")).Number(TEXT("v"));
			})
			.Number(TEXT("hardEdgeAngle"),
				TEXT("Angle threshold for hard edges (degrees)."))
			.Bool(TEXT("computeWeightedNormals"),
				TEXT("Use area-weighted normals."))
			.StringEnum(TEXT("collisionType"), {
				TEXT("Default"),
				TEXT("Simple"),
				TEXT("Complex"),
				TEXT("UseComplexAsSimple"),
				TEXT("UseSimpleAsComplex")
			}, TEXT("Collision complexity type."))
			.Number(TEXT("hullCount"),
				TEXT("Number of convex hulls for decomposition."))
			.Number(TEXT("hullPrecision"),
				TEXT("Precision for convex hull generation (0-1)."))
			.Number(TEXT("maxVerticesPerHull"),
				TEXT("Maximum vertices per convex hull."))
			.Number(TEXT("lodCount"),
				TEXT("Number of LOD levels to generate."))
			.Number(TEXT("lodIndex"),
				TEXT("Specific LOD index to configure."))
			.Number(TEXT("reductionPercent"),
				TEXT("Percent of triangles to reduce per LOD."))
			.Array(TEXT("screenSizes"),
				TEXT("Array of screen sizes for each LOD."), TEXT("number"))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageGeometry);
