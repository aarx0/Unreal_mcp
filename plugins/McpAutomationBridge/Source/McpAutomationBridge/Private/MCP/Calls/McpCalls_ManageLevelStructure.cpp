// LINT-TOOL: manage_level_structure
// LINT-SCHEMA-DERIVED
// manage_level_structure as FMcpCall classes — adopts schema-from-decls
// (docs/action-declarations.md). Each class AUTHORS its schema fragment in a
// S_<Suffix>() function; the published facade schema folds those fragments and
// GetDecl() derives the validation decl from the same fragment via
// McpDeriveDecl(), so schema and decl are one source and cannot drift. Run()
// delegates to the namespaced free handlers —
// McpHandlers::LevelStructure::HandleLevelStructure* (LevelStructureHandlers.cpp)
// for the 16 core actions and McpHandlers::LevelStructure::HandleVolume*
// (VolumeHandlers.cpp) for the 28 volume actions.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_LevelStructureHandlers.h"
#include "McpAutomationBridge_VolumeHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageLevelStructure
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl.
//
// Ported from this family's retired param arrays and re-verified against the
// handler bodies. The alias pairs volumeLocation/location and
// volumeExtent/extent (plus boxExtent on create_trigger_box) are one-of reads
// (VolumeHelpers::GetVolumeExtent / GetVectorFromPayloadAliases — first present
// spelling wins, default otherwise), so each is authored optional and the
// requirement is handler-enforced. set_volume_extent's volumeExtent/extent are
// likewise both optional with the at-least-one requirement handler-enforced.
// Five params the previous flat facade never advertised are surfaced here
// because the handlers do read them and the transport validator rejects any
// undeclared payload key: dataLayerAssetPath, location, extent, boxExtent, path.

static void S_CreateLevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelName"),
		TEXT("create_level: name of the new level asset (no path separators; "
			"created under levelPath, default /Game/Maps). "
			"configure_level_streaming/set_streaming_distance: streaming-level "
			"name or /Game/ path (substring match; a streaming reference is "
			"auto-created when the level exists on disk)."))
	 .String(TEXT("levelPath"), TEXT("Level asset path."))
	 .Bool(TEXT("bCreateWorldPartition"), TEXT("Create with World Partition enabled."))
	 .Bool(TEXT("bUseExternalActors"), TEXT("Enable One File Per Actor (OFPA/External Actors) for Data Layer "
		"compatibility. Automatically enabled when bCreateWorldPartition is true."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Bool(TEXT("loadAfterCreate"), TEXT("create_level: open the new level in the editor after creation."))
	 .Required({TEXT("levelName")});
}

static void S_CreateSublevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sublevelName"), TEXT("Name of the sublevel."))
	 .String(TEXT("sublevelPath"), TEXT("Level asset path."))
	 .String(TEXT("parentLevel"), TEXT("Parent level path."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("sublevelName")});
}

static void S_ConfigureLevelStreaming(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelName"),
		TEXT("create_level: name of the new level asset (no path separators; "
			"created under levelPath, default /Game/Maps). "
			"configure_level_streaming/set_streaming_distance: streaming-level "
			"name or /Game/ path (substring match; a streaming reference is "
			"auto-created when the level exists on disk)."))
	 .StringEnum(TEXT("streamingMethod"), {
		TEXT("Blueprint"), TEXT("AlwaysLoaded"), TEXT("Disabled")
	 }, TEXT("Level streaming method."))
	 .Bool(TEXT("bShouldBeVisible"), TEXT("Level should be visible when loaded."))
	 .Bool(TEXT("bShouldBlockOnLoad"), TEXT("Block game until level is loaded."))
	 .Bool(TEXT("bDisableDistanceStreaming"), TEXT("Disable distance-based streaming."))
	 .Required({TEXT("levelName")});
}

