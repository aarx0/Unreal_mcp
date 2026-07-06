// LINT-TOOL: manage_gas
// LINT-SCHEMA-DERIVED
// manage_gas as FMcpCall classes — twelfth classed family
// (docs/action-declarations.md). Adopts schema-from-decls: each class AUTHORS
// its schema fragment in a S_<Suffix>() function; the published facade schema
// folds those fragments and GetDecl() derives the validation decl from the same
// fragment via McpDeriveDecl(), so schema and decl are one source and cannot
// drift. Run() delegates to the subsystem member handlers (HandleGas*,
// McpAutomationBridge_GASHandlers.cpp) until the module split de-members those
// bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageGas
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions + builder methods
// are copied verbatim from the retired facade; McpDeriveDecl() reads the param
// kinds + required-set back out of these to build the transport validation decl.
// The five path spellings (blueprintPath + attributeSetPath/effectPath/
// abilityPath/cuePath aliases) and the name/path/assetPath prologue keys stay
// optional: the handler resolves the alias fallback itself, so the "at least
// one" requirement is handler-enforced, not declared. Same handler-enforced
// one-of for add_tag_to_asset (tag/tagName), add_ability (abilityPath/
// abilityClass), and create_ability_set (setPath/assetPath).

static void S_AddAbilitySystemComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("componentName"), TEXT("add_ability_system_component/configure_asc: SCS component name (default 'AbilitySystemComponent')."));
}

static void S_ConfigureAsc(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("componentName"), TEXT("add_ability_system_component/configure_asc: SCS component name (default 'AbilitySystemComponent')."))
	 .StringEnum(TEXT("replicationMode"), {
		TEXT("Full"),
		TEXT("Minimal"),
		TEXT("Mixed")
	 }, TEXT("ASC replication mode."));
}

static void S_CreateAttributeSet(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("name")});
}

static void S_AddAttribute(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("attributeName"), TEXT("Name of the attribute."))
	 .Number(TEXT("baseValue"), TEXT("Base value for attribute."))
	 .Number(TEXT("defaultValue"), TEXT("add_attribute: alias for baseValue."))
	 .Required({TEXT("attributeName")});
}

static void S_SetAttributeBaseValue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("attributeName"), TEXT("Name of the attribute."))
	 .Number(TEXT("baseValue"), TEXT("Base value for attribute."))
	 .Required({TEXT("attributeName")});
}

static void S_SetAttributeClamping(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("attributeName"), TEXT("Name of the attribute."))
	 .Number(TEXT("minValue"), TEXT("Minimum value for clamping."))
	 .Number(TEXT("maxValue"), TEXT("Maximum value for clamping."))
	 .Required({TEXT("attributeName")});
}

static void S_CreateGameplayAbility(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("name")});
}

static void S_SetAbilityTags(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Array(TEXT("abilityTags"), TEXT("Gameplay tags for this ability."))
	 .Array(TEXT("cancelAbilitiesWithTags"), TEXT("set_ability_tags: alias for cancelAbilitiesWithTag (checked first)."))
	 .Array(TEXT("cancelAbilitiesWithTag"), TEXT("Tags of abilities to cancel when this activates."))
	 .Array(TEXT("blockAbilitiesWithTags"), TEXT("set_ability_tags: alias for blockAbilitiesWithTag (checked first)."))
	 .Array(TEXT("blockAbilitiesWithTag"), TEXT("Tags of abilities blocked while this is active."))
	 .Array(TEXT("activationRequiredTags"), TEXT("Tags required to activate this ability."))
	 .Array(TEXT("activationBlockedTags"), TEXT("Tags that block activation of this ability."));
}

static void S_SetAbilityCosts(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("costEffectPath"), TEXT("Path to cost Gameplay Effect."));
}

static void S_SetAbilityCooldown(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("cooldownEffectPath"), TEXT("Path to cooldown Gameplay Effect."));
}

static void S_SetAbilityTargeting(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("targetingType"), TEXT("set_ability_targeting: alias for targetingMode (checked first)."))
	 .StringEnum(TEXT("targetingMode"), {
		TEXT("None"),
		TEXT("SingleTarget"),
		TEXT("AOE"),
		TEXT("Directional"),
		TEXT("Ground"),
		TEXT("ActorPlacement")
	 }, TEXT("Targeting mode for ability."))
	 .Number(TEXT("targetingRange"), TEXT("set_ability_targeting: alias for targetRange (checked first)."))
	 .Number(TEXT("targetRange"), TEXT("Maximum targeting range."))
	 .Number(TEXT("aoeRadius"), TEXT("Area of effect radius."))
	 .Bool(TEXT("requiresLineOfSight"), TEXT("set_ability_targeting: whether targeting requires line of sight (default false)."))
	 .Number(TEXT("targetingAngle"), TEXT("set_ability_targeting: cone angle in degrees for cone-based targeting (default 360)."));
}

