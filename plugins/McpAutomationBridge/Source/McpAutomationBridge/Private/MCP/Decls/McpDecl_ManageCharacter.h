// Action declarations for manage_character — the server's contract: which params
// each action reads, and which are required. Fleet-authored from handler
// source (three-witness cross-check, 2026-07-04), hand-maintained since:
// adding an action = adding its declaration here (the boot validation and
// tests/schema/action-decl-lint.ps1 enforce both directions).
// UnverifiedDecl = no reachable read path was attributable; validation skips
// those actions and the lint nags until someone verifies or removes them.
#pragma once

#include "MCP/McpCallRegistry.h"

namespace McpDecls
{
inline const FMcpParamDecl P_ManageCharacter_0[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("modeName"), EMcpParamKind::String, false }, { TEXT("modeId"), EMcpParamKind::Number, false }, { TEXT("customSpeed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_1[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("springArmLength"), EMcpParamKind::Number, false }, { TEXT("cameraUsePawnControlRotation"), EMcpParamKind::Bool, false }, { TEXT("springArmLagEnabled"), EMcpParamKind::Bool, false }, { TEXT("springArmLagSpeed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_2[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("capsuleRadius"), EMcpParamKind::Number, false }, { TEXT("capsuleHalfHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_3[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("crouchSpeed"), EMcpParamKind::Number, false }, { TEXT("crouchedHalfHeight"), EMcpParamKind::Number, false }, { TEXT("canCrouch"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageCharacter_4[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("volumeMultiplier"), EMcpParamKind::Number, false }, { TEXT("particleScale"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_5[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("gravityScale"), EMcpParamKind::Number, false }, { TEXT("jumpHeight"), EMcpParamKind::Number, false }, { TEXT("airControl"), EMcpParamKind::Number, false }, { TEXT("fallingLateralFriction"), EMcpParamKind::Number, false }, { TEXT("maxJumpCount"), EMcpParamKind::Number, false }, { TEXT("jumpHoldTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_6[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false }, { TEXT("animBlueprintPath"), EMcpParamKind::String, false }, { TEXT("meshOffset"), EMcpParamKind::Object, false }, { TEXT("meshRotation"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ManageCharacter_7[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("walkSpeed"), EMcpParamKind::Number, false }, { TEXT("runSpeed"), EMcpParamKind::Number, false }, { TEXT("sprintSpeed"), EMcpParamKind::Number, false }, { TEXT("crouchSpeed"), EMcpParamKind::Number, false }, { TEXT("swimSpeed"), EMcpParamKind::Number, false }, { TEXT("flySpeed"), EMcpParamKind::Number, false }, { TEXT("acceleration"), EMcpParamKind::Number, false }, { TEXT("deceleration"), EMcpParamKind::Number, false }, { TEXT("groundFriction"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_8[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("navAgentRadius"), EMcpParamKind::Number, false }, { TEXT("navAgentHeight"), EMcpParamKind::Number, false }, { TEXT("avoidanceEnabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageCharacter_9[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("orientToMovement"), EMcpParamKind::Bool, false }, { TEXT("useControllerRotationYaw"), EMcpParamKind::Bool, false }, { TEXT("useControllerRotationPitch"), EMcpParamKind::Bool, false }, { TEXT("useControllerRotationRoll"), EMcpParamKind::Bool, false }, { TEXT("rotationRate"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_10[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("sprintSpeed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_11[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCharacter_12[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageCharacter_13[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("surfaceType"), EMcpParamKind::String, true }, { TEXT("soundPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageCharacter_14[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("brakingDeceleration"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_15[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("gravityScale"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_16[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("groundFriction"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_17[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("jumpHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_18[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("walkSpeed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_19[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("climbSpeed"), EMcpParamKind::Number, false }, { TEXT("climbableTag"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCharacter_20[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("footstepEnabled"), EMcpParamKind::Bool, false }, { TEXT("footstepSocketLeft"), EMcpParamKind::String, false }, { TEXT("footstepSocketRight"), EMcpParamKind::String, false }, { TEXT("footstepTraceDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_21[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("grappleRange"), EMcpParamKind::Number, false }, { TEXT("grappleSpeed"), EMcpParamKind::Number, false }, { TEXT("grappleTargetTag"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCharacter_22[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("mantleHeight"), EMcpParamKind::Number, false }, { TEXT("mantleReachDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_23[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("walkSpeed"), EMcpParamKind::Number, false }, { TEXT("runSpeed"), EMcpParamKind::Number, false }, { TEXT("acceleration"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_24[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("slideSpeed"), EMcpParamKind::Number, false }, { TEXT("slideDuration"), EMcpParamKind::Number, false }, { TEXT("slideCooldown"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_25[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("vaultHeight"), EMcpParamKind::Number, false }, { TEXT("vaultDepth"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCharacter_26[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("wallRunSpeed"), EMcpParamKind::Number, false }, { TEXT("wallRunDuration"), EMcpParamKind::Number, false }, { TEXT("wallRunGravityScale"), EMcpParamKind::Number, false } };

inline const FMcpCallDecl GManageCharacter[] =
{
	{ TEXT("manage_character"), TEXT("add_custom_movement_mode"), P_ManageCharacter_0, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("configure_camera_component"), P_ManageCharacter_1, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("configure_capsule_component"), P_ManageCharacter_2, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("configure_crouch"), P_ManageCharacter_3, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("configure_footstep_fx"), P_ManageCharacter_4, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("configure_jump"), P_ManageCharacter_5, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("configure_mesh_component"), P_ManageCharacter_6, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("configure_movement_speeds"), P_ManageCharacter_7, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("configure_nav_movement"), P_ManageCharacter_8, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("configure_rotation"), P_ManageCharacter_9, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("configure_sprint"), P_ManageCharacter_10, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("create_character_blueprint"), P_ManageCharacter_11, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("get_character_info"), P_ManageCharacter_12, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("map_surface_to_sound"), P_ManageCharacter_13, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("set_braking_deceleration"), P_ManageCharacter_14, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("set_gravity_scale"), P_ManageCharacter_15, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("set_ground_friction"), P_ManageCharacter_16, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("set_jump_height"), P_ManageCharacter_17, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("set_walk_speed"), P_ManageCharacter_18, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("setup_climbing"), P_ManageCharacter_19, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("setup_footstep_system"), P_ManageCharacter_20, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("setup_grappling"), P_ManageCharacter_21, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("setup_mantling"), P_ManageCharacter_22, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("setup_movement"), P_ManageCharacter_23, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("setup_sliding"), P_ManageCharacter_24, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("setup_vaulting"), P_ManageCharacter_25, EMcpCallFlags::None },
	{ TEXT("manage_character"), TEXT("setup_wall_running"), P_ManageCharacter_26, EMcpCallFlags::None },
};
}
