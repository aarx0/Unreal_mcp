// LINT-TOOL: manage_geometry
// LINT-SCHEMA-DERIVED
// manage_geometry as FMcpCall classes — eighteenth classed family
// (docs/action-declarations.md). Adopts schema-from-decls: each class AUTHORS
// its schema fragment in a S_<Suffix>() function; the published facade schema
// folds those fragments and GetDecl() derives the validation decl from the same
// fragment via McpDeriveDecl(), so schema and decl are one source and cannot
// drift. Run() delegates to the subsystem member handlers — HandleGeometry*
// (GeometryHandlers.cpp) — until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageGeometry
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Builder method + description are
// the retired facade's authored help text, verbatim; McpDeriveDecl() reads the
// param names + required-set back out of these to build the transport validation
// decl (only names + required drive validation — the transport never type-checks
// a value against a param's kind, so kind is display metadata). The joint boolean
// rejects require BOTH named spellings (targetActor AND toolActor; actorName AND
// trimActorName; actorName AND splineActorName on duplicate/extrude spline), so
// those keep both required — AND requirements, not one-of aliases. get_mesh_info
// and generate_lods reject only when BOTH actorName and assetPath are absent, so
// both stay optional (at-least-one handler-enforced); sweep's splineActorName
// stays optional (absent → straight-path sweep). Drift corrected against the
// handler read-sets: dropped 'amount' (no action reads it — it lives on
// manage_asset), and added areaWeighted/direction/path/factor/center which the
// bodies read but the facade never declared.

// Shared 3D transform triple for the create_* actions (location/rotation/scale),
// factored to keep the 15 create fragments in sync; emits the same builder calls
// the facade inlined.
static void S_Transform(FMcpSchemaBuilder& B)
{
	B.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
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
		});
}

// Primitives
static void S_CreateBox(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("width"), TEXT("Width value."))
	 .Number(TEXT("height"), TEXT("Height value."))
	 .Number(TEXT("depth"), TEXT("Depth value."))
	 .Object(TEXT("dimensions"), TEXT("Box dimensions (width, height, depth)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("width")).Number(TEXT("height")).Number(TEXT("depth"));
		})
	 .Number(TEXT("widthSegments"), TEXT("Segments along width."))
	 .Number(TEXT("heightSegments"), TEXT("Segments along height."))
	 .Number(TEXT("depthSegments"), TEXT("Segments along depth."));
}

static void S_CreateSphere(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("radius"), TEXT("Radius value."))
	 .Integer(TEXT("subdivisions"), TEXT("create_sphere: icosphere subdivision count (default topology; alternative to numRings/radialSegments)."))
	 .Number(TEXT("numRings"), TEXT("Ring count: sphere latitude rings (switches to lat-long topology) or torus major segments."))
	 .Number(TEXT("radialSegments"), TEXT("Radial segments: sphere longitude steps (switches to lat-long topology) or torus minor segments."));
}

static void S_CreateCylinder(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("radius"), TEXT("Radius value."))
	 .Number(TEXT("height"), TEXT("Height value."))
	 .Number(TEXT("segments"), TEXT("Number of segments for bevel, subdivide."))
	 .Number(TEXT("numSides"), TEXT("Radial side count for cylinder and cone (alias of segments)."));
}

static void S_CreateCone(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("baseRadius"), TEXT("create_cone: radius at the base."))
	 .Number(TEXT("topRadius"), TEXT("create_cone: radius at the top (0 for a point)."))
	 .Number(TEXT("height"), TEXT("Height value."))
	 .Number(TEXT("segments"), TEXT("Number of segments for bevel, subdivide."))
	 .Number(TEXT("numSides"), TEXT("Radial side count for cylinder and cone (alias of segments)."));
}

static void S_CreateCapsule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("radius"), TEXT("Radius value."))
	 .Number(TEXT("length"), TEXT("create_capsule/create_ramp: cylinder/ramp length."))
	 .Integer(TEXT("hemisphereSteps"), TEXT("create_capsule: hemisphere cap subdivision count."))
	 .Number(TEXT("segments"), TEXT("Number of segments for bevel, subdivide."));
}

static void S_CreateTorus(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("majorRadius"), TEXT("create_torus/create_arch: major (ring) radius."))
	 .Number(TEXT("minorRadius"), TEXT("create_torus/create_arch: minor (tube) radius."))
	 .Integer(TEXT("majorSegments"), TEXT("create_torus: major ring segment count (alias of numRings)."))
	 .Integer(TEXT("minorSegments"), TEXT("create_torus: minor tube segment count (alias of radialSegments)."))
	 .Number(TEXT("numRings"), TEXT("Ring count: sphere latitude rings (switches to lat-long topology) or torus major segments."))
	 .Number(TEXT("radialSegments"), TEXT("Radial segments: sphere longitude steps (switches to lat-long topology) or torus minor segments."));
}