static void S_AddAbilityTask(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
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
	 .String(TEXT("taskClassName"), TEXT("add_ability_task: optional soft class name recorded for the task (informational)."))
	 .Required({TEXT("taskType")});
}

static void S_SetActivationPolicy(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .StringEnum(TEXT("activationPolicy"), {
		TEXT("OnInputPressed"),
		TEXT("WhileInputActive"),
		TEXT("OnSpawn"),
		TEXT("OnGiven")
	 }, TEXT("When the ability activates."))
	 .String(TEXT("policy"), TEXT("set_activation_policy/set_instancing_policy: generic alias for activationPolicy/instancingPolicy (checked first)."));
}

static void S_SetInstancingPolicy(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("policy"), TEXT("set_activation_policy/set_instancing_policy: generic alias for activationPolicy/instancingPolicy (checked first)."))
	 .StringEnum(TEXT("instancingPolicy"), {
		TEXT("NonInstanced"),
		TEXT("InstancedPerActor"),
		TEXT("InstancedPerExecution")
	 }, TEXT("How the ability is instanced."));
}

static void S_CreateGameplayEffect(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .StringEnum(TEXT("durationType"), {
		TEXT("Instant"),
		TEXT("Infinite"),
		TEXT("HasDuration")
	 }, TEXT("Effect duration type."))
	 .Required({TEXT("name")});
}

static void S_SetEffectDuration(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .StringEnum(TEXT("durationType"), {
		TEXT("Instant"),
		TEXT("Infinite"),
		TEXT("HasDuration")
	 }, TEXT("Effect duration type."))
	 .Number(TEXT("duration"), TEXT("Duration in seconds."))
	 .Number(TEXT("period"), TEXT("set_effect_duration: execution period in seconds (> 0) for periodic effects; requires durationType HasDuration or Infinite."));
}

static void S_AddEffectModifier(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("operation"), TEXT("add_effect_modifier: alias for modifierOperation (checked first)."))
	 .StringEnum(TEXT("modifierOperation"), {
		TEXT("Add"),
		TEXT("Multiply"),
		TEXT("Divide"),
		TEXT("Override")
	 }, TEXT("Modifier operation on attribute."))
	 .Number(TEXT("magnitude"), TEXT("add_effect_modifier: alias for modifierMagnitude (checked first)."))
	 .Number(TEXT("modifierMagnitude"), TEXT("Magnitude of the modifier."))
	 .String(TEXT("attribute"), TEXT("get_attribute: attribute to read at runtime (e.g. 'Health'). add_effect_modifier: REQUIRED target attribute for the modifier - 'AttributeName' or 'SetClassName.AttributeName' (must be unambiguous across loaded AttributeSets)."))
	 .String(TEXT("setByCallerTag"), TEXT("add_effect_modifier/set_modifier_magnitude: registered gameplay tag for a "
		"SetByCaller modifier magnitude; mutually exclusive with a numeric magnitude."))
	 .Required({TEXT("attribute")});
}

static void S_SetModifierMagnitude(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("modifierIndex"), TEXT("set_modifier_magnitude: index into the effect's Modifiers array (0-based)."))
	 .Number(TEXT("value"), TEXT("set_modifier_magnitude: alias for modifierMagnitude (checked first)."))
	 .Number(TEXT("modifierMagnitude"), TEXT("Magnitude of the modifier."))
	 .String(TEXT("magnitudeType"), TEXT("set_modifier_magnitude: alias for magnitudeCalculationType (checked first)."))
	 .StringEnum(TEXT("magnitudeCalculationType"), {
		TEXT("ScalableFloat"),
		TEXT("AttributeBased"),
		TEXT("SetByCaller"),
		TEXT("CustomCalculationClass")
	 }, TEXT("How magnitude is calculated."))
	 .String(TEXT("setByCallerTag"), TEXT("add_effect_modifier/set_modifier_magnitude: registered gameplay tag for a "
		"SetByCaller modifier magnitude; mutually exclusive with a numeric magnitude."));
}

static void S_AddEffectExecutionCalculation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("calculationClass"), TEXT("UGameplayEffectExecutionCalculation class path."))
	 .Required({TEXT("calculationClass")});
}

static void S_AddEffectCue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("cueTag"), TEXT("Gameplay Cue tag (e.g., GameplayCue.Damage.Fire)."))
	 .Required({TEXT("cueTag")});
}

static void S_SetEffectStacking(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .StringEnum(TEXT("stackingType"), {
		TEXT("None"),
		TEXT("AggregateBySource"),
		TEXT("AggregateByTarget")
	 }, TEXT("Stacking type for effect."))
	 .Number(TEXT("stackLimit"), TEXT("set_effect_stacking: alias for stackLimitCount (checked first)."))
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
	 }, TEXT("What happens when stack expires."));
}

