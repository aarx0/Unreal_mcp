// LINT-TOOL: manage_character
// LINT-SCHEMA-DERIVED
// manage_character as FMcpCall classes — ninth classed family
// (docs/action-declarations.md). Adopts schema-from-decls: each class AUTHORS
// its schema fragment in a S_<Suffix>() function; the published facade schema
// folds those fragments and GetDecl() derives the validation decl from the same
// fragment via McpDeriveDecl(), so schema and decl are one source and cannot
// drift. Run() delegates to the namespaced free handlers (McpHandlers::Character::HandleCharacter*,
// McpAutomationBridge_CharacterHandlers.cpp).
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_CharacterHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageCharacter
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. Every fragment authors
// name/path/blueprintPath because the retired dispatcher's prologue read all
// three: create_character_blueprint reads name/path (name required); the other
// 26 read blueprintPath (required). The spellings a body does not read stay
// declared-optional so payloads the family accepts today keep validating.

static void S_CreateBlueprint(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .Required({TEXT("name")});
}

static void S_ConfigureCapsuleComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("capsuleRadius"), TEXT("Capsule radius in cm; writes the CapsuleComponent unscaled radius (clamped against half-height)."))
	 .Number(TEXT("capsuleHalfHeight"), TEXT("Capsule half-height in cm; writes the CapsuleComponent unscaled half-height."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureMeshComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
	 .String(TEXT("animBlueprintPath"), TEXT("Path to animation blueprint."))
	 .Object(TEXT("meshOffset"), TEXT("Mesh location offset."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Object(TEXT("meshRotation"), TEXT("Mesh rotation offset."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
		})
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureCameraComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("springArmLength"), TEXT("Camera boom length in cm; writes the SpringArm TargetArmLength (component created if missing)."))
	 .Bool(TEXT("cameraUsePawnControlRotation"), TEXT("Camera follows controller rotation."))
	 .Bool(TEXT("springArmLagEnabled"), TEXT("Enable camera lag."))
	 .Number(TEXT("springArmLagSpeed"), TEXT("Camera lag speed."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureMovementSpeeds(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("walkSpeed"), TEXT("Ground speed; writes CharacterMovement MaxWalkSpeed."))
	 .Number(TEXT("runSpeed"), TEXT("Run speed; writes MaxWalkSpeed when walkSpeed is absent, otherwise stored as the RunSpeed variable default."))
	 .Number(TEXT("sprintSpeed"), TEXT("Sprint speed; stored as the SprintSpeed variable default (no CharacterMovement property)."))
	 .Number(TEXT("crouchSpeed"), TEXT("Crouched walk speed; writes CharacterMovement MaxWalkSpeedCrouched."))
	 .Number(TEXT("swimSpeed"), TEXT("Swim speed; writes CharacterMovement MaxSwimSpeed."))
	 .Number(TEXT("flySpeed"), TEXT("Fly speed; writes CharacterMovement MaxFlySpeed."))
	 .Number(TEXT("acceleration"), TEXT("Writes CharacterMovement MaxAcceleration."))
	 .Number(TEXT("deceleration"), TEXT("Writes CharacterMovement BrakingDecelerationWalking."))
	 .Number(TEXT("groundFriction"), TEXT("Writes CharacterMovement GroundFriction."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureJump(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("gravityScale"), TEXT("Gravity multiplier; writes CharacterMovement GravityScale."))
	 .Number(TEXT("jumpHeight"), TEXT("Desired jump apex height in cm; converted to JumpZVelocity via sqrt(2*g*h) using effective gravity."))
	 .Number(TEXT("airControl"), TEXT("Air steering fraction 0-1; writes CharacterMovement AirControl."))
	 .Number(TEXT("fallingLateralFriction"), TEXT("Air friction."))
	 .Integer(TEXT("maxJumpCount"), TEXT("Writes Character JumpMaxCount (2 = double jump)."))
	 .Number(TEXT("jumpHoldTime"), TEXT("Max hold time for variable jump."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureRotation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("orientToMovement"), TEXT("Orient rotation to movement direction."))
	 .Bool(TEXT("useControllerRotationYaw"), TEXT("Use controller yaw rotation."))
	 .Bool(TEXT("useControllerRotationPitch"), TEXT("Use controller pitch rotation."))
	 .Bool(TEXT("useControllerRotationRoll"), TEXT("Use controller roll rotation."))
	 .Number(TEXT("rotationRate"), TEXT("Yaw turn rate in degrees/second; writes CharacterMovement RotationRate (pitch/roll zeroed)."))
	 .Required({TEXT("blueprintPath")});
}

static void S_AddCustomMovementMode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("modeName"), TEXT("Name for custom movement mode."))
	 .Integer(TEXT("modeId"), TEXT("Custom movement mode ID."))
	 .Number(TEXT("customSpeed"), TEXT("add_custom_movement_mode: speed stored as the {modeName}Speed variable default and MaxCustomMovementSpeed."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureNavMovement(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("navAgentRadius"), TEXT("Nav agent radius in cm; writes NavAgentProps.AgentRadius."))
	 .Number(TEXT("navAgentHeight"), TEXT("Nav agent height in cm; writes NavAgentProps.AgentHeight."))
	 .Bool(TEXT("avoidanceEnabled"), TEXT("Enable AI avoidance."))
	 .Required({TEXT("blueprintPath")});
}








static void S_MapSurfaceToSound(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .StringEnum(TEXT("surfaceType"), {
		TEXT("Default"),
		TEXT("Concrete"),
		TEXT("Grass"),
		TEXT("Dirt"),
		TEXT("Metal"),
		TEXT("Wood"),
		TEXT("Water"),
		TEXT("Snow"),
		TEXT("Sand"),
		TEXT("Gravel"),
		TEXT("Custom")
	 }, TEXT("Physical surface type."))
	 .String(TEXT("soundPath"), TEXT("Sound asset path (SoundBase) mapped to surfaceType by map_surface_to_sound."))
	 .Required({TEXT("blueprintPath"), TEXT("surfaceType"), TEXT("soundPath")});
}


static void S_GetInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetupMovement(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("walkSpeed"), TEXT("Ground speed; writes CharacterMovement MaxWalkSpeed."))
	 .Number(TEXT("runSpeed"), TEXT("Run speed; writes MaxWalkSpeed when walkSpeed is absent, otherwise stored as the RunSpeed variable default."))
	 .Number(TEXT("acceleration"), TEXT("Writes CharacterMovement MaxAcceleration."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetWalkSpeed(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("walkSpeed"), TEXT("Ground speed; writes CharacterMovement MaxWalkSpeed."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetJumpHeight(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("jumpHeight"), TEXT("Desired jump apex height in cm; converted to JumpZVelocity via sqrt(2*g*h) using effective gravity."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetGravityScale(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("gravityScale"), TEXT("Gravity multiplier; writes CharacterMovement GravityScale."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetGroundFriction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("groundFriction"), TEXT("Writes CharacterMovement GroundFriction."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetBrakingDeceleration(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("brakingDeceleration"), TEXT("set_braking_deceleration: BrakingDecelerationWalking on CharacterMovement."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureCrouch(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("crouchSpeed"), TEXT("Crouched walk speed; writes CharacterMovement MaxWalkSpeedCrouched."))
	 .Number(TEXT("crouchedHalfHeight"), TEXT("configure_crouch: capsule half-height while crouched (default 44)."))
	 .Bool(TEXT("canCrouch"), TEXT("configure_crouch: whether the character can crouch (default true)."))
	 .Required({TEXT("blueprintPath")});
}


// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row (the implementation TU is editor-
// gated; the members answer the EDITOR_ONLY stub in non-editor builds).
// Mutating on all writers; the only reader is get_info.

#define MCP_MC_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                      \
class FMcpCall_ManageCharacter_##ClassSuffix final : public FMcpCall                        \
{                                                                                           \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }          \
	const FMcpCallDecl& GetDecl() const override                                            \
	{                                                                                       \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_character"),              \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),             \
			&S_##ClassSuffix);                                                              \
		return D;                                                                           \
	}                                                                                       \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                    \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override    \
	{                                                                                       \
		return HandlerFn(S, RequestId, Payload, Socket);                                     \
	}                                                                                       \
};

// Character creation (14.1)
MCP_MC_CALL(CreateBlueprint, "create_character_blueprint", McpHandlers::Character::HandleCharacterCreateBlueprint, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureCapsuleComponent, "configure_capsule_component", McpHandlers::Character::HandleCharacterConfigureCapsuleComponent, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureMeshComponent, "configure_mesh_component", McpHandlers::Character::HandleCharacterConfigureMeshComponent, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureCameraComponent, "configure_camera_component", McpHandlers::Character::HandleCharacterConfigureCameraComponent, EMcpCallFlags::Mutating)

// Movement component (14.2)
MCP_MC_CALL(ConfigureMovementSpeeds, "configure_movement_speeds", McpHandlers::Character::HandleCharacterConfigureMovementSpeeds, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureJump, "configure_jump", McpHandlers::Character::HandleCharacterConfigureJump, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureRotation, "configure_rotation", McpHandlers::Character::HandleCharacterConfigureRotation, EMcpCallFlags::Mutating)
MCP_MC_CALL(AddCustomMovementMode, "add_custom_movement_mode", McpHandlers::Character::HandleCharacterAddCustomMovementMode, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureNavMovement, "configure_nav_movement", McpHandlers::Character::HandleCharacterConfigureNavMovement, EMcpCallFlags::Mutating)

// Advanced movement (14.3)

// Footstep system (14.4)
MCP_MC_CALL(MapSurfaceToSound, "map_surface_to_sound", McpHandlers::Character::HandleCharacterMapSurfaceToSound, EMcpCallFlags::Mutating)

// Utility
MCP_MC_CALL(GetInfo, "get_info", McpHandlers::Character::HandleCharacterGetInfo, EMcpCallFlags::None)

// Aliases & convenience setters
MCP_MC_CALL(SetupMovement, "setup_movement", McpHandlers::Character::HandleCharacterSetupMovement, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetWalkSpeed, "set_walk_speed", McpHandlers::Character::HandleCharacterSetWalkSpeed, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetJumpHeight, "set_jump_height", McpHandlers::Character::HandleCharacterSetJumpHeight, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetGravityScale, "set_gravity_scale", McpHandlers::Character::HandleCharacterSetGravityScale, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetGroundFriction, "set_ground_friction", McpHandlers::Character::HandleCharacterSetGroundFriction, EMcpCallFlags::Mutating)
MCP_MC_CALL(SetBrakingDeceleration, "set_braking_deceleration", McpHandlers::Character::HandleCharacterSetBrakingDeceleration, EMcpCallFlags::Mutating)
MCP_MC_CALL(ConfigureCrouch, "configure_crouch", McpHandlers::Character::HandleCharacterConfigureCrouch, EMcpCallFlags::Mutating)

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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_MapSurfaceToSound>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_GetInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetupMovement>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetWalkSpeed>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetJumpHeight>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetGravityScale>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetGroundFriction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_SetBrakingDeceleration>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCharacter_ConfigureCrouch>());
}