static void S_CreatePlane(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("width"), TEXT("Width value."))
	 .Number(TEXT("height"), TEXT("Height value."))
	 .Number(TEXT("depth"), TEXT("Depth value."))
	 .Number(TEXT("widthSegments"), TEXT("Segments along width."))
	 .Integer(TEXT("widthSubdivisions"), TEXT("create_plane: subdivision count along width."))
	 .Number(TEXT("heightSegments"), TEXT("Segments along height."))
	 .Integer(TEXT("depthSubdivisions"), TEXT("create_plane: subdivision count along depth."));
}

static void S_CreateDisc(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("radius"), TEXT("Radius value."))
	 .Number(TEXT("segments"), TEXT("Number of segments for bevel, subdivide."));
}

static void S_CreateStairs(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("stepWidth"), TEXT("Width of each stair step."))
	 .Number(TEXT("stepHeight"), TEXT("Height of each stair step."))
	 .Number(TEXT("stepDepth"), TEXT("Depth of each stair step."))
	 .Number(TEXT("numSteps"), TEXT("Number of steps for stairs."))
	 .Bool(TEXT("floating"), TEXT("create_stairs/create_spiral_stairs: omit the support structure beneath the steps."));
}

static void S_CreateSpiralStairs(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("stepWidth"), TEXT("Width of each stair step."))
	 .Number(TEXT("stepHeight"), TEXT("Height of each stair step."))
	 .Number(TEXT("innerRadius"), TEXT("Inner radius for torus."))
	 .Number(TEXT("curveAngle"), TEXT("Total sweep angle in degrees for create_spiral_stairs (mutually exclusive with numTurns; default 90)."))
	 .Number(TEXT("numTurns"), TEXT("Number of full revolutions for create_spiral_stairs (mutually exclusive with curveAngle)."))
	 .Number(TEXT("numSteps"), TEXT("Number of steps for stairs."))
	 .Bool(TEXT("floating"), TEXT("create_stairs/create_spiral_stairs: omit the support structure beneath the steps."));
}

static void S_CreateRing(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("outerRadius"), TEXT("create_ring/create_pipe: outer radius."))
	 .Number(TEXT("innerRadius"), TEXT("Inner radius for torus."))
	 .Number(TEXT("segments"), TEXT("Number of segments for bevel, subdivide."));
}

static void S_CreateArch(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("majorRadius"), TEXT("create_torus/create_arch: major (ring) radius."))
	 .Number(TEXT("minorRadius"), TEXT("create_torus/create_arch: minor (tube) radius."))
	 .Number(TEXT("angle"), TEXT("Angle in degrees."))
	 .Integer(TEXT("majorSteps"), TEXT("create_arch: major ring step count."))
	 .Integer(TEXT("minorSteps"), TEXT("create_arch: minor tube step count."));
}

static void S_CreatePipe(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("outerRadius"), TEXT("create_ring/create_pipe: outer radius."))
	 .Number(TEXT("innerRadius"), TEXT("Inner radius for torus."))
	 .Number(TEXT("height"), TEXT("Height value."))
	 .Integer(TEXT("radialSteps"), TEXT("create_pipe: radial step count."))
	 .Integer(TEXT("heightSteps"), TEXT("create_pipe: height step count."));
}

static void S_CreateRamp(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("width"), TEXT("Width value."))
	 .Number(TEXT("length"), TEXT("create_capsule/create_ramp: cylinder/ramp length."))
	 .Number(TEXT("height"), TEXT("Height value."));
}

static void S_Revolve(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Alias of actorName for create_* actions."))
	 .String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."));
	S_Transform(B);
	B.Number(TEXT("angle"), TEXT("Angle in degrees."))
	 .Integer(TEXT("steps"), TEXT("revolve/sweep/chamfer: step count (distinct from numSteps, which is stairs-specific)."))
	 .Bool(TEXT("capped"), TEXT("revolve: cap the ends of the revolved profile."))
	 .ArrayOfObjects(TEXT("profile"), TEXT("revolve: 2D profile points to revolve, each {x, y}."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y"));
		});
}

// Booleans
static void S_BooleanUnion(FMcpSchemaBuilder& B)
{
	B.String(TEXT("targetActor"), TEXT("Actor receiving the result of a boolean operation."))
	 .String(TEXT("toolActor"), TEXT("Actor used as the cutting/combining tool in a boolean operation."))
	 .Bool(TEXT("keepTool"), TEXT("Keep the tool actor after a boolean operation (default true)."))
	 .Required({TEXT("targetActor"), TEXT("toolActor")});
}

static void S_BooleanSubtract(FMcpSchemaBuilder& B)
{
	B.String(TEXT("targetActor"), TEXT("Actor receiving the result of a boolean operation."))
	 .String(TEXT("toolActor"), TEXT("Actor used as the cutting/combining tool in a boolean operation."))
	 .Bool(TEXT("keepTool"), TEXT("Keep the tool actor after a boolean operation (default true)."))
	 .Required({TEXT("targetActor"), TEXT("toolActor")});
}

static void S_BooleanIntersection(FMcpSchemaBuilder& B)
{
	B.String(TEXT("targetActor"), TEXT("Actor receiving the result of a boolean operation."))
	 .String(TEXT("toolActor"), TEXT("Actor used as the cutting/combining tool in a boolean operation."))
	 .Bool(TEXT("keepTool"), TEXT("Keep the tool actor after a boolean operation (default true)."))
	 .Required({TEXT("targetActor"), TEXT("toolActor")});
}