static void S_SetStreamingDistance(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelName"),
		TEXT("create_level: name of the new level asset (no path separators; "
			"created under levelPath, default /Game/Maps). "
			"configure_level_streaming/set_streaming_distance: streaming-level "
			"name or /Game/ path (substring match; a streaming reference is "
			"auto-created when the level exists on disk)."))
	 .Number(TEXT("streamingDistance"), TEXT("Distance/radius for streaming volume "
		"(creates ALevelStreamingVolume)."))
	 .StringEnum(TEXT("streamingUsage"), {
		TEXT("Loading"), TEXT("LoadingAndVisibility"), TEXT("VisibilityBlockingOnLoad"),
		TEXT("BlockingOnLoad"), TEXT("LoadingNotVisible")
	 }, TEXT("Streaming volume usage mode (default: LoadingAndVisibility)."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Bool(TEXT("createVolume"), TEXT("Create a streaming volume (true) or just report existing "
		"volumes (false). Default: true."))
	 .Required({TEXT("levelName")});
}


static void S_EnableWorldPartition(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("bEnableWorldPartition"), TEXT("Enable World Partition for level."));
}

static void S_ConfigureGridSize(FMcpSchemaBuilder& B)
{
	B.String(TEXT("gridName"), TEXT("configure_grid_size: name of the World Partition runtime grid "
		"(default grid if omitted)."))
	 .Integer(TEXT("gridCellSize"), TEXT("World Partition grid cell size."))
	 .Number(TEXT("loadingRange"), TEXT("Loading range for grid cells."))
	 .Bool(TEXT("bBlockOnSlowStreaming"), TEXT("configure_grid_size: block on slow streaming for this grid."))
	 .Number(TEXT("priority"), TEXT("configure_grid_size: grid priority. Also create_physics_volume/"
		"set_volume_properties (physics volume priority) and "
		"create_post_process_volume/add_post_process_volume (blend priority)."))
	 .Bool(TEXT("createIfMissing"), TEXT("configure_grid_size: create the named grid if it doesn't exist "
		"(default: true)."));
}

static void S_CreateDataLayer(FMcpSchemaBuilder& B)
{
	B.String(TEXT("dataLayerName"), TEXT("Name of the data layer."))
	 .String(TEXT("dataLayerAssetPath"), TEXT("create_data_layer: data layer asset directory "
		"(default: /Game/DataLayers)."))
	 .Bool(TEXT("bIsInitiallyVisible"), TEXT("Data layer initially visible."))
	 .Bool(TEXT("bIsInitiallyLoaded"), TEXT("Data layer initially loaded."))
	 .StringEnum(TEXT("dataLayerType"), {
		TEXT("Runtime"), TEXT("Editor")
	 }, TEXT("Type of data layer."))
	 .Bool(TEXT("bIsPrivate"), TEXT("create_data_layer: mark the data layer private."))
	 .Required({TEXT("dataLayerName")});
}

static void S_AssignActorToDataLayer(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("dataLayerName"), TEXT("Name of the data layer."))
	 .Required({TEXT("actorName"), TEXT("dataLayerName")});
}

static void S_ConfigureHlodLayer(FMcpSchemaBuilder& B)
{
	B.String(TEXT("hlodLayerName"), TEXT("Name of the HLOD layer."))
	 .String(TEXT("hlodLayerPath"), TEXT("Path to HLOD layer."))
	 .Bool(TEXT("bIsSpatiallyLoaded"), TEXT("HLOD is spatially loaded."))
	 .Number(TEXT("cellSize"), TEXT("HLOD cell size."))
	 .Number(TEXT("loadingDistance"), TEXT("HLOD loading distance."))
	 .StringEnum(TEXT("layerType"), {
		TEXT("MeshMerge"), TEXT("Instancing"), TEXT("MeshSimplify"), TEXT("MeshApproximate")
	 }, TEXT("configure_hlod_layer: HLOD generation method (default: MeshMerge). "
		"Legacy aliases SimplifiedMesh/ApproximatedMesh are also accepted."))
	 .Required({TEXT("hlodLayerName")});
}

static void S_CreateMinimapVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); });
}

static void S_OpenLevelBlueprint(FMcpSchemaBuilder&) {}

