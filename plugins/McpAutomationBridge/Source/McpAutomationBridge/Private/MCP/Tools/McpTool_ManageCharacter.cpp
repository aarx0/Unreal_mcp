// McpTool_ManageCharacter.cpp — manage_character tool definition (27 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageCharacter : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_character"); }

	FString GetDescription() const override
	{
		return TEXT("Create Character Blueprints with movement, "
			"locomotion, and animation state machines.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ManageCharacter(), TEXT("Character action to perform."))
			.String(TEXT("name"), TEXT("Name of the asset to create."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.StringEnum(TEXT("parentClass"), {
				TEXT("Character"),
				TEXT("ACharacter"),
				TEXT("PlayerCharacter"),
				TEXT("AICharacter")
			}, TEXT("Parent class for character blueprint."))
			.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
			.String(TEXT("animBlueprintPath"), TEXT("Path to animation blueprint."))
			.Number(TEXT("capsuleRadius"), TEXT(""))
			.Number(TEXT("capsuleHalfHeight"), TEXT(""))
			.Object(TEXT("meshOffset"), TEXT("Mesh location offset."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("meshRotation"), TEXT("Mesh rotation offset."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Bool(TEXT("cameraUsePawnControlRotation"), TEXT("Camera follows controller rotation."))
			.Number(TEXT("springArmLength"), TEXT(""))
			.Bool(TEXT("springArmLagEnabled"), TEXT("Enable camera lag."))
			.Number(TEXT("springArmLagSpeed"), TEXT("Camera lag speed."))
			.Number(TEXT("walkSpeed"), TEXT(""))
			.Number(TEXT("runSpeed"), TEXT(""))
			.Number(TEXT("sprintSpeed"), TEXT(""))
			.Number(TEXT("crouchSpeed"), TEXT(""))
			.Number(TEXT("swimSpeed"), TEXT(""))
			.Number(TEXT("flySpeed"), TEXT(""))
			.Number(TEXT("acceleration"), TEXT(""))
			.Number(TEXT("deceleration"), TEXT(""))
			.Number(TEXT("groundFriction"), TEXT(""))
			.Number(TEXT("jumpHeight"), TEXT(""))
			.Number(TEXT("airControl"), TEXT(""))
			.Number(TEXT("maxJumpCount"), TEXT(""))
			.Number(TEXT("jumpHoldTime"), TEXT("Max hold time for variable jump."))
			.Number(TEXT("gravityScale"), TEXT(""))
			.Number(TEXT("fallingLateralFriction"), TEXT("Air friction."))
			.Bool(TEXT("orientToMovement"), TEXT("Orient rotation to movement direction."))
			.Bool(TEXT("useControllerRotationYaw"), TEXT("Use controller yaw rotation."))
			.Bool(TEXT("useControllerRotationPitch"), TEXT("Use controller pitch rotation."))
			.Bool(TEXT("useControllerRotationRoll"), TEXT("Use controller roll rotation."))
			.Number(TEXT("rotationRate"), TEXT(""))
			.String(TEXT("modeName"), TEXT("Name for custom movement mode."))
			.Number(TEXT("modeId"), TEXT("Custom movement mode ID."))
			.Number(TEXT("navAgentRadius"), TEXT(""))
			.Number(TEXT("navAgentHeight"), TEXT(""))
			.Bool(TEXT("avoidanceEnabled"), TEXT("Enable AI avoidance."))
			.Number(TEXT("mantleHeight"), TEXT("Maximum mantle height."))
			.Number(TEXT("mantleReachDistance"), TEXT("Forward reach for mantle check."))
			.Number(TEXT("vaultHeight"), TEXT("Maximum vault obstacle height."))
			.Number(TEXT("vaultDepth"), TEXT("Obstacle depth to check."))
			.Number(TEXT("climbSpeed"), TEXT(""))
			.String(TEXT("climbableTag"), TEXT("Tag for climbable surfaces."))
			.Number(TEXT("slideSpeed"), TEXT(""))
			.Number(TEXT("slideDuration"), TEXT(""))
			.Number(TEXT("slideCooldown"), TEXT(""))
			.Number(TEXT("wallRunSpeed"), TEXT("Wall running speed."))
			.Number(TEXT("wallRunDuration"), TEXT("Maximum wall run duration."))
			.Number(TEXT("wallRunGravityScale"), TEXT("Gravity during wall run."))
			.Number(TEXT("grappleRange"), TEXT("Maximum grapple distance."))
			.Number(TEXT("grappleSpeed"), TEXT("Grapple pull speed."))
			.String(TEXT("grappleTargetTag"), TEXT("Tag for grapple targets."))
			.Bool(TEXT("footstepEnabled"), TEXT("Enable footstep system."))
			.String(TEXT("footstepSocketLeft"), TEXT("Left foot socket name."))
			.String(TEXT("footstepSocketRight"), TEXT("Right foot socket name."))
			.Number(TEXT("footstepTraceDistance"), TEXT("Ground trace distance."))
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
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageCharacter);