static void S_BooleanTrim(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .String(TEXT("trimActorName"), TEXT("boolean_trim: actor used as the trimming surface."))
	 .Bool(TEXT("keepInside"), TEXT("boolean_trim: keep the portion inside the trim surface."))
	 .Required({TEXT("actorName"), TEXT("trimActorName")});
}

static void S_SelfUnion(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Bool(TEXT("fillHoles"), TEXT("self_union/remesh_voxel: fill holes left by the operation."))
	 .Required({TEXT("actorName")});
}

// Mesh Utils
static void S_GetMeshInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .String(TEXT("assetPath"), TEXT("Destination asset path for convert_to_static_mesh/convert_to_nanite (e.g. /Game/Meshes/SM_Rock)."));
}

static void S_RecalculateNormals(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Bool(TEXT("areaWeighted"), TEXT("recalculate_normals: legacy alias of computeWeightedNormals (area-weighted normals)."))
	 .Bool(TEXT("computeWeightedNormals"), TEXT("Use area-weighted normals for recalculate_normals."))
	 .Number(TEXT("hardEdgeAngle"), TEXT("Opening angle in degrees above which recalculate_normals splits hard edges."))
	 .Required({TEXT("actorName")});
}

static void S_FlipNormals(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Required({TEXT("actorName")});
}

static void S_SimplifyMesh(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("targetTriangleCount"), TEXT("Target triangle count for simplify_mesh and remesh_uniform."))
	 .Number(TEXT("targetPercentage"), TEXT("Percent of triangles to keep for simplify_mesh when targetTriangleCount is absent (default 50)."))
	 .Number(TEXT("reductionPercent"), TEXT("Percent of triangles to remove for simplify_mesh (mutually exclusive with targetTriangleCount/targetPercentage)."))
	 .Required({TEXT("actorName")});
}

static void S_Subdivide(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("iterations"), TEXT("Number of iterations for smooth, remesh."))
	 .Required({TEXT("actorName")});
}

static void S_AutoUV(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Required({TEXT("actorName")});
}

static void S_ConvertToStaticMesh(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .String(TEXT("assetPath"), TEXT("Destination asset path for convert_to_static_mesh/convert_to_nanite (e.g. /Game/Meshes/SM_Rock)."))
	 .String(TEXT("outputPath"), TEXT("Alias of assetPath."))
	 .String(TEXT("path"), TEXT("Alias of assetPath."))
	 .Required({TEXT("actorName")});
}

// Modeling Operations
static void S_Extrude(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("distance"), TEXT("Distance value."))
	 .Object(TEXT("direction"), TEXT("extrude: direction vector (x, y, z) to extrude along (default +Z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Required({TEXT("actorName")});
}

static void S_Inset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("distance"), TEXT("Distance value."))
	 .Required({TEXT("actorName")});
}

static void S_Outset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("distance"), TEXT("Distance value."))
	 .Required({TEXT("actorName")});
}

static void S_Bevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("distance"), TEXT("Distance value."))
	 .Integer(TEXT("subdivisions"), TEXT("create_sphere: icosphere subdivision count (default topology; alternative to numRings/radialSegments)."))
	 .Required({TEXT("actorName")});
}

static void S_OffsetFaces(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("distance"), TEXT("Distance value."))
	 .Required({TEXT("actorName")});
}

static void S_Shell(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("thickness"), TEXT("shell: wall thickness."))
	 .Required({TEXT("actorName")});
}

static void S_Chamfer(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("distance"), TEXT("Distance value."))
	 .Integer(TEXT("steps"), TEXT("revolve/sweep/chamfer: step count (distinct from numSteps, which is stairs-specific)."))
	 .Required({TEXT("actorName")});
}

// Deformers
static void S_Bend(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("angle"), TEXT("Angle in degrees."))
	 .Number(TEXT("extent"), TEXT("bend/twist: warp extent along the axis."))
	 .Required({TEXT("actorName")});
}

static void S_Twist(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("angle"), TEXT("Angle in degrees."))
	 .Number(TEXT("extent"), TEXT("bend/twist: warp extent along the axis."))
	 .Required({TEXT("actorName")});
}

static void S_Taper(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("flareX"), TEXT("taper: flare percentage along X."))
	 .Number(TEXT("flareY"), TEXT("taper: flare percentage along Y."))
	 .Number(TEXT("extent"), TEXT("bend/twist: warp extent along the axis."))
	 .Required({TEXT("actorName")});
}

static void S_NoiseDeform(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("magnitude"), TEXT("noise_deform: displacement magnitude."))
	 .Number(TEXT("frequency"), TEXT("noise_deform: noise frequency."))
	 .Required({TEXT("actorName")});
}

static void S_Smooth(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("iterations"), TEXT("Number of iterations for smooth, remesh."))
	 .Number(TEXT("alpha"), TEXT("smooth: per-iteration smoothing factor."))
	 .Required({TEXT("actorName")});
}

