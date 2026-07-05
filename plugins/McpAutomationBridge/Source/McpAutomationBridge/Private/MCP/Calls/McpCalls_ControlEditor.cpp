// LINT-TOOL: control_editor
// control_editor as FMcpCall classes — third classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ControlEditor
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Authored from the handler bodies' actual payload reads (the shim decls for
// this family were the 04e mega-bag class: execute_command and
// single_frame_step carried identical 30-param unions with required params
// their handlers never read). Actions that accept one-of-several params
// (possess, open_level, simulate_nav's direction/key) declare each optional:
// the requirement is "at least one", which the decl vocabulary cannot express
// and the handler enforces itself. PIE-only preconditions (pause, possess,
// simulate_nav, ...) are handler-enforced too — RequiresEditor only covers
// GEditor existing.

inline const FMcpParamDecl P_Possess[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetGameSpeed[] = { { TEXT("speed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetFixedDeltaTime[] = { { TEXT("deltaTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetCamera[] = { { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_SetCameraFov[] = { { TEXT("fov"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetViewMode[] = { { TEXT("viewMode"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetViewportRealtime[] = { { TEXT("realtime"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetGameView[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetImmersiveMode[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_FocusActor[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Screenshot[] = { { TEXT("filename"), EMcpParamKind::String, false }, { TEXT("maxWidth"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_OpenAsset[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CloseAsset[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_OpenLevel[] = { { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetEditorMode[] = { { TEXT("mode"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetPreferences[] = { { TEXT("preferences"), EMcpParamKind::Object, false }, { TEXT("category"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateBookmark[] = { { TEXT("index"), EMcpParamKind::Number, false }, { TEXT("id"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_JumpToBookmark[] = { { TEXT("index"), EMcpParamKind::Number, false }, { TEXT("id"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_StartRecording[] = { { TEXT("filename"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SimulateInput[] = { { TEXT("type"), EMcpParamKind::String, false }, { TEXT("inputType"), EMcpParamKind::String, false }, { TEXT("inputAction"), EMcpParamKind::String, false }, { TEXT("key"), EMcpParamKind::String, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("button"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Number, false }, { TEXT("route"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SimulateNav[] = { { TEXT("direction"), EMcpParamKind::String, false }, { TEXT("device"), EMcpParamKind::String, false }, { TEXT("key"), EMcpParamKind::String, false }, { TEXT("stabilizeFocus"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ExecuteCommand[] = { { TEXT("command"), EMcpParamKind::String, true } };

// ─── Classes ─────────────────────────────────────────────────────────────────

#define MCP_CE_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)       \
class FMcpCall_ControlEditor_##ClassSuffix final : public FMcpCall                        \
{                                                                                         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl Decl{ TEXT("control_editor"), TEXT(ActionLiteral),      \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                  \
		return Decl;                                                                      \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return S.HandlerFn(RequestId, Payload, Socket);                                   \
	}                                                                                     \
};

MCP_CE_CALL(Play, "play", {}, HandleControlEditorPlay, EMcpCallFlags::None)
MCP_CE_CALL(Stop, "stop", {}, HandleControlEditorStop, EMcpCallFlags::None)
MCP_CE_CALL(Pause, "pause", {}, HandleControlEditorPause, EMcpCallFlags::None)
MCP_CE_CALL(Resume, "resume", {}, HandleControlEditorResume, EMcpCallFlags::None)
MCP_CE_CALL(Eject, "eject", {}, HandleControlEditorEject, EMcpCallFlags::None)
MCP_CE_CALL(Possess, "possess", P_Possess, HandleControlEditorPossess, EMcpCallFlags::None)
MCP_CE_CALL(StepFrame, "step_frame", {}, HandleControlEditorStepFrame, EMcpCallFlags::None)
MCP_CE_CALL(SingleFrameStep, "single_frame_step", {}, HandleControlEditorStepFrame, EMcpCallFlags::None)
MCP_CE_CALL(SetGameSpeed, "set_game_speed", P_SetGameSpeed, HandleControlEditorSetGameSpeed, EMcpCallFlags::None)
MCP_CE_CALL(SetFixedDeltaTime, "set_fixed_delta_time", P_SetFixedDeltaTime, HandleControlEditorSetFixedDeltaTime, EMcpCallFlags::None)
MCP_CE_CALL(SetCamera, "set_camera", P_SetCamera, HandleControlEditorSetCamera, EMcpCallFlags::None)
MCP_CE_CALL(SetCameraFov, "set_camera_fov", P_SetCameraFov, HandleControlEditorSetCameraFov, EMcpCallFlags::None)
MCP_CE_CALL(SetViewMode, "set_view_mode", P_SetViewMode, HandleControlEditorSetViewMode, EMcpCallFlags::None)
MCP_CE_CALL(SetViewportRealtime, "set_viewport_realtime", P_SetViewportRealtime, HandleControlEditorSetViewportRealtime, EMcpCallFlags::None)
MCP_CE_CALL(SetGameView, "set_game_view", P_SetGameView, HandleControlEditorSetGameView, EMcpCallFlags::None)
MCP_CE_CALL(SetImmersiveMode, "set_immersive_mode", P_SetImmersiveMode, HandleControlEditorSetImmersiveMode, EMcpCallFlags::None)
MCP_CE_CALL(ShowStats, "show_stats", {}, HandleControlEditorShowStats, EMcpCallFlags::None)
MCP_CE_CALL(HideStats, "hide_stats", {}, HandleControlEditorHideStats, EMcpCallFlags::None)
MCP_CE_CALL(FocusActor, "focus_actor", P_FocusActor, HandleControlEditorFocusActor, EMcpCallFlags::None)
MCP_CE_CALL(Screenshot, "screenshot", P_Screenshot, HandleControlEditorScreenshot, EMcpCallFlags::None)
MCP_CE_CALL(OpenAsset, "open_asset", P_OpenAsset, HandleControlEditorOpenAsset, EMcpCallFlags::None)
MCP_CE_CALL(CloseAsset, "close_asset", P_CloseAsset, HandleControlEditorCloseAsset, EMcpCallFlags::None)
MCP_CE_CALL(OpenLevel, "open_level", P_OpenLevel, HandleControlEditorOpenLevel, EMcpCallFlags::None)
MCP_CE_CALL(SetEditorMode, "set_editor_mode", P_SetEditorMode, HandleControlEditorSetEditorMode, EMcpCallFlags::None)
MCP_CE_CALL(SetPreferences, "set_preferences", P_SetPreferences, HandleControlEditorSetPreferences, EMcpCallFlags::None)
MCP_CE_CALL(CreateBookmark, "create_bookmark", P_CreateBookmark, HandleControlEditorCreateBookmark, EMcpCallFlags::None)
MCP_CE_CALL(JumpToBookmark, "jump_to_bookmark", P_JumpToBookmark, HandleControlEditorJumpToBookmark, EMcpCallFlags::None)
MCP_CE_CALL(StartRecording, "start_recording", P_StartRecording, HandleControlEditorStartRecording, EMcpCallFlags::None)
MCP_CE_CALL(StopRecording, "stop_recording", {}, HandleControlEditorStopRecording, EMcpCallFlags::None)
MCP_CE_CALL(SimulateInput, "simulate_input", P_SimulateInput, HandleControlEditorSimulateInput, EMcpCallFlags::None)
MCP_CE_CALL(SimulateNav, "simulate_nav", P_SimulateNav, HandleControlEditorSimulateNav, EMcpCallFlags::None)
MCP_CE_CALL(ExecuteCommand, "execute_command", P_ExecuteCommand, HandleControlEditorConsoleCommand, EMcpCallFlags::Mutating)
MCP_CE_CALL(Undo, "undo", {}, HandleControlEditorUndo, EMcpCallFlags::Mutating)
MCP_CE_CALL(Redo, "redo", {}, HandleControlEditorRedo, EMcpCallFlags::Mutating)
MCP_CE_CALL(SaveAll, "save_all", {}, HandleControlEditorSaveAll, EMcpCallFlags::Mutating)

#undef MCP_CE_CALL

} // namespace McpCalls::ControlEditor

void McpRegisterControlEditorCalls()
{
	using namespace McpCalls::ControlEditor;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_Play>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_Stop>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_Pause>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_Resume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_Eject>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_Possess>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_StepFrame>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SingleFrameStep>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SetGameSpeed>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SetFixedDeltaTime>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SetCamera>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SetCameraFov>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SetViewMode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SetViewportRealtime>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SetGameView>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SetImmersiveMode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_ShowStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_HideStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_FocusActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_Screenshot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_OpenAsset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_CloseAsset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_OpenLevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SetEditorMode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SetPreferences>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_CreateBookmark>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_JumpToBookmark>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_StartRecording>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_StopRecording>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SimulateInput>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SimulateNav>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_ExecuteCommand>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_Undo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_Redo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SaveAll>());
}