static void S_SetEffectTags(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Array(TEXT("grantedTags"), TEXT("Tags granted while effect is active."))
	 .Array(TEXT("applicationRequiredTags"), TEXT("Tags required to apply this effect."))
	 .Array(TEXT("removalTags"), TEXT("Tags that cause effect removal."))
	 .Array(TEXT("immunityTags"), TEXT("Tags that block this effect."));
}

static void S_CreateGameplayCueNotify(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .StringEnum(TEXT("cueType"), {
		TEXT("Static"),
		TEXT("Actor")
	 }, TEXT("Type of gameplay cue notify."))
	 .String(TEXT("cueTag"), TEXT("Gameplay Cue tag (e.g., GameplayCue.Damage.Fire)."))
	 .Required({TEXT("name")});
}

static void S_ConfigureCueTrigger(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .StringEnum(TEXT("triggerType"), {
		TEXT("OnActive"),
		TEXT("WhileActive"),
		TEXT("Executed"),
		TEXT("OnRemove")
	 }, TEXT("When the cue triggers."));
}

static void S_SetCueEffects(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("particleSystem"), TEXT("set_cue_effects: alias for particleSystemPath (checked first)."))
	 .String(TEXT("particleSystemPath"), TEXT("Path to particle system."))
	 .String(TEXT("sound"), TEXT("set_cue_effects: alias for soundPath (checked first)."))
	 .String(TEXT("soundPath"), TEXT("Sound asset path."))
	 .String(TEXT("cameraShake"), TEXT("set_cue_effects: alias for cameraShakePath (checked first)."))
	 .String(TEXT("cameraShakePath"), TEXT("Path to camera shake asset."))
	 .String(TEXT("decalPath"), TEXT("Path to decal material."));
}

static void S_AddTagToAsset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("tag"), TEXT("add_tag_to_asset: gameplay tag to add (must be registered in the project's gameplay tag registry)."))
	 .String(TEXT("tagName"), TEXT("Alias for 'tag'."))
	 .Required({TEXT("assetPath")});
}

static void S_GetAttribute(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("actorName"), TEXT("get_attribute: live PIE actor to read (object name or label; defaults to the player pawn)."))
	 .String(TEXT("attribute"), TEXT("get_attribute: attribute to read at runtime (e.g. 'Health'). add_effect_modifier: REQUIRED target attribute for the modifier - 'AttributeName' or 'SetClassName.AttributeName' (must be unambiguous across loaded AttributeSets)."))
	 .Required({TEXT("attribute")});
}

static void S_GetInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_CreateAbilitySet(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("setPath"), TEXT("create_ability_set/add_ability: path of the ability-set data asset. "
		"Note: create_ability_set scaffolds a PrimaryDataAsset blueprint with GrantedAbilities/"
		"GrantedEffects/GrantedTags variables; it is not an engine ability-set type."))
	 .String(TEXT("setName"), TEXT("create_ability_set: display name stored on the set."));
}

static void S_AddAbility(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("setPath"), TEXT("create_ability_set/add_ability: path of the ability-set data asset. "
		"Note: create_ability_set scaffolds a PrimaryDataAsset blueprint with GrantedAbilities/"
		"GrantedEffects/GrantedTags variables; it is not an engine ability-set type."))
	 .String(TEXT("abilityClass"), TEXT("add_ability: alias for abilityPath."))
	 .Required({TEXT("setPath")});
}

