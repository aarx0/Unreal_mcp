// McpTool_ManageGAS.cpp — manage_gas tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageGAS : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_gas"); }

	FString GetDescription() const override
	{
		return TEXT("Create Gameplay Abilities, Effects, Attribute Sets, "
			"and Gameplay Cues for ability systems.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ManageGAS(), TEXT("GAS action to perform."))
			.String(TEXT("name"), TEXT("Name of the asset to create."))
			.String(TEXT("actorName"), TEXT("get_attribute: live PIE actor to read (object name or label; defaults to the player pawn)."))
			.String(TEXT("attribute"), TEXT("get_attribute: attribute to read at runtime (e.g. 'Health')."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
			.StringEnum(TEXT("replicationMode"), {
				TEXT("Full"),
				TEXT("Minimal"),
				TEXT("Mixed")
			}, TEXT("ASC replication mode."))
			.String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
			.String(TEXT("attributeName"), TEXT("Name of the attribute."))
			.Number(TEXT("baseValue"), TEXT("Base value for attribute."))
			.Number(TEXT("minValue"), TEXT("Minimum value for clamping."))
			.Number(TEXT("maxValue"), TEXT("Maximum value for clamping."))
			.String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
			.Array(TEXT("abilityTags"), TEXT("Gameplay tags for this ability."))
			.Array(TEXT("cancelAbilitiesWithTag"), TEXT("Tags of abilities to cancel when this activates."))
			.Array(TEXT("blockAbilitiesWithTag"), TEXT("Tags of abilities blocked while this is active."))
			.Array(TEXT("activationRequiredTags"), TEXT("Tags required to activate this ability."))
			.Array(TEXT("activationBlockedTags"), TEXT("Tags that block activation of this ability."))
			.String(TEXT("costEffectPath"), TEXT("Path to cost Gameplay Effect."))
			.String(TEXT("cooldownEffectPath"), TEXT("Path to cooldown Gameplay Effect."))
			.StringEnum(TEXT("targetingMode"), {
				TEXT("None"),
				TEXT("SingleTarget"),
				TEXT("AOE"),
				TEXT("Directional"),
				TEXT("Ground"),
				TEXT("ActorPlacement")
			}, TEXT("Targeting mode for ability."))
			.Number(TEXT("targetRange"), TEXT("Maximum targeting range."))
			.Number(TEXT("aoeRadius"), TEXT("Area of effect radius."))
			.StringEnum(TEXT("taskType"), {
				TEXT("WaitDelay"),
				TEXT("WaitInputPress"),
				TEXT("WaitInputRelease"),
				TEXT("WaitGameplayEvent"),
				TEXT("WaitTargetData"),
				TEXT("WaitConfirmCancel"),
				TEXT("PlayMontageAndWait"),
				TEXT("ApplyRootMotionConstantForce"),
				TEXT("WaitMovementModeChange")
			}, TEXT("Type of ability task to add."))
			.StringEnum(TEXT("activationPolicy"), {
				TEXT("OnInputPressed"),
				TEXT("WhileInputActive"),
				TEXT("OnSpawn"),
				TEXT("OnGiven")
			}, TEXT("When the ability activates."))
			.StringEnum(TEXT("instancingPolicy"), {
				TEXT("NonInstanced"),
				TEXT("InstancedPerActor"),
				TEXT("InstancedPerExecution")
			}, TEXT("How the ability is instanced."))
			.String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
			.StringEnum(TEXT("durationType"), {
				TEXT("Instant"),
				TEXT("Infinite"),
				TEXT("HasDuration")
			}, TEXT("Effect duration type."))
			.Number(TEXT("duration"), TEXT("Duration in seconds."))
			.Number(TEXT("period"), TEXT("Period for periodic effects."))
			.StringEnum(TEXT("modifierOperation"), {
				TEXT("Add"),
				TEXT("Multiply"),
				TEXT("Divide"),
				TEXT("Override")
			}, TEXT("Modifier operation on attribute."))
			.Number(TEXT("modifierMagnitude"), TEXT("Magnitude of the modifier."))
			.StringEnum(TEXT("magnitudeCalculationType"), {
				TEXT("ScalableFloat"),
				TEXT("AttributeBased"),
				TEXT("SetByCaller"),
				TEXT("CustomCalculationClass")
			}, TEXT("How magnitude is calculated."))
			.String(TEXT("setByCallerTag"), TEXT("Tag for SetByCaller magnitude."))
			.String(TEXT("targetAttribute"), TEXT("Target attribute for modifier."))
			.String(TEXT("calculationClass"), TEXT("UGameplayEffectExecutionCalculation class path."))
			.String(TEXT("cueTag"), TEXT("Gameplay Cue tag (e.g., GameplayCue.Damage.Fire)."))
			.String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
			.StringEnum(TEXT("stackingType"), {
				TEXT("None"),
				TEXT("AggregateBySource"),
				TEXT("AggregateByTarget")
			}, TEXT("Stacking type for effect."))
			.Number(TEXT("stackLimitCount"), TEXT("Maximum stack count."))
			.StringEnum(TEXT("stackDurationRefreshPolicy"), {
				TEXT("RefreshOnSuccessfulApplication"),
				TEXT("NeverRefresh")
			}, TEXT("When to refresh stack duration."))
			.StringEnum(TEXT("stackPeriodResetPolicy"), {
				TEXT("ResetOnSuccessfulApplication"),
				TEXT("NeverReset")
			}, TEXT("When to reset stack period."))
			.StringEnum(TEXT("stackExpirationPolicy"), {
				TEXT("ClearEntireStack"),
				TEXT("RemoveSingleStackAndRefreshDuration"),
				TEXT("RefreshDuration")
			}, TEXT("What happens when stack expires."))
			.Array(TEXT("grantedTags"), TEXT("Tags granted while effect is active."))
			.Array(TEXT("applicationRequiredTags"), TEXT("Tags required to apply this effect."))
			.Array(TEXT("removalTags"), TEXT("Tags that cause effect removal."))
			.Array(TEXT("immunityTags"), TEXT("Tags that block this effect."))
			.StringEnum(TEXT("cueType"), {
				TEXT("Static"),
				TEXT("Actor")
			}, TEXT("Type of gameplay cue notify."))
			.StringEnum(TEXT("triggerType"), {
				TEXT("OnActive"),
				TEXT("WhileActive"),
				TEXT("Executed"),
				TEXT("OnRemove")
			}, TEXT("When the cue triggers."))
			.String(TEXT("particleSystemPath"), TEXT("Path to particle system."))
			.String(TEXT("soundPath"), TEXT("Sound asset path."))
			.String(TEXT("cameraShakePath"), TEXT("Path to camera shake asset."))
			.String(TEXT("decalPath"), TEXT("Path to decal material."))
			.String(TEXT("tag"), TEXT("add_tag_to_asset: gameplay tag to add (must be registered in the project's gameplay tag registry)."))
			.String(TEXT("tagName"), TEXT("Alias for 'tag'."))
			.String(TEXT("setPath"), TEXT("create_ability_set/add_ability: path of the ability-set data asset. "
				"Note: create_ability_set scaffolds a PrimaryDataAsset blueprint with GrantedAbilities/"
				"GrantedEffects/GrantedTags variables; it is not an engine ability-set type."))
			.String(TEXT("setName"), TEXT("create_ability_set: display name stored on the set."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageGAS);