static void S_Relax(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("iterations"), TEXT("Number of iterations for smooth, remesh."))
	 .Number(TEXT("strength"), TEXT("Strength or weight."))
	 .Required({TEXT("actorName")});
}

static void S_Stretch(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .StringEnum(TEXT("axis"), {
		TEXT("X"),
		TEXT("Y"),
		TEXT("Z")
	}, TEXT("Axis for deformation operations."))
	 .Number(TEXT("factor"), TEXT("stretch/spherify/cylindrify: deformation strength factor."))
	 .Required({TEXT("actorName")});
}

static void S_Spherify(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("factor"), TEXT("stretch/spherify/cylindrify: deformation strength factor."))
	 .Required({TEXT("actorName")});
}

static void S_Cylindrify(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .StringEnum(TEXT("axis"), {
		TEXT("X"),
		TEXT("Y"),
		TEXT("Z")
	}, TEXT("Axis for deformation operations."))
	 .Number(TEXT("factor"), TEXT("stretch/spherify/cylindrify: deformation strength factor."))
	 .Required({TEXT("actorName")});
}

static void S_LatticeDeform(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("latticeResolution"), TEXT("Control lattice resolution for lattice deformation."))
	 .Number(TEXT("weight"), TEXT("Weight for lattice deformation."))
	 .Number(TEXT("strength"), TEXT("Strength or weight."))
	 .Object(TEXT("position"), TEXT("lattice_deform: deformation center (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Number(TEXT("radius"), TEXT("Radius value."))
	 .StringEnum(TEXT("axis"), {
		TEXT("X"),
		TEXT("Y"),
		TEXT("Z")
	}, TEXT("Axis for deformation operations."))
	 .Required({TEXT("actorName")});
}

static void S_DisplaceByTexture(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .String(TEXT("texturePath"), TEXT("Texture asset path for displacement."))
	 .Number(TEXT("heightScale"), TEXT("Texture displacement height scale."))
	 .Number(TEXT("strength"), TEXT("Strength or weight."))
	 .Number(TEXT("midpoint"), TEXT("Texture luminance midpoint for displacement."))
	 .StringEnum(TEXT("axis"), {
		TEXT("X"),
		TEXT("Y"),
		TEXT("Z")
	}, TEXT("Axis for deformation operations."))
	 .Required({TEXT("actorName"), TEXT("texturePath")});
}

// Mesh Repair
static void S_WeldVertices(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("tolerance"), TEXT("weld_vertices/merge_vertices: distance threshold for merging coincident vertices."))
	 .Number(TEXT("weldDistance"), TEXT("Distance threshold for weld_vertices (alias of tolerance)."))
	 .Required({TEXT("actorName")});
}

static void S_FillHoles(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Required({TEXT("actorName")});
}

static void S_RemoveDegenerates(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Required({TEXT("actorName")});
}

static void S_RemeshUniform(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("targetTriangleCount"), TEXT("Target triangle count for simplify_mesh and remesh_uniform."))
	 .Number(TEXT("targetEdgeLength"), TEXT("Target edge length for remesh_uniform (mutually exclusive with targetTriangleCount)."))
	 .Required({TEXT("actorName")});
}

static void S_MergeVertices(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("tolerance"), TEXT("weld_vertices/merge_vertices: distance threshold for merging coincident vertices."))
	 .Bool(TEXT("compact"), TEXT("merge_vertices: compact the mesh buffers after merging."))
	 .Required({TEXT("actorName")});
}

// Collision Generation
static void S_GenerateCollision(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .StringEnum(TEXT("collisionType"), {
		TEXT("box"),
		TEXT("sphere"),
		TEXT("capsule"),
		TEXT("convex"),
		TEXT("convex_decomposition")
	}, TEXT("generate_collision: shape/method to generate."))
	 .Number(TEXT("hullCount"), TEXT("Max convex hulls for generate_collision (convex/convex_decomposition only)."))
	 .Required({TEXT("actorName")});
}

// Transform Operations
static void S_Mirror(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .StringEnum(TEXT("axis"), {
		TEXT("X"),
		TEXT("Y"),
		TEXT("Z")
	}, TEXT("Axis for deformation operations."))
	 .Bool(TEXT("weld"), TEXT("mirror: weld vertices along the mirror plane."))
	 .Required({TEXT("actorName")});
}

static void S_ArrayLinear(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Integer(TEXT("count"), TEXT("array_linear/array_radial/duplicate_along_spline: number of copies."))
	 .Number(TEXT("offset"), TEXT("offset_faces/loop_cut: offset distance."))
	 .Required({TEXT("actorName")});
}

static void S_ArrayRadial(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Integer(TEXT("count"), TEXT("array_linear/array_radial/duplicate_along_spline: number of copies."))
	 .Object(TEXT("center"), TEXT("array_radial: center of the radial array (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .StringEnum(TEXT("axis"), {
		TEXT("X"),
		TEXT("Y"),
		TEXT("Z")
	}, TEXT("Axis for deformation operations."))
	 .Number(TEXT("angle"), TEXT("Angle in degrees."))
	 .Required({TEXT("actorName")});
}

// Mesh Topology Operations
static void S_Triangulate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Required({TEXT("actorName")});
}

