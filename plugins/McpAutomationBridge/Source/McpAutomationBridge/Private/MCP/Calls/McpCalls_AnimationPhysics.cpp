// LINT-TOOL: animation_physics
// LINT-SCHEMA-DERIVED
// animation_physics as FMcpCall classes — seventeenth classed family, adopting
// schema-from-decls (docs/action-declarations.md). Each class AUTHORS its schema
// fragment in a S_<Suffix>() function; the published facade schema folds those
// fragments and GetDecl() derives the validation decl from the same fragment via
// McpDeriveDecl(), so schema and decl are one source and cannot drift. Run()
// delegates to the subsystem member handlers — HandleAnimPhys* (AnimationHandlers.cpp)
// for the 14 core actions, HandleAnimAuthoring* (AnimationAuthoringHandlers.cpp)
// for the 42 authoring actions, HandleSkeleton* (SkeletonHandlers.cpp) for the 29
// skeleton actions — until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_AnimationHandlers.h"
#include "McpAutomationBridge_AnimationAuthoringHandlers.h"
#include "McpAutomationBridge_SkeletonHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::AnimationPhysics
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads;
// the facade fold dedups shared params to one entry and McpDeriveDecl() reads the
// param kinds + required-set back out to build the transport validation decl.
// Actions that accept one-of-several spellings (assets/retargetAssets, the
// state-machine blueprintPath/assetPath aliases, play_montage's montagePath/
// assetPath, add_montage_section's assetPath/montagePath/name, the notifyClass/
// notifyName one-ofs, create_socket's attachBoneName/boneName, ...) author each
// optional: the requirement is "at least one", which the decl vocabulary cannot
// express and each handler enforces itself. setup_ragdoll/activate_ragdoll are
// advertised-but-unwired stubs (see the class comment below) — their fragments
// are authored from their contract like any other action.

// Core (AnimationHandlers.cpp)

static void S_Cleanup(FMcpSchemaBuilder& B)
{
	B.Array(TEXT("artifacts"), TEXT(""))
	 .Required({TEXT("artifacts")});
}

static void S_CreateBlendSpace(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Integer(TEXT("dimensions"), TEXT("create_blend_space: 1 or 2 (default 1)."))
	 .Number(TEXT("minX"), TEXT("create_blend_space: axis-0 minimum (default 0)."))
	 .Number(TEXT("maxX"), TEXT("create_blend_space: axis-0 maximum (default 1)."))
	 .Integer(TEXT("gridX"), TEXT("create_blend_space: axis-0 grid divisions (default 3)."))
	 .Number(TEXT("minY"), TEXT("create_blend_space: axis-1 minimum when dimensions=2 (default 0)."))
	 .Number(TEXT("maxY"), TEXT("create_blend_space: axis-1 maximum when dimensions=2 (default 1)."))
	 .Integer(TEXT("gridY"), TEXT("create_blend_space: axis-1 grid divisions when dimensions=2 (default 3)."))
	 .Required({TEXT("name"), TEXT("skeletonPath")});
}

static void S_CreateBlendTree(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("treeName"), TEXT("create_blend_tree: name for the new blend tree node."))
	 .ArrayOfObjects(TEXT("blendParameters"), TEXT("create_blend_tree: blend axis configs ({name, min, max})."))
	 .ArrayOfObjects(TEXT("children"), TEXT("create_blend_tree: child samples ({animationPath, blendWeight})."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("blueprintPath")});
}

static void S_CreateProceduralAnim(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .ArrayOfObjects(TEXT("boneTracks"), TEXT("create_procedural_animation: per-bone keyframe tracks ({boneName, frames:[{frame, location, rotation}]})."))
	 .Integer(TEXT("numFrames"), TEXT("create_animation_sequence/set_sequence_length/create_procedural_animation: explicit frame count (overrides length*frameRate)."))
	 .Integer(TEXT("frameRate"), TEXT("create_animation_sequence/set_sequence_length/create_procedural_animation: sample rate in fps (default 30)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("name"), TEXT("skeletonPath"), TEXT("boneTracks")});
}

static void S_CreateStateMachine(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("machineName"), TEXT("Alias of stateMachineName."))
	 .ArrayOfObjects(TEXT("states"), TEXT("create_state_machine: states to add ({name})."))
	 .ArrayOfObjects(TEXT("transitions"), TEXT("create_state_machine: transitions to add ({sourceState, targetState, crossfadeDuration})."));
}

static void S_SetupIk(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("name"), TEXT("skeletonPath")});
}

static void S_ConfigureVehicle(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("vehicleName"), TEXT("Legacy alias of actorName for configure_vehicle."))
	 .String(TEXT("vehicleType"), TEXT(""))
	 .ArrayOfObjects(TEXT("wheels"), TEXT("configure_vehicle: per-wheel configs ({boneName, offset, radius, width, friction})."))
	 .Object(TEXT("engine"), TEXT("configure_vehicle: engine settings (maxRPM, maxTorque, gears)."))
	 .Object(TEXT("transmission"), TEXT("configure_vehicle: transmission settings (finalDrive/finalDriveRatio, gearRatios array)."))
	 .Number(TEXT("mass"), TEXT(""))
	 .Number(TEXT("dragCoefficient"), TEXT(""));
}

static void S_SetupPhysicsSimulation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("physicsAssetName"), TEXT("Physics asset name for setup_physics_simulation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .Bool(TEXT("assignToMesh"), TEXT("Assign the created physics asset to the skeletal mesh."));
}

static void S_CreateAnimationAsset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("assetType"), TEXT("create_animation_asset: 'sequence' (default) or 'montage'."))
	 .Required({TEXT("name"), TEXT("skeletonPath")});
}

