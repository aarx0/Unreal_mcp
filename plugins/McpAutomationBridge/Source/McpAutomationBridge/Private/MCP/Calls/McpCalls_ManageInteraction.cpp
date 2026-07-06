// LINT-TOOL: manage_interaction
// manage_interaction as FMcpCall classes — eighth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers (HandleInteraction*, InteractionHandlers.cpp) until the
// module split de-members those bodies. Five of the 22 are shallow scaffolds
// preserved as-is at classing — the configure_destruction_* marker-tag
// writers and the configure_trigger_filter/response variable scaffolds;
// deepening or retiring them is a logged product decision (TODO.md).
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageInteraction
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations
// (McpDecl_ManageInteraction.h) and re-verified against the extracted
// HandleInteraction* bodies. The configure door/switch/chest property rows
// accept blueprintPath as a fallback for their primary path spelling, so
// both spellings are optional and the "at least one" requirement is
// handler-enforced — same for get_info's resolver bag.

inline const FMcpParamDecl P_CreateInteractionComponent[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("traceDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureInteractionTrace[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("traceType"), EMcpParamKind::String, false }, { TEXT("traceDistance"), EMcpParamKind::Number, false }, { TEXT("traceRadius"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureInteractionWidget[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("widgetClass"), EMcpParamKind::String, false }, { TEXT("showOnHover"), EMcpParamKind::Bool, false }, { TEXT("showPromptText"), EMcpParamKind::Bool, false }, { TEXT("promptTextFormat"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddInteractionEvents[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateInteractableInterface[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateDoorActor[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("openAngle"), EMcpParamKind::Number, false }, { TEXT("openTime"), EMcpParamKind::Number, false }, { TEXT("autoClose"), EMcpParamKind::Bool, false }, { TEXT("autoCloseDelay"), EMcpParamKind::Number, false }, { TEXT("requiresKey"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureDoorProperties[] = { { TEXT("doorPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("openAngle"), EMcpParamKind::Number, false }, { TEXT("openTime"), EMcpParamKind::Number, false }, { TEXT("locked"), EMcpParamKind::Bool, false }, { TEXT("autoClose"), EMcpParamKind::Bool, false }, { TEXT("autoCloseDelay"), EMcpParamKind::Number, false }, { TEXT("requiresKey"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateSwitchActor[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("switchType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureSwitchProperties[] = { { TEXT("switchPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("switchType"), EMcpParamKind::String, false }, { TEXT("canToggle"), EMcpParamKind::Bool, false }, { TEXT("resetTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateChestActor[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("locked"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureChestProperties[] = { { TEXT("chestPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("locked"), EMcpParamKind::Bool, false }, { TEXT("openAngle"), EMcpParamKind::Number, false }, { TEXT("openTime"), EMcpParamKind::Number, false }, { TEXT("lootTablePath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateLeverActor[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ActorNameRequired[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddDestructionComponent[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateTriggerActor[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("triggerShape"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureTriggerEvents[] = { { TEXT("triggerPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_TriggerPathRequired[] = { { TEXT("triggerPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_GetInteractionInfo[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("doorPath"), EMcpParamKind::String, false }, { TEXT("switchPath"), EMcpParamKind::String, false }, { TEXT("chestPath"), EMcpParamKind::String, false }, { TEXT("triggerPath"), EMcpParamKind::String, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row (every implementation body is
// editor-gated). Mutating on all writers; the only reader is
// get_info.

#define MCP_MI_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)        \
class FMcpCall_ManageInteraction_##ClassSuffix final : public FMcpCall                     \
{                                                                                          \
	const FMcpCallDecl& GetDecl() const override                                           \
	{                                                                                      \
		static const FMcpCallDecl Decl{ TEXT("manage_interaction"), TEXT(ActionLiteral),   \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                   \
		return Decl;                                                                       \
	}                                                                                      \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                   \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override   \
	{                                                                                      \
		return S.HandlerFn(RequestId, Payload, Socket);                                    \
	}                                                                                      \
};

// Interaction component (18.1)
MCP_MI_CALL(CreateComponent, "create_interaction_component", P_CreateInteractionComponent, HandleInteractionCreateComponent, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureTrace, "configure_interaction_trace", P_ConfigureInteractionTrace, HandleInteractionConfigureTrace, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureWidget, "configure_interaction_widget", P_ConfigureInteractionWidget, HandleInteractionConfigureWidget, EMcpCallFlags::Mutating)
MCP_MI_CALL(AddEvents, "add_interaction_events", P_AddInteractionEvents, HandleInteractionAddEvents, EMcpCallFlags::Mutating)

// Interactables (18.2)
MCP_MI_CALL(CreateInteractableInterface, "create_interactable_interface", P_CreateInteractableInterface, HandleInteractionCreateInteractableInterface, EMcpCallFlags::Mutating)
MCP_MI_CALL(CreateDoorActor, "create_door_actor", P_CreateDoorActor, HandleInteractionCreateDoorActor, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureDoorProperties, "configure_door_properties", P_ConfigureDoorProperties, HandleInteractionConfigureDoorProperties, EMcpCallFlags::Mutating)
MCP_MI_CALL(CreateSwitchActor, "create_switch_actor", P_CreateSwitchActor, HandleInteractionCreateSwitchActor, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureSwitchProperties, "configure_switch_properties", P_ConfigureSwitchProperties, HandleInteractionConfigureSwitchProperties, EMcpCallFlags::Mutating)
MCP_MI_CALL(CreateChestActor, "create_chest_actor", P_CreateChestActor, HandleInteractionCreateChestActor, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureChestProperties, "configure_chest_properties", P_ConfigureChestProperties, HandleInteractionConfigureChestProperties, EMcpCallFlags::Mutating)
MCP_MI_CALL(CreateLeverActor, "create_lever_actor", P_CreateLeverActor, HandleInteractionCreateLeverActor, EMcpCallFlags::Mutating)

// Destructibles (18.3; the configure_destruction_* trio are marker-tag scaffolds)
MCP_MI_CALL(SetupDestructibleMesh, "setup_destructible_mesh", P_ActorNameRequired, HandleInteractionSetupDestructibleMesh, EMcpCallFlags::Mutating)
MCP_MI_CALL(AddDestructionComponent, "add_destruction_component", P_AddDestructionComponent, HandleInteractionAddDestructionComponent, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureDestructionLevels, "configure_destruction_levels", P_ActorNameRequired, HandleInteractionConfigureDestructionLevels, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureDestructionEffects, "configure_destruction_effects", P_ActorNameRequired, HandleInteractionConfigureDestructionEffects, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureDestructionDamage, "configure_destruction_damage", P_ActorNameRequired, HandleInteractionConfigureDestructionDamage, EMcpCallFlags::Mutating)

// Trigger system (18.4; filter/response are variable scaffolds)
MCP_MI_CALL(CreateTriggerActor, "create_trigger_actor", P_CreateTriggerActor, HandleInteractionCreateTriggerActor, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureTriggerEvents, "configure_trigger_events", P_ConfigureTriggerEvents, HandleInteractionConfigureTriggerEvents, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureTriggerFilter, "configure_trigger_filter", P_TriggerPathRequired, HandleInteractionConfigureTriggerFilter, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureTriggerResponse, "configure_trigger_response", P_TriggerPathRequired, HandleInteractionConfigureTriggerResponse, EMcpCallFlags::Mutating)

// Utility
MCP_MI_CALL(GetInfo, "get_info", P_GetInteractionInfo, HandleInteractionGetInfo, EMcpCallFlags::None)

#undef MCP_MI_CALL

} // namespace McpCalls::ManageInteraction

void McpRegisterManageInteractionCalls()
{
	using namespace McpCalls::ManageInteraction;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_CreateComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_ConfigureTrace>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_ConfigureWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_AddEvents>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_CreateInteractableInterface>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_CreateDoorActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_ConfigureDoorProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_CreateSwitchActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_ConfigureSwitchProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_CreateChestActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_ConfigureChestProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_CreateLeverActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_SetupDestructibleMesh>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_AddDestructionComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_ConfigureDestructionLevels>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_ConfigureDestructionEffects>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_ConfigureDestructionDamage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_CreateTriggerActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_ConfigureTriggerEvents>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_ConfigureTriggerFilter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_ConfigureTriggerResponse>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_GetInfo>());
}