static void S_Poke(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("offset"), TEXT("offset_faces/loop_cut: offset distance."))
	 .Required({TEXT("actorName")});
}

// UV Operations
static void S_ProjectUV(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .String(TEXT("projectionType"), TEXT("project_uv: projection mode (e.g. box, planar, cylindrical)."))
	 .Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Number(TEXT("uvChannel"), TEXT("UV channel index (0-7)."))
	 .Object(TEXT("uvScale"), TEXT("UV scale."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("u")).Number(TEXT("v"));
		})
	 .Object(TEXT("uvOffset"), TEXT("UV offset."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("u")).Number(TEXT("v"));
		})
	 .Required({TEXT("actorName")});
}

static void S_TransformUVs(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("uvChannel"), TEXT("UV channel index (0-7)."))
	 .Number(TEXT("translateU"), TEXT("transform_uvs: U-axis translation."))
	 .Number(TEXT("translateV"), TEXT("transform_uvs: V-axis translation."))
	 .Number(TEXT("scaleU"), TEXT("transform_uvs: U-axis scale."))
	 .Number(TEXT("scaleV"), TEXT("transform_uvs: V-axis scale."))
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
		})
	 .Required({TEXT("actorName")});
}

// Tangent Operations
static void S_RecomputeTangents(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Required({TEXT("actorName")});
}

// Advanced Operations (Bridge, Loft, Sweep)
static void S_Bridge(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Integer(TEXT("edgeGroupA"), TEXT("bridge: first polygroup edge to bridge from."))
	 .Integer(TEXT("edgeGroupB"), TEXT("bridge: second polygroup edge to bridge to."))
	 .Integer(TEXT("subdivisions"), TEXT("create_sphere: icosphere subdivision count (default topology; alternative to numRings/radialSegments)."))
	 .Required({TEXT("actorName")});
}

static void S_Loft(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Integer(TEXT("subdivisions"), TEXT("create_sphere: icosphere subdivision count (default topology; alternative to numRings/radialSegments)."))
	 .Bool(TEXT("smooth"), TEXT("loft: smooth the interpolated cross-sections."))
	 .Bool(TEXT("cap"), TEXT("loft/sweep/extrude_along_spline: cap the open ends."))
	 .Array(TEXT("profileActors"), TEXT("loft: actor names providing the cross-section profiles."), TEXT("string"))
	 .Required({TEXT("actorName")});
}

static void S_Sweep(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .String(TEXT("splineActorName"), TEXT("sweep/duplicate_along_spline/extrude_along_spline: spline actor to follow."))
	 .Integer(TEXT("steps"), TEXT("revolve/sweep/chamfer: step count (distinct from numSteps, which is stairs-specific)."))
	 .Number(TEXT("twist"), TEXT("sweep/extrude_along_spline: twist in degrees along the path."))
	 .Number(TEXT("scaleStart"), TEXT("sweep/extrude_along_spline: cross-section scale at the path start."))
	 .Number(TEXT("scaleEnd"), TEXT("sweep/extrude_along_spline: cross-section scale at the path end."))
	 .Bool(TEXT("cap"), TEXT("loft/sweep/extrude_along_spline: cap the open ends."))
	 .Required({TEXT("actorName")});
}

static void S_LoopCut(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Integer(TEXT("numCuts"), TEXT("loop_cut: number of cuts to insert."))
	 .Number(TEXT("offset"), TEXT("offset_faces/loop_cut: offset distance."))
	 .StringEnum(TEXT("axis"), {
		TEXT("X"),
		TEXT("Y"),
		TEXT("Z")
	}, TEXT("Axis for deformation operations."))
	 .Required({TEXT("actorName")});
}

static void S_DuplicateAlongSpline(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .String(TEXT("splineActorName"), TEXT("sweep/duplicate_along_spline/extrude_along_spline: spline actor to follow."))
	 .Integer(TEXT("count"), TEXT("array_linear/array_radial/duplicate_along_spline: number of copies."))
	 .Bool(TEXT("alignToSpline"), TEXT("duplicate_along_spline: orient copies to the spline tangent."))
	 .Number(TEXT("scaleVariation"), TEXT("duplicate_along_spline: random per-copy scale variation."))
	 .Required({TEXT("actorName"), TEXT("splineActorName")});
}

// Additional UV Operations
static void S_UnwrapUV(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("uvChannel"), TEXT("UV channel index (0-7)."))
	 .Required({TEXT("actorName")});
}

static void S_PackUVIslands(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("uvChannel"), TEXT("UV channel index (0-7)."))
	 .Integer(TEXT("textureResolution"), TEXT("pack_uv_islands: target texture resolution."))
	 .Required({TEXT("actorName")});
}

// Nanite Conversion
static void S_ConvertToNanite(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .String(TEXT("assetPath"), TEXT("Destination asset path for convert_to_static_mesh/convert_to_nanite (e.g. /Game/Meshes/SM_Rock)."))
	 .String(TEXT("outputPath"), TEXT("Alias of assetPath."))
	 .String(TEXT("path"), TEXT("Alias of assetPath."))
	 .Required({TEXT("actorName")});
}