static void S_AddLevelBlueprintNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("nodeClass"), TEXT("Node class path."))
	 .String(TEXT("nodeName"), TEXT("Name of the node."))
	 .Object(TEXT("nodePosition"), TEXT("Position of node in graph."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
	 .Required({TEXT("nodeClass")});
}

static void S_ConnectLevelBlueprintNodes(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sourceNodeName"), TEXT("Source node name."))
	 .String(TEXT("sourcePinName"), TEXT("Name of the source pin."))
	 .String(TEXT("targetNodeName"), TEXT("Target node name."))
	 .String(TEXT("targetPinName"), TEXT("Name of the target pin."))
	 .Required({TEXT("sourceNodeName"), TEXT("targetNodeName")});
}

static void S_CreateLevelInstance(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelInstanceName"), TEXT("Level instance name."))
	 .String(TEXT("levelAssetPath"), TEXT("Path to the level asset for instancing."))
	 .Object(TEXT("instanceLocation"), TEXT("Location of the level instance."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("instanceRotation"), TEXT("Rotation of the level instance."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("instanceScale"), TEXT("Scale of the level instance."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("levelAssetPath")});
}

static void S_CreatePackedLevelActor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("packedLevelName"), TEXT("Name for the packed level actor."))
	 .String(TEXT("levelAssetPath"), TEXT("Path to the level asset for instancing."))
	 .Object(TEXT("instanceLocation"), TEXT("Location of the level instance."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("instanceRotation"), TEXT("Rotation of the level instance."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Bool(TEXT("bPackBlueprints"), TEXT("Include blueprints in packed level."))
	 .Bool(TEXT("bPackStaticMeshes"), TEXT("Include static meshes in packed level."));
}

static void S_GetInfo(FMcpSchemaBuilder&) {}

static void S_CreateTriggerVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); });
}

static void S_CreateTriggerBox(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("boxExtent"), TEXT("create_trigger_box: box extent alias for volumeExtent."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); });
}

static void S_CreateTriggerSphere(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Number(TEXT("sphereRadius"), TEXT("create_trigger_sphere: sphere radius."));
}

static void S_CreateTriggerCapsule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Number(TEXT("capsuleRadius"), TEXT("create_trigger_capsule: capsule radius."))
	 .Number(TEXT("capsuleHalfHeight"), TEXT("create_trigger_capsule: capsule half height."));
}

static void S_CreateBlockingVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); });
}

static void S_CreateKillZVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); });
}

static void S_CreatePainCausingVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Bool(TEXT("bPainCausing"), TEXT("create_pain_causing_volume/set_volume_properties: enable "
		"damage-over-time."))
	 .Number(TEXT("damagePerSec"), TEXT("create_pain_causing_volume/set_volume_properties: damage "
		"per second."));
}

static void S_CreatePhysicsVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Bool(TEXT("bWaterVolume"), TEXT("create_physics_volume/add_physics_volume/"
		"set_volume_properties: treat as water."))
	 .Number(TEXT("fluidFriction"), TEXT("create_physics_volume/add_physics_volume/"
		"set_volume_properties: fluid friction."))
	 .Number(TEXT("terminalVelocity"), TEXT("create_physics_volume/add_physics_volume/"
		"set_volume_properties: terminal velocity."))
	 .Number(TEXT("priority"), TEXT("configure_grid_size: grid priority. Also create_physics_volume/"
		"set_volume_properties (physics volume priority) and "
		"create_post_process_volume/add_post_process_volume (blend priority)."));
}

static void S_CreateAudioVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Bool(TEXT("bEnabled"), TEXT("create_audio_volume/create_reverb_volume/"
		"set_volume_properties: enable the volume's effect."));
}

static void S_CreateReverbVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Bool(TEXT("bEnabled"), TEXT("create_audio_volume/create_reverb_volume/"
		"set_volume_properties: enable the volume's effect."))
	 .Number(TEXT("reverbVolume"), TEXT("create_reverb_volume/set_volume_properties: reverb wet "
		"level (0-1)."))
	 .Number(TEXT("fadeTime"), TEXT("create_reverb_volume/set_volume_properties: reverb fade "
		"time in seconds."));
}

