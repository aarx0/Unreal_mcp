// LINT-TOOL: manage_input
// LINT-SCHEMA-DERIVED
// manage_input as FMcpCall classes — Enhanced Input authoring, split out of
// manage_networking (docs/action-declarations.md). Adopts schema-from-decls:
// each class AUTHORS its schema fragment in a S_<Suffix>() function; the
// published facade schema folds those fragments and GetDecl() derives the
// validation decl from the same fragment via McpDeriveDecl(), so schema and
// decl are one source and cannot drift. Run() delegates to the namespaced free
// handlers (McpHandlers::Input::HandleInput*, McpAutomationBridge_InputHandlers.cpp)
// for the 7 input actions.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_InputHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageInput
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. set_input_modifier and
// remove_mapping author some params optional: the handler enforces the "at
// least one target" semantics itself.

static void S_CreateInputAction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_input_action / create_input_mapping_context)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created input asset."))
	 .StringEnum(TEXT("valueType"), {
		TEXT("Boolean"), TEXT("Axis1D"), TEXT("Axis2D"), TEXT("Axis3D")
	 }, TEXT("InputAction value type (create_input_action; default Boolean)."))
	 .Required({TEXT("name"), TEXT("path")});
}

static void S_CreateInputMappingContext(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_input_action / create_input_mapping_context)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created input asset."))
	 .Required({TEXT("name"), TEXT("path")});
}

static void S_AddMapping(FMcpSchemaBuilder& B)
{
	B.String(TEXT("contextPath"), TEXT("InputMappingContext asset path (add_mapping / remove_mapping)."))
	 .String(TEXT("actionPath"), TEXT("InputAction asset path (mappings / triggers)."))
	 .String(TEXT("key"), TEXT("Key id for a mapping (e.g. SpaceBar, Gamepad_FaceButton_Bottom)."))
	 .Required({TEXT("contextPath"), TEXT("actionPath"), TEXT("key")});
}

static void S_RemoveMapping(FMcpSchemaBuilder& B)
{
	B.String(TEXT("contextPath"), TEXT("InputMappingContext asset path (add_mapping / remove_mapping)."))
	 .String(TEXT("actionPath"), TEXT("InputAction asset path (mappings / triggers)."))
	 .String(TEXT("key"), TEXT("Key id for a mapping (e.g. SpaceBar, Gamepad_FaceButton_Bottom)."))
	 .Required({TEXT("contextPath"), TEXT("actionPath")});
}

static void S_SetInputTrigger(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actionPath"), TEXT("InputAction asset path (mappings / triggers)."))
	 .StringEnum(TEXT("triggerType"), {
		TEXT("Pressed"), TEXT("Released"), TEXT("Down"), TEXT("Tap"), TEXT("Hold"),
		TEXT("HoldAndRelease"), TEXT("Pulse"), TEXT("RepeatedTap")
	 }, TEXT("Input trigger class (set_input_trigger)."))
	 .Bool(TEXT("replace"), TEXT("set_input_trigger: clear existing triggers before adding (default false = idempotent dedupe)."))
	 .Required({TEXT("actionPath"), TEXT("triggerType")});
}

static void S_SetInputModifier(FMcpSchemaBuilder& B)
{
	B.String(TEXT("contextPath"), TEXT("InputMappingContext asset path (add_mapping / remove_mapping)."))
	 .String(TEXT("actionPath"), TEXT("InputAction asset path (mappings / triggers)."))
	 .String(TEXT("key"), TEXT("Key id for a mapping (e.g. SpaceBar, Gamepad_FaceButton_Bottom)."))
	 .String(TEXT("modifierType"), TEXT("Input modifier class (set_input_modifier, e.g. Negate, SwizzleAxis)."))
	 .Required({TEXT("actionPath"), TEXT("modifierType")});
}

static void S_GetInputInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Input asset to read (get_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset whose input mappings to read (get_info; accepted as a fallback for assetPath)."));
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are authored per action and carried over verbatim from manage_networking:
// RequiresEditor on every input action (the chain was whole-editor-gated and the
// members answer their editor-build stubs in non-editor builds); Mutating on the
// writers only. The reader is get_info (was get_input_info).

#define MCP_MINPUT_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                     \
class FMcpCall_ManageInput_##ClassSuffix final : public FMcpCall                          \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_input"),                \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                             \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return HandlerFn(S, RequestId, Payload, Socket);                                     \
	}                                                                                     \
};

// Input (InputHandlers.cpp)
MCP_MINPUT_CALL(CreateInputAction, "create_input_action", McpHandlers::Input::HandleInputCreateInputAction, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(CreateInputMappingContext, "create_input_mapping_context", McpHandlers::Input::HandleInputCreateInputMappingContext, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(AddMapping, "add_mapping", McpHandlers::Input::HandleInputAddMapping, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(RemoveMapping, "remove_mapping", McpHandlers::Input::HandleInputRemoveMapping, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(SetInputTrigger, "set_input_trigger", McpHandlers::Input::HandleInputSetInputTrigger, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(SetInputModifier, "set_input_modifier", McpHandlers::Input::HandleInputSetInputModifier, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(GetInputInfo, "get_info", McpHandlers::Input::HandleInputGetInputInfo, EMcpCallFlags::RequiresEditor)

#undef MCP_MINPUT_CALL

} // namespace McpCalls::ManageInput

void McpRegisterManageInputCalls()
{
	using namespace McpCalls::ManageInput;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInput_CreateInputAction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInput_CreateInputMappingContext>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInput_AddMapping>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInput_RemoveMapping>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInput_SetInputTrigger>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInput_SetInputModifier>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInput_GetInputInfo>());
}