static void S_SetupRetargeting(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sourceSkeleton"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("targetSkeleton"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Array(TEXT("assets"), TEXT("setup_retargeting: source animation sequence paths to duplicate onto targetSkeleton ('retargetAssets' accepted as fallback)."))
	 .Array(TEXT("retargetAssets"), TEXT("setup_retargeting: alias of 'assets'."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("suffix"), TEXT("setup_retargeting: filename suffix for duplicated assets (default '_Retargeted')."))
	 .Bool(TEXT("overwrite"), TEXT("setup_retargeting: overwrite an existing retarget destination asset (default false)."))
	 .Required({TEXT("sourceSkeleton"), TEXT("targetSkeleton")});
}

static void S_PlayMontage(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("montagePath"), TEXT("play_montage: montage to play; add_montage_section: montage to edit ('assetPath'/'name' accepted as fallback)."))
	 .String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .Number(TEXT("playRate"), TEXT(""))
	 .Required({TEXT("actorName")});
}

static void S_CreatePoseLibrary(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .String(TEXT("directory"), TEXT("Alias of savePath for create_pose_library."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("name"), TEXT("skeletonPath")});
}


static void S_ActivateRagdoll(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Bool(TEXT("activate"), TEXT("activate_ragdoll: activate (true) or deactivate the ragdoll."))
	 .Required({TEXT("actorName")});
}

// Animation authoring (AnimationAuthoringHandlers.cpp)

static void S_CreateAnimationSequence(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .Integer(TEXT("frameRate"), TEXT("create_animation_sequence/set_sequence_length/create_procedural_animation: sample rate in fps (default 30)."))
	 .Integer(TEXT("numFrames"), TEXT("create_animation_sequence/set_sequence_length/create_procedural_animation: explicit frame count (overrides length*frameRate)."))
	 .Number(TEXT("length"), TEXT("Length in seconds (create_animation_sequence, set_sequence_length, create_montage)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("name"), TEXT("skeletonPath")});
}

static void S_SetSequenceLength(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .Number(TEXT("length"), TEXT("Length in seconds (create_animation_sequence, set_sequence_length, create_montage)."))
	 .Integer(TEXT("frameRate"), TEXT("create_animation_sequence/set_sequence_length/create_procedural_animation: sample rate in fps (default 30)."))
	 .Integer(TEXT("numFrames"), TEXT("create_animation_sequence/set_sequence_length/create_procedural_animation: explicit frame count (overrides length*frameRate)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath"), TEXT("length")});
}

static void S_AddBoneTrack(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath"), TEXT("boneName")});
}

static void S_SetBoneKey(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
	})
	 .Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Integer(TEXT("frame"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Required({TEXT("assetPath"), TEXT("boneName")});
}

static void S_SetCurveKey(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("curveName"), TEXT(""))
	 .Number(TEXT("floatValue"), TEXT("Curve key value."))
	 .Integer(TEXT("frame"), TEXT(""))
	 .Bool(TEXT("createIfMissing"), TEXT("set_curve_key: create the curve if it doesn't exist yet (default true)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath"), TEXT("curveName")});
}

static void S_AddNotify(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("notifyName"), TEXT(""))
	 .String(TEXT("notifyClass"), TEXT("add_notify/add_notify_state/add_montage_notify: AnimNotify(State) class name (e.g. PlaySound; 'AnimNotify_' prefix optional)."))
	 .Integer(TEXT("frame"), TEXT(""))
	 .Integer(TEXT("trackIndex"), TEXT("add_notify/add_notify_state/add_montage_notify: notify track index (default 0)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath")});
}

