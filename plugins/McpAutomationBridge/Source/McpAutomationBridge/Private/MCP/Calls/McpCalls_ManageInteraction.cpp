// LINT-TOOL: manage_interaction
// LINT-SCHEMA-DERIVED
// manage_interaction as FMcpCall classes — eighth classed family
// (docs/action-declarations.md). Adopts schema-from-decls: each class AUTHORS
// its schema fragment in a S_<Suffix>() function; the published facade schema
// folds those fragments and GetDecl() derives the validation decl from the same
// fragment via McpDeriveDecl(), so schema and decl are one source and cannot
// drift. Run() delegates to the namespaced free handlers (McpHandlers::Interaction::HandleInteraction*,
// McpAutomationBridge_InteractionHandlers.cpp).
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_InteractionHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageInteraction
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. The configure
// door/switch/chest property rows and get_info accept several path spellings as
// fallbacks (blueprintPath for the primary path, or a resolver bag); all are
// authored optional and the "at least one" requirement is handler-enforced.

static void S_CreateComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Number(TEXT("traceDistance"), TEXT("Trace distance."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureTrace(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .StringEnum(TEXT("traceType"), {
		TEXT("line"), TEXT("sphere"), TEXT("box")
	 }, TEXT("Type of interaction trace."))
	 .Number(TEXT("traceDistance"), TEXT("Trace distance."))
	 .Number(TEXT("traceRadius"), TEXT("Trace radius."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureWidget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("widgetClass"), TEXT("Widget class path."))
	 .Bool(TEXT("showOnHover"), TEXT("Show widget when hovering."))
	 .Bool(TEXT("showPromptText"), TEXT("Show interaction prompt text."))
	 .String(TEXT("promptTextFormat"), TEXT("Format string for prompt (e.g., \"Press {Key} to {Action}\")."))
	 .Required({TEXT("blueprintPath")});
}

static void S_AddEvents(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."));
}

static void S_CreateInteractableInterface(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("folder"), TEXT("Destination folder for create_* actions ('path'/'savePath' accepted as aliases; default /Game/Interactables)."))
	 .Required({TEXT("name")});
}

static void S_CreateDoorActor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("folder"), TEXT("Destination folder for create_* actions ('path'/'savePath' accepted as aliases; default /Game/Interactables)."))
	 .String(TEXT("path"), TEXT("Alias of folder (create_* destination folder)."))
	 .String(TEXT("savePath"), TEXT("Alias of folder (create_* destination folder)."))
	 .Number(TEXT("openAngle"), TEXT("Door open rotation angle in degrees."))
	 .Number(TEXT("openTime"), TEXT("Time to open/close door in seconds."))
	 .Bool(TEXT("autoClose"), TEXT("Automatically close after opening."))
	 .Number(TEXT("autoCloseDelay"), TEXT("Delay before auto-close in seconds."))
	 .Bool(TEXT("requiresKey"), TEXT("Whether interaction requires a key item."))
	 .Required({TEXT("name")});
}

static void S_ConfigureDoorProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("doorPath"), TEXT("Path to door actor blueprint."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("openAngle"), TEXT("Door open rotation angle in degrees."))
	 .Number(TEXT("openTime"), TEXT("Time to open/close door in seconds."))
	 .Bool(TEXT("locked"), TEXT("Whether the item is locked."))
	 .Bool(TEXT("autoClose"), TEXT("Automatically close after opening."))
	 .Number(TEXT("autoCloseDelay"), TEXT("Delay before auto-close in seconds."))
	 .Bool(TEXT("requiresKey"), TEXT("Whether interaction requires a key item."));
}

static void S_CreateSwitchActor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("folder"), TEXT("Destination folder for create_* actions ('path'/'savePath' accepted as aliases; default /Game/Interactables)."))
	 .String(TEXT("path"), TEXT("Alias of folder (create_* destination folder)."))
	 .String(TEXT("savePath"), TEXT("Alias of folder (create_* destination folder)."))
	 .StringEnum(TEXT("switchType"), {
		TEXT("button"), TEXT("lever"), TEXT("pressure_plate"), TEXT("toggle")
	 }, TEXT("Type of switch."))
	 .Required({TEXT("name")});
}

static void S_ConfigureSwitchProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("switchPath"), TEXT("Path to switch actor blueprint."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .StringEnum(TEXT("switchType"), {
		TEXT("button"), TEXT("lever"), TEXT("pressure_plate"), TEXT("toggle")
	 }, TEXT("Type of switch."))
	 .Bool(TEXT("canToggle"), TEXT("Whether switch can be toggled."))
	 .Number(TEXT("resetTime"), TEXT("Time to reset switch in seconds."));
}

static void S_CreateChestActor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("folder"), TEXT("Destination folder for create_* actions ('path'/'savePath' accepted as aliases; default /Game/Interactables)."))
	 .String(TEXT("path"), TEXT("Alias of folder (create_* destination folder)."))
	 .String(TEXT("savePath"), TEXT("Alias of folder (create_* destination folder)."))
	 .Bool(TEXT("locked"), TEXT("Whether the item is locked."))
	 .Required({TEXT("name")});
}

