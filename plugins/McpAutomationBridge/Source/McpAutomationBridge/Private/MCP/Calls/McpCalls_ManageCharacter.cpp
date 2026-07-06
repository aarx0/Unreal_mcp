// LINT-TOOL: manage_character
// manage_character as FMcpCall classes — ninth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers (HandleCharacter*, CharacterHandlers.cpp) until the
// module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageCharacter
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations
// (McpDecl_ManageCharacter.h) and re-verified against the extracted
// HandleCharacter* bodies. Every row declares name/path/blueprintPath because
// the retired dispatcher's prologue read all three for every action;
// the spellings a body does not read stay declared-optional so payloads the
// family accepts today keep validating (create_character_blueprint reads
// name/path, the other 26 read blueprintPath).

inline const FMcpParamDecl P_CreateCharacterBlueprint[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureCapsuleComponent[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("capsuleRadius"), EMcpParamKind::Number, false }, { TEXT("capsuleHalfHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureMeshComponent[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false }, { TEXT("animBlueprintPath"), EMcpParamKind::String, false }, { TEXT("meshOffset"), EMcpParamKind::Object, false }, { TEXT("meshRotation"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ConfigureCameraComponent[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("springArmLength"), EMcpParamKind::Number, false }, { TEXT("cameraUsePawnControlRotation"), EMcpParamKind::Bool, false }, { TEXT("springArmLagEnabled"), EMcpParamKind::Bool, false }, { TEXT("springArmLagSpeed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureMovementSpeeds[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("walkSpeed"), EMcpParamKind::Number, false }, { TEXT("runSpeed"), EMcpParamKind::Number, false }, { TEXT("sprintSpeed"), EMcpParamKind::Number, false }, { TEXT("crouchSpeed"), EMcpParamKind::Number, false }, { TEXT("swimSpeed"), EMcpParamKind::Number, false }, { TEXT("flySpeed"), EMcpParamKind::Number, false }, { TEXT("acceleration"), EMcpParamKind::Number, false }, { TEXT("deceleration"), EMcpParamKind::Number, false }, { TEXT("groundFriction"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureJump[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("gravityScale"), EMcpParamKind::Number, false }, { TEXT("jumpHeight"), EMcpParamKind::Number, false }, { TEXT("airControl"), EMcpParamKind::Number, false }, { TEXT("fallingLateralFriction"), EMcpParamKind::Number, false }, { TEXT("maxJumpCount"), EMcpParamKind::Number, false }, { TEXT("jumpHoldTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureRotation[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("orientToMovement"), EMcpParamKind::Bool, false }, { TEXT("useControllerRotationYaw"), EMcpParamKind::Bool, false }, { TEXT("useControllerRotationPitch"), EMcpParamKind::Bool, false }, { TEXT("useControllerRotationRoll"), EMcpParamKind::Bool, false }, { TEXT("rotationRate"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddCustomMovementMode[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("modeName"), EMcpParamKind::String, false }, { TEXT("modeId"), EMcpParamKind::Number, false }, { TEXT("customSpeed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureNavMovement[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("navAgentRadius"), EMcpParamKind::Number, false }, { TEXT("navAgentHeight"), EMcpParamKind::Number, false }, { TEXT("avoidanceEnabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetupMantling[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("mantleHeight"), EMcpParamKind::Number, false }, { TEXT("mantleReachDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetupVaulting[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("vaultHeight"), EMcpParamKind::Number, false }, { TEXT("vaultDepth"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetupClimbing[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("climbSpeed"), EMcpParamKind::Number, false }, { TEXT("climbableTag"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetupSliding[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("slideSpeed"), EMcpParamKind::Number, false }, { TEXT("slideDuration"), EMcpParamKind::Number, false }, { TEXT("slideCooldown"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetupWallRunning[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("wallRunSpeed"), EMcpParamKind::Number, false }, { TEXT("wallRunDuration"), EMcpParamKind::Number, false }, { TEXT("wallRunGravityScale"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetupGrappling[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("grappleRange"), EMcpParamKind::Number, false }, { TEXT("grappleSpeed"), EMcpParamKind::Number, false }, { TEXT("grappleTargetTag"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetupFootstepSystem[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("footstepEnabled"), EMcpParamKind::Bool, false }, { TEXT("footstepSocketLeft"), EMcpParamKind::String, false }, { TEXT("footstepSocketRight"), EMcpParamKind::String, false }, { TEXT("footstepTraceDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_MapSurfaceToSound[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("surfaceType"), EMcpParamKind::String, true }, { TEXT("soundPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureFootstepFx[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("volumeMultiplier"), EMcpParamKind::Number, false }, { TEXT("particleScale"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_GetCharacterInfo[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetupMovement[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("walkSpeed"), EMcpParamKind::Number, false }, { TEXT("runSpeed"), EMcpParamKind::Number, false }, { TEXT("acceleration"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetWalkSpeed[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("walkSpeed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetJumpHeight[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("jumpHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetGravityScale[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("gravityScale"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetGroundFriction[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("groundFriction"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetBrakingDeceleration[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("brakingDeceleration"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureCrouch[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("crouchSpeed"), EMcpParamKind::Number, false }, { TEXT("crouchedHalfHeight"), EMcpParamKind::Number, false }, { TEXT("canCrouch"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureSprint[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("sprintSpeed"), EMcpParamKind::Number, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row (the implementation TU is editor-
// gated; the members answer the EDITOR_ONLY stub in non-editor builds).
// Mutating on all writers; the only reader is get_info.

#define MCP_MC_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)        \
class FMcpCall_ManageCharacter_##ClassSuffix final : public FMcpCall                       \
{                                                                                          \
	const FMcpCallDecl& GetDecl() const override                                           \
	{                                                                                      \
		static const FMcpCallDecl Decl{ TEXT("manage_character"), TEXT(ActionLiteral),     \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                   \
		return Decl;                                                                       \
	}                                                                                      \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                   \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override   \
	{                                                                                      \
		return S.HandlerFn(RequestId, Payload, Socket);                                    \
	}                                                                                      \
};

// Character creation (14.1)
MCP_MC_CALL(CreateBlueprint, "create_character_blueprint", P_CreateCharacterBlueprint, HandleCharacterCreateBlueprint, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureCapsuleComponent, "configure_capsule_component", P_ConfigureCapsuleComponent, HandleCharacterConfigureCapsuleComponent, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureMeshComponent, "configure_mesh_component", P_ConfigureMeshComponent, HandleCharacterConfigureMeshComponent, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureCameraComponent, "configure_camera_component", P_ConfigureCameraComponent, HandleCharacterConfigureCameraComponent, EMcpCallFlags::Mutating)

// Movement component (14.2)
MCP_MC_CALL(ConfigureMovementSpeeds, "configure_movement_speeds", P_ConfigureMovementSpeeds, HandleCharacterConfigureMovementSpeeds, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureJump, "configure_jump", P_ConfigureJump, HandleCharacterConfigureJump, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureRotation, "configure_rotation", P_ConfigureRotation, HandleCharacterConfigureRotation, EMcpCallFlags::Mutating)
MCP_MC_CALL(AddCustomMovementMode, "add_custom_movement_mode", P_AddCustomMovementMode, HandleCharacterAddCustomMovementMode, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureNavMovement, "configure_nav_movement", P_ConfigureNavMovement, HandleCharacterConfigureNavMovement, EMcpCallFlags::Mutating)

// Advanced movement (14.3)
MCP_MC_CALL(SetupMantling, "setup_mantling", P_SetupMantling, HandleCharacterSetupMantling, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetupVaulting, "setup_vaulting", P_SetupVaulting, HandleCharacterSetupVaulting, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetupClimbing, "setup_climbing", P_SetupClimbing, HandleCharacterSetupClimbing, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetupSliding, "setup_sliding", P_SetupSliding, HandleCharacterSetupSliding, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetupWallRunning, "setup_wall_running", P_SetupWallRunning, HandleCharacterSetupWallRunning, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetupGrappling, "setup_grappling", P_SetupGrappling, HandleCharacterSetupGrappling, EMcpCallFlags::Mutating)

// Footstep system (14.4)
MCP_MC_CALL(SetupFootstepSystem, "setup_footstep_system", P_SetupFootstepSystem, HandleCharacterSetupFootstepSystem, EMcpCallFlags::Mutating)
MCP_MC_CALL(MapSurfaceToSound, "map_surface_to_sound", P_MapSurfaceToSound, HandleCharacterMapSurfaceToSound, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureFootstepFx, "configure_footstep_fx", P_ConfigureFootstepFx, HandleCharacterConfigureFootstepFx, EMcpCallFlags::Mutating)

// Utility
MCP_MC_CALL(GetInfo, "get_info", P_GetCharacterInfo, HandleCharacterGetInfo, EMcpCallFlags::None)

// Aliases & convenience setters
MCP_MC_CALL(SetupMovement, "setup_movement", P_SetupMovement, HandleCharacterSetupMovement, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetWalkSpeed, "set_walk_speed", P_SetWalkSpeed, HandleCharacterSetWalkSpeed, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetJumpHeight, "set_jump_height", P_SetJumpHeight, HandleCharacterSetJumpHeight, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetGravityScale, "set_gravity_scale", P_SetGravityScale, HandleCharacterSetGravityScale, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetGroundFriction, "set_ground_friction", P_SetGroundFriction, HandleCharacterSetGroundFriction, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetBrakingDeceleration, "set_braking_deceleration", P_SetBrakingDeceleration, HandleCharacterSetBrakingDeceleration, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureCrouch, "configure_crouch", P_ConfigureCrouch, HandleCharacterConfigureCrouch, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureSprint, "configure_sprint", P_ConfigureSprint, HandleCharacterConfigureSprint, EMcpCallFlags::Mutating)

#undef MCP_MC_CALL

} // namespace McpCalls::ManageCharacter

void McpRegisterManageCharacterCalls()
{
	using namespace McpCalls::ManageCharacter;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_CreateBlueprint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_ConfigureCapsuleComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_ConfigureMeshComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_ConfigureCameraComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_ConfigureMovementSpeeds>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_ConfigureJump>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_ConfigureRotation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_AddCustomMovementMode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_ConfigureNavMovement>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetupMantling>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetupVaulting>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetupClimbing>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetupSliding>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetupWallRunning>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetupGrappling>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetupFootstepSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_MapSurfaceToSound>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_ConfigureFootstepFx>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_GetInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetupMovement>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetWalkSpeed>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetJumpHeight>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetGravityScale>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetGroundFriction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetBrakingDeceleration>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_ConfigureCrouch>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_ConfigureSprint>());
}