static void S_CreatePostProcessVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Number(TEXT("priority"), TEXT("configure_grid_size: grid priority. Also create_physics_volume/"
		"set_volume_properties (physics volume priority) and "
		"create_post_process_volume/add_post_process_volume (blend priority)."))
	 .Number(TEXT("blendRadius"), TEXT("create_post_process_volume/add_post_process_volume: "
		"blend radius."))
	 .Number(TEXT("blendWeight"), TEXT("create_post_process_volume/add_post_process_volume: "
		"blend weight."))
	 .Bool(TEXT("enabled"), TEXT("create_post_process_volume/add_post_process_volume: "
		"whether the volume is enabled."))
	 .Bool(TEXT("bUnbound"), TEXT("create_post_process_volume/add_post_process_volume: "
		"affect the whole level regardless of extent."))
	 .Object(TEXT("postProcessSettings"), TEXT("create_post_process_volume: post-process override "
		"key/values (bloomEnabled, exposureBias, vignetteIntensity, saturation, "
		"contrast, gamma)."));
}

static void S_CreateCullDistanceVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .ArrayOfObjects(TEXT("cullDistances"), TEXT("create_cull_distance_volume/add_cull_distance_volume: "
		"size/cullDistance pairs, e.g. [{\"size\": 100, \"cullDistance\": 5000}]."));
}

static void S_CreatePrecomputedVisibilityVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); });
}

static void S_CreateLightmassImportanceVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); });
}

static void S_CreateNavMeshBoundsVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); });
}

static void S_CreateNavModifierVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); });
}

static void S_CreateCameraBlockingVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll); create_*_volume actions."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); });
}

static void S_SetVolumeExtent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("volumeName")});
}

static void S_SetVolumeProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Bool(TEXT("bWaterVolume"), TEXT("create_physics_volume/add_physics_volume/"
		"set_volume_properties: treat as water."))
	 .Number(TEXT("fluidFriction"), TEXT("create_physics_volume/add_physics_volume/"
		"set_volume_properties: fluid friction."))
	 .Number(TEXT("terminalVelocity"), TEXT("create_physics_volume/add_physics_volume/"
		"set_volume_properties: terminal velocity."))
	 .Number(TEXT("priority"), TEXT("configure_grid_size: grid priority. Also create_physics_volume/"
		"set_volume_properties (physics volume priority) and "
		"create_post_process_volume/add_post_process_volume (blend priority)."))
	 .Bool(TEXT("bPainCausing"), TEXT("create_pain_causing_volume/set_volume_properties: enable "
		"damage-over-time."))
	 .Number(TEXT("damagePerSec"), TEXT("create_pain_causing_volume/set_volume_properties: damage "
		"per second."))
	 .Bool(TEXT("bEnabled"), TEXT("create_audio_volume/create_reverb_volume/"
		"set_volume_properties: enable the volume's effect."))
	 .Number(TEXT("reverbVolume"), TEXT("create_reverb_volume/set_volume_properties: reverb wet "
		"level (0-1)."))
	 .Number(TEXT("fadeTime"), TEXT("create_reverb_volume/set_volume_properties: reverb fade "
		"time in seconds."))
	 .Required({TEXT("volumeName")});
}

static void S_SetVolumeBounds(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Array(TEXT("bounds"), TEXT("Volume bounds as [minX, minY, minZ, maxX, maxY, maxZ]; "
		"set_volume_bounds alternative to volumeLocation + volumeExtent."), TEXT("number"))
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("volumeLocation"), TEXT("Location of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Object(TEXT("location"), TEXT("Volume location alias for volumeLocation "
		"(create_*_volume actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")).Required({TEXT("x"), TEXT("y"), TEXT("z")}); })
	 .Required({TEXT("volumeName")});
}

static void S_RemoveVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("volumeName"), TEXT("Name of the volume."))
	 .Required({TEXT("volumeName")});
}

static void S_GetVolumesInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("path"), TEXT("get_volumes_info: unused — the handler rejects a path param "
		"(this action does not scope by path)."))
	 .String(TEXT("filter"), TEXT("get_volumes_info: filter volumes by actor label substring."))
	 .String(TEXT("volumeType"), TEXT("get_volumes_info: filter volumes by class name substring."));
}

