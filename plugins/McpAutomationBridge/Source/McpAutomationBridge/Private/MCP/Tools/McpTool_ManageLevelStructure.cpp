// McpTool_ManageLevelStructure.cpp — manage_level_structure tool definition (17 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageLevelStructure : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_level_structure"); }

	FString GetDescription() const override
	{
		return TEXT("Create levels and sublevels. Configure World Partition, streaming, "
			"data layers, HLOD, and level instances.");
	}

	FString GetCategory() const override { return TEXT("world"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
				.StringEnum(TEXT("action"), McpConsolidatedActions::ManageLevelStructure(),
					TEXT("Level structure action to perform."))
			.String(TEXT("levelName"), TEXT(""))
			.String(TEXT("levelPath"), TEXT("Level asset path."))
			.String(TEXT("parentLevel"), TEXT("Parent level path."))
			.Bool(TEXT("bCreateWorldPartition"),
				TEXT("Create with World Partition enabled."))
			.Bool(TEXT("bUseExternalActors"),
				TEXT("Enable One File Per Actor (OFPA/External Actors) for Data Layer "
					"compatibility. Automatically enabled when bCreateWorldPartition "
					"is true."))
			.Bool(TEXT("loadAfterCreate"),
				TEXT("create_level: open the new level in the editor after creation."))
			.String(TEXT("sublevelName"), TEXT("Name of the sublevel."))
			.String(TEXT("sublevelPath"), TEXT("Level asset path."))
			.StringEnum(TEXT("streamingMethod"), {
				TEXT("Blueprint"),
				TEXT("AlwaysLoaded"),
				TEXT("Disabled")
			}, TEXT("Level streaming method."))
			.Bool(TEXT("bShouldBeVisible"),
				TEXT("Level should be visible when loaded."))
			.Bool(TEXT("bShouldBlockOnLoad"),
				TEXT("Block game until level is loaded."))
			.Bool(TEXT("bDisableDistanceStreaming"),
				TEXT("Disable distance-based streaming."))
			.Number(TEXT("streamingDistance"),
				TEXT("Distance/radius for streaming volume "
					"(creates ALevelStreamingVolume)."))
			.StringEnum(TEXT("streamingUsage"), {
				TEXT("Loading"),
				TEXT("LoadingAndVisibility"),
				TEXT("VisibilityBlockingOnLoad"),
				TEXT("BlockingOnLoad"),
				TEXT("LoadingNotVisible")
			}, TEXT("Streaming volume usage mode (default: LoadingAndVisibility)."))
			.Bool(TEXT("createVolume"),
				TEXT("Create a streaming volume (true) or just report existing "
					"volumes (false). Default: true."))
			.Object(TEXT("boundsOrigin"), TEXT("Origin of level bounds."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("boundsExtent"), TEXT("Extent of level bounds."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Bool(TEXT("bAutoCalculateBounds"),
				TEXT("Auto-calculate bounds from content."))
			.Bool(TEXT("bEnableWorldPartition"),
				TEXT("Enable World Partition for level."))
			.String(TEXT("gridName"),
				TEXT("configure_grid_size: name of the World Partition runtime grid "
					"(default grid if omitted)."))
			.Number(TEXT("gridCellSize"),
				TEXT("World Partition grid cell size."))
			.Number(TEXT("loadingRange"),
				TEXT("Loading range for grid cells."))
			.Bool(TEXT("bBlockOnSlowStreaming"),
				TEXT("configure_grid_size: block on slow streaming for this grid."))
			.Number(TEXT("priority"),
				TEXT("configure_grid_size: grid priority. Also create_physics_volume/"
					"set_volume_properties (physics volume priority) and "
					"create_post_process_volume/add_post_process_volume (blend priority)."))
			.Bool(TEXT("createIfMissing"),
				TEXT("configure_grid_size: create the named grid if it doesn't exist "
					"(default: true)."))
			.String(TEXT("dataLayerName"), TEXT("Name of the data layer."))
			.Bool(TEXT("bIsInitiallyVisible"),
				TEXT("Data layer initially visible."))
			.Bool(TEXT("bIsInitiallyLoaded"),
				TEXT("Data layer initially loaded."))
			.Bool(TEXT("bIsPrivate"),
				TEXT("create_data_layer: mark the data layer private."))
			.StringEnum(TEXT("dataLayerType"), {
				TEXT("Runtime"),
				TEXT("Editor")
			}, TEXT("Type of data layer."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("actorPath"), TEXT("Path to actor."))
			.String(TEXT("hlodLayerName"), TEXT("Name of the HLOD layer."))
			.String(TEXT("hlodLayerPath"), TEXT("Path to HLOD layer."))
			.Bool(TEXT("bIsSpatiallyLoaded"),
				TEXT("HLOD is spatially loaded."))
			.Number(TEXT("cellSize"), TEXT("HLOD cell size."))
			.Number(TEXT("loadingDistance"), TEXT("HLOD loading distance."))
			.StringEnum(TEXT("layerType"), {
				TEXT("MeshMerge"),
				TEXT("Instancing"),
				TEXT("MeshSimplify"),
				TEXT("MeshApproximate")
			}, TEXT("configure_hlod_layer: HLOD generation method (default: MeshMerge). "
				"Legacy aliases SimplifiedMesh/ApproximatedMesh are also accepted."))
			.String(TEXT("volumeName"), TEXT("Name of the volume."))
			.Object(TEXT("volumeLocation"),
				TEXT("Location of the volume."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); "
				"create_*_volume actions."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Number(TEXT("sphereRadius"),
				TEXT("create_trigger_sphere: sphere radius."))
			.Number(TEXT("capsuleRadius"),
				TEXT("create_trigger_capsule: capsule radius."))
			.Number(TEXT("capsuleHalfHeight"),
				TEXT("create_trigger_capsule: capsule half height."))
			.ArrayOfObjects(TEXT("cullDistances"),
				TEXT("create_cull_distance_volume/add_cull_distance_volume: size/"
					"cullDistance pairs, e.g. [{\"size\": 100, \"cullDistance\": "
					"5000}]."))
			.Array(TEXT("bounds"),
				TEXT("Volume bounds as [minX, minY, minZ, maxX, maxY, maxZ]; "
					"set_volume_bounds alternative to volumeLocation + "
					"volumeExtent."),
				TEXT("number"))
			.Bool(TEXT("bPainCausing"),
				TEXT("create_pain_causing_volume/set_volume_properties: enable "
					"damage-over-time."))
			.Number(TEXT("damagePerSec"),
				TEXT("create_pain_causing_volume/set_volume_properties: damage "
					"per second."))
			.Bool(TEXT("bWaterVolume"),
				TEXT("create_physics_volume/add_physics_volume/"
					"set_volume_properties: treat as water."))
			.Number(TEXT("fluidFriction"),
				TEXT("create_physics_volume/add_physics_volume/"
					"set_volume_properties: fluid friction."))
			.Number(TEXT("terminalVelocity"),
				TEXT("create_physics_volume/add_physics_volume/"
					"set_volume_properties: terminal velocity."))
			.Bool(TEXT("bEnabled"),
				TEXT("create_audio_volume/create_reverb_volume/"
					"set_volume_properties: enable the volume's effect."))
			.Number(TEXT("reverbVolume"),
				TEXT("create_reverb_volume/set_volume_properties: reverb wet "
					"level (0-1)."))
			.Number(TEXT("fadeTime"),
				TEXT("create_reverb_volume/set_volume_properties: reverb fade "
					"time in seconds."))
			.Number(TEXT("blendRadius"),
				TEXT("create_post_process_volume/add_post_process_volume: "
					"blend radius."))
			.Number(TEXT("blendWeight"),
				TEXT("create_post_process_volume/add_post_process_volume: "
					"blend weight."))
			.Bool(TEXT("enabled"),
				TEXT("create_post_process_volume/add_post_process_volume: "
					"whether the volume is enabled."))
			.Bool(TEXT("bUnbound"),
				TEXT("create_post_process_volume/add_post_process_volume: "
					"affect the whole level regardless of extent."))
			.FreeformObject(TEXT("postProcessSettings"),
				TEXT("create_post_process_volume: post-process override key/values "
					"(bloomEnabled, exposureBias, vignetteIntensity, saturation, "
					"contrast, gamma)."))
			.String(TEXT("filter"),
				TEXT("get_volumes_info: filter volumes by actor label substring."))
			.String(TEXT("volumeType"),
				TEXT("get_volumes_info: filter volumes by class name substring."))
			.Number(TEXT("killZHeight"),
				TEXT("add_kill_z_volume: Z height for the kill volume."))
			.String(TEXT("nodeClass"), TEXT("Node class path."))
			.Object(TEXT("nodePosition"),
				TEXT("Position of node in graph."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.String(TEXT("nodeName"), TEXT("Name of the node."))
			.String(TEXT("sourceNodeName"), TEXT("Source node name."))
			.String(TEXT("sourcePinName"), TEXT("Name of the source pin."))
			.String(TEXT("targetNodeName"), TEXT("Target node name."))
			.String(TEXT("targetPinName"), TEXT("Name of the target pin."))
			.String(TEXT("levelInstanceName"),
				TEXT("Level instance name."))
			.String(TEXT("levelAssetPath"),
				TEXT("Path to the level asset for instancing."))
			.Object(TEXT("instanceLocation"),
				TEXT("Location of the level instance."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("instanceRotation"),
				TEXT("Rotation of the level instance."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("instanceScale"),
				TEXT("Scale of the level instance."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("packedLevelName"),
				TEXT("Name for the packed level actor."))
			.Bool(TEXT("bPackBlueprints"),
				TEXT("Include blueprints in packed level."))
			.Bool(TEXT("bPackStaticMeshes"),
				TEXT("Include static meshes in packed level."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageLevelStructure);
