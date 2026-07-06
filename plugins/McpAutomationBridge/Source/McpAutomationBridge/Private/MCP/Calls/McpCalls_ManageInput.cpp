// LINT-TOOL: manage_input
// manage_input as FMcpCall classes — Enhanced Input authoring, split out of
// manage_networking (docs/action-declarations.md). Each class co-locates the
// action's declaration with its implementation. Run() delegates to the
// subsystem member handlers — HandleInput* (InputHandlers.cpp) for the 9 input
// actions — until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageInput
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Moved verbatim from the manage_networking family when Enhanced Input split
// out into its own tool.
inline const FMcpParamDecl P_CreateInputAction[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("valueType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateInputMappingContext[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddMapping[] = { { TEXT("contextPath"), EMcpParamKind::String, true }, { TEXT("actionPath"), EMcpParamKind::String, true }, { TEXT("key"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_RemoveMapping[] = { { TEXT("contextPath"), EMcpParamKind::String, true }, { TEXT("actionPath"), EMcpParamKind::String, true }, { TEXT("key"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetInputTrigger[] = { { TEXT("actionPath"), EMcpParamKind::String, true }, { TEXT("triggerType"), EMcpParamKind::String, true }, { TEXT("replace"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetInputModifier[] = { { TEXT("contextPath"), EMcpParamKind::String, false }, { TEXT("actionPath"), EMcpParamKind::String, true }, { TEXT("key"), EMcpParamKind::String, false }, { TEXT("modifierType"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_EnableInputMapping[] = { { TEXT("contextPath"), EMcpParamKind::String, true }, { TEXT("priority"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_DisableInputAction[] = { { TEXT("actionPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_GetInputInfo[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are authored per action and carried over verbatim from manage_networking:
// RequiresEditor on every input action (the chain was whole-editor-gated and the
// members answer their editor-build stubs in non-editor builds); Mutating on the
// writers only. enable_input_mapping and disable_input_action parse and
// acknowledge without writing anything and stay unflagged alongside the reader
// get_info (was get_input_info).

#define MCP_MINPUT_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, Flags)       \
class FMcpCall_ManageInput_##ClassSuffix final : public FMcpCall                         \
{                                                                                        \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl Decl{ TEXT("manage_input"), TEXT(ActionLiteral),       \
			ParamsArray, (Flags) };                                                      \
		return Decl;                                                                     \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

// Input (InputHandlers.cpp)
MCP_MINPUT_CALL(CreateInputAction, "create_input_action", P_CreateInputAction, HandleInputCreateInputAction, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(CreateInputMappingContext, "create_input_mapping_context", P_CreateInputMappingContext, HandleInputCreateInputMappingContext, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(AddMapping, "add_mapping", P_AddMapping, HandleInputAddMapping, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(RemoveMapping, "remove_mapping", P_RemoveMapping, HandleInputRemoveMapping, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(SetInputTrigger, "set_input_trigger", P_SetInputTrigger, HandleInputSetInputTrigger, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(SetInputModifier, "set_input_modifier", P_SetInputModifier, HandleInputSetInputModifier, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MINPUT_CALL(EnableInputMapping, "enable_input_mapping", P_EnableInputMapping, HandleInputEnableInputMapping, EMcpCallFlags::RequiresEditor)
MCP_MINPUT_CALL(DisableInputAction, "disable_input_action", P_DisableInputAction, HandleInputDisableInputAction, EMcpCallFlags::RequiresEditor)
MCP_MINPUT_CALL(GetInputInfo, "get_info", P_GetInputInfo, HandleInputGetInputInfo, EMcpCallFlags::RequiresEditor)

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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInput_EnableInputMapping>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInput_DisableInputAction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInput_GetInputInfo>());
}