static void S_AddTriggerVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorPath"), TEXT("Path to actor."))
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("actorPath")});
}

static void S_AddBlockingVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorPath"), TEXT("Path to actor."))
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("actorPath")});
}

static void S_AddKillZVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorPath"), TEXT("Path to actor."))
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Number(TEXT("killZHeight"), TEXT("add_kill_z_volume: Z height for the kill volume."))
	 .Required({TEXT("actorPath")});
}

static void S_AddPhysicsVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorPath"), TEXT("Path to actor."))
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Bool(TEXT("bWaterVolume"), TEXT("create_physics_volume/add_physics_volume/"
		"set_volume_properties: treat as water."))
	 .Number(TEXT("fluidFriction"), TEXT("create_physics_volume/add_physics_volume/"
		"set_volume_properties: fluid friction."))
	 .Number(TEXT("terminalVelocity"), TEXT("create_physics_volume/add_physics_volume/"
		"set_volume_properties: terminal velocity."))
	 .Required({TEXT("actorPath")});
}

static void S_AddCullDistanceVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorPath"), TEXT("Path to actor."))
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .ArrayOfObjects(TEXT("cullDistances"), TEXT("create_cull_distance_volume/add_cull_distance_volume: "
		"size/cullDistance pairs, e.g. [{\"size\": 100, \"cullDistance\": 5000}]."))
	 .Required({TEXT("actorPath")});
}

static void S_AddPostProcessVolume(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorPath"), TEXT("Path to actor."))
	 .Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("extent"), TEXT("Volume extent alias for volumeExtent "
		"(create_*_volume / set_volume_* actions; first present wins)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Number(TEXT("priority"), TEXT("configure_grid_size: grid priority. Also create_physics_volume/"
		"set_volume_properties (physics volume priority) and "
		"create_post_process_volume/add_post_process_volume (blend priority)."))
	 .Number(TEXT("blendRadius"), TEXT("create_post_process_volume/add_post_process_volume: "
		"blend radius."))
	 .Number(TEXT("blendWeight"), TEXT("create_post_process_volume/add_post_process_volume: "
		"blend weight."))
	 .Bool(TEXT("enabled"), TEXT("create_post_process_volume/add_post_process_volume: "
		"whether the volume is enabled."))
	 .Bool(TEXT("bUnbound"), TEXT("create_post_process_volume/add_post_process_volume: "
		"affect the whole level regardless of extent."))
	 .Required({TEXT("actorPath")});
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are authored per action: RequiresEditor is baked into every row (both
// implementation TUs are whole-editor-gated, and the members answer the retired
// chains' editor-build stubs in non-editor builds — the two post-process members
// also answer the UNSUPPORTED_VERSION stub when MCP_HAS_POSTPROCESS_VOLUME is 0).
// Mutating on all writers — including open_level_blueprint, which lazily creates
// the Level Script Blueprint — the only readers are get_info and get_volumes_info.

#define MCP_LS_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                     \
class FMcpCall_ManageLevelStructure_##ClassSuffix final : public FMcpCall                  \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_level_structure"),      \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),           \
			&S_##ClassSuffix);                                                            \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return HandlerFn(S, RequestId, Payload, Socket);                                  \
	}                                                                                     \
};

// Levels
MCP_LS_CALL(CreateLevel, "create_level", McpHandlers::LevelStructure::HandleLevelStructureCreateLevel, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateSublevel, "create_sublevel", McpHandlers::LevelStructure::HandleLevelStructureCreateSublevel, EMcpCallFlags::Mutating)
MCP_LS_CALL(ConfigureLevelStreaming, "configure_level_streaming", McpHandlers::LevelStructure::HandleLevelStructureConfigureLevelStreaming, EMcpCallFlags::Mutating)
MCP_LS_CALL(SetStreamingDistance, "set_streaming_distance", McpHandlers::LevelStructure::HandleLevelStructureSetStreamingDistance, EMcpCallFlags::Mutating)

