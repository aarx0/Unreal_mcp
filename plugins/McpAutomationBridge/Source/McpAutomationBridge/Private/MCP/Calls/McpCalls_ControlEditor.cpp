// LINT-TOOL: control_editor
// LINT-SCHEMA-DERIVED
// control_editor as FMcpCall classes — third classed family, first to adopt
// schema-from-decls (docs/action-declarations.md). Each class AUTHORS its schema
// fragment in a S_<Suffix>() function; the published facade schema folds those
// fragments and GetDecl() derives the validation decl from the same fragment via
// McpDeriveDecl(), so schema and decl are one source and cannot drift. Run()
// delegates to the subsystem member handlers until the module split de-members
// those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_ControlHandlers.h"
#include "McpAutomationBridge_FocusInputHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ControlEditor
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. Actions that accept
// one-of-several params (possess, open_level, simulate_nav's direction/key)
// author each optional: the requirement is "at least one", which the decl
// vocabulary cannot express and the handler enforces itself. PIE-only
// preconditions (pause, possess, simulate_nav, ...) are handler-enforced too —
// RequiresEditor only covers GEditor existing.

static void S_Play(FMcpSchemaBuilder&) {}
static void S_Stop(FMcpSchemaBuilder&) {}
static void S_Pause(FMcpSchemaBuilder&) {}
static void S_Resume(FMcpSchemaBuilder&) {}
static void S_Eject(FMcpSchemaBuilder&) {}
static void S_StepFrame(FMcpSchemaBuilder&) {}
static void S_ShowStats(FMcpSchemaBuilder&) {}
static void S_HideStats(FMcpSchemaBuilder&) {}
static void S_StopRecording(FMcpSchemaBuilder&) {}
static void S_Undo(FMcpSchemaBuilder&) {}
static void S_Redo(FMcpSchemaBuilder&) {}
static void S_SaveAll(FMcpSchemaBuilder&) {}

static void S_Possess(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("possess: pawn/actor to possess. Omit entirely to repossess the player pawn after eject (the editor's Eject/Possess toggle)."))
	 .String(TEXT("objectPath"), TEXT("possess: object path of the pawn/actor to possess (alternative to actorName)."));
}

static void S_SetGameSpeed(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("speed"), TEXT("set_game_speed: world time-dilation multiplier, >0 and <=20 (default 1.0)."));
}

static void S_SetFixedDeltaTime(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("deltaTime"), TEXT("set_fixed_delta_time: fixed frame time in seconds passed to r.FixedDeltaTime (default 0.01667, ~60fps)."));
}

static void S_SetCamera(FMcpSchemaBuilder& B)
{
	B.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		})
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
		});
}

static void S_SetCameraFov(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("fov"), TEXT("set_camera_fov: viewport field of view in degrees, exclusive 1-179 (default 90)."));
}

static void S_SetViewMode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("viewMode"), TEXT("set_view_mode: Lit|Unlit|Wireframe|DetailLighting|LightingOnly|LightComplexity|ShaderComplexity|LightmapDensity|StationaryLightOverlap|ReflectionOverride, or any other single token passed to the 'viewmode' console command."));
}

static void S_SetViewportRealtime(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("realtime"), TEXT("set_viewport_realtime: enable realtime rendering for the active viewport (default true)."));
}

static void S_SetGameView(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."));
}

static void S_SetImmersiveMode(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."));
}

static void S_FocusActor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Required({TEXT("actorName")});
}

static void S_Screenshot(FMcpSchemaBuilder& B)
{
	B.String(TEXT("filename"), TEXT("screenshot: output PNG name (saved under Saved/Screenshots; defaults to a timestamp)."))
	 .Integer(TEXT("maxWidth"), TEXT("screenshot: downscale the capture to at most this width in px (0/omit = native viewport size)."));
}

static void S_OpenAsset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_CloseAsset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

static void S_OpenLevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelPath"), TEXT("Level asset path."))
	 .String(TEXT("path"), TEXT("Path to a directory."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."));
}

static void S_SetEditorMode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("mode"), TEXT("set_editor_mode: mode name passed verbatim to the 'mode <name>' editor console command (single token; the mode itself is not validated)."))
	 .Required({TEXT("mode")});
}

static void S_SetPreferences(FMcpSchemaBuilder& B)
{
	B.Object(TEXT("preferences"), TEXT("set_preferences: object of console-variable name -> value (string/number/bool) pairs; unmatched names are reported in 'failed'."))
	 .String(TEXT("category"), TEXT("set_preferences: preference category; only 'LevelEditor' is meaningful (enables the non-CVar RealtimeAudio key)."));
}

