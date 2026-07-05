// LINT-TOOL: manage_level_structure
// manage_level_structure as FMcpCall classes — thirteenth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers — HandleLevelStructure* (LevelStructureHandlers.cpp) for
// the 17 core actions, HandleVolume* (VolumeHandlers.cpp) for the 28 volume
// actions — until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageLevelStructure
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations
// (McpDecl_ManageLevelStructure.h) and re-verified against the free-function
// bodies both retired dispatchers delegated to. One decl fix shipped at
// classing: the shim's set_volume_extent row declared BOTH volumeExtent AND
// extent required, but the handler resolves them as a one-of alias pair
// (VolumeHelpers::GetVolumeExtent — first present spelling wins) and rejects
// only when neither is present, so requiring both false-rejected every
// payload the handler serves; both are optional now with the at-least-one
// requirement handler-enforced. The volumeLocation/location and
// volumeExtent/extent alias pairs the other volume rows carry as optional
// are handler-true (same first-present-wins read, defaults otherwise).

inline const FMcpParamDecl P_CreateLevel[] = { { TEXT("levelName"), EMcpParamKind::String, true }, { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("bCreateWorldPartition"), EMcpParamKind::Bool, false }, { TEXT("bUseExternalActors"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("loadAfterCreate"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateSublevel[] = { { TEXT("sublevelName"), EMcpParamKind::String, true }, { TEXT("sublevelPath"), EMcpParamKind::String, false }, { TEXT("parentLevel"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureLevelStreaming[] = { { TEXT("levelName"), EMcpParamKind::String, true }, { TEXT("streamingMethod"), EMcpParamKind::String, false }, { TEXT("bShouldBeVisible"), EMcpParamKind::Bool, false }, { TEXT("bShouldBlockOnLoad"), EMcpParamKind::Bool, false }, { TEXT("bDisableDistanceStreaming"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetStreamingDistance[] = { { TEXT("levelName"), EMcpParamKind::String, true }, { TEXT("streamingDistance"), EMcpParamKind::Number, false }, { TEXT("streamingUsage"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("createVolume"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureLevelBounds[] = { { TEXT("bAutoCalculateBounds"), EMcpParamKind::Bool, false }, { TEXT("boundsOrigin"), EMcpParamKind::Object, false }, { TEXT("boundsExtent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_EnableWorldPartition[] = { { TEXT("bEnableWorldPartition"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureGridSize[] = { { TEXT("gridName"), EMcpParamKind::String, false }, { TEXT("gridCellSize"), EMcpParamKind::Number, false }, { TEXT("loadingRange"), EMcpParamKind::Number, false }, { TEXT("bBlockOnSlowStreaming"), EMcpParamKind::Bool, false }, { TEXT("priority"), EMcpParamKind::Number, false }, { TEXT("createIfMissing"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateDataLayer[] = { { TEXT("dataLayerName"), EMcpParamKind::String, true }, { TEXT("dataLayerAssetPath"), EMcpParamKind::String, false }, { TEXT("bIsInitiallyVisible"), EMcpParamKind::Bool, false }, { TEXT("bIsInitiallyLoaded"), EMcpParamKind::Bool, false }, { TEXT("dataLayerType"), EMcpParamKind::String, false }, { TEXT("bIsPrivate"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AssignActorToDataLayer[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("dataLayerName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureHlodLayer[] = { { TEXT("hlodLayerName"), EMcpParamKind::String, true }, { TEXT("hlodLayerPath"), EMcpParamKind::String, false }, { TEXT("bIsSpatiallyLoaded"), EMcpParamKind::Bool, false }, { TEXT("cellSize"), EMcpParamKind::Number, false }, { TEXT("loadingDistance"), EMcpParamKind::Number, false }, { TEXT("layerType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateMinimapVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_AddLevelBlueprintNode[] = { { TEXT("nodeClass"), EMcpParamKind::String, true }, { TEXT("nodeName"), EMcpParamKind::String, false }, { TEXT("nodePosition"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ConnectLevelBlueprintNodes[] = { { TEXT("sourceNodeName"), EMcpParamKind::String, true }, { TEXT("sourcePinName"), EMcpParamKind::String, false }, { TEXT("targetNodeName"), EMcpParamKind::String, true }, { TEXT("targetPinName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateLevelInstance[] = { { TEXT("levelInstanceName"), EMcpParamKind::String, false }, { TEXT("levelAssetPath"), EMcpParamKind::String, true }, { TEXT("instanceLocation"), EMcpParamKind::Object, false }, { TEXT("instanceRotation"), EMcpParamKind::Object, false }, { TEXT("instanceScale"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreatePackedLevelActor[] = { { TEXT("packedLevelName"), EMcpParamKind::String, false }, { TEXT("levelAssetPath"), EMcpParamKind::String, false }, { TEXT("instanceLocation"), EMcpParamKind::Object, false }, { TEXT("instanceRotation"), EMcpParamKind::Object, false }, { TEXT("bPackBlueprints"), EMcpParamKind::Bool, false }, { TEXT("bPackStaticMeshes"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateTriggerVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateTriggerBox[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("boxExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateTriggerSphere[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("sphereRadius"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateTriggerCapsule[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("capsuleRadius"), EMcpParamKind::Number, false }, { TEXT("capsuleHalfHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateBlockingVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateKillZVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreatePainCausingVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false }, { TEXT("bPainCausing"), EMcpParamKind::Bool, false }, { TEXT("damagePerSec"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreatePhysicsVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false }, { TEXT("bWaterVolume"), EMcpParamKind::Bool, false }, { TEXT("fluidFriction"), EMcpParamKind::Number, false }, { TEXT("terminalVelocity"), EMcpParamKind::Number, false }, { TEXT("priority"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateAudioVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false }, { TEXT("bEnabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateReverbVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false }, { TEXT("bEnabled"), EMcpParamKind::Bool, false }, { TEXT("reverbVolume"), EMcpParamKind::Number, false }, { TEXT("fadeTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreatePostProcessVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false }, { TEXT("priority"), EMcpParamKind::Number, false }, { TEXT("blendRadius"), EMcpParamKind::Number, false }, { TEXT("blendWeight"), EMcpParamKind::Number, false }, { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("bUnbound"), EMcpParamKind::Bool, false }, { TEXT("postProcessSettings"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateCullDistanceVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false }, { TEXT("cullDistances"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_CreatePrecomputedVisibilityVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateLightmassImportanceVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateNavMeshBoundsVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateNavModifierVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateCameraBlockingVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_SetVolumeExtent[] = { { TEXT("volumeName"), EMcpParamKind::String, true }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_SetVolumeProperties[] = { { TEXT("volumeName"), EMcpParamKind::String, true }, { TEXT("bWaterVolume"), EMcpParamKind::Bool, false }, { TEXT("fluidFriction"), EMcpParamKind::Number, false }, { TEXT("terminalVelocity"), EMcpParamKind::Number, false }, { TEXT("priority"), EMcpParamKind::Number, false }, { TEXT("bPainCausing"), EMcpParamKind::Bool, false }, { TEXT("damagePerSec"), EMcpParamKind::Number, false }, { TEXT("bEnabled"), EMcpParamKind::Bool, false }, { TEXT("reverbVolume"), EMcpParamKind::Number, false }, { TEXT("fadeTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetVolumeBounds[] = { { TEXT("volumeName"), EMcpParamKind::String, true }, { TEXT("bounds"), EMcpParamKind::Array, false }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false }, { TEXT("volumeLocation"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_RemoveVolume[] = { { TEXT("volumeName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_GetVolumesInfo[] = { { TEXT("path"), EMcpParamKind::String, false }, { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("volumeType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddTriggerVolume[] = { { TEXT("actorPath"), EMcpParamKind::String, true }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_AddBlockingVolume[] = { { TEXT("actorPath"), EMcpParamKind::String, true }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_AddKillZVolume[] = { { TEXT("actorPath"), EMcpParamKind::String, true }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false }, { TEXT("killZHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddPhysicsVolume[] = { { TEXT("actorPath"), EMcpParamKind::String, true }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false }, { TEXT("bWaterVolume"), EMcpParamKind::Bool, false }, { TEXT("fluidFriction"), EMcpParamKind::Number, false }, { TEXT("terminalVelocity"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddCullDistanceVolume[] = { { TEXT("actorPath"), EMcpParamKind::String, true }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false }, { TEXT("cullDistances"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_AddPostProcessVolume[] = { { TEXT("actorPath"), EMcpParamKind::String, true }, { TEXT("volumeExtent"), EMcpParamKind::Object, false }, { TEXT("extent"), EMcpParamKind::Object, false }, { TEXT("priority"), EMcpParamKind::Number, false }, { TEXT("blendRadius"), EMcpParamKind::Number, false }, { TEXT("blendWeight"), EMcpParamKind::Number, false }, { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("bUnbound"), EMcpParamKind::Bool, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row: both implementation TUs are
// whole-editor-gated, and the members answer the retired chains' editor-build
// stubs in non-editor builds (the two post-process members also answer the
// UNSUPPORTED_VERSION stub when MCP_HAS_POSTPROCESS_VOLUME is 0). Mutating on
// all writers — including open_level_blueprint, which lazily creates the
// Level Script Blueprint — the only readers are get_level_structure_info
// and get_volumes_info.

#define MCP_LS_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)            \
class FMcpCall_ManageLevelStructure_##ClassSuffix final : public FMcpCall                      \
{                                                                                              \
	const FMcpCallDecl& GetDecl() const override                                               \
	{                                                                                          \
		static const FMcpCallDecl Decl{ TEXT("manage_level_structure"), TEXT(ActionLiteral),   \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                       \
		return Decl;                                                                           \
	}                                                                                          \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                       \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override       \
	{                                                                                          \
		return S.HandlerFn(RequestId, Payload, Socket);                                        \
	}                                                                                          \
};

// Levels
MCP_LS_CALL(CreateLevel, "create_level", P_CreateLevel, HandleLevelStructureCreateLevel, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateSublevel, "create_sublevel", P_CreateSublevel, HandleLevelStructureCreateSublevel, EMcpCallFlags::Mutating)
MCP_LS_CALL(ConfigureLevelStreaming, "configure_level_streaming", P_ConfigureLevelStreaming, HandleLevelStructureConfigureLevelStreaming, EMcpCallFlags::Mutating)
MCP_LS_CALL(SetStreamingDistance, "set_streaming_distance", P_SetStreamingDistance, HandleLevelStructureSetStreamingDistance, EMcpCallFlags::Mutating)
MCP_LS_CALL(ConfigureLevelBounds, "configure_level_bounds", P_ConfigureLevelBounds, HandleLevelStructureConfigureLevelBounds, EMcpCallFlags::Mutating)

// World Partition
MCP_LS_CALL(EnableWorldPartition, "enable_world_partition", P_EnableWorldPartition, HandleLevelStructureEnableWorldPartition, EMcpCallFlags::Mutating)
MCP_LS_CALL(ConfigureGridSize, "configure_grid_size", P_ConfigureGridSize, HandleLevelStructureConfigureGridSize, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateDataLayer, "create_data_layer", P_CreateDataLayer, HandleLevelStructureCreateDataLayer, EMcpCallFlags::Mutating)
MCP_LS_CALL(AssignActorToDataLayer, "assign_actor_to_data_layer", P_AssignActorToDataLayer, HandleLevelStructureAssignActorToDataLayer, EMcpCallFlags::Mutating)
MCP_LS_CALL(ConfigureHlodLayer, "configure_hlod_layer", P_ConfigureHlodLayer, HandleLevelStructureConfigureHlodLayer, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateMinimapVolume, "create_minimap_volume", P_CreateMinimapVolume, HandleLevelStructureCreateMinimapVolume, EMcpCallFlags::Mutating)

// Level Blueprint
MCP_LS_CALL(OpenLevelBlueprint, "open_level_blueprint", {}, HandleLevelStructureOpenLevelBlueprint, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddLevelBlueprintNode, "add_level_blueprint_node", P_AddLevelBlueprintNode, HandleLevelStructureAddLevelBlueprintNode, EMcpCallFlags::Mutating)
MCP_LS_CALL(ConnectLevelBlueprintNodes, "connect_level_blueprint_nodes", P_ConnectLevelBlueprintNodes, HandleLevelStructureConnectLevelBlueprintNodes, EMcpCallFlags::Mutating)

// Level Instances
MCP_LS_CALL(CreateLevelInstance, "create_level_instance", P_CreateLevelInstance, HandleLevelStructureCreateLevelInstance, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreatePackedLevelActor, "create_packed_level_actor", P_CreatePackedLevelActor, HandleLevelStructureCreatePackedLevelActor, EMcpCallFlags::Mutating)

// Utility
MCP_LS_CALL(GetInfo, "get_level_structure_info", {}, HandleLevelStructureGetInfo, EMcpCallFlags::None)

// Trigger Volumes
MCP_LS_CALL(CreateTriggerVolume, "create_trigger_volume", P_CreateTriggerVolume, HandleVolumeCreateTriggerVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateTriggerBox, "create_trigger_box", P_CreateTriggerBox, HandleVolumeCreateTriggerBox, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateTriggerSphere, "create_trigger_sphere", P_CreateTriggerSphere, HandleVolumeCreateTriggerSphere, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateTriggerCapsule, "create_trigger_capsule", P_CreateTriggerCapsule, HandleVolumeCreateTriggerCapsule, EMcpCallFlags::Mutating)

// Gameplay Volumes
MCP_LS_CALL(CreateBlockingVolume, "create_blocking_volume", P_CreateBlockingVolume, HandleVolumeCreateBlockingVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateKillZVolume, "create_kill_z_volume", P_CreateKillZVolume, HandleVolumeCreateKillZVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreatePainCausingVolume, "create_pain_causing_volume", P_CreatePainCausingVolume, HandleVolumeCreatePainCausingVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreatePhysicsVolume, "create_physics_volume", P_CreatePhysicsVolume, HandleVolumeCreatePhysicsVolume, EMcpCallFlags::Mutating)

// Audio Volumes
MCP_LS_CALL(CreateAudioVolume, "create_audio_volume", P_CreateAudioVolume, HandleVolumeCreateAudioVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateReverbVolume, "create_reverb_volume", P_CreateReverbVolume, HandleVolumeCreateReverbVolume, EMcpCallFlags::Mutating)

// Rendering Volumes
MCP_LS_CALL(CreatePostProcessVolume, "create_post_process_volume", P_CreatePostProcessVolume, HandleVolumeCreatePostProcessVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateCullDistanceVolume, "create_cull_distance_volume", P_CreateCullDistanceVolume, HandleVolumeCreateCullDistanceVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreatePrecomputedVisibilityVolume, "create_precomputed_visibility_volume", P_CreatePrecomputedVisibilityVolume, HandleVolumeCreatePrecomputedVisibilityVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateLightmassImportanceVolume, "create_lightmass_importance_volume", P_CreateLightmassImportanceVolume, HandleVolumeCreateLightmassImportanceVolume, EMcpCallFlags::Mutating)

// Navigation Volumes
MCP_LS_CALL(CreateNavMeshBoundsVolume, "create_nav_mesh_bounds_volume", P_CreateNavMeshBoundsVolume, HandleVolumeCreateNavMeshBoundsVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateNavModifierVolume, "create_nav_modifier_volume", P_CreateNavModifierVolume, HandleVolumeCreateNavModifierVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(CreateCameraBlockingVolume, "create_camera_blocking_volume", P_CreateCameraBlockingVolume, HandleVolumeCreateCameraBlockingVolume, EMcpCallFlags::Mutating)

// Volume Configuration
MCP_LS_CALL(SetVolumeExtent, "set_volume_extent", P_SetVolumeExtent, HandleVolumeSetVolumeExtent, EMcpCallFlags::Mutating)
MCP_LS_CALL(SetVolumeProperties, "set_volume_properties", P_SetVolumeProperties, HandleVolumeSetVolumeProperties, EMcpCallFlags::Mutating)
MCP_LS_CALL(SetVolumeBounds, "set_volume_bounds", P_SetVolumeBounds, HandleVolumeSetVolumeBounds, EMcpCallFlags::Mutating)

// Volume Removal
MCP_LS_CALL(RemoveVolume, "remove_volume", P_RemoveVolume, HandleVolumeRemoveVolume, EMcpCallFlags::Mutating)

// Utility
MCP_LS_CALL(GetVolumesInfo, "get_volumes_info", P_GetVolumesInfo, HandleVolumeGetVolumesInfo, EMcpCallFlags::None)

// Add Volume To Actor handlers
MCP_LS_CALL(AddTriggerVolume, "add_trigger_volume", P_AddTriggerVolume, HandleVolumeAddTriggerVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddBlockingVolume, "add_blocking_volume", P_AddBlockingVolume, HandleVolumeAddBlockingVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddKillZVolume, "add_kill_z_volume", P_AddKillZVolume, HandleVolumeAddKillZVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddPhysicsVolume, "add_physics_volume", P_AddPhysicsVolume, HandleVolumeAddPhysicsVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddCullDistanceVolume, "add_cull_distance_volume", P_AddCullDistanceVolume, HandleVolumeAddCullDistanceVolume, EMcpCallFlags::Mutating)
MCP_LS_CALL(AddPostProcessVolume, "add_post_process_volume", P_AddPostProcessVolume, HandleVolumeAddPostProcessVolume, EMcpCallFlags::Mutating)

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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevelStructure_ConfigureLevelBounds>());
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