// World Partition
MCP_LS_CALL(EnableWorldPartition, "enable_world_partition", McpHandlers::LevelStructure::HandleLevelStructureEnableWorldPartition, EMcpCallFlags::Mutating)
MCP_LS_CALL(ConfigureGridSize, "configure_grid_size", McpHandlers::LevelStructure::HandleLevelStructureConfigureGridSize, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateDataLayer, "create_data_layer", McpHandlers::LevelStructure::HandleLevelStructureCreateDataLayer, EMcpCallFlags::Mutating)
MCP_LS_CALL(AssignActorToDataLayer, "assign_actor_to_data_layer", McpHandlers::LevelStructure::HandleLevelStructureAssignActorToDataLayer, EMcpCallFlags::Mutating)
MCP_LS_CALL(ConfigureHlodLayer, "configure_hlod_layer", McpHandlers::LevelStructure::HandleLevelStructureConfigureHlodLayer, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateMinimapVolume, "create_minimap_volume", McpHandlers::LevelStructure::HandleLevelStructureCreateMinimapVolume, EMcpCallFlags::Mutating)

// Level Blueprint
MCP_LS_CALL(OpenLevelBlueprint, "open_level_blueprint", McpHandlers::LevelStructure::HandleLevelStructureOpenLevelBlueprint, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddLevelBlueprintNode, "add_level_blueprint_node", McpHandlers::LevelStructure::HandleLevelStructureAddLevelBlueprintNode, EMcpCallFlags::Mutating)
MCP_LS_CALL(ConnectLevelBlueprintNodes, "connect_level_blueprint_nodes", McpHandlers::LevelStructure::HandleLevelStructureConnectLevelBlueprintNodes, EMcpCallFlags::Mutating)

// Level Instances
MCP_LS_CALL(CreateLevelInstance, "create_level_instance", McpHandlers::LevelStructure::HandleLevelStructureCreateLevelInstance, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreatePackedLevelActor, "create_packed_level_actor", McpHandlers::LevelStructure::HandleLevelStructureCreatePackedLevelActor, EMcpCallFlags::Mutating)

// Utility
MCP_LS_CALL(GetInfo, "get_info", McpHandlers::LevelStructure::HandleLevelStructureGetInfo, EMcpCallFlags::None)

// Trigger Volumes
MCP_LS_CALL(CreateTriggerVolume, "create_trigger_volume", McpHandlers::LevelStructure::HandleVolumeCreateTriggerVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateTriggerBox, "create_trigger_box", McpHandlers::LevelStructure::HandleVolumeCreateTriggerBox, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateTriggerSphere, "create_trigger_sphere", McpHandlers::LevelStructure::HandleVolumeCreateTriggerSphere, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateTriggerCapsule, "create_trigger_capsule", McpHandlers::LevelStructure::HandleVolumeCreateTriggerCapsule, EMcpCallFlags::Mutating)

// Gameplay Volumes
MCP_LS_CALL(CreateBlockingVolume, "create_blocking_volume", McpHandlers::LevelStructure::HandleVolumeCreateBlockingVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateKillZVolume, "create_kill_z_volume", McpHandlers::LevelStructure::HandleVolumeCreateKillZVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreatePainCausingVolume, "create_pain_causing_volume", McpHandlers::LevelStructure::HandleVolumeCreatePainCausingVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreatePhysicsVolume, "create_physics_volume", McpHandlers::LevelStructure::HandleVolumeCreatePhysicsVolume, EMcpCallFlags::Mutating)

// Audio Volumes
MCP_LS_CALL(CreateAudioVolume, "create_audio_volume", McpHandlers::LevelStructure::HandleVolumeCreateAudioVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateReverbVolume, "create_reverb_volume", McpHandlers::LevelStructure::HandleVolumeCreateReverbVolume, EMcpCallFlags::Mutating)

// Rendering Volumes
MCP_LS_CALL(CreatePostProcessVolume, "create_post_process_volume", McpHandlers::LevelStructure::HandleVolumeCreatePostProcessVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateCullDistanceVolume, "create_cull_distance_volume", McpHandlers::LevelStructure::HandleVolumeCreateCullDistanceVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreatePrecomputedVisibilityVolume, "create_precomputed_visibility_volume", McpHandlers::LevelStructure::HandleVolumeCreatePrecomputedVisibilityVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateLightmassImportanceVolume, "create_lightmass_importance_volume", McpHandlers::LevelStructure::HandleVolumeCreateLightmassImportanceVolume, EMcpCallFlags::Mutating)