static void S_CreateBookmark(FMcpSchemaBuilder& B)
{
	B.Integer(TEXT("index"), TEXT("Bookmark slot index (create_bookmark / jump_to_bookmark)."))
	 .Integer(TEXT("id"), TEXT("Bookmark identifier/index."));
}

static void S_JumpToBookmark(FMcpSchemaBuilder& B)
{
	B.Integer(TEXT("index"), TEXT("Bookmark slot index (create_bookmark / jump_to_bookmark)."))
	 .Integer(TEXT("id"), TEXT("Bookmark identifier/index."));
}

static void S_StartRecording(FMcpSchemaBuilder& B)
{
	B.String(TEXT("filename"), TEXT("screenshot: output PNG name (saved under Saved/Screenshots; defaults to a timestamp)."))
	 .String(TEXT("name"), TEXT("Name identifier."));
}

static void S_SimulateInput(FMcpSchemaBuilder& B)
{
	B.String(TEXT("type"), TEXT("Input event type for simulate_input: key_down, key_up, mouse_click, mouse_move, analog "
		"(pressed/released/click/move also accepted). "
		"analog injects a 1D axis value (key=Gamepad_LeftY/Gamepad_RightX/MouseX/... + value); the value persists in "
		"PlayerInput until the next sample, so hold = inject once, release = inject 0."))
	 .String(TEXT("key"), TEXT("simulate_input: FKey name — the key to press/release for key_down/key_up (e.g. SpaceBar, Gamepad_FaceButton_Bottom), or the 1D axis key for analog (Gamepad_LeftY, MouseX, ...)."))
	 .Number(TEXT("x"), TEXT("Mouse X coordinate for simulate_input."))
	 .Number(TEXT("y"), TEXT("Mouse Y coordinate for simulate_input."))
	 .String(TEXT("button"), TEXT("Mouse button for simulate_input."))
	 .Number(TEXT("value"), TEXT("simulate_input analog: axis value (typically -1..1; mouse axes take pixel-ish deltas)."))
	 .String(TEXT("route"), TEXT("simulate_input analog: 'pie' (default — straight to the PIE player controller, focus-immune) or "
		"'slate' (through FSlateApplication + input preprocessors, exercising the CommonUI layer)."));
}

static void S_SimulateNav(FMcpSchemaBuilder& B)
{
	B.String(TEXT("direction"), TEXT("simulate_nav direction: Up/Down/Left/Right/Accept/Back/Next/Previous (PIE-only; faithfully routes a nav key through CommonUI, then returns the post-nav focus snapshot)."))
	 .String(TEXT("device"), TEXT("simulate_nav input device: 'gamepad' (default, sends DPad/face-button keys) or 'keyboard' (arrows/Enter/Esc/Tab)."))
	 .String(TEXT("key"), TEXT("simulate_nav: explicit FKey name to send instead of resolving direction/device (e.g. Gamepad_DPad_Up, Enter)."))
	 .Bool(TEXT("stabilizeFocus"), TEXT("simulate_nav: default true — before the nav, focus the PIE game viewport if focus isn't already inside it, so the faithful drive routes through CommonUI deterministically (never steals focus from an already-focused in-game widget). Set false to drive from whatever currently has focus."));
}

// ─── Classes ─────────────────────────────────────────────────────────────────

#define MCP_CE_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                    \
class FMcpCall_ControlEditor_##ClassSuffix final : public FMcpCall                        \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("control_editor"),              \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),            \
			&S_##ClassSuffix);                                                            \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return HandlerFn(S, RequestId, Payload, Socket);                                   \
	}                                                                                     \
};

