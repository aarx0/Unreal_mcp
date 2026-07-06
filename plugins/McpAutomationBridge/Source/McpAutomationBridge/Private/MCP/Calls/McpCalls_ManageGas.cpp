// LINT-TOOL: manage_gas
// manage_gas as FMcpCall classes — twelfth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers (HandleGas*, GASHandlers.cpp) until the
// module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageGas
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations (McpDecl_ManageGas.h)
// and re-verified against the extracted HandleGas* bodies. The retired
// dispatcher resolved blueprintPath through the attributeSetPath/effectPath/
// abilityPath/cuePath alias fallbacks for every action, so the 21 rows that
// required blueprintPath were contaminated — a payload passing only an alias
// satisfied the handler but failed the declared contract; those rows now
// declare all five spellings optional with the at-least-one requirement
// handler-enforced. Same fix for the three handler-enforced one-ofs the shim
// rows declared as all-required: add_tag_to_asset's tag/tagName,
// add_ability's abilityPath/abilityClass, and create_ability_set's
// setPath/assetPath. The dispatcher-prologue spellings a body does not read
// (name/path/assetPath and the alias spellings themselves) stay
// declared-optional so payloads the family accepts today keep validating.

inline const FMcpParamDecl P_AddAbilitySystemComponent[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureAsc[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("replicationMode"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateAttributeSet[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddAttribute[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("attributeName"), EMcpParamKind::String, true }, { TEXT("baseValue"), EMcpParamKind::Number, false }, { TEXT("defaultValue"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetAttributeBaseValue[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("attributeName"), EMcpParamKind::String, true }, { TEXT("baseValue"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetAttributeClamping[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("attributeName"), EMcpParamKind::String, true }, { TEXT("minValue"), EMcpParamKind::Number, false }, { TEXT("maxValue"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateGameplayAbility[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetAbilityTags[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("abilityTags"), EMcpParamKind::Array, false }, { TEXT("cancelAbilitiesWithTags"), EMcpParamKind::Array, false }, { TEXT("cancelAbilitiesWithTag"), EMcpParamKind::Array, false }, { TEXT("blockAbilitiesWithTags"), EMcpParamKind::Array, false }, { TEXT("blockAbilitiesWithTag"), EMcpParamKind::Array, false }, { TEXT("activationRequiredTags"), EMcpParamKind::Array, false }, { TEXT("activationBlockedTags"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_SetAbilityCosts[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("costEffectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetAbilityCooldown[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("cooldownEffectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetAbilityTargeting[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("targetingType"), EMcpParamKind::String, false }, { TEXT("targetingMode"), EMcpParamKind::String, false }, { TEXT("targetingRange"), EMcpParamKind::Number, false }, { TEXT("targetRange"), EMcpParamKind::Number, false }, { TEXT("aoeRadius"), EMcpParamKind::Number, false }, { TEXT("requiresLineOfSight"), EMcpParamKind::Bool, false }, { TEXT("targetingAngle"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddAbilityTask[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("taskType"), EMcpParamKind::String, true }, { TEXT("taskClassName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetActivationPolicy[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("activationPolicy"), EMcpParamKind::String, false }, { TEXT("policy"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetInstancingPolicy[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("policy"), EMcpParamKind::String, false }, { TEXT("instancingPolicy"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateGameplayEffect[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("durationType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetEffectDuration[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("durationType"), EMcpParamKind::String, false }, { TEXT("duration"), EMcpParamKind::Number, false }, { TEXT("period"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddEffectModifier[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("operation"), EMcpParamKind::String, false }, { TEXT("modifierOperation"), EMcpParamKind::String, false }, { TEXT("magnitude"), EMcpParamKind::Number, false }, { TEXT("modifierMagnitude"), EMcpParamKind::Number, false }, { TEXT("attribute"), EMcpParamKind::String, true }, { TEXT("setByCallerTag"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetModifierMagnitude[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("modifierIndex"), EMcpParamKind::Number, false }, { TEXT("value"), EMcpParamKind::Number, false }, { TEXT("modifierMagnitude"), EMcpParamKind::Number, false }, { TEXT("magnitudeType"), EMcpParamKind::String, false }, { TEXT("magnitudeCalculationType"), EMcpParamKind::String, false }, { TEXT("setByCallerTag"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddEffectExecutionCalculation[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("calculationClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddEffectCue[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("cueTag"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetEffectStacking[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("stackingType"), EMcpParamKind::String, false }, { TEXT("stackLimit"), EMcpParamKind::Number, false }, { TEXT("stackLimitCount"), EMcpParamKind::Number, false }, { TEXT("stackDurationRefreshPolicy"), EMcpParamKind::String, false }, { TEXT("stackPeriodResetPolicy"), EMcpParamKind::String, false }, { TEXT("stackExpirationPolicy"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetEffectTags[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("grantedTags"), EMcpParamKind::Array, false }, { TEXT("applicationRequiredTags"), EMcpParamKind::Array, false }, { TEXT("removalTags"), EMcpParamKind::Array, false }, { TEXT("immunityTags"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_CreateGameplayCueNotify[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("cueType"), EMcpParamKind::String, false }, { TEXT("cueTag"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureCueTrigger[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("triggerType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetCueEffects[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("particleSystem"), EMcpParamKind::String, false }, { TEXT("particleSystemPath"), EMcpParamKind::String, false }, { TEXT("sound"), EMcpParamKind::String, false }, { TEXT("soundPath"), EMcpParamKind::String, false }, { TEXT("cameraShake"), EMcpParamKind::String, false }, { TEXT("cameraShakePath"), EMcpParamKind::String, false }, { TEXT("decalPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddTagToAsset[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, false }, { TEXT("tagName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetAttribute[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("attribute"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_GetGasInfo[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateAbilitySet[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("setPath"), EMcpParamKind::String, false }, { TEXT("setName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddAbility[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("setPath"), EMcpParamKind::String, true }, { TEXT("abilityClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateExecutionCalculation[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("attributeSetPath"), EMcpParamKind::String, false }, { TEXT("effectPath"), EMcpParamKind::String, false }, { TEXT("abilityPath"), EMcpParamKind::String, false }, { TEXT("cuePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row (the implementation TU is editor-
// gated; the members answer the EDITOR_ONLY stub in non-editor builds and
// the GAS_NOT_AVAILABLE stub when the GameplayAbilities headers are absent).
// Mutating on all writers; the only readers are get_attribute and
// get_info.

#define MCP_GA_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)        \
class FMcpCall_ManageGas_##ClassSuffix final : public FMcpCall                             \
{                                                                                          \
	const FMcpCallDecl& GetDecl() const override                                           \
	{                                                                                      \
		static const FMcpCallDecl Decl{ TEXT("manage_gas"), TEXT(ActionLiteral),           \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                   \
		return Decl;                                                                       \
	}                                                                                      \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                   \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override   \
	{                                                                                      \
		return S.HandlerFn(RequestId, Payload, Socket);                                    \
	}                                                                                      \
};

// Components & attributes (13.1)
MCP_GA_CALL(AddAbilitySystemComponent, "add_ability_system_component", P_AddAbilitySystemComponent, HandleGasAddAbilitySystemComponent, EMcpCallFlags::Mutating)
MCP_GA_CALL(ConfigureAsc, "configure_asc", P_ConfigureAsc, HandleGasConfigureAsc, EMcpCallFlags::Mutating)
MCP_GA_CALL(CreateAttributeSet, "create_attribute_set", P_CreateAttributeSet, HandleGasCreateAttributeSet, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddAttribute, "add_attribute", P_AddAttribute, HandleGasAddAttribute, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAttributeBaseValue, "set_attribute_base_value", P_SetAttributeBaseValue, HandleGasSetAttributeBaseValue, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAttributeClamping, "set_attribute_clamping", P_SetAttributeClamping, HandleGasSetAttributeClamping, EMcpCallFlags::Mutating)

// Gameplay abilities (13.2)
MCP_GA_CALL(CreateGameplayAbility, "create_gameplay_ability", P_CreateGameplayAbility, HandleGasCreateGameplayAbility, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAbilityTags, "set_ability_tags", P_SetAbilityTags, HandleGasSetAbilityTags, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAbilityCosts, "set_ability_costs", P_SetAbilityCosts, HandleGasSetAbilityCosts, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAbilityCooldown, "set_ability_cooldown", P_SetAbilityCooldown, HandleGasSetAbilityCooldown, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetAbilityTargeting, "set_ability_targeting", P_SetAbilityTargeting, HandleGasSetAbilityTargeting, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddAbilityTask, "add_ability_task", P_AddAbilityTask, HandleGasAddAbilityTask, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetActivationPolicy, "set_activation_policy", P_SetActivationPolicy, HandleGasSetActivationPolicy, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetInstancingPolicy, "set_instancing_policy", P_SetInstancingPolicy, HandleGasSetInstancingPolicy, EMcpCallFlags::Mutating)

// Gameplay effects (13.3)
MCP_GA_CALL(CreateGameplayEffect, "create_gameplay_effect", P_CreateGameplayEffect, HandleGasCreateGameplayEffect, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetEffectDuration, "set_effect_duration", P_SetEffectDuration, HandleGasSetEffectDuration, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddEffectModifier, "add_effect_modifier", P_AddEffectModifier, HandleGasAddEffectModifier, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetModifierMagnitude, "set_modifier_magnitude", P_SetModifierMagnitude, HandleGasSetModifierMagnitude, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddEffectExecutionCalculation, "add_effect_execution_calculation", P_AddEffectExecutionCalculation, HandleGasAddEffectExecutionCalculation, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddEffectCue, "add_effect_cue", P_AddEffectCue, HandleGasAddEffectCue, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetEffectStacking, "set_effect_stacking", P_SetEffectStacking, HandleGasSetEffectStacking, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetEffectTags, "set_effect_tags", P_SetEffectTags, HandleGasSetEffectTags, EMcpCallFlags::Mutating)

// Gameplay cues + tags (13.4)
MCP_GA_CALL(CreateGameplayCueNotify, "create_gameplay_cue_notify", P_CreateGameplayCueNotify, HandleGasCreateGameplayCueNotify, EMcpCallFlags::Mutating)
MCP_GA_CALL(ConfigureCueTrigger, "configure_cue_trigger", P_ConfigureCueTrigger, HandleGasConfigureCueTrigger, EMcpCallFlags::Mutating)
MCP_GA_CALL(SetCueEffects, "set_cue_effects", P_SetCueEffects, HandleGasSetCueEffects, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddTagToAsset, "add_tag_to_asset", P_AddTagToAsset, HandleGasAddTagToAsset, EMcpCallFlags::Mutating)

// Utility (13.5)
MCP_GA_CALL(GetAttribute, "get_attribute", P_GetAttribute, HandleGasGetAttribute, EMcpCallFlags::None)
MCP_GA_CALL(GetInfo, "get_info", P_GetGasInfo, HandleGasGetInfo, EMcpCallFlags::None)

// Ability sets (13.6)
MCP_GA_CALL(CreateAbilitySet, "create_ability_set", P_CreateAbilitySet, HandleGasCreateAbilitySet, EMcpCallFlags::Mutating)
MCP_GA_CALL(AddAbility, "add_ability", P_AddAbility, HandleGasAddAbility, EMcpCallFlags::Mutating)

// Execution calculations (13.7)
MCP_GA_CALL(CreateExecutionCalculation, "create_execution_calculation", P_CreateExecutionCalculation, HandleGasCreateExecutionCalculation, EMcpCallFlags::Mutating)

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
