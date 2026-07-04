// Action declarations for manage_sequence — the server's contract: which params
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
inline const FMcpParamDecl P_ManageSequence_0[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_1[] = { { TEXT("actorNames"), EMcpParamKind::Array, true }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_2[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_3[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("bindingId"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("property"), EMcpParamKind::String, false }, { TEXT("frame"), EMcpParamKind::Number, true }, { TEXT("value"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ManageSequence_4[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("startFrame"), EMcpParamKind::Number, false }, { TEXT("endFrame"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageSequence_5[] = { { TEXT("className"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_6[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackType"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageSequence_10[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_12[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_15[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_16[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_19[] = { { TEXT("actorNames"), EMcpParamKind::Array, true }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_20[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageSequence_22[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("frameRate"), EMcpParamKind::Any, true } };
inline const FMcpParamDecl P_ManageSequence_24[] = { { TEXT("speed"), EMcpParamKind::Number, false }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_25[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("frameRate"), EMcpParamKind::Number, false }, { TEXT("lengthInFrames"), EMcpParamKind::Number, false }, { TEXT("playbackStart"), EMcpParamKind::Number, false }, { TEXT("playbackEnd"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageSequence_26[] = { { TEXT("resolution"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_27[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false }, { TEXT("locked"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageSequence_28[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false }, { TEXT("muted"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageSequence_29[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false }, { TEXT("solo"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageSequence_30[] = { { TEXT("start"), EMcpParamKind::Number, false }, { TEXT("end"), EMcpParamKind::Number, false }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageSequence_31[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("start"), EMcpParamKind::Number, false }, { TEXT("end"), EMcpParamKind::Number, false } };

inline const FMcpCallDecl GManageSequence[] =
{
	{ TEXT("manage_sequence"), TEXT("add_actor"), P_ManageSequence_0, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_sequence"), TEXT("add_actors"), P_ManageSequence_1, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("add_camera"), P_ManageSequence_2, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("add_keyframe"), P_ManageSequence_3, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("add_section"), P_ManageSequence_4, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("add_spawnable_from_class"), P_ManageSequence_5, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("add_track"), P_ManageSequence_6, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("create"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_sequence"), TEXT("delete"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_sequence"), TEXT("duplicate"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_sequence"), TEXT("get_bindings"), P_ManageSequence_10, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("get_metadata"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_sequence"), TEXT("get_properties"), P_ManageSequence_12, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("list"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_sequence"), TEXT("list_track_types"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_sequence"), TEXT("list_tracks"), P_ManageSequence_15, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("open"), P_ManageSequence_16, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("pause"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_sequence"), TEXT("play"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_sequence"), TEXT("remove_actors"), P_ManageSequence_19, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("remove_track"), P_ManageSequence_20, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("rename"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_sequence"), TEXT("set_display_rate"), P_ManageSequence_22, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("set_metadata"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_sequence"), TEXT("set_playback_speed"), P_ManageSequence_24, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("set_properties"), P_ManageSequence_25, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("set_tick_resolution"), P_ManageSequence_26, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("set_track_locked"), P_ManageSequence_27, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("set_track_muted"), P_ManageSequence_28, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("set_track_solo"), P_ManageSequence_29, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("set_view_range"), P_ManageSequence_30, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("set_work_range"), P_ManageSequence_31, EMcpCallFlags::None },
	{ TEXT("manage_sequence"), TEXT("stop"), {}, EMcpCallFlags::UnverifiedDecl },
};
}