// Navigation Volumes
MCP_LS_CALL(CreateNavMeshBoundsVolume, "create_nav_mesh_bounds_volume", McpHandlers::LevelStructure::HandleVolumeCreateNavMeshBoundsVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateNavModifierVolume, "create_nav_modifier_volume", McpHandlers::LevelStructure::HandleVolumeCreateNavModifierVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateCameraBlockingVolume, "create_camera_blocking_volume", McpHandlers::LevelStructure::HandleVolumeCreateCameraBlockingVolume, EMcpCallFlags::Mutating)

// Volume Configuration
MCP_LS_CALL(SetVolumeExtent, "set_volume_extent", McpHandlers::LevelStructure::HandleVolumeSetVolumeExtent, EMcpCallFlags::Mutating)
MCP_LS_CALL(SetVolumeProperties, "set_volume_properties", McpHandlers::LevelStructure::HandleVolumeSetVolumeProperties, EMcpCallFlags::Mutating)
MCP_LS_CALL(SetVolumeBounds, "set_volume_bounds", McpHandlers::LevelStructure::HandleVolumeSetVolumeBounds, EMcpCallFlags::Mutating)

// Volume Removal
MCP_LS_CALL(RemoveVolume, "remove_volume", McpHandlers::LevelStructure::HandleVolumeRemoveVolume, EMcpCallFlags::Mutating)

// Utility
MCP_LS_CALL(GetVolumesInfo, "get_volumes_info", McpHandlers::LevelStructure::HandleVolumeGetVolumesInfo, EMcpCallFlags::None)

// Add Volume To Actor handlers
MCP_LS_CALL(AddTriggerVolume, "add_trigger_volume", McpHandlers::LevelStructure::HandleVolumeAddTriggerVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddBlockingVolume, "add_blocking_volume", McpHandlers::LevelStructure::HandleVolumeAddBlockingVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddKillZVolume, "add_kill_z_volume", McpHandlers::LevelStructure::HandleVolumeAddKillZVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddPhysicsVolume, "add_physics_volume", McpHandlers::LevelStructure::HandleVolumeAddPhysicsVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddCullDistanceVolume, "add_cull_distance_volume", McpHandlers::LevelStructure::HandleVolumeAddCullDistanceVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddPostProcessVolume, "add_post_process_volume", McpHandlers::LevelStructure::HandleVolumeAddPostProcessVolume, EMcpCallFlags::Mutating)

#undef MCP_LS_CALL

} // namespace McpCalls::ManageLevelStructure

void McpRegisterManageLevelStructureCalls()
{
	using namespace McpCalls::ManageLevelStructure;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateLevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateSublevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_ConfigureLevelStreaming>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_SetStreamingDistance>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_EnableWorldPartition>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_ConfigureGridSize>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateDataLayer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_AssignActorToDataLayer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_ConfigureHlodLayer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateMinimapVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_OpenLevelBlueprint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_AddLevelBlueprintNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_ConnectLevelBlueprintNodes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateLevelInstance>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreatePackedLevelActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_GetInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateTriggerVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateTriggerBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateTriggerSphere>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateTriggerCapsule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateBlockingVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateKillZVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreatePainCausingVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreatePhysicsVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateAudioVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateReverbVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreatePostProcessVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateCullDistanceVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreatePrecomputedVisibilityVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateLightmassImportanceVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateNavMeshBoundsVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateNavModifierVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_CreateCameraBlockingVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_SetVolumeExtent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_SetVolumeProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_SetVolumeBounds>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_RemoveVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_GetVolumesInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_AddTriggerVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_AddBlockingVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_AddKillZVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_AddPhysicsVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_AddCullDistanceVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_AddPostProcessVolume>());
}