// Spline-based Operations
static void S_ExtrudeAlongSpline(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .String(TEXT("splineActorName"), TEXT("sweep/duplicate_along_spline/extrude_along_spline: spline actor to follow."))
	 .Number(TEXT("segments"), TEXT("Number of segments for bevel, subdivide."))
	 .Bool(TEXT("cap"), TEXT("loft/sweep/extrude_along_spline: cap the open ends."))
	 .Number(TEXT("scaleStart"), TEXT("sweep/extrude_along_spline: cross-section scale at the path start."))
	 .Number(TEXT("scaleEnd"), TEXT("sweep/extrude_along_spline: cross-section scale at the path end."))
	 .Number(TEXT("twist"), TEXT("sweep/extrude_along_spline: twist in degrees along the path."))
	 .Required({TEXT("actorName"), TEXT("splineActorName")});
}

// Edge Operations
static void S_EdgeSplit(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Array(TEXT("edges"), TEXT("edge_split: edge indices to split."), TEXT("number"))
	 .Integer(TEXT("edgeIndex"), TEXT("edge_split: single edge index to split (alternative to edges)."))
	 .Number(TEXT("splitFactor"), TEXT("edge_split: split position along the edge in [0,1]."))
	 .Bool(TEXT("weldVertices"), TEXT("edge_split: weld coincident vertices after splitting."))
	 .Number(TEXT("weldTolerance"), TEXT("edge_split: distance tolerance for post-split welding."))
	 .Required({TEXT("actorName")});
}

// Topology Operations
static void S_Quadrangulate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("targetQuadSize"), TEXT("quadrangulate: target quad edge length."))
	 .Bool(TEXT("preserveFeatures"), TEXT("quadrangulate: preserve sharp features."))
	 .Number(TEXT("featureAngleThreshold"), TEXT("quadrangulate: angle in degrees above which an edge is treated as a feature."))
	 .Required({TEXT("actorName")});
}

// Remesh Operations
static void S_RemeshVoxel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("voxelSize"), TEXT("remesh_voxel: voxel grid cell size."))
	 .Number(TEXT("surfaceDistance"), TEXT("remesh_voxel: surface extraction distance/offset."))
	 .Bool(TEXT("fillHoles"), TEXT("self_union/remesh_voxel: fill holes left by the operation."))
	 .Required({TEXT("actorName")});
}

// Complex Collision
static void S_GenerateComplexCollision(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Integer(TEXT("maxHullCount"), TEXT("generate_complex_collision: max convex hulls to generate."))
	 .Number(TEXT("hullPrecision"), TEXT("generate_complex_collision: convex decomposition search factor in [0,1] (default 0.5; higher searches harder)."))
	 .Required({TEXT("actorName")});
}

// Collision Simplification
static void S_SimplifyCollision(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("simplificationFactor"), TEXT("simplify_collision: simplification strength in [0,1]."))
	 .Integer(TEXT("targetHullCount"), TEXT("simplify_collision: target convex hull count."))
	 .Required({TEXT("actorName")});
}

// LOD Operations (Geometry-specific)
static void S_GenerateLODs(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor to operate on (also sets the label for create_* actions)."))
	 .Number(TEXT("lodCount"), TEXT("Number of LOD levels to generate."))
	 .String(TEXT("assetPath"), TEXT("Destination asset path for convert_to_static_mesh/convert_to_nanite (e.g. /Game/Meshes/SM_Rock)."));
}

static void S_SetLODSettings(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Destination asset path for convert_to_static_mesh/convert_to_nanite (e.g. /Game/Meshes/SM_Rock)."))
	 .Number(TEXT("lodIndex"), TEXT("Specific LOD index to configure."))
	 .Number(TEXT("trianglePercent"), TEXT("set_lod_settings: percent of triangles to keep at this LOD."))
	 .Bool(TEXT("recomputeNormals"), TEXT("set_lod_settings: recompute normals for this LOD."))
	 .Bool(TEXT("recomputeTangents"), TEXT("set_lod_settings: recompute tangents for this LOD."))
	 .Required({TEXT("assetPath")});
}

static void S_SetLODScreenSizes(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Destination asset path for convert_to_static_mesh/convert_to_nanite (e.g. /Game/Meshes/SM_Rock)."))
	 .Array(TEXT("screenSizes"), TEXT("Array of screen sizes for each LOD."), TEXT("number"))
	 .Required({TEXT("assetPath"), TEXT("screenSizes")});
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row: the implementation TU is compiled
// only WITH_EDITOR (dispatcher included), so the members answer exactly as
// the retired chain did in every build flavor — absent in non-editor builds
// with the flag answering at Execute(), and each member's
// MCP_HAS_FULL_GEOMETRY_SCRIPT stub answering the chain's NOT_SUPPORTED on
// pre-5.1 engines. Mutating on everything except the one reader,
// get_mesh_info.

#define MCP_GE_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                    \
class FMcpCall_ManageGeometry_##ClassSuffix final : public FMcpCall                       \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_geometry"),             \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),            \
			&S_##ClassSuffix);                                                            \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return S.HandlerFn(RequestId, Payload, Socket);                                   \
	}                                                                                     \
};