MCP_CE_CALL(Play, "play", McpHandlers::ControlEditor::HandleControlEditorPlay, EMcpCallFlags::None)
MCP_CE_CALL(Stop, "stop", McpHandlers::ControlEditor::HandleControlEditorStop, EMcpCallFlags::None)
MCP_CE_CALL(Pause, "pause", McpHandlers::ControlEditor::HandleControlEditorPause, EMcpCallFlags::None)
MCP_CE_CALL(Resume, "resume", McpHandlers::ControlEditor::HandleControlEditorResume, EMcpCallFlags::None)
MCP_CE_CALL(Eject, "eject", McpHandlers::ControlEditor::HandleControlEditorEject, EMcpCallFlags::None)
MCP_CE_CALL(Possess, "possess", McpHandlers::ControlEditor::HandleControlEditorPossess, EMcpCallFlags::None)
MCP_CE_CALL(StepFrame, "step_frame", McpHandlers::ControlEditor::HandleControlEditorStepFrame, EMcpCallFlags::None)
MCP_CE_CALL(SetGameSpeed, "set_game_speed", McpHandlers::ControlEditor::HandleControlEditorSetGameSpeed, EMcpCallFlags::None)
MCP_CE_CALL(SetFixedDeltaTime, "set_fixed_delta_time", McpHandlers::ControlEditor::HandleControlEditorSetFixedDeltaTime, EMcpCallFlags::None)
MCP_CE_CALL(SetCamera, "set_camera", McpHandlers::ControlEditor::HandleControlEditorSetCamera, EMcpCallFlags::None)
MCP_CE_CALL(SetCameraFov, "set_camera_fov", McpHandlers::ControlEditor::HandleControlEditorSetCameraFov, EMcpCallFlags::None)
MCP_CE_CALL(SetViewMode, "set_view_mode", McpHandlers::ControlEditor::HandleControlEditorSetViewMode, EMcpCallFlags::None)
MCP_CE_CALL(SetViewportRealtime, "set_viewport_realtime", McpHandlers::ControlEditor::HandleControlEditorSetViewportRealtime, EMcpCallFlags::None)
MCP_CE_CALL(SetGameView, "set_game_view", McpHandlers::ControlEditor::HandleControlEditorSetGameView, EMcpCallFlags::None)
MCP_CE_CALL(SetImmersiveMode, "set_immersive_mode", McpHandlers::ControlEditor::HandleControlEditorSetImmersiveMode, EMcpCallFlags::None)
MCP_CE_CALL(ShowStats, "show_stats", McpHandlers::ControlEditor::HandleControlEditorShowStats, EMcpCallFlags::None)
MCP_CE_CALL(HideStats, "hide_stats", McpHandlers::ControlEditor::HandleControlEditorHideStats, EMcpCallFlags::None)
MCP_CE_CALL(FocusActor, "focus_actor", McpHandlers::ControlEditor::HandleControlEditorFocusActor, EMcpCallFlags::None)
MCP_CE_CALL(Screenshot, "screenshot", McpHandlers::ControlEditor::HandleControlEditorScreenshot, EMcpCallFlags::None)
MCP_CE_CALL(OpenAsset, "open_asset", McpHandlers::ControlEditor::HandleControlEditorOpenAsset, EMcpCallFlags::None)
MCP_CE_CALL(CloseAsset, "close_asset", McpHandlers::ControlEditor::HandleControlEditorCloseAsset, EMcpCallFlags::None)
MCP_CE_CALL(OpenLevel, "open_level", McpHandlers::ControlEditor::HandleControlEditorOpenLevel, EMcpCallFlags::None)
MCP_CE_CALL(SetEditorMode, "set_editor_mode", McpHandlers::ControlEditor::HandleControlEditorSetEditorMode, EMcpCallFlags::None)
MCP_CE_CALL(SetPreferences, "set_preferences", McpHandlers::ControlEditor::HandleControlEditorSetPreferences, EMcpCallFlags::None)
MCP_CE_CALL(CreateBookmark, "create_bookmark", McpHandlers::ControlEditor::HandleControlEditorCreateBookmark, EMcpCallFlags::None)
MCP_CE_CALL(JumpToBookmark, "jump_to_bookmark", McpHandlers::ControlEditor::HandleControlEditorJumpToBookmark, EMcpCallFlags::None)
MCP_CE_CALL(StartRecording, "start_recording", McpHandlers::ControlEditor::HandleControlEditorStartRecording, EMcpCallFlags::None)
MCP_CE_CALL(StopRecording, "stop_recording", McpHandlers::ControlEditor::HandleControlEditorStopRecording, EMcpCallFlags::None)
MCP_CE_CALL(SimulateInput, "simulate_input", McpHandlers::ControlEditor::HandleControlEditorSimulateInput, EMcpCallFlags::None)
MCP_CE_CALL(SimulateNav, "simulate_nav", McpHandlers::ControlEditor::HandleControlEditorSimulateNav, EMcpCallFlags::None)
MCP_CE_CALL(Undo, "undo", McpHandlers::ControlEditor::HandleControlEditorUndo, EMcpCallFlags::Mutating)
MCP_CE_CALL(Redo, "redo", McpHandlers::ControlEditor::HandleControlEditorRedo, EMcpCallFlags::Mutating)
MCP_CE_CALL(SaveAll, "save_all", McpHandlers::ControlEditor::HandleControlEditorSaveAll, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

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
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_Undo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_Redo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlEditor_SaveAll>());
}
