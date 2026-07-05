// LINT-TOOL: animation_physics
// animation_physics as FMcpCall classes — seventeenth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers — HandleAnimPhys* (AnimationHandlers.cpp) for the 14 core
// actions, HandleAnimAuthoring* (AnimationAuthoringHandlers.cpp) for the 42
// authoring actions, HandleSkeleton* (SkeletonHandlers.cpp) for the 29
// skeleton actions — until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::AnimationPhysics
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations
// (McpDecl_AnimationPhysics.h) and re-verified against the member bodies.
// 27 rows shipped with decl fixes at classing — the deleted shadowed copies
// in the retired animation_physics dispatcher were the only readers of a
// number of spellings the fleet had unioned into the rows (sequenceName,
// montageName, add_notify's animationPath/time, set_bone_key's time/position,
// set_curve_key's time, add_blend_sample's sampleX/sampleY, set_axis_settings'
// axisIndex/gridNum, set_interpolation_settings' interpolationSpeed,
// add_state's animationPath/isEntry/isExit, add_transition's and
// set_transition_rules' sourceState/targetState (+condition), add_blend_node's
// nodeType, add_cached_pose's poseName, create_control_rig's and
// create_ik_rig's savePath (+meshPath), create_aim_offset's is1D,
// create_pose_library's save — read only by the dead authoring copy); the
// min/max/grid spellings on create_aim_offset and create_blend_space_1d/2d had
// no reader in either copy (bootstrap residue), and create_anim_blueprint's
// meshPath lost its last reader at the 2026-07-04i shadowed-duplicate
// deletion. Required-flag corrections against the live bodies' rejects:
// the blueprintPath/assetPath alias pairs (add_state, add_state_machine,
// add_transition, set_transition_rules), create_state_machine's
// blueprintPath/name pair, play_montage's montagePath/assetPath pair,
// add_montage_section's assetPath/montagePath/name triple, setup_retargeting's
// assets/retargetAssets pair, and the notifyClass/notifyName one-ofs
// (add_notify, add_montage_notify) each joint-reject only when every spelling
// is absent, so the required spelling went optional (at-least-one stays
// handler-enforced); create_anim_blueprint's savePath falls back to path with
// a default; create_control_rig's skeletonPath is only consulted when
// skeletalMeshPath is absent; add_notify's assetPath and set_section_timing's
// startTime are required by the live bodies' own rejects. setup_ragdoll and
// activate_ragdoll keep their rows AS-IS (advertised-but-unwired — see the
// class comment below).