static void S_CreateExecutionCalculation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path. attributeSetPath/effectPath/abilityPath/cuePath are accepted aliases."))
	 .String(TEXT("attributeSetPath"), TEXT("Path to Attribute Set asset (alias for blueprintPath)."))
	 .String(TEXT("effectPath"), TEXT("Path to effect asset (alias for blueprintPath)."))
	 .String(TEXT("abilityPath"), TEXT("Path to ability asset. add_ability: the ability class to add to the set; elsewhere an alias for blueprintPath."))
	 .String(TEXT("cuePath"), TEXT("Path to Gameplay Cue asset (alias for blueprintPath)."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("name")});
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row (the implementation TU is editor-
// gated; the members answer the EDITOR_ONLY stub in non-editor builds and
// the GAS_NOT_AVAILABLE stub when the GameplayAbilities headers are absent).
// Mutating on all writers; the only readers are get_attribute and
// get_info.

#define MCP_GA_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                     \
class FMcpCall_ManageGas_##ClassSuffix final : public FMcpCall                             \
{                                                                                          \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }         \
	const FMcpCallDecl& GetDecl() const override                                           \
	{                                                                                      \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_gas"),                    \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),             \
			&S_##ClassSuffix);                                                             \
		return D;                                                                          \
	}                                                                                      \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                   \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override   \
	{                                                                                      \
		return S.HandlerFn(RequestId, Payload, Socket);                                    \
	}                                                                                      \
};

// Components & attributes (13.1)
MCP_GA_CALL(AddAbilitySystemComponent, "add_ability_system_component", HandleGasAddAbilitySystemComponent, EMcpCallFlags::Mutating)
MCP_GA_CALL(ConfigureAsc, "configure_asc", HandleGasConfigureAsc, EMcpCallFlags::Mutating)
MCP_GA_CALL(CreateAttributeSet, "create_attribute_set", HandleGasCreateAttributeSet, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddAttribute, "add_attribute", HandleGasAddAttribute, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAttributeBaseValue, "set_attribute_base_value", HandleGasSetAttributeBaseValue, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAttributeClamping, "set_attribute_clamping", HandleGasSetAttributeClamping, EMcpCallFlags::Mutating)

// Gameplay abilities (13.2)
MCP_GA_CALL(CreateGameplayAbility, "create_gameplay_ability", HandleGasCreateGameplayAbility, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAbilityTags, "set_ability_tags", HandleGasSetAbilityTags, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAbilityCosts, "set_ability_costs", HandleGasSetAbilityCosts, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAbilityCooldown, "set_ability_cooldown", HandleGasSetAbilityCooldown, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAbilityTargeting, "set_ability_targeting", HandleGasSetAbilityTargeting, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddAbilityTask, "add_ability_task", HandleGasAddAbilityTask, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetActivationPolicy, "set_activation_policy", HandleGasSetActivationPolicy, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetInstancingPolicy, "set_instancing_policy", HandleGasSetInstancingPolicy, EMcpCallFlags::Mutating)

// Gameplay effects (13.3)
MCP_GA_CALL(CreateGameplayEffect, "create_gameplay_effect", HandleGasCreateGameplayEffect, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetEffectDuration, "set_effect_duration", HandleGasSetEffectDuration, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddEffectModifier, "add_effect_modifier", HandleGasAddEffectModifier, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetModifierMagnitude, "set_modifier_magnitude", HandleGasSetModifierMagnitude, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddEffectExecutionCalculation, "add_effect_execution_calculation", HandleGasAddEffectExecutionCalculation, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddEffectCue, "add_effect_cue", HandleGasAddEffectCue, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetEffectStacking, "set_effect_stacking", HandleGasSetEffectStacking, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetEffectTags, "set_effect_tags", HandleGasSetEffectTags, EMcpCallFlags::Mutating)

// Gameplay cues + tags (13.4)
MCP_GA_CALL(CreateGameplayCueNotify, "create_gameplay_cue_notify", HandleGasCreateGameplayCueNotify, EMcpCallFlags::Mutating)
MCP_GA_CALL(ConfigureCueTrigger, "configure_cue_trigger", HandleGasConfigureCueTrigger, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetCueEffects, "set_cue_effects", HandleGasSetCueEffects, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddTagToAsset, "add_tag_to_asset", HandleGasAddTagToAsset, EMcpCallFlags::Mutating)

// Utility (13.5)
MCP_GA_CALL(GetAttribute, "get_attribute", HandleGasGetAttribute, EMcpCallFlags::None)
MCP_GA_CALL(GetInfo, "get_info", HandleGasGetInfo, EMcpCallFlags::None)

// Ability sets (13.6)
MCP_GA_CALL(CreateAbilitySet, "create_ability_set", HandleGasCreateAbilitySet, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddAbility, "add_ability", HandleGasAddAbility, EMcpCallFlags::Mutating)

// Execution calculations (13.7)
MCP_GA_CALL(CreateExecutionCalculation, "create_execution_calculation", HandleGasCreateExecutionCalculation, EMcpCallFlags::Mutating)

#undef MCP_GA_CALL

} // namespace McpCalls::ManageGas

void McpRegisterManageGasCalls()
{
	using namespace McpCalls::ManageGas;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_AddAbilitySystemComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_ConfigureAsc>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_CreateAttributeSet>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_AddAttribute>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetAttributeBaseValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetAttributeClamping>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_CreateGameplayAbility>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetAbilityTags>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetAbilityCosts>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetAbilityCooldown>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetAbilityTargeting>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_AddAbilityTask>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetActivationPolicy>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetInstancingPolicy>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_CreateGameplayEffect>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetEffectDuration>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_AddEffectModifier>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetModifierMagnitude>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_AddEffectExecutionCalculation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_AddEffectCue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetEffectStacking>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetEffectTags>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_CreateGameplayCueNotify>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_ConfigureCueTrigger>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_SetCueEffects>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_AddTagToAsset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_GetAttribute>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_GetInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_CreateAbilitySet>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_AddAbility>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageGas_CreateExecutionCalculation>());
}