// Primitives
MCP_GE_CALL(CreateBox, "create_box", HandleGeometryCreateBox, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateSphere, "create_sphere", HandleGeometryCreateSphere, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateCylinder, "create_cylinder", HandleGeometryCreateCylinder, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateCone, "create_cone", HandleGeometryCreateCone, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateCapsule, "create_capsule", HandleGeometryCreateCapsule, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateTorus, "create_torus", HandleGeometryCreateTorus, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreatePlane, "create_plane", HandleGeometryCreatePlane, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateDisc, "create_disc", HandleGeometryCreateDisc, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateStairs, "create_stairs", HandleGeometryCreateStairs, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateSpiralStairs, "create_spiral_stairs", HandleGeometryCreateSpiralStairs, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateRing, "create_ring", HandleGeometryCreateRing, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateArch, "create_arch", HandleGeometryCreateArch, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreatePipe, "create_pipe", HandleGeometryCreatePipe, EMcpCallFlags::Mutating)
MCP_GE_CALL(CreateRamp, "create_ramp", HandleGeometryCreateRamp, EMcpCallFlags::Mutating)
MCP_GE_CALL(Revolve, "revolve", HandleGeometryRevolve, EMcpCallFlags::Mutating)

// Booleans
MCP_GE_CALL(BooleanUnion, "boolean_union", HandleGeometryBooleanUnion, EMcpCallFlags::Mutating)
MCP_GE_CALL(BooleanSubtract, "boolean_subtract", HandleGeometryBooleanSubtract, EMcpCallFlags::Mutating)
MCP_GE_CALL(BooleanIntersection, "boolean_intersection", HandleGeometryBooleanIntersection, EMcpCallFlags::Mutating)
MCP_GE_CALL(BooleanTrim, "boolean_trim", HandleGeometryBooleanTrim, EMcpCallFlags::Mutating)
MCP_GE_CALL(SelfUnion, "self_union", HandleGeometrySelfUnion, EMcpCallFlags::Mutating)

// Mesh Utils
MCP_GE_CALL(GetMeshInfo, "get_mesh_info", HandleGeometryGetMeshInfo, EMcpCallFlags::None)
MCP_GE_CALL(RecalculateNormals, "recalculate_normals", HandleGeometryRecalculateNormals, EMcpCallFlags::Mutating)
MCP_GE_CALL(FlipNormals, "flip_normals", HandleGeometryFlipNormals, EMcpCallFlags::Mutating)
MCP_GE_CALL(SimplifyMesh, "simplify_mesh", HandleGeometrySimplifyMesh, EMcpCallFlags::Mutating)
MCP_GE_CALL(Subdivide, "subdivide", HandleGeometrySubdivide, EMcpCallFlags::Mutating)
MCP_GE_CALL(AutoUV, "auto_uv", HandleGeometryAutoUV, EMcpCallFlags::Mutating)
MCP_GE_CALL(ConvertToStaticMesh, "convert_to_static_mesh", HandleGeometryConvertToStaticMesh, EMcpCallFlags::Mutating)

// Modeling Operations
MCP_GE_CALL(Extrude, "extrude", HandleGeometryExtrude, EMcpCallFlags::Mutating)
MCP_GE_CALL(Inset, "inset", HandleGeometryInset, EMcpCallFlags::Mutating)
MCP_GE_CALL(Outset, "outset", HandleGeometryOutset, EMcpCallFlags::Mutating)
MCP_GE_CALL(Bevel, "bevel", HandleGeometryBevel, EMcpCallFlags::Mutating)
MCP_GE_CALL(OffsetFaces, "offset_faces", HandleGeometryOffsetFaces, EMcpCallFlags::Mutating)
MCP_GE_CALL(Shell, "shell", HandleGeometryShell, EMcpCallFlags::Mutating)
MCP_GE_CALL(Chamfer, "chamfer", HandleGeometryChamfer, EMcpCallFlags::Mutating)

// Deformers
MCP_GE_CALL(Bend, "bend", HandleGeometryBend, EMcpCallFlags::Mutating)
MCP_GE_CALL(Twist, "twist", HandleGeometryTwist, EMcpCallFlags::Mutating)
MCP_GE_CALL(Taper, "taper", HandleGeometryTaper, EMcpCallFlags::Mutating)
MCP_GE_CALL(NoiseDeform, "noise_deform", HandleGeometryNoiseDeform, EMcpCallFlags::Mutating)
MCP_GE_CALL(Smooth, "smooth", HandleGeometrySmooth, EMcpCallFlags::Mutating)
MCP_GE_CALL(Relax, "relax", HandleGeometryRelax, EMcpCallFlags::Mutating)
MCP_GE_CALL(Stretch, "stretch", HandleGeometryStretch, EMcpCallFlags::Mutating)
MCP_GE_CALL(Spherify, "spherify", HandleGeometrySpherify, EMcpCallFlags::Mutating)
MCP_GE_CALL(Cylindrify, "cylindrify", HandleGeometryCylindrify, EMcpCallFlags::Mutating)
MCP_GE_CALL(LatticeDeform, "lattice_deform", HandleGeometryLatticeDeform, EMcpCallFlags::Mutating)
MCP_GE_CALL(DisplaceByTexture, "displace_by_texture", HandleGeometryDisplaceByTexture, EMcpCallFlags::Mutating)