inline const FMcpParamDecl P_Cleanup[] = { { TEXT("artifacts"), EMcpParamKind::Array, true } };
inline const FMcpParamDecl P_CreateBlendSpace[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("dimensions"), EMcpParamKind::Number, false }, { TEXT("minX"), EMcpParamKind::Number, false }, { TEXT("maxX"), EMcpParamKind::Number, false }, { TEXT("gridX"), EMcpParamKind::Number, false }, { TEXT("minY"), EMcpParamKind::Number, false }, { TEXT("maxY"), EMcpParamKind::Number, false }, { TEXT("gridY"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateBlendTree[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("treeName"), EMcpParamKind::String, false }, { TEXT("blendParameters"), EMcpParamKind::Array, false }, { TEXT("children"), EMcpParamKind::Array, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateProceduralAnim[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("boneTracks"), EMcpParamKind::Array, true }, { TEXT("numFrames"), EMcpParamKind::Number, false }, { TEXT("frameRate"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateStateMachine[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("machineName"), EMcpParamKind::String, false }, { TEXT("states"), EMcpParamKind::Array, false }, { TEXT("transitions"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_SetupIk[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureVehicle[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("vehicleName"), EMcpParamKind::String, false }, { TEXT("vehicleType"), EMcpParamKind::String, false }, { TEXT("wheels"), EMcpParamKind::Array, false }, { TEXT("engine"), EMcpParamKind::Object, false }, { TEXT("transmission"), EMcpParamKind::Object, false }, { TEXT("mass"), EMcpParamKind::Number, false }, { TEXT("dragCoefficient"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetupPhysicsSimulation[] = { { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("physicsAssetName"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assignToMesh"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateAnimationAsset[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("assetType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetupRetargeting[] = { { TEXT("sourceSkeleton"), EMcpParamKind::String, true }, { TEXT("targetSkeleton"), EMcpParamKind::String, true }, { TEXT("assets"), EMcpParamKind::Array, false }, { TEXT("retargetAssets"), EMcpParamKind::Array, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("suffix"), EMcpParamKind::String, false }, { TEXT("overwrite"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_PlayMontage[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("montagePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("playRate"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreatePoseLibrary[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("directory"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetupRagdoll[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("blendWeight"), EMcpParamKind::Number, false }, { TEXT("skeletonPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ActivateRagdoll[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("activate"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateAnimationSequence[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("frameRate"), EMcpParamKind::Number, false }, { TEXT("numFrames"), EMcpParamKind::Number, false }, { TEXT("length"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetSequenceLength[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("length"), EMcpParamKind::Number, true }, { TEXT("frameRate"), EMcpParamKind::Number, false }, { TEXT("numFrames"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddBoneTrack[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("boneName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetBoneKey[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("boneName"), EMcpParamKind::String, true }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("frame"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("location"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_SetCurveKey[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("curveName"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::Number, false }, { TEXT("frame"), EMcpParamKind::Number, false }, { TEXT("createIfMissing"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddNotify[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("notifyName"), EMcpParamKind::String, false }, { TEXT("notifyClass"), EMcpParamKind::String, false }, { TEXT("frame"), EMcpParamKind::Number, false }, { TEXT("trackIndex"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddNotifyState[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("notifyClass"), EMcpParamKind::String, false }, { TEXT("startFrame"), EMcpParamKind::Number, false }, { TEXT("endFrame"), EMcpParamKind::Number, false }, { TEXT("trackIndex"), EMcpParamKind::Number, false }, { TEXT("notifyName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddSyncMarker[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("markerName"), EMcpParamKind::String, true }, { TEXT("frame"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetRootMotionSettings[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("enableRootMotion"), EMcpParamKind::Bool, false }, { TEXT("rootMotionRootLock"), EMcpParamKind::String, false }, { TEXT("forceRootLock"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetAdditiveSettings[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("additiveAnimType"), EMcpParamKind::String, false }, { TEXT("basePoseType"), EMcpParamKind::String, false }, { TEXT("basePoseAnimation"), EMcpParamKind::String, false }, { TEXT("basePoseFrame"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateMontage[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("length"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddMontageSection[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("sectionName"), EMcpParamKind::String, true }, { TEXT("startTime"), EMcpParamKind::Number, false }, { TEXT("montagePath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("time"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddMontageSlot[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("animationPath"), EMcpParamKind::String, true }, { TEXT("startTime"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetSectionTiming[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("sectionName"), EMcpParamKind::String, true }, { TEXT("startTime"), EMcpParamKind::Number, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddMontageNotify[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("notifyName"), EMcpParamKind::String, false }, { TEXT("time"), EMcpParamKind::Number, false }, { TEXT("notifyClass"), EMcpParamKind::String, false }, { TEXT("trackIndex"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetBlendIn[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blendTime"), EMcpParamKind::Number, false }, { TEXT("time"), EMcpParamKind::Number, false }, { TEXT("blendOption"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetBlendOut[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blendTime"), EMcpParamKind::Number, false }, { TEXT("time"), EMcpParamKind::Number, false }, { TEXT("blendOption"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_LinkSections[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("fromSection"), EMcpParamKind::String, true }, { TEXT("toSection"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateBlendSpace1D[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("axisName"), EMcpParamKind::String, false }, { TEXT("axisMin"), EMcpParamKind::Number, false }, { TEXT("axisMax"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateBlendSpace2D[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("horizontalAxisName"), EMcpParamKind::String, false }, { TEXT("horizontalMin"), EMcpParamKind::Number, false }, { TEXT("horizontalMax"), EMcpParamKind::Number, false }, { TEXT("verticalAxisName"), EMcpParamKind::String, false }, { TEXT("verticalMin"), EMcpParamKind::Number, false }, { TEXT("verticalMax"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddBlendSample[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("animationPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("sampleValue"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ForceRebuildBlendSpace[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("rebuildBlendParameters"), EMcpParamKind::Bool, false }, { TEXT("compileReferencers"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetAxisSettings[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("minValue"), EMcpParamKind::Number, false }, { TEXT("maxValue"), EMcpParamKind::Number, false }, { TEXT("axisName"), EMcpParamKind::String, false }, { TEXT("axis"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gridDivisions"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetInterpolationSettings[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("interpolationType"), EMcpParamKind::String, false }, { TEXT("targetWeightInterpolationSpeed"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateAimOffset[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddAimOffsetSample[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("animationPath"), EMcpParamKind::String, true }, { TEXT("yaw"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateAnimBlueprint[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddStateMachine[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("machineName"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("stateMachineName"), EMcpParamKind::String, false }, { TEXT("positionX"), EMcpParamKind::Number, false }, { TEXT("positionY"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddState[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("stateName"), EMcpParamKind::String, true }, { TEXT("machineName"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("stateMachineName"), EMcpParamKind::String, false }, { TEXT("positionX"), EMcpParamKind::Number, false }, { TEXT("positionY"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddTransition[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("machineName"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("stateMachineName"), EMcpParamKind::String, false }, { TEXT("fromState"), EMcpParamKind::String, true }, { TEXT("toState"), EMcpParamKind::String, true }, { TEXT("crossfadeDuration"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetTransitionRules[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("machineName"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("stateMachineName"), EMcpParamKind::String, false }, { TEXT("fromState"), EMcpParamKind::String, false }, { TEXT("toState"), EMcpParamKind::String, false }, { TEXT("crossfadeDuration"), EMcpParamKind::Number, false }, { TEXT("priorityOrder"), EMcpParamKind::Number, false }, { TEXT("automaticRule"), EMcpParamKind::Bool, false }, { TEXT("bidirectional"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddBlendNode[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("nodeName"), EMcpParamKind::String, false }, { TEXT("blendType"), EMcpParamKind::String, false }, { TEXT("positionX"), EMcpParamKind::Number, false }, { TEXT("positionY"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddCachedPose[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("cacheName"), EMcpParamKind::String, true }, { TEXT("positionX"), EMcpParamKind::Number, false }, { TEXT("positionY"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddSlotNode[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("groupName"), EMcpParamKind::String, false }, { TEXT("positionX"), EMcpParamKind::Number, false }, { TEXT("positionY"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddLayeredBlendPerBone[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("boneName"), EMcpParamKind::String, false }, { TEXT("positionX"), EMcpParamKind::Number, false }, { TEXT("positionY"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetAnimGraphNodeValue[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("nodeName"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("value"), EMcpParamKind::Any, true } };
inline const FMcpParamDecl P_CreateControlRig[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false }, { TEXT("modularRig"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateIkRig[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateIkRetargeter[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("sourceIKRigPath"), EMcpParamKind::String, false }, { TEXT("targetIKRigPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetRetargetChainMapping[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("sourceChain"), EMcpParamKind::String, true }, { TEXT("targetChain"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_GetAnimationInfo[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_BindAnimNotify[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("notifyName"), EMcpParamKind::String, true }, { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("targetClass"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("positionX"), EMcpParamKind::Number, false }, { TEXT("positionY"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_GetSkeletonInfo[] = { { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ListBones[] = { { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ListSockets[] = { { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateSocket[] = { { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false }, { TEXT("socketName"), EMcpParamKind::String, true }, { TEXT("attachBoneName"), EMcpParamKind::String, false }, { TEXT("boneName"), EMcpParamKind::String, false }, { TEXT("relativeLocation"), EMcpParamKind::Object, false }, { TEXT("relativeRotation"), EMcpParamKind::Object, false }, { TEXT("relativeScale"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureSocket[] = { { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false }, { TEXT("socketName"), EMcpParamKind::String, true }, { TEXT("attachBoneName"), EMcpParamKind::String, false }, { TEXT("relativeLocation"), EMcpParamKind::Object, false }, { TEXT("relativeRotation"), EMcpParamKind::Object, false }, { TEXT("relativeScale"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateVirtualBone[] = { { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("sourceBoneName"), EMcpParamKind::String, true }, { TEXT("targetBoneName"), EMcpParamKind::String, true }, { TEXT("boneName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreatePhysicsAsset[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ListPhysicsBodies[] = { { TEXT("physicsAssetPath"), EMcpParamKind::String, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddPhysicsBody[] = { { TEXT("physicsAssetPath"), EMcpParamKind::String, true }, { TEXT("boneName"), EMcpParamKind::String, true }, { TEXT("bodyType"), EMcpParamKind::String, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("length"), EMcpParamKind::Number, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false }, { TEXT("depth"), EMcpParamKind::Number, false }, { TEXT("center"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigurePhysicsBody[] = { { TEXT("physicsAssetPath"), EMcpParamKind::String, true }, { TEXT("boneName"), EMcpParamKind::String, true }, { TEXT("mass"), EMcpParamKind::Number, false }, { TEXT("linearDamping"), EMcpParamKind::Number, false }, { TEXT("angularDamping"), EMcpParamKind::Number, false }, { TEXT("collisionEnabled"), EMcpParamKind::Bool, false }, { TEXT("simulatePhysics"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddPhysicsConstraint[] = { { TEXT("physicsAssetPath"), EMcpParamKind::String, true }, { TEXT("bodyA"), EMcpParamKind::String, true }, { TEXT("bodyB"), EMcpParamKind::String, true }, { TEXT("constraintName"), EMcpParamKind::String, false }, { TEXT("limits"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureConstraintLimits[] = { { TEXT("physicsAssetPath"), EMcpParamKind::String, true }, { TEXT("bodyA"), EMcpParamKind::String, true }, { TEXT("bodyB"), EMcpParamKind::String, true }, { TEXT("limits"), EMcpParamKind::Object, false }, { TEXT("swing1LimitAngle"), EMcpParamKind::Number, false }, { TEXT("swing2LimitAngle"), EMcpParamKind::Number, false }, { TEXT("twistLimitAngle"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_RenameBone[] = { { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("boneName"), EMcpParamKind::String, true }, { TEXT("newBoneName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetBoneTransform[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("boneName"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateMorphTarget[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, true }, { TEXT("morphTargetName"), EMcpParamKind::String, true }, { TEXT("deltas"), EMcpParamKind::Array, true }, { TEXT("lodIndex"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetMorphTargetDeltas[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, true }, { TEXT("morphTargetName"), EMcpParamKind::String, true }, { TEXT("deltas"), EMcpParamKind::Array, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ImportMorphTargets[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, true }, { TEXT("morphTargetPath"), EMcpParamKind::String, false }, { TEXT("sourcePath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_NormalizeWeights[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_PruneWeights[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, true }, { TEXT("threshold"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_BindClothToSkeletalMesh[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, true }, { TEXT("clothAssetName"), EMcpParamKind::String, false }, { TEXT("meshLodIndex"), EMcpParamKind::Number, false }, { TEXT("sectionIndex"), EMcpParamKind::Number, false }, { TEXT("assetLodIndex"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AssignClothAssetToMesh[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateSkeleton[] = { { TEXT("path"), EMcpParamKind::String, false }, { TEXT("skeletonPath"), EMcpParamKind::String, false }, { TEXT("rootBoneName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddBone[] = { { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("boneName"), EMcpParamKind::String, true }, { TEXT("parentBone"), EMcpParamKind::String, false }, { TEXT("parentBoneName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_RemoveBone[] = { { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("boneName"), EMcpParamKind::String, true }, { TEXT("removeChildren"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetBoneParent[] = { { TEXT("skeletonPath"), EMcpParamKind::String, true }, { TEXT("boneName"), EMcpParamKind::String, true }, { TEXT("parentBone"), EMcpParamKind::String, false }, { TEXT("newParentBone"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetVertexWeights[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, true }, { TEXT("profileName"), EMcpParamKind::String, false }, { TEXT("weights"), EMcpParamKind::Array, true }, { TEXT("lodIndex"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AutoSkinWeights[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CopyWeights[] = { { TEXT("sourceMeshPath"), EMcpParamKind::String, true }, { TEXT("targetMeshPath"), EMcpParamKind::String, true }, { TEXT("profileName"), EMcpParamKind::String, false }, { TEXT("lodIndex"), EMcpParamKind::Number, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_MirrorWeights[] = { { TEXT("skeletalMeshPath"), EMcpParamKind::String, true }, { TEXT("axis"), EMcpParamKind::String, false }, { TEXT("profileName"), EMcpParamKind::String, false }, { TEXT("lodIndex"), EMcpParamKind::Number, false } };

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

#define MCP_AP_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, Flags)           \
class FMcpCall_AnimationPhysics_##ClassSuffix final : public FMcpCall                    \
{                                                                                        \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl Decl{ TEXT("animation_physics"), TEXT(ActionLiteral),  \
			ParamsArray, (Flags) };                                                      \
		return Decl;                                                                     \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

// Core (AnimationHandlers.cpp)
MCP_AP_CALL(Cleanup, "cleanup", P_Cleanup, HandleAnimPhysCleanup, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateBlendSpace, "create_blend_space", P_CreateBlendSpace, HandleAnimPhysCreateBlendSpace, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateBlendTree, "create_blend_tree", P_CreateBlendTree, HandleAnimPhysCreateBlendTree, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateProceduralAnim, "create_procedural_anim", P_CreateProceduralAnim, HandleAnimPhysCreateProceduralAnim, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateStateMachine, "create_state_machine", P_CreateStateMachine, HandleAnimPhysCreateStateMachine, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetupIk, "setup_ik", P_SetupIk, HandleAnimPhysSetupIk, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ConfigureVehicle, "configure_vehicle", P_ConfigureVehicle, HandleAnimPhysConfigureVehicle, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetupPhysicsSimulation, "setup_physics_simulation", P_SetupPhysicsSimulation, HandleAnimPhysSetupPhysicsSimulation, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateAnimationAsset, "create_animation_asset", P_CreateAnimationAsset, HandleAnimPhysCreateAnimationAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetupRetargeting, "setup_retargeting", P_SetupRetargeting, HandleAnimPhysSetupRetargeting, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(PlayMontage, "play_montage", P_PlayMontage, HandleAnimPhysPlayMontage, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreatePoseLibrary, "create_pose_library", P_CreatePoseLibrary, HandleAnimPhysCreatePoseLibrary, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetupRagdoll, "setup_ragdoll", P_SetupRagdoll, HandleAnimPhysSetupRagdoll, EMcpCallFlags::None)
MCP_AP_CALL(ActivateRagdoll, "activate_ragdoll", P_ActivateRagdoll, HandleAnimPhysActivateRagdoll, EMcpCallFlags::None)

// Animation authoring (AnimationAuthoringHandlers.cpp)
MCP_AP_CALL(CreateAnimationSequence, "create_animation_sequence", P_CreateAnimationSequence, HandleAnimAuthoringCreateAnimationSequence, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetSequenceLength, "set_sequence_length", P_SetSequenceLength, HandleAnimAuthoringSetSequenceLength, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddBoneTrack, "add_bone_track", P_AddBoneTrack, HandleAnimAuthoringAddBoneTrack, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetBoneKey, "set_bone_key", P_SetBoneKey, HandleAnimAuthoringSetBoneKey, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetCurveKey, "set_curve_key", P_SetCurveKey, HandleAnimAuthoringSetCurveKey, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddNotify, "add_notify", P_AddNotify, HandleAnimAuthoringAddNotify, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddNotifyState, "add_notify_state", P_AddNotifyState, HandleAnimAuthoringAddNotifyState, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddSyncMarker, "add_sync_marker", P_AddSyncMarker, HandleAnimAuthoringAddSyncMarker, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetRootMotionSettings, "set_root_motion_settings", P_SetRootMotionSettings, HandleAnimAuthoringSetRootMotionSettings, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetAdditiveSettings, "set_additive_settings", P_SetAdditiveSettings, HandleAnimAuthoringSetAdditiveSettings, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateMontage, "create_montage", P_CreateMontage, HandleAnimAuthoringCreateMontage, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddMontageSection, "add_montage_section", P_AddMontageSection, HandleAnimAuthoringAddMontageSection, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddMontageSlot, "add_montage_slot", P_AddMontageSlot, HandleAnimAuthoringAddMontageSlot, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetSectionTiming, "set_section_timing", P_SetSectionTiming, HandleAnimAuthoringSetSectionTiming, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddMontageNotify, "add_montage_notify", P_AddMontageNotify, HandleAnimAuthoringAddMontageNotify, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetBlendIn, "set_blend_in", P_SetBlendIn, HandleAnimAuthoringSetBlendIn, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetBlendOut, "set_blend_out", P_SetBlendOut, HandleAnimAuthoringSetBlendOut, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(LinkSections, "link_sections", P_LinkSections, HandleAnimAuthoringLinkSections, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateBlendSpace1D, "create_blend_space_1d", P_CreateBlendSpace1D, HandleAnimAuthoringCreateBlendSpace1D, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateBlendSpace2D, "create_blend_space_2d", P_CreateBlendSpace2D, HandleAnimAuthoringCreateBlendSpace2D, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddBlendSample, "add_blend_sample", P_AddBlendSample, HandleAnimAuthoringAddBlendSample, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ForceRebuildBlendSpace, "force_rebuild_blend_space", P_ForceRebuildBlendSpace, HandleAnimAuthoringForceRebuildBlendSpace, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetAxisSettings, "set_axis_settings", P_SetAxisSettings, HandleAnimAuthoringSetAxisSettings, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetInterpolationSettings, "set_interpolation_settings", P_SetInterpolationSettings, HandleAnimAuthoringSetInterpolationSettings, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateAimOffset, "create_aim_offset", P_CreateAimOffset, HandleAnimAuthoringCreateAimOffset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddAimOffsetSample, "add_aim_offset_sample", P_AddAimOffsetSample, HandleAnimAuthoringAddAimOffsetSample, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateAnimBlueprint, "create_anim_blueprint", P_CreateAnimBlueprint, HandleAnimAuthoringCreateAnimBlueprint, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddStateMachine, "add_state_machine", P_AddStateMachine, HandleAnimAuthoringAddStateMachine, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddState, "add_state", P_AddState, HandleAnimAuthoringAddState, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddTransition, "add_transition", P_AddTransition, HandleAnimAuthoringAddTransition, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetTransitionRules, "set_transition_rules", P_SetTransitionRules, HandleAnimAuthoringSetTransitionRules, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddBlendNode, "add_blend_node", P_AddBlendNode, HandleAnimAuthoringAddBlendNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddCachedPose, "add_cached_pose", P_AddCachedPose, HandleAnimAuthoringAddCachedPose, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddSlotNode, "add_slot_node", P_AddSlotNode, HandleAnimAuthoringAddSlotNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddLayeredBlendPerBone, "add_layered_blend_per_bone", P_AddLayeredBlendPerBone, HandleAnimAuthoringAddLayeredBlendPerBone, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetAnimGraphNodeValue, "set_anim_graph_node_value", P_SetAnimGraphNodeValue, HandleAnimAuthoringSetAnimGraphNodeValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateControlRig, "create_control_rig", P_CreateControlRig, HandleAnimAuthoringCreateControlRig, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateIkRig, "create_ik_rig", P_CreateIkRig, HandleAnimAuthoringCreateIkRig, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateIkRetargeter, "create_ik_retargeter", P_CreateIkRetargeter, HandleAnimAuthoringCreateIkRetargeter, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetRetargetChainMapping, "set_retarget_chain_mapping", P_SetRetargetChainMapping, HandleAnimAuthoringSetRetargetChainMapping, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(GetAnimationInfo, "get_animation_info", P_GetAnimationInfo, HandleAnimAuthoringGetAnimationInfo, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(BindAnimNotify, "bind_anim_notify", P_BindAnimNotify, HandleAnimAuthoringBindAnimNotify, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Skeleton, physics assets, skin weights, cloth, morphs (SkeletonHandlers.cpp)
MCP_AP_CALL(GetSkeletonInfo, "get_skeleton_info", P_GetSkeletonInfo, HandleSkeletonGetSkeletonInfo, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(ListBones, "list_bones", P_ListBones, HandleSkeletonListBones, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(ListSockets, "list_sockets", P_ListSockets, HandleSkeletonListSockets, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(CreateSocket, "create_socket", P_CreateSocket, HandleSkeletonCreateSocket, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ConfigureSocket, "configure_socket", P_ConfigureSocket, HandleSkeletonConfigureSocket, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateVirtualBone, "create_virtual_bone", P_CreateVirtualBone, HandleSkeletonCreateVirtualBone, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreatePhysicsAsset, "create_physics_asset", P_CreatePhysicsAsset, HandleSkeletonCreatePhysicsAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ListPhysicsBodies, "list_physics_bodies", P_ListPhysicsBodies, HandleSkeletonListPhysicsBodies, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(AddPhysicsBody, "add_physics_body", P_AddPhysicsBody, HandleSkeletonAddPhysicsBody, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ConfigurePhysicsBody, "configure_physics_body", P_ConfigurePhysicsBody, HandleSkeletonConfigurePhysicsBody, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddPhysicsConstraint, "add_physics_constraint", P_AddPhysicsConstraint, HandleSkeletonAddPhysicsConstraint, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ConfigureConstraintLimits, "configure_constraint_limits", P_ConfigureConstraintLimits, HandleSkeletonConfigureConstraintLimits, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(RenameBone, "rename_bone", P_RenameBone, HandleSkeletonRenameBone, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetBoneTransform, "set_bone_transform", P_SetBoneTransform, HandleSkeletonSetBoneTransform, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(CreateMorphTarget, "create_morph_target", P_CreateMorphTarget, HandleSkeletonCreateMorphTarget, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetMorphTargetDeltas, "set_morph_target_deltas", P_SetMorphTargetDeltas, HandleSkeletonSetMorphTargetDeltas, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(ImportMorphTargets, "import_morph_targets", P_ImportMorphTargets, HandleSkeletonImportMorphTargets, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(NormalizeWeights, "normalize_weights", P_NormalizeWeights, HandleSkeletonNormalizeWeights, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(PruneWeights, "prune_weights", P_PruneWeights, HandleSkeletonPruneWeights, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(BindClothToSkeletalMesh, "bind_cloth_to_skeletal_mesh", P_BindClothToSkeletalMesh, HandleSkeletonBindClothToSkeletalMesh, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AssignClothAssetToMesh, "assign_cloth_asset_to_mesh", P_AssignClothAssetToMesh, HandleSkeletonAssignClothAssetToMesh, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(CreateSkeleton, "create_skeleton", P_CreateSkeleton, HandleSkeletonCreateSkeleton, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AddBone, "add_bone", P_AddBone, HandleSkeletonAddBone, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(RemoveBone, "remove_bone", P_RemoveBone, HandleSkeletonRemoveBone, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetBoneParent, "set_bone_parent", P_SetBoneParent, HandleSkeletonSetBoneParent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(SetVertexWeights, "set_vertex_weights", P_SetVertexWeights, HandleSkeletonSetVertexWeights, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AP_CALL(AutoSkinWeights, "auto_skin_weights", P_AutoSkinWeights, HandleSkeletonAutoSkinWeights, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(CopyWeights, "copy_weights", P_CopyWeights, HandleSkeletonCopyWeights, EMcpCallFlags::RequiresEditor)
MCP_AP_CALL(MirrorWeights, "mirror_weights", P_MirrorWeights, HandleSkeletonMirrorWeights, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

#undef MCP_AP_CALL

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
	Registry.RegisterCall(MakeUnique<FMcpCall_AnimationPhysics_SetupRagdoll>());
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