static void S_ConfigureChestProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("chestPath"), TEXT("Path to chest actor blueprint."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("locked"), TEXT("Whether the item is locked."))
	 .Number(TEXT("openAngle"), TEXT("Door open rotation angle in degrees."))
	 .Number(TEXT("openTime"), TEXT("Time to open/close door in seconds."))
	 .String(TEXT("lootTablePath"), TEXT("Path to loot table asset."));
}

static void S_CreateLeverActor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("folder"), TEXT("Destination folder for create_* actions ('path'/'savePath' accepted as aliases; default /Game/Interactables)."))
	 .String(TEXT("path"), TEXT("Alias of folder (create_* destination folder)."))
	 .String(TEXT("savePath"), TEXT("Alias of folder (create_* destination folder)."))
	 .Required({TEXT("name")});
}


static void S_AddDestructionComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("componentName"), TEXT("Name of the component."));
}




static void S_CreateTriggerActor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("folder"), TEXT("Destination folder for create_* actions ('path'/'savePath' accepted as aliases; default /Game/Interactables)."))
	 .StringEnum(TEXT("triggerShape"), {
		TEXT("box"), TEXT("sphere"), TEXT("capsule")
	 }, TEXT("Shape of trigger volume."))
	 .Required({TEXT("name")});
}




static void S_GetInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("doorPath"), TEXT("Path to door actor blueprint."))
	 .String(TEXT("switchPath"), TEXT("Path to switch actor blueprint."))
	 .String(TEXT("chestPath"), TEXT("Path to chest actor blueprint."))
	 .String(TEXT("triggerPath"), TEXT("Path to trigger actor blueprint."));
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row (every implementation body is
// editor-gated). Mutating on all writers; the only reader is get_info.

#define MCP_MI_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                      \
class FMcpCall_ManageInteraction_##ClassSuffix final : public FMcpCall                      \
{                                                                                          \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }          \
	const FMcpCallDecl& GetDecl() const override                                            \
	{                                                                                      \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_interaction"),            \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),              \
			&S_##ClassSuffix);                                                              \
		return D;                                                                          \
	}                                                                                      \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                    \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override    \
	{                                                                                      \
		return HandlerFn(S, RequestId, Payload, Socket);                                     \
	}                                                                                      \
};

// Interaction component (18.1)
MCP_MI_CALL(CreateComponent, "create_interaction_component", McpHandlers::Interaction::HandleInteractionCreateComponent, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureTrace, "configure_interaction_trace", McpHandlers::Interaction::HandleInteractionConfigureTrace, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureWidget, "configure_interaction_widget", McpHandlers::Interaction::HandleInteractionConfigureWidget, EMcpCallFlags::Mutating)
MCP_MI_CALL(AddEvents, "add_interaction_events", McpHandlers::Interaction::HandleInteractionAddEvents, EMcpCallFlags::Mutating)

// Interactables (18.2)
MCP_MI_CALL(CreateInteractableInterface, "create_interactable_interface", McpHandlers::Interaction::HandleInteractionCreateInteractableInterface, EMcpCallFlags::Mutating)
MCP_MI_CALL(CreateDoorActor, "create_door_actor", McpHandlers::Interaction::HandleInteractionCreateDoorActor, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureDoorProperties, "configure_door_properties", McpHandlers::Interaction::HandleInteractionConfigureDoorProperties, EMcpCallFlags::Mutating)
MCP_MI_CALL(CreateSwitchActor, "create_switch_actor", McpHandlers::Interaction::HandleInteractionCreateSwitchActor, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureSwitchProperties, "configure_switch_properties", McpHandlers::Interaction::HandleInteractionConfigureSwitchProperties, EMcpCallFlags::Mutating)
MCP_MI_CALL(CreateChestActor, "create_chest_actor", McpHandlers::Interaction::HandleInteractionCreateChestActor, EMcpCallFlags::Mutating)
MCP_MI_CALL(ConfigureChestProperties, "configure_chest_properties", McpHandlers::Interaction::HandleInteractionConfigureChestProperties, EMcpCallFlags::Mutating)
MCP_MI_CALL(CreateLeverActor, "create_lever_actor", McpHandlers::Interaction::HandleInteractionCreateLeverActor, EMcpCallFlags::Mutating)

// Destructibles (18.3; the configure_destruction_* trio are marker-tag scaffolds)
MCP_MI_CALL(AddDestructionComponent, "add_destruction_component", McpHandlers::Interaction::HandleInteractionAddDestructionComponent, EMcpCallFlags::Mutating)

// Trigger system (18.4; filter/response are variable scaffolds)
MCP_MI_CALL(CreateTriggerActor, "create_trigger_actor", McpHandlers::Interaction::HandleInteractionCreateTriggerActor, EMcpCallFlags::Mutating)

// Utility
MCP_MI_CALL(GetInfo, "get_info", McpHandlers::Interaction::HandleInteractionGetInfo, EMcpCallFlags::None)

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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_AddDestructionComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_CreateTriggerActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInteraction_GetInfo>());
}
