// Action declarations for control_editor — the server's contract: which params
// each action reads, and which are required. Fleet-authored from handler
// source (three-witness cross-check, 2026-07-04), hand-maintained since:
// adding an action = adding its declaration here (the boot validation and
// tests/schema/action-decl-lint.ps1 enforce both directions).
// UnverifiedDecl = no reachable read path was attributable; validation skips
// those actions and the lint nags until someone verifies or removes them.
#pragma once

#include "MCP/McpCallRegistry.h"

namespace McpDecls
{
inline const FMcpParamDecl P_ControlEditor_0[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ControlEditor_1[] = { { TEXT("command"), EMcpParamKind::String, false }, { TEXT("params"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ControlEditor_2[] = { { TEXT("index"), EMcpParamKind::Number, false }, { TEXT("id"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ControlEditor_5[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ControlEditor_7[] = { { TEXT("index"), EMcpParamKind::Number, false }, { TEXT("id"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ControlEditor_8[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ControlEditor_9[] = { { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlEditor_12[] = { { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("actor_name"), EMcpParamKind::String, true }, { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlEditor_16[] = { { TEXT("filename"), EMcpParamKind::String, false }, { TEXT("maxWidth"), EMcpParamKind::Number, false }, { TEXT("path"), EMcpParamKind::Any, false }, { TEXT("returnBase64"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ControlEditor_17[] = { { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ControlEditor_18[] = { { TEXT("fov"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ControlEditor_19[] = { { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("params"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Any, false }, { TEXT("rotation"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ControlEditor_20[] = { { TEXT("mode"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ControlEditor_21[] = { { TEXT("deltaTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ControlEditor_22[] = { { TEXT("speed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ControlEditor_23[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ControlEditor_24[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ControlEditor_25[] = { { TEXT("category"), EMcpParamKind::String, false }, { TEXT("preferences"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ControlEditor_26[] = { { TEXT("viewMode"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlEditor_27[] = { { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("params"), EMcpParamKind::Object, false }, { TEXT("location"), EMcpParamKind::Any, false }, { TEXT("rotation"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ControlEditor_28[] = { { TEXT("realtime"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ControlEditor_31[] = { { TEXT("keyName"), EMcpParamKind::String, false }, { TEXT("key"), EMcpParamKind::String, false }, { TEXT("eventType"), EMcpParamKind::String, false }, { TEXT("type"), EMcpParamKind::String, false }, { TEXT("inputType"), EMcpParamKind::String, false }, { TEXT("inputAction"), EMcpParamKind::String, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("button"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Number, false }, { TEXT("route"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlEditor_32[] = { { TEXT("direction"), EMcpParamKind::String, false }, { TEXT("device"), EMcpParamKind::String, false }, { TEXT("key"), EMcpParamKind::String, false }, { TEXT("stabilizeFocus"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ControlEditor_34[] = { { TEXT("filename"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };

inline const FMcpCallDecl GControlEditor[] =
{
	{ TEXT("control_editor"), TEXT("close_asset"), P_ControlEditor_0, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("console_command"), P_ControlEditor_1, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("create_bookmark"), P_ControlEditor_2, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("eject"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("execute_command"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("focus_actor"), P_ControlEditor_5, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("hide_stats"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("jump_to_bookmark"), P_ControlEditor_7, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("open_asset"), P_ControlEditor_8, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("open_level"), P_ControlEditor_9, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("pause"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("play"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("possess"), P_ControlEditor_12, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("redo"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("resume"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("save_all"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("screenshot"), P_ControlEditor_16, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_camera"), P_ControlEditor_17, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_camera_fov"), P_ControlEditor_18, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_camera_position"), P_ControlEditor_19, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_editor_mode"), P_ControlEditor_20, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_fixed_delta_time"), P_ControlEditor_21, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_game_speed"), P_ControlEditor_22, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_game_view"), P_ControlEditor_23, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_immersive_mode"), P_ControlEditor_24, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_preferences"), P_ControlEditor_25, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_view_mode"), P_ControlEditor_26, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_viewport_camera"), P_ControlEditor_27, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_viewport_realtime"), P_ControlEditor_28, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("set_viewport_resolution"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("show_stats"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("simulate_input"), P_ControlEditor_31, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("simulate_nav"), P_ControlEditor_32, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("single_frame_step"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("start_recording"), P_ControlEditor_34, EMcpCallFlags::None },
	{ TEXT("control_editor"), TEXT("step_frame"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("stop"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("stop_pie"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("stop_recording"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("take_screenshot"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("control_editor"), TEXT("undo"), {}, EMcpCallFlags::UnverifiedDecl },
};
}
