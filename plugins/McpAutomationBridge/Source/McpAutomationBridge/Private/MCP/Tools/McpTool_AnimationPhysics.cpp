// McpTool_AnimationPhysics.cpp — animation_physics tool definition (85 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_AnimationPhysics : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("animation_physics"); }

	FString GetDescription() const override
	{
		return TEXT("Create animation blueprints, blend spaces, montages, "
			"state machines, Control Rig, IK rigs, ragdolls, and vehicle physics.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }


	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
				.StringEnum(TEXT("action"), McpConsolidatedActions::AnimationPhysics(),
					TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.String(TEXT("assetPath"), TEXT("Existing asset to modify (e.g. the montage for add_montage_section)."))
			.Number(TEXT("startTime"), TEXT("Section start time in seconds (alias of 'time')."))
			.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
			.String(TEXT("parentClass"), TEXT(""))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("targetSkeleton"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("slotName"), TEXT(""))
			.String(TEXT("sectionName"), TEXT(""))
			.String(TEXT("notifyName"), TEXT(""))
			.String(TEXT("functionName"), TEXT("bind_anim_notify: BlueprintCallable function to call when the notify fires."))
			.String(TEXT("targetClass"), TEXT("bind_anim_notify: class to cast the owner to (e.g. /Game/.../BP_CPP_Character or a /Script class)."))
			.String(TEXT("blueprintPath"), TEXT("Anim Blueprint asset path (bind_anim_notify, add_state_machine, add_state, add_transition, set_transition_rules; 'assetPath' accepted for the state-machine actions)."))
			.Number(TEXT("positionX"), TEXT(""))
			.Number(TEXT("positionY"), TEXT(""))
			.String(TEXT("boneName"), TEXT("Name of the bone."))
			.String(TEXT("curveName"), TEXT(""))
			.String(TEXT("stateName"), TEXT(""))
			.String(TEXT("stateMachineName"), TEXT("State machine name for add_state_machine/add_state/add_transition/set_transition_rules."))
			.String(TEXT("machineName"), TEXT("Alias of stateMachineName."))
			.String(TEXT("interpolationType"), TEXT(""))
			.String(TEXT("axisName"), TEXT(""))
			.Number(TEXT("playRate"), TEXT(""))
			.Number(TEXT("frame"), TEXT(""))
			.Number(TEXT("time"), TEXT(""))
			.Number(TEXT("length"), TEXT("Length in seconds (create_animation_sequence, set_sequence_length, create_montage)."))
			.Number(TEXT("blendTime"), TEXT("set_blend_in/set_blend_out: blend duration in seconds ('time' accepted as alias)."))
			.String(TEXT("blendOption"), TEXT("set_blend_in/set_blend_out: Linear, Cubic, or Sinusoidal."))
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
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.String(TEXT("vehicleType"), TEXT(""))
			.Number(TEXT("mass"), TEXT(""))
			.Number(TEXT("dragCoefficient"), TEXT(""))
			.Array(TEXT("artifacts"), TEXT(""))
			.String(TEXT("physicsAssetName"), TEXT("Physics asset name for setup_physics_simulation."))
			.Bool(TEXT("assignToMesh"), TEXT("Assign the created physics asset to the skeletal mesh."))
			.String(TEXT("sourceSkeleton"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("sourceIKRigPath"), TEXT("Source IK Rig asset path for create_ik_retargeter."))
			.String(TEXT("targetIKRigPath"), TEXT("Target IK Rig asset path for create_ik_retargeter."))
			.Bool(TEXT("save"), TEXT("Persist the created/modified asset to disk (default true; most authoring actions)."))
			.String(TEXT("path"), TEXT("Legacy alias of savePath (create_procedural_anim, create_pose_library, create_montage, create_blend_space_1d/2d, create_aim_offset, create_anim_blueprint, create_control_rig, create_ik_rig, create_ik_retargeter) or of skeletonPath (create_skeleton)."))
			.String(TEXT("directory"), TEXT("Alias of savePath for create_pose_library."))
			.Number(TEXT("numFrames"), TEXT("create_animation_sequence/set_sequence_length/create_procedural_anim: explicit frame count (overrides length*frameRate)."))
			.Number(TEXT("frameRate"), TEXT("create_animation_sequence/set_sequence_length/create_procedural_anim: sample rate in fps (default 30)."))
			.Bool(TEXT("createIfMissing"), TEXT("set_curve_key: create the curve if it doesn't exist yet (default true)."))
			.String(TEXT("notifyClass"), TEXT("add_notify/add_notify_state/add_montage_notify: AnimNotify(State) class name (e.g. PlaySound; 'AnimNotify_' prefix optional)."))
			.Number(TEXT("trackIndex"), TEXT("add_notify/add_notify_state/add_montage_notify: notify track index (default 0)."))
			.Number(TEXT("startFrame"), TEXT("add_notify_state: start frame."))
			.Number(TEXT("endFrame"), TEXT("add_notify_state: end frame."))
			.String(TEXT("markerName"), TEXT("add_sync_marker: marker name."))
			.Bool(TEXT("enableRootMotion"), TEXT("set_root_motion_settings: enable root motion (default true)."))
			.String(TEXT("rootMotionRootLock"), TEXT("set_root_motion_settings: root lock mode (default RefPose)."))
			.Bool(TEXT("forceRootLock"), TEXT("set_root_motion_settings: force root lock even when not additive (default false)."))
			.String(TEXT("additiveAnimType"), TEXT("set_additive_settings: additive type (default NoAdditive)."))
			.String(TEXT("basePoseType"), TEXT("set_additive_settings: base pose type (default RefPose)."))
			.String(TEXT("basePoseAnimation"), TEXT("set_additive_settings: base pose animation asset path."))
			.Number(TEXT("basePoseFrame"), TEXT("set_additive_settings: base pose frame index."))
			.String(TEXT("montagePath"), TEXT("play_montage: montage to play; add_montage_section: montage to edit ('assetPath'/'name' accepted as fallback)."))
			.String(TEXT("animationPath"), TEXT("add_montage_slot/add_blend_sample/add_aim_offset_sample: animation sequence asset path."))
			.String(TEXT("fromSection"), TEXT("link_sections: section to link from."))
			.String(TEXT("toSection"), TEXT("link_sections: section to link to."))
			.Number(TEXT("dimensions"), TEXT("create_blend_space: 1 or 2 (default 1)."))
			.Number(TEXT("minX"), TEXT("create_blend_space: axis-0 minimum (default 0)."))
			.Number(TEXT("maxX"), TEXT("create_blend_space: axis-0 maximum (default 1)."))
			.Number(TEXT("gridX"), TEXT("create_blend_space: axis-0 grid divisions (default 3)."))
			.Number(TEXT("minY"), TEXT("create_blend_space: axis-1 minimum when dimensions=2 (default 0)."))
			.Number(TEXT("maxY"), TEXT("create_blend_space: axis-1 maximum when dimensions=2 (default 1)."))
			.Number(TEXT("gridY"), TEXT("create_blend_space: axis-1 grid divisions when dimensions=2 (default 3)."))
			.Number(TEXT("axisMin"), TEXT("create_blend_space_1d: axis minimum (default 0)."))
			.Number(TEXT("axisMax"), TEXT("create_blend_space_1d: axis maximum (default 600)."))
			.String(TEXT("horizontalAxisName"), TEXT("create_blend_space_2d: horizontal axis name (default Direction)."))
			.Number(TEXT("horizontalMin"), TEXT("create_blend_space_2d: horizontal axis minimum (default -180)."))
			.Number(TEXT("horizontalMax"), TEXT("create_blend_space_2d: horizontal axis maximum (default 180)."))
			.String(TEXT("verticalAxisName"), TEXT("create_blend_space_2d: vertical axis name (default Speed)."))
			.Number(TEXT("verticalMin"), TEXT("create_blend_space_2d: vertical axis minimum (default 0)."))
			.Number(TEXT("verticalMax"), TEXT("create_blend_space_2d: vertical axis maximum (default 600)."))
			.FreeformObject(TEXT("sampleValue"), TEXT("add_blend_sample: sample coordinate — a number for 1D blend spaces or {x, y} for 2D."))
			.Bool(TEXT("rebuildBlendParameters"), TEXT("force_rebuild_blend_space: also revalidate BlendParameters (default false)."))
			.Bool(TEXT("compileReferencers"), TEXT("force_rebuild_blend_space: recompile AnimBlueprints referencing this blend space (default true)."))
			.Number(TEXT("minValue"), TEXT("set_axis_settings: axis minimum."))
			.Number(TEXT("maxValue"), TEXT("set_axis_settings: axis maximum."))
			.Number(TEXT("gridDivisions"), TEXT("set_axis_settings: axis grid divisions."))
			.Number(TEXT("targetWeightInterpolationSpeed"), TEXT("set_interpolation_settings: blend weight interpolation speed per second (default 5)."))
			.String(TEXT("fromState"), TEXT("add_transition/set_transition_rules: source state name."))
			.String(TEXT("toState"), TEXT("add_transition/set_transition_rules: target state name."))
			.Number(TEXT("crossfadeDuration"), TEXT("add_transition/set_transition_rules: crossfade time in seconds."))
			.Number(TEXT("priorityOrder"), TEXT("set_transition_rules: transition priority order."))
			.Bool(TEXT("automaticRule"), TEXT("set_transition_rules: auto-trigger when the source sequence player finishes (default false)."))
			.Bool(TEXT("bidirectional"), TEXT("set_transition_rules: also allow the reverse transition (default false)."))
			.String(TEXT("blendType"), TEXT("add_blend_node: node type, e.g. TwoWayBlend (default)."))
			.String(TEXT("nodeName"), TEXT("add_blend_node/set_anim_graph_node_value: AnimGraph node name/comment."))
			.String(TEXT("cacheName"), TEXT("add_cached_pose: cached-pose node name."))
			.String(TEXT("groupName"), TEXT("add_slot_node: slot group name (default DefaultGroup)."))
			.String(TEXT("propertyName"), TEXT("set_anim_graph_node_value: node property to set (dot-notation for nested structs)."))
			.Bool(TEXT("modularRig"), TEXT("create_control_rig: create as a modular rig (default false)."))
			.String(TEXT("sourceChain"), TEXT("set_retarget_chain_mapping: source IK Rig chain name."))
			.String(TEXT("targetChain"), TEXT("set_retarget_chain_mapping: target IK Rig chain name."))
			.String(TEXT("treeName"), TEXT("create_blend_tree: name for the new blend tree node."))
			.ArrayOfObjects(TEXT("blendParameters"), TEXT("create_blend_tree: blend axis configs ({name, min, max})."))
			.ArrayOfObjects(TEXT("children"), TEXT("create_blend_tree: child samples ({animationPath, blendWeight})."))
			.ArrayOfObjects(TEXT("boneTracks"), TEXT("create_procedural_anim: per-bone keyframe tracks ({boneName, frames:[{frame, location, rotation}]})."))
			.ArrayOfObjects(TEXT("states"), TEXT("create_state_machine: states to add ({name})."))
			.ArrayOfObjects(TEXT("transitions"), TEXT("create_state_machine: transitions to add ({sourceState, targetState, crossfadeDuration})."))
			.String(TEXT("vehicleName"), TEXT("Legacy alias of actorName for configure_vehicle."))
			.ArrayOfObjects(TEXT("wheels"), TEXT("configure_vehicle: per-wheel configs ({boneName, offset, radius, width, friction})."))
			.Object(TEXT("engine"), TEXT("configure_vehicle: engine settings (maxRPM, maxTorque, gears)."))
			.Object(TEXT("transmission"), TEXT("configure_vehicle: transmission settings (finalDrive/finalDriveRatio, gearRatios array)."))
			.String(TEXT("assetType"), TEXT("create_animation_asset: 'sequence' (default) or 'montage'."))
			.Array(TEXT("assets"), TEXT("setup_retargeting: source animation sequence paths to duplicate onto targetSkeleton ('retargetAssets' accepted as fallback)."))
			.String(TEXT("suffix"), TEXT("setup_retargeting: filename suffix for duplicated assets (default '_Retargeted')."))
			.Bool(TEXT("overwrite"), TEXT("setup_retargeting: overwrite an existing retarget destination asset (default false)."))
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
			.String(TEXT("sourceBoneName"), TEXT("create_virtual_bone: source bone."))
			.String(TEXT("targetBoneName"), TEXT("create_virtual_bone: target bone."))
			.String(TEXT("outputPath"), TEXT("create_physics_asset: output asset path (default: <mesh>_PhysicsAsset next to the mesh)."))
			.String(TEXT("physicsAssetPath"), TEXT("Existing physics asset path (list_physics_bodies, add_physics_body, configure_physics_body, add_physics_constraint, configure_constraint_limits)."))
			.String(TEXT("bodyType"), TEXT("add_physics_body: Sphere, Box, or Capsule/Sphyl (default Capsule)."))
			.Number(TEXT("radius"), TEXT("add_physics_body: sphere/capsule radius."))
			.Number(TEXT("width"), TEXT("add_physics_body: box width."))
			.Number(TEXT("height"), TEXT("add_physics_body: box height."))
			.Number(TEXT("depth"), TEXT("add_physics_body: box depth."))
			.Object(TEXT("center"), TEXT("add_physics_body: primitive center offset (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Number(TEXT("linearDamping"), TEXT("configure_physics_body: linear damping."))
			.Number(TEXT("angularDamping"), TEXT("configure_physics_body: angular damping."))
			.Bool(TEXT("collisionEnabled"), TEXT("configure_physics_body: enable query+physics collision (default true)."))
			.Bool(TEXT("simulatePhysics"), TEXT("configure_physics_body: enable physics simulation on this body (default true)."))
			.String(TEXT("bodyA"), TEXT("add_physics_constraint/configure_constraint_limits: first body's bone name."))
			.String(TEXT("bodyB"), TEXT("add_physics_constraint/configure_constraint_limits: second body's bone name."))
			.String(TEXT("constraintName"), TEXT("add_physics_constraint: joint name for the new constraint."))
			.Object(TEXT("limits"), TEXT("add_physics_constraint/configure_constraint_limits: angular limits (swing1LimitAngle, swing2LimitAngle, twistLimitAngle, swing1Motion, swing2Motion, twistMotion); falls back to the flat swing/twist fields below when omitted."))
			.Number(TEXT("swing1LimitAngle"), TEXT("configure_constraint_limits: swing1 angle when 'limits' is omitted."))
			.Number(TEXT("swing2LimitAngle"), TEXT("configure_constraint_limits: swing2 angle when 'limits' is omitted."))
			.Number(TEXT("twistLimitAngle"), TEXT("configure_constraint_limits: twist angle when 'limits' is omitted."))
			.String(TEXT("newBoneName"), TEXT("rename_bone: new name for the (virtual) bone."))
			.String(TEXT("rootBoneName"), TEXT("create_skeleton: name for the initial root bone (default 'Root')."))
			.String(TEXT("parentBone"), TEXT("add_bone: parent bone name; set_bone_parent: new parent bone (empty unparents to root; 'parentBoneName'/'newParentBone' accepted as fallback)."))
			.String(TEXT("parentBoneName"), TEXT("Alias of parentBone for add_bone."))
			.String(TEXT("newParentBone"), TEXT("Alias of parentBone for set_bone_parent."))
			.Bool(TEXT("removeChildren"), TEXT("remove_bone: also remove child bones (default false)."))
			.String(TEXT("profileName"), TEXT("set_vertex_weights/copy_weights/mirror_weights: skin weight profile name."))
			.ArrayOfObjects(TEXT("weights"), TEXT("set_vertex_weights: per-vertex influences ({vertexIndex, influences:[{boneIndex, weight}]})."))
			.Number(TEXT("lodIndex"), TEXT("set_vertex_weights/copy_weights/mirror_weights/set_morph_target_deltas: LOD index (default 0)."))
			.String(TEXT("sourceMeshPath"), TEXT("copy_weights: source skeletal mesh path."))
			.String(TEXT("targetMeshPath"), TEXT("copy_weights: target skeletal mesh path."))
			.String(TEXT("axis"), TEXT("mirror_weights: mirror axis X/Y/Z (default X); set_axis_settings: Horizontal/Vertical (default Horizontal)."))
			.String(TEXT("morphTargetName"), TEXT("create_morph_target/set_morph_target_deltas: morph target name."))
			.ArrayOfObjects(TEXT("deltas"), TEXT("create_morph_target/set_morph_target_deltas: vertex deltas ({vertexIndex, positionDelta, tangentDelta})."))
			.String(TEXT("morphTargetPath"), TEXT("import_morph_targets: source FBX file path."))
			.String(TEXT("sourcePath"), TEXT("Alias of morphTargetPath for import_morph_targets."))
			.String(TEXT("clothAssetName"), TEXT("bind_cloth_to_skeletal_mesh: cloth asset to bind (omit to list available cloth assets)."))
			.Number(TEXT("meshLodIndex"), TEXT("bind_cloth_to_skeletal_mesh: skeletal mesh LOD index."))
			.Number(TEXT("sectionIndex"), TEXT("bind_cloth_to_skeletal_mesh: mesh section index to bind."))
			.Number(TEXT("assetLodIndex"), TEXT("bind_cloth_to_skeletal_mesh: cloth asset LOD index."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_AnimationPhysics);