static void S_AddNotifyState(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("notifyClass"), TEXT("add_notify/add_notify_state/add_montage_notify: AnimNotify(State) class name (e.g. PlaySound; 'AnimNotify_' prefix optional)."))
	 .Integer(TEXT("startFrame"), TEXT("add_notify_state: start frame."))
	 .Integer(TEXT("endFrame"), TEXT("add_notify_state: end frame."))
	 .Integer(TEXT("trackIndex"), TEXT("add_notify/add_notify_state/add_montage_notify: notify track index (default 0)."))
	 .String(TEXT("notifyName"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath")});
}

static void S_AddSyncMarker(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("markerName"), TEXT("add_sync_marker: marker name."))
	 .Integer(TEXT("frame"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath"), TEXT("markerName")});
}

static void S_SetRootMotionSettings(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .Bool(TEXT("enableRootMotion"), TEXT("set_root_motion_settings: enable root motion (default true)."))
	 .String(TEXT("rootMotionRootLock"), TEXT("set_root_motion_settings: root lock mode (default RefPose)."))
	 .Bool(TEXT("forceRootLock"), TEXT("set_root_motion_settings: force root lock even when not additive (default false)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath")});
}

static void S_SetAdditiveSettings(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("additiveAnimType"), TEXT("set_additive_settings: additive type (default NoAdditive)."))
	 .String(TEXT("basePoseType"), TEXT("set_additive_settings: base pose type (default RefPose)."))
	 .String(TEXT("basePoseAnimation"), TEXT("set_additive_settings: base pose animation asset path."))
	 .Integer(TEXT("basePoseFrame"), TEXT("set_additive_settings: base pose frame index."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath")});
}

static void S_CreateMontage(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .String(TEXT("slotName"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Number(TEXT("length"), TEXT("Length in seconds (create_animation_sequence, set_sequence_length, create_montage)."))
	 .Required({TEXT("name"), TEXT("skeletonPath")});
}

static void S_AddMontageSection(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("sectionName"), TEXT(""))
	 .Number(TEXT("startTime"), TEXT("Section start time in seconds (alias of 'time')."))
	 .String(TEXT("montagePath"), TEXT("play_montage: montage to play; add_montage_section: montage to edit ('assetPath'/'name' accepted as fallback)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .Number(TEXT("time"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("sectionName")});
}

static void S_AddMontageSlot(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("slotName"), TEXT(""))
	 .String(TEXT("animationPath"), TEXT("add_montage_slot/add_blend_sample/add_aim_offset_sample: animation sequence asset path."))
	 .Number(TEXT("startTime"), TEXT("Section start time in seconds (alias of 'time')."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath"), TEXT("animationPath")});
}

static void S_SetSectionTiming(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("sectionName"), TEXT(""))
	 .Number(TEXT("startTime"), TEXT("Section start time in seconds (alias of 'time')."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath"), TEXT("sectionName"), TEXT("startTime")});
}

static void S_AddMontageNotify(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("notifyName"), TEXT(""))
	 .Number(TEXT("time"), TEXT(""))
	 .String(TEXT("notifyClass"), TEXT("add_notify/add_notify_state/add_montage_notify: AnimNotify(State) class name (e.g. PlaySound; 'AnimNotify_' prefix optional)."))
	 .Integer(TEXT("trackIndex"), TEXT("add_notify/add_notify_state/add_montage_notify: notify track index (default 0)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath")});
}

static void S_SetBlendIn(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .Number(TEXT("blendTime"), TEXT("set_blend_in/set_blend_out: blend duration in seconds ('time' accepted as alias)."))
	 .Number(TEXT("time"), TEXT(""))
	 .String(TEXT("blendOption"), TEXT("set_blend_in/set_blend_out: Linear, Cubic, or Sinusoidal."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath")});
}

static void S_SetBlendOut(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .Number(TEXT("blendTime"), TEXT("set_blend_in/set_blend_out: blend duration in seconds ('time' accepted as alias)."))
	 .Number(TEXT("time"), TEXT(""))
	 .String(TEXT("blendOption"), TEXT("set_blend_in/set_blend_out: Linear, Cubic, or Sinusoidal."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath")});
}

static void S_LinkSections(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("fromSection"), TEXT("link_sections: section to link from."))
	 .String(TEXT("toSection"), TEXT("link_sections: section to link to."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath"), TEXT("fromSection"), TEXT("toSection")});
}

static void S_CreateBlendSpace1D(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .String(TEXT("axisName"), TEXT(""))
	 .Number(TEXT("axisMin"), TEXT("create_blend_space_1d: axis minimum (default 0)."))
	 .Number(TEXT("axisMax"), TEXT("create_blend_space_1d: axis maximum (default 600)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("name"), TEXT("skeletonPath")});
}

static void S_CreateBlendSpace2D(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .String(TEXT("horizontalAxisName"), TEXT("create_blend_space_2d: horizontal axis name (default Direction)."))
	 .Number(TEXT("horizontalMin"), TEXT("create_blend_space_2d: horizontal axis minimum (default -180)."))
	 .Number(TEXT("horizontalMax"), TEXT("create_blend_space_2d: horizontal axis maximum (default 180)."))
	 .String(TEXT("verticalAxisName"), TEXT("create_blend_space_2d: vertical axis name (default Speed)."))
	 .Number(TEXT("verticalMin"), TEXT("create_blend_space_2d: vertical axis minimum (default 0)."))
	 .Number(TEXT("verticalMax"), TEXT("create_blend_space_2d: vertical axis maximum (default 600)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("name"), TEXT("skeletonPath")});
}

static void S_AddBlendSample(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("animationPath"), TEXT("add_montage_slot/add_blend_sample/add_aim_offset_sample: animation sequence asset path."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Number(TEXT("floatValue"), TEXT("add_blend_sample: 1D blend space sample coordinate."))
	 .Object(TEXT("vector2Value"), TEXT("add_blend_sample: 2D blend space sample coordinate (x, y)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
	 .Required({TEXT("assetPath"), TEXT("animationPath")});
}

static void S_ForceRebuildBlendSpace(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .Bool(TEXT("rebuildBlendParameters"), TEXT("rebuild_blend_space: also revalidate BlendParameters (default false)."))
	 .Bool(TEXT("compileReferencers"), TEXT("rebuild_blend_space: recompile AnimBlueprints referencing this blend space (default true)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath")});
}

static void S_SetAxisSettings(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .Number(TEXT("minValue"), TEXT("set_axis_settings: axis minimum."))
	 .Number(TEXT("maxValue"), TEXT("set_axis_settings: axis maximum."))
	 .String(TEXT("axisName"), TEXT(""))
	 .String(TEXT("axis"), TEXT("mirror_weights: mirror axis X/Y/Z (default X); set_axis_settings: Horizontal/Vertical (default Horizontal)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Integer(TEXT("gridDivisions"), TEXT("set_axis_settings: axis grid divisions."))
	 .Required({TEXT("assetPath")});
}

static void S_SetInterpolationSettings(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("interpolationType"), TEXT(""))
	 .Number(TEXT("targetWeightInterpolationSpeed"), TEXT("set_interpolation_settings: blend weight interpolation speed per second (default 5)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath")});
}

static void S_CreateAimOffset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("name"), TEXT("skeletonPath")});
}

static void S_AddAimOffsetSample(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("animationPath"), TEXT("add_montage_slot/add_blend_sample/add_aim_offset_sample: animation sequence asset path."))
	 .Number(TEXT("yaw"), TEXT("add_aim_offset_sample: sample yaw angle."))
	 .Number(TEXT("pitch"), TEXT("add_aim_offset_sample: sample pitch angle."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("assetPath"), TEXT("animationPath")});
}

static void S_CreateAnimBlueprint(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .String(TEXT("parentClass"), TEXT(""))
	 .Required({TEXT("name")});
}

static void S_AddStateMachine(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("machineName"), TEXT("Alias of stateMachineName."))
	 .String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("stateMachineName"), TEXT("State machine name for add_state_machine/add_state/add_transition/set_transition_rules."))
	 .Number(TEXT("positionX"), TEXT(""))
	 .Number(TEXT("positionY"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."));
}

static void S_AddState(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("stateName"), TEXT(""))
	 .String(TEXT("machineName"), TEXT("Alias of stateMachineName."))
	 .String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("stateMachineName"), TEXT("State machine name for add_state_machine/add_state/add_transition/set_transition_rules."))
	 .Number(TEXT("positionX"), TEXT(""))
	 .Number(TEXT("positionY"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("stateName")});
}

static void S_AddTransition(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("machineName"), TEXT("Alias of stateMachineName."))
	 .String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("stateMachineName"), TEXT("State machine name for add_state_machine/add_state/add_transition/set_transition_rules."))
	 .String(TEXT("fromState"), TEXT("add_transition/set_transition_rules: source state name."))
	 .String(TEXT("toState"), TEXT("add_transition/set_transition_rules: target state name."))
	 .Number(TEXT("crossfadeDuration"), TEXT("add_transition/set_transition_rules: crossfade time in seconds."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("fromState"), TEXT("toState")});
}

static void S_SetTransitionRules(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("machineName"), TEXT("Alias of stateMachineName."))
	 .String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("stateMachineName"), TEXT("State machine name for add_state_machine/add_state/add_transition/set_transition_rules."))
	 .String(TEXT("fromState"), TEXT("add_transition/set_transition_rules: source state name."))
	 .String(TEXT("toState"), TEXT("add_transition/set_transition_rules: target state name."))
	 .Number(TEXT("crossfadeDuration"), TEXT("add_transition/set_transition_rules: crossfade time in seconds."))
	 .Integer(TEXT("priorityOrder"), TEXT("set_transition_rules: transition priority order."))
	 .Bool(TEXT("automaticRule"), TEXT("set_transition_rules: auto-trigger when the source sequence player finishes (default false)."))
	 .Bool(TEXT("bidirectional"), TEXT("set_transition_rules: also allow the reverse transition (default false)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."));
}

static void S_AddBlendNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("nodeName"), TEXT("add_blend_node/set_animation_graph_node_value: AnimGraph node name/comment."))
	 .String(TEXT("blendType"), TEXT("add_blend_node: node type, e.g. TwoWayBlend (default)."))
	 .Number(TEXT("positionX"), TEXT(""))
	 .Number(TEXT("positionY"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("blueprintPath")});
}

static void S_AddCachedPose(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("cacheName"), TEXT("add_cached_pose: cached-pose node name."))
	 .Number(TEXT("positionX"), TEXT(""))
	 .Number(TEXT("positionY"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("blueprintPath"), TEXT("cacheName")});
}

static void S_AddSlotNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("slotName"), TEXT(""))
	 .String(TEXT("groupName"), TEXT("add_slot_node: slot group name (default DefaultGroup)."))
	 .Number(TEXT("positionX"), TEXT(""))
	 .Number(TEXT("positionY"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("blueprintPath")});
}

static void S_AddLayeredBlendPerBone(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
	 .Number(TEXT("positionX"), TEXT(""))
	 .Number(TEXT("positionY"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetAnimGraphNodeValue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("nodeName"), TEXT("add_blend_node/set_animation_graph_node_value: AnimGraph node name/comment."))
	 .String(TEXT("propertyName"), TEXT("set_animation_graph_node_value: node property to set (dot-notation for nested structs)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 // Discriminated value: populate exactly ONE typed field matching the node property.
	 .Bool(TEXT("boolValue"), TEXT("Set a bool node property."))
	 .Integer(TEXT("intValue"), TEXT("Set an integer node property."))
	 .Number(TEXT("floatValue"), TEXT("Set a float/double node property."))
	 .String(TEXT("stringValue"), TEXT("Set a string / name / text / enum / object-path node property."))
	 .Object(TEXT("colorValue"), TEXT("Set an FLinearColor/FColor node property (r,g,b,a)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Object(TEXT("vectorValue"), TEXT("Set an FVector node property (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("structValue"), TEXT("Set a struct / instanced subobject node property."))
	 .Array(TEXT("arrayValue"), TEXT("Set an array node property."), TEXT("object"))
	 .Required({TEXT("blueprintPath"), TEXT("nodeName"), TEXT("propertyName")});
}

static void S_CreateControlRig(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .Bool(TEXT("modularRig"), TEXT("create_control_rig: create as a modular rig (default false)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("name")});
}

static void S_CreateIkRig(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("name")});
}

static void S_CreateIkRetargeter(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .String(TEXT("sourceIKRigPath"), TEXT("Source IK Rig asset path for create_ik_retargeter."))
	 .String(TEXT("targetIKRigPath"), TEXT("Target IK Rig asset path for create_ik_retargeter."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("name")});
}

static void S_SetRetargetChainMapping(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("sourceChain"), TEXT("set_retarget_chain_mapping: source IK Rig chain name."))
	 .String(TEXT("targetChain"), TEXT("set_retarget_chain_mapping: target IK Rig chain name."))
	 .Required({TEXT("sourceChain"), TEXT("targetChain")});
}

static void S_GetAnimationInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .Required({TEXT("assetPath")});
}

static void S_BindAnimNotify(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_animation_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
	 .String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
	 .String(TEXT("notifyName"), TEXT(""))
	 .String(TEXT("functionName"), TEXT("bind_animation_notify: BlueprintCallable function to call when the notify fires."))
	 .String(TEXT("targetClass"), TEXT("bind_animation_notify: class to cast the owner to (e.g. /Game/.../BP_CPP_Character or a /Script class)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Number(TEXT("positionX"), TEXT(""))
	 .Number(TEXT("positionY"), TEXT(""))
	 .Required({TEXT("notifyName"), TEXT("functionName")});
}

// Skeleton, physics assets, skin weights, cloth, morphs (SkeletonHandlers.cpp)

static void S_GetSkeletonInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."));
}

static void S_ListBones(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."));
}

static void S_ListSockets(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."));
}

static void S_CreateSocket(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("socketName"), TEXT("create_socket/configure_socket: socket name."))
	 .String(TEXT("attachBoneName"), TEXT("create_socket/configure_socket: bone the socket attaches to ('boneName' accepted as fallback)."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
	 .Object(TEXT("relativeLocation"), TEXT("create_socket/configure_socket: socket location relative to its bone (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Object(TEXT("relativeRotation"), TEXT("create_socket/configure_socket: socket rotation relative to its bone (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
	})
	 .Object(TEXT("relativeScale"), TEXT("create_socket/configure_socket: socket scale relative to its bone (x, y, z; default 1,1,1)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("socketName")});
}

static void S_ConfigureSocket(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("socketName"), TEXT("create_socket/configure_socket: socket name."))
	 .String(TEXT("attachBoneName"), TEXT("create_socket/configure_socket: bone the socket attaches to ('boneName' accepted as fallback)."))
	 .Object(TEXT("relativeLocation"), TEXT("create_socket/configure_socket: socket location relative to its bone (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Object(TEXT("relativeRotation"), TEXT("create_socket/configure_socket: socket rotation relative to its bone (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
	})
	 .Object(TEXT("relativeScale"), TEXT("create_socket/configure_socket: socket scale relative to its bone (x, y, z; default 1,1,1)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("socketName")});
}

static void S_CreateVirtualBone(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("sourceBoneName"), TEXT("create_virtual_bone: source bone."))
	 .String(TEXT("targetBoneName"), TEXT("create_virtual_bone: target bone."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("skeletonPath"), TEXT("sourceBoneName"), TEXT("targetBoneName")});
}

static void S_CreatePhysicsAsset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("outputPath"), TEXT("create_physics_asset: output asset path (default: <mesh>_PhysicsAsset next to the mesh)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."));
}

static void S_ListPhysicsBodies(FMcpSchemaBuilder& B)
{
	B.String(TEXT("physicsAssetPath"), TEXT("Existing physics asset path (list_physics_bodies, add_physics_body, configure_physics_body, add_physics_constraint, configure_constraint_limits)."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."));
}

static void S_AddPhysicsBody(FMcpSchemaBuilder& B)
{
	B.String(TEXT("physicsAssetPath"), TEXT("Existing physics asset path (list_physics_bodies, add_physics_body, configure_physics_body, add_physics_constraint, configure_constraint_limits)."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
	 .String(TEXT("bodyType"), TEXT("add_physics_body: Sphere, Box, or Capsule/Sphyl (default Capsule)."))
	 .Number(TEXT("radius"), TEXT("add_physics_body: sphere/capsule radius."))
	 .Number(TEXT("length"), TEXT("Length in seconds (create_animation_sequence, set_sequence_length, create_montage)."))
	 .Number(TEXT("width"), TEXT("add_physics_body: box width."))
	 .Number(TEXT("height"), TEXT("add_physics_body: box height."))
	 .Number(TEXT("depth"), TEXT("add_physics_body: box depth."))
	 .Object(TEXT("center"), TEXT("add_physics_body: primitive center offset (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
	})
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("physicsAssetPath"), TEXT("boneName")});
}

static void S_ConfigurePhysicsBody(FMcpSchemaBuilder& B)
{
	B.String(TEXT("physicsAssetPath"), TEXT("Existing physics asset path (list_physics_bodies, add_physics_body, configure_physics_body, add_physics_constraint, configure_constraint_limits)."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
	 .Number(TEXT("mass"), TEXT(""))
	 .Number(TEXT("linearDamping"), TEXT("configure_physics_body: linear damping."))
	 .Number(TEXT("angularDamping"), TEXT("configure_physics_body: angular damping."))
	 .Bool(TEXT("collisionEnabled"), TEXT("configure_physics_body: enable query+physics collision (default true)."))
	 .Bool(TEXT("simulatePhysics"), TEXT("configure_physics_body: enable physics simulation on this body (default true)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("physicsAssetPath"), TEXT("boneName")});
}

static void S_AddPhysicsConstraint(FMcpSchemaBuilder& B)
{
	B.String(TEXT("physicsAssetPath"), TEXT("Existing physics asset path (list_physics_bodies, add_physics_body, configure_physics_body, add_physics_constraint, configure_constraint_limits)."))
	 .String(TEXT("bodyA"), TEXT("add_physics_constraint/configure_constraint_limits: first body's bone name."))
	 .String(TEXT("bodyB"), TEXT("add_physics_constraint/configure_constraint_limits: second body's bone name."))
	 .String(TEXT("constraintName"), TEXT("add_physics_constraint: joint name for the new constraint."))
	 .Object(TEXT("limits"), TEXT("add_physics_constraint/configure_constraint_limits: angular limits (swing1LimitAngle, swing2LimitAngle, twistLimitAngle, swing1Motion, swing2Motion, twistMotion); falls back to the flat swing/twist fields below when omitted."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("physicsAssetPath"), TEXT("bodyA"), TEXT("bodyB")});
}

static void S_ConfigureConstraintLimits(FMcpSchemaBuilder& B)
{
	B.String(TEXT("physicsAssetPath"), TEXT("Existing physics asset path (list_physics_bodies, add_physics_body, configure_physics_body, add_physics_constraint, configure_constraint_limits)."))
	 .String(TEXT("bodyA"), TEXT("add_physics_constraint/configure_constraint_limits: first body's bone name."))
	 .String(TEXT("bodyB"), TEXT("add_physics_constraint/configure_constraint_limits: second body's bone name."))
	 .Object(TEXT("limits"), TEXT("add_physics_constraint/configure_constraint_limits: angular limits (swing1LimitAngle, swing2LimitAngle, twistLimitAngle, swing1Motion, swing2Motion, twistMotion); falls back to the flat swing/twist fields below when omitted."))
	 .Number(TEXT("swing1LimitAngle"), TEXT("configure_constraint_limits: swing1 angle when 'limits' is omitted."))
	 .Number(TEXT("swing2LimitAngle"), TEXT("configure_constraint_limits: swing2 angle when 'limits' is omitted."))
	 .Number(TEXT("twistLimitAngle"), TEXT("configure_constraint_limits: twist angle when 'limits' is omitted."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("physicsAssetPath"), TEXT("bodyA"), TEXT("bodyB")});
}

static void S_RenameBone(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
	 .String(TEXT("newBoneName"), TEXT("rename_bone: new name for the (virtual) bone."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("skeletonPath"), TEXT("boneName"), TEXT("newBoneName")});
}

static void S_SetBoneTransform(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
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
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("boneName")});
}

static void S_CreateMorphTarget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("morphTargetName"), TEXT("create_morph_target/set_morph_target_deltas: morph target name."))
	 .ArrayOfObjects(TEXT("deltas"), TEXT("create_morph_target/set_morph_target_deltas: vertex deltas ({vertexIndex, positionDelta, tangentDelta})."))
	 .Integer(TEXT("lodIndex"), TEXT("set_vertex_weights/copy_weights/mirror_weights/set_morph_target_deltas: LOD index (default 0)."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("skeletalMeshPath"), TEXT("morphTargetName"), TEXT("deltas")});
}

static void S_SetMorphTargetDeltas(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("morphTargetName"), TEXT("create_morph_target/set_morph_target_deltas: morph target name."))
	 .ArrayOfObjects(TEXT("deltas"), TEXT("create_morph_target/set_morph_target_deltas: vertex deltas ({vertexIndex, positionDelta, tangentDelta})."))
	 .Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
	 .Required({TEXT("skeletalMeshPath"), TEXT("morphTargetName"), TEXT("deltas")});
}

static void S_ImportMorphTargets(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("morphTargetPath"), TEXT("import_morph_targets: source FBX file path."))
	 .String(TEXT("sourcePath"), TEXT("Alias of morphTargetPath for import_morph_targets."))
	 .Required({TEXT("skeletalMeshPath")});
}

static void S_NormalizeWeights(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .Required({TEXT("skeletalMeshPath")});
}

static void S_PruneWeights(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .Number(TEXT("threshold"), TEXT("prune_weights: influence weight threshold below which weights are pruned."))
	 .Required({TEXT("skeletalMeshPath")});
}

static void S_BindClothToSkeletalMesh(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("clothAssetName"), TEXT("bind_cloth_to_skeletal_mesh: cloth asset to bind (omit to list available cloth assets)."))
	 .Integer(TEXT("meshLodIndex"), TEXT("bind_cloth_to_skeletal_mesh: skeletal mesh LOD index."))
	 .Integer(TEXT("sectionIndex"), TEXT("bind_cloth_to_skeletal_mesh: mesh section index to bind."))
	 .Integer(TEXT("assetLodIndex"), TEXT("bind_cloth_to_skeletal_mesh: cloth asset LOD index."))
	 .Required({TEXT("skeletalMeshPath")});
}

static void S_AssignClothAssetToMesh(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .Required({TEXT("skeletalMeshPath")});
}

static void S_CreateSkeleton(FMcpSchemaBuilder& B)
{
	B.String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_animation, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_animation_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
	 .String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("rootBoneName"), TEXT("create_skeleton: name for the initial root bone (default 'Root')."));
}

static void S_AddBone(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
	 .String(TEXT("parentBone"), TEXT("add_bone: parent bone name; set_bone_parent: new parent bone (empty unparents to root; 'parentBoneName'/'newParentBone' accepted as fallback)."))
	 .String(TEXT("parentBoneName"), TEXT("Alias of parentBone for add_bone."))
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
	 .Required({TEXT("skeletonPath"), TEXT("boneName")});
}

static void S_RemoveBone(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
	 .Bool(TEXT("removeChildren"), TEXT("remove_bone: also remove child bones (default false)."))
	 .Required({TEXT("skeletonPath"), TEXT("boneName")});
}

static void S_SetBoneParent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("boneName"), TEXT("Name of the bone."))
	 .String(TEXT("parentBone"), TEXT("add_bone: parent bone name; set_bone_parent: new parent bone (empty unparents to root; 'parentBoneName'/'newParentBone' accepted as fallback)."))
	 .String(TEXT("newParentBone"), TEXT("Alias of parentBone for set_bone_parent."))
	 .Required({TEXT("skeletonPath"), TEXT("boneName")});
}

static void S_SetVertexWeights(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("profileName"), TEXT("set_vertex_weights/copy_weights/mirror_weights: skin weight profile name."))
	 .ArrayOfObjects(TEXT("weights"), TEXT("set_vertex_weights: per-vertex influences ({vertexIndex, influences:[{boneIndex, weight}]})."))
	 .Integer(TEXT("lodIndex"), TEXT("set_vertex_weights/copy_weights/mirror_weights/set_morph_target_deltas: LOD index (default 0)."))
	 .Required({TEXT("skeletalMeshPath"), TEXT("weights")});
}

static void S_AutoSkinWeights(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .Required({TEXT("skeletalMeshPath")});
}

static void S_CopyWeights(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sourceMeshPath"), TEXT("copy_weights: source skeletal mesh path."))
	 .String(TEXT("targetMeshPath"), TEXT("copy_weights: target skeletal mesh path."))
	 .String(TEXT("profileName"), TEXT("set_vertex_weights/copy_weights/mirror_weights: skin weight profile name."))
	 .Integer(TEXT("lodIndex"), TEXT("set_vertex_weights/copy_weights/mirror_weights/set_morph_target_deltas: LOD index (default 0)."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .Required({TEXT("sourceMeshPath"), TEXT("targetMeshPath")});
}

static void S_MirrorWeights(FMcpSchemaBuilder& B)
{
	B.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("axis"), TEXT("mirror_weights: mirror axis X/Y/Z (default X); set_axis_settings: Horizontal/Vertical (default Horizontal)."))
	 .String(TEXT("profileName"), TEXT("set_vertex_weights/copy_weights/mirror_weights: skin weight profile name."))
	 .Integer(TEXT("lodIndex"), TEXT("set_vertex_weights/copy_weights/mirror_weights/set_morph_target_deltas: LOD index (default 0)."))
	 .Required({TEXT("skeletalMeshPath")});
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor on 83 of 85: all three retired chains were whole-editor-gated
// (the primary dispatcher answered a NOT_IMPLEMENTED stub in non-editor
// builds; the authoring and skeleton TUs are compiled only WITH_EDITOR).
// setup_ragdoll/activate_ragdoll carry EMcpCallFlags::None so their members
// preserve the retired chain's exact behavior in every build flavor: the two
// names are advertised but the dispatcher had no branch for them — they always
// answered the terminal NOT_IMPLEMENTED else, and the orphaned
// HandleSetupRagdoll/HandleActivateRagdoll implementations have no call site
// (wire-up vs retire is Aaron's call — TODO.md). Mutating on the writers; the
// readers are get_animation_info, get_skeleton_info, list_bones, list_sockets,
// and list_physics_bodies, and seven more actions write nothing:
// import_morph_targets (info/redirect only), normalize_weights, prune_weights,
// auto_skin_weights, copy_weights, assign_cloth_asset_to_mesh (honest
// MANUAL_INTERVENTION_REQUIRED errors), and the two ragdoll stubs.

#define MCP_AP_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                         \
class FMcpCall_AnimationPhysics_##ClassSuffix final : public FMcpCall                     \
{                                                                                        \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }       \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("animation_physics"),          \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                             \
		return D;                                                                        \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return HandlerFn(S, RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

// configure_vehicle/setup_physics_simulation still touch the subsystem's private
// FindActorByName, so their handlers stay members (F1 rule 4): dispatch via S.
#define MCP_AP_MEMBER_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                  \
class FMcpCall_AnimationPhysics_##ClassSuffix final : public FMcpCall                     \
{                                                                                        \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }       \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("animation_physics"),          \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                             \
		return D;                                                                        \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

// Core (AnimationHandlers.cpp)
MCP_AP_CALL(Cleanup, "cleanup", McpHandlers::AnimationPhysics::HandleAnimPhysCleanup, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateBlendSpace, "create_blend_space", McpHandlers::AnimationPhysics::HandleAnimPhysCreateBlendSpace, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateBlendTree, "create_blend_tree", McpHandlers::AnimationPhysics::HandleAnimPhysCreateBlendTree, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateProceduralAnim, "create_procedural_animation", McpHandlers::AnimationPhysics::HandleAnimPhysCreateProceduralAnim, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateStateMachine, "create_state_machine", McpHandlers::AnimationPhysics::HandleAnimPhysCreateStateMachine, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetupIk, "setup_ik", McpHandlers::AnimationPhysics::HandleAnimPhysSetupIk, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_MEMBER_CALL(ConfigureVehicle, "configure_vehicle", HandleAnimPhysConfigureVehicle, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_MEMBER_CALL(SetupPhysicsSimulation, "setup_physics_simulation", HandleAnimPhysSetupPhysicsSimulation, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateAnimationAsset, "create_animation_asset", McpHandlers::AnimationPhysics::HandleAnimPhysCreateAnimationAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetupRetargeting, "setup_retargeting", McpHandlers::AnimationPhysics::HandleAnimPhysSetupRetargeting, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(PlayMontage, "play_montage", McpHandlers::AnimationPhysics::HandleAnimPhysPlayMontage, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreatePoseLibrary, "create_pose_library", McpHandlers::AnimationPhysics::HandleAnimPhysCreatePoseLibrary, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ActivateRagdoll, "activate_ragdoll", McpHandlers::AnimationPhysics::HandleAnimPhysActivateRagdoll, EMcpCallFlags::None)

// Animation authoring (AnimationAuthoringHandlers.cpp)
MCP_AP_CALL(CreateAnimationSequence, "create_animation_sequence", McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateAnimationSequence, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetSequenceLength, "set_sequence_length", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetSequenceLength, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddBoneTrack, "add_bone_track", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddBoneTrack, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetBoneKey, "set_bone_key", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetBoneKey, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetCurveKey, "set_curve_key", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetCurveKey, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddNotify, "add_notify", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddNotify, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddNotifyState, "add_notify_state", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddNotifyState, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddSyncMarker, "add_sync_marker", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddSyncMarker, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetRootMotionSettings, "set_root_motion_settings", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetRootMotionSettings, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetAdditiveSettings, "set_additive_settings", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetAdditiveSettings, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateMontage, "create_montage", McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateMontage, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddMontageSection, "add_montage_section", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddMontageSection, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddMontageSlot, "add_montage_slot", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddMontageSlot, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetSectionTiming, "set_section_timing", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetSectionTiming, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddMontageNotify, "add_montage_notify", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddMontageNotify, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetBlendIn, "set_blend_in", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetBlendIn, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetBlendOut, "set_blend_out", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetBlendOut, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(LinkSections, "link_sections", McpHandlers::AnimationPhysics::HandleAnimAuthoringLinkSections, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateBlendSpace1D, "create_blend_space_1d", McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateBlendSpace1D, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateBlendSpace2D, "create_blend_space_2d", McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateBlendSpace2D, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddBlendSample, "add_blend_sample", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddBlendSample, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ForceRebuildBlendSpace, "rebuild_blend_space", McpHandlers::AnimationPhysics::HandleAnimAuthoringForceRebuildBlendSpace, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetAxisSettings, "set_axis_settings", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetAxisSettings, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetInterpolationSettings, "set_interpolation_settings", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetInterpolationSettings, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateAimOffset, "create_aim_offset", McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateAimOffset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddAimOffsetSample, "add_aim_offset_sample", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddAimOffsetSample, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateAnimBlueprint, "create_animation_blueprint", McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateAnimBlueprint, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddStateMachine, "add_state_machine", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddStateMachine, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddState, "add_state", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddState, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddTransition, "add_transition", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddTransition, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetTransitionRules, "set_transition_rules", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetTransitionRules, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddBlendNode, "add_blend_node", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddBlendNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddCachedPose, "add_cached_pose", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddCachedPose, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddSlotNode, "add_slot_node", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddSlotNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddLayeredBlendPerBone, "add_layered_blend_per_bone", McpHandlers::AnimationPhysics::HandleAnimAuthoringAddLayeredBlendPerBone, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetAnimGraphNodeValue, "set_animation_graph_node_value", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetAnimGraphNodeValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateControlRig, "create_control_rig", McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateControlRig, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateIkRig, "create_ik_rig", McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateIkRig, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateIkRetargeter, "create_ik_retargeter", McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateIkRetargeter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetRetargetChainMapping, "set_retarget_chain_mapping", McpHandlers::AnimationPhysics::HandleAnimAuthoringSetRetargetChainMapping, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(GetAnimationInfo, "get_animation_info", McpHandlers::AnimationPhysics::HandleAnimAuthoringGetAnimationInfo, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(BindAnimNotify, "bind_animation_notify", McpHandlers::AnimationPhysics::HandleAnimAuthoringBindAnimNotify, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Skeleton, physics assets, skin weights, cloth, morphs (SkeletonHandlers.cpp)
MCP_AP_CALL(GetSkeletonInfo, "get_skeleton_info", McpHandlers::AnimationPhysics::HandleSkeletonGetSkeletonInfo, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(ListBones, "list_bones", McpHandlers::AnimationPhysics::HandleSkeletonListBones, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(ListSockets, "list_sockets", McpHandlers::AnimationPhysics::HandleSkeletonListSockets, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(CreateSocket, "create_socket", McpHandlers::AnimationPhysics::HandleSkeletonCreateSocket, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ConfigureSocket, "configure_socket", McpHandlers::AnimationPhysics::HandleSkeletonConfigureSocket, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateVirtualBone, "create_virtual_bone", McpHandlers::AnimationPhysics::HandleSkeletonCreateVirtualBone, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreatePhysicsAsset, "create_physics_asset", McpHandlers::AnimationPhysics::HandleSkeletonCreatePhysicsAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ListPhysicsBodies, "list_physics_bodies", McpHandlers::AnimationPhysics::HandleSkeletonListPhysicsBodies, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(AddPhysicsBody, "add_physics_body", McpHandlers::AnimationPhysics::HandleSkeletonAddPhysicsBody, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ConfigurePhysicsBody, "configure_physics_body", McpHandlers::AnimationPhysics::HandleSkeletonConfigurePhysicsBody, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddPhysicsConstraint, "add_physics_constraint", McpHandlers::AnimationPhysics::HandleSkeletonAddPhysicsConstraint, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ConfigureConstraintLimits, "configure_constraint_limits", McpHandlers::AnimationPhysics::HandleSkeletonConfigureConstraintLimits, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(RenameBone, "rename_bone", McpHandlers::AnimationPhysics::HandleSkeletonRenameBone, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetBoneTransform, "set_bone_transform", McpHandlers::AnimationPhysics::HandleSkeletonSetBoneTransform, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateMorphTarget, "create_morph_target", McpHandlers::AnimationPhysics::HandleSkeletonCreateMorphTarget, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetMorphTargetDeltas, "set_morph_target_deltas", McpHandlers::AnimationPhysics::HandleSkeletonSetMorphTargetDeltas, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ImportMorphTargets, "import_morph_targets", McpHandlers::AnimationPhysics::HandleSkeletonImportMorphTargets, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(NormalizeWeights, "normalize_weights", McpHandlers::AnimationPhysics::HandleSkeletonNormalizeWeights, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(PruneWeights, "prune_weights", McpHandlers::AnimationPhysics::HandleSkeletonPruneWeights, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(BindClothToSkeletalMesh, "bind_cloth_to_skeletal_mesh", McpHandlers::AnimationPhysics::HandleSkeletonBindClothToSkeletalMesh, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AssignClothAssetToMesh, "assign_cloth_asset_to_mesh", McpHandlers::AnimationPhysics::HandleSkeletonAssignClothAssetToMesh, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(CreateSkeleton, "create_skeleton", McpHandlers::AnimationPhysics::HandleSkeletonCreateSkeleton, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddBone, "add_bone", McpHandlers::AnimationPhysics::HandleSkeletonAddBone, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(RemoveBone, "remove_bone", McpHandlers::AnimationPhysics::HandleSkeletonRemoveBone, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetBoneParent, "set_bone_parent", McpHandlers::AnimationPhysics::HandleSkeletonSetBoneParent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetVertexWeights, "set_vertex_weights", McpHandlers::AnimationPhysics::HandleSkeletonSetVertexWeights, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AutoSkinWeights, "auto_skin_weights", McpHandlers::AnimationPhysics::HandleSkeletonAutoSkinWeights, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(CopyWeights, "copy_weights", McpHandlers::AnimationPhysics::HandleSkeletonCopyWeights, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(MirrorWeights, "mirror_weights", McpHandlers::AnimationPhysics::HandleSkeletonMirrorWeights, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

#undef MCP_AP_CALL
#undef MCP_AP_MEMBER_CALL

} // namespace McpCalls::AnimationPhysics

void McpRegisterAnimationPhysicsCalls()
{
	using namespace McpCalls::AnimationPhysics;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_Cleanup>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateBlendSpace>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateBlendTree>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateProceduralAnim>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateStateMachine>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetupIk>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_ConfigureVehicle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetupPhysicsSimulation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateAnimationAsset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetupRetargeting>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_PlayMontage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreatePoseLibrary>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_ActivateRagdoll>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateAnimationSequence>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetSequenceLength>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddBoneTrack>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetBoneKey>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetCurveKey>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddNotify>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddNotifyState>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddSyncMarker>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetRootMotionSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetAdditiveSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateMontage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddMontageSection>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddMontageSlot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetSectionTiming>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddMontageNotify>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetBlendIn>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetBlendOut>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_LinkSections>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateBlendSpace1D>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateBlendSpace2D>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddBlendSample>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_ForceRebuildBlendSpace>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetAxisSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetInterpolationSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateAimOffset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddAimOffsetSample>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateAnimBlueprint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddStateMachine>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddState>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddTransition>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetTransitionRules>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddBlendNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddCachedPose>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddSlotNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddLayeredBlendPerBone>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetAnimGraphNodeValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateControlRig>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateIkRig>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateIkRetargeter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetRetargetChainMapping>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_GetAnimationInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_BindAnimNotify>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_GetSkeletonInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_ListBones>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_ListSockets>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateSocket>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_ConfigureSocket>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateVirtualBone>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreatePhysicsAsset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_ListPhysicsBodies>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddPhysicsBody>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_ConfigurePhysicsBody>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddPhysicsConstraint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_ConfigureConstraintLimits>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_RenameBone>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetBoneTransform>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateMorphTarget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetMorphTargetDeltas>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_ImportMorphTargets>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_NormalizeWeights>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_PruneWeights>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_BindClothToSkeletalMesh>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AssignClothAssetToMesh>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CreateSkeleton>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AddBone>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_RemoveBone>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetBoneParent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetVertexWeights>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_AutoSkinWeights>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_CopyWeights>());
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_MirrorWeights>());
}