// Mesh Repair
MCP_GE_CALL(WeldVertices, "weld_vertices", HandleGeometryWeldVertices, EMcpCallFlags::Mutating)
MCP_GE_CALL(FillHoles, "fill_holes", HandleGeometryFillHoles, EMcpCallFlags::Mutating)
MCP_GE_CALL(RemoveDegenerates, "remove_degenerates", HandleGeometryRemoveDegenerates, EMcpCallFlags::Mutating)
MCP_GE_CALL(RemeshUniform, "remesh_uniform", HandleGeometryRemeshUniform, EMcpCallFlags::Mutating)
MCP_GE_CALL(MergeVertices, "merge_vertices", HandleGeometryMergeVertices, EMcpCallFlags::Mutating)

// Collision Generation
MCP_GE_CALL(GenerateCollision, "generate_collision", HandleGeometryGenerateCollision, EMcpCallFlags::Mutating)

// Transform Operations
MCP_GE_CALL(Mirror, "mirror", HandleGeometryMirror, EMcpCallFlags::Mutating)
MCP_GE_CALL(ArrayLinear, "array_linear", HandleGeometryArrayLinear, EMcpCallFlags::Mutating)
MCP_GE_CALL(ArrayRadial, "array_radial", HandleGeometryArrayRadial, EMcpCallFlags::Mutating)

// Mesh Topology Operations
MCP_GE_CALL(Triangulate, "triangulate", HandleGeometryTriangulate, EMcpCallFlags::Mutating)
MCP_GE_CALL(Poke, "poke", HandleGeometryPoke, EMcpCallFlags::Mutating)

// UV Operations
MCP_GE_CALL(ProjectUV, "project_uv", HandleGeometryProjectUV, EMcpCallFlags::Mutating)
MCP_GE_CALL(TransformUVs, "transform_uvs", HandleGeometryTransformUVs, EMcpCallFlags::Mutating)

// Tangent Operations
MCP_GE_CALL(RecomputeTangents, "recompute_tangents", HandleGeometryRecomputeTangents, EMcpCallFlags::Mutating)

// Advanced Operations (Bridge, Loft, Sweep)
MCP_GE_CALL(Bridge, "bridge", HandleGeometryBridge, EMcpCallFlags::Mutating)
MCP_GE_CALL(Loft, "loft", HandleGeometryLoft, EMcpCallFlags::Mutating)
MCP_GE_CALL(Sweep, "sweep", HandleGeometrySweep, EMcpCallFlags::Mutating)
MCP_GE_CALL(LoopCut, "loop_cut", HandleGeometryLoopCut, EMcpCallFlags::Mutating)
MCP_GE_CALL(DuplicateAlongSpline, "duplicate_along_spline", HandleGeometryDuplicateAlongSpline, EMcpCallFlags::Mutating)

// Additional UV Operations
MCP_GE_CALL(UnwrapUV, "unwrap_uv", HandleGeometryUnwrapUV, EMcpCallFlags::Mutating)
MCP_GE_CALL(PackUVIslands, "pack_uv_islands", HandleGeometryPackUVIslands, EMcpCallFlags::Mutating)

// Nanite Conversion
MCP_GE_CALL(ConvertToNanite, "convert_to_nanite", HandleGeometryConvertToNanite, EMcpCallFlags::Mutating)

// Spline-based Operations
MCP_GE_CALL(ExtrudeAlongSpline, "extrude_along_spline", HandleGeometryExtrudeAlongSpline, EMcpCallFlags::Mutating)

// Edge Operations
MCP_GE_CALL(EdgeSplit, "edge_split", HandleGeometryEdgeSplit, EMcpCallFlags::Mutating)

// Topology Operations
MCP_GE_CALL(Quadrangulate, "quadrangulate", HandleGeometryQuadrangulate, EMcpCallFlags::Mutating)

// Remesh Operations
MCP_GE_CALL(RemeshVoxel, "remesh_voxel", HandleGeometryRemeshVoxel, EMcpCallFlags::Mutating)

// Complex Collision
MCP_GE_CALL(GenerateComplexCollision, "generate_complex_collision", HandleGeometryGenerateComplexCollision, EMcpCallFlags::Mutating)

// Collision Simplification
MCP_GE_CALL(SimplifyCollision, "simplify_collision", HandleGeometrySimplifyCollision, EMcpCallFlags::Mutating)

// LOD Operations (Geometry-specific)
MCP_GE_CALL(GenerateLODs, "generate_lods", HandleGeometryGenerateLODs, EMcpCallFlags::Mutating)
MCP_GE_CALL(SetLODSettings, "set_lod_settings", HandleGeometrySetLODSettings, EMcpCallFlags::Mutating)
MCP_GE_CALL(SetLODScreenSizes, "set_lod_screen_sizes", HandleGeometrySetLODScreenSizes, EMcpCallFlags::Mutating)

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
