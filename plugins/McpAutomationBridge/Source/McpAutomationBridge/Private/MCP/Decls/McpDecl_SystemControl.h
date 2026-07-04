// Action declarations for system_control — the server's contract: which params
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
inline const FMcpParamDecl P_SystemControl_0[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("childClass"), EMcpParamKind::String, true }, { TEXT("parentName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SystemControl_1[] = { { TEXT("profile"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SystemControl_4[] = { { TEXT("lodBias"), EMcpParamKind::Number, false }, { TEXT("forceLOD"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SystemControl_5[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_6[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("slop"), EMcpParamKind::Number, false }, { TEXT("minScreenRadius"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SystemControl_7[] = { { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("poolSize"), EMcpParamKind::Number, false }, { TEXT("boostPlayerLocation"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_8[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("cellSize"), EMcpParamKind::Number, false }, { TEXT("loadingRange"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SystemControl_9[] = { { TEXT("command"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SystemControl_10[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("widgetType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SystemControl_11[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_13[] = { { TEXT("code"), EMcpParamKind::String, false }, { TEXT("file"), EMcpParamKind::String, false }, { TEXT("allowModalApis"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_14[] = { { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("detailed"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_15[] = { { TEXT("testName"), EMcpParamKind::String, true }, { TEXT("testType"), EMcpParamKind::String, false }, { TEXT("className"), EMcpParamKind::String, false }, { TEXT("flagsExpr"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("overwrite"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_16[] = { { TEXT("buildId"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SystemControl_17[] = { { TEXT("count"), EMcpParamKind::Number, false }, { TEXT("sinceSeq"), EMcpParamKind::Number, false }, { TEXT("verbosity"), EMcpParamKind::String, false }, { TEXT("category"), EMcpParamKind::String, false }, { TEXT("contains"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SystemControl_20[] = { { TEXT("section"), EMcpParamKind::String, false }, { TEXT("category"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SystemControl_21[] = { { TEXT("runId"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SystemControl_22[] = { { TEXT("filter"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SystemControl_25[] = { { TEXT("actors"), EMcpParamKind::Array, true }, { TEXT("packageName"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("replaceSourceActors"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_26[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("instancing"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_27[] = { { TEXT("mode"), EMcpParamKind::String, false }, { TEXT("forceRecompile"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_28[] = { { TEXT("duration"), EMcpParamKind::Number, false }, { TEXT("type"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SystemControl_29[] = { { TEXT("suite"), EMcpParamKind::String, false }, { TEXT("spec"), EMcpParamKind::String, false }, { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("test"), EMcpParamKind::String, false }, { TEXT("maxTests"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SystemControl_30[] = { { TEXT("target"), EMcpParamKind::String, true }, { TEXT("platform"), EMcpParamKind::String, false }, { TEXT("configuration"), EMcpParamKind::String, false }, { TEXT("additionalArgs"), EMcpParamKind::String, false }, { TEXT("arguments"), EMcpParamKind::String, false }, { TEXT("noUBA"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_31[] = { { TEXT("path"), EMcpParamKind::String, false }, { TEXT("filename"), EMcpParamKind::String, false }, { TEXT("returnBase64"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_33[] = { { TEXT("maxFPS"), EMcpParamKind::Number, true } };
inline const FMcpParamDecl P_SystemControl_34[] = { { TEXT("section"), EMcpParamKind::String, true }, { TEXT("key"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SystemControl_35[] = { { TEXT("scale"), EMcpParamKind::Number, true } };
inline const FMcpParamDecl P_SystemControl_36[] = { { TEXT("level"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SystemControl_37[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_38[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_39[] = { { TEXT("category"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SystemControl_40[] = { { TEXT("categoryName"), EMcpParamKind::String, false }, { TEXT("category"), EMcpParamKind::String, false }, { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SystemControl_42[] = { { TEXT("channels"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SystemControl_45[] = { { TEXT("count"), EMcpParamKind::Number, false }, { TEXT("sinceSeq"), EMcpParamKind::Number, false }, { TEXT("verbosity"), EMcpParamKind::String, false }, { TEXT("category"), EMcpParamKind::String, false }, { TEXT("contains"), EMcpParamKind::String, false } };

inline const FMcpCallDecl GSystemControl[] =
{
	{ TEXT("system_control"), TEXT("add_widget_child"), P_SystemControl_0, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("apply_baseline_settings"), P_SystemControl_1, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("capture_stats"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("system_control"), TEXT("clear_log"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("system_control"), TEXT("configure_lod"), P_SystemControl_4, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("configure_nanite"), P_SystemControl_5, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("configure_occlusion_culling"), P_SystemControl_6, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("configure_texture_streaming"), P_SystemControl_7, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("configure_world_partition"), P_SystemControl_8, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("console_command"), P_SystemControl_9, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("create_widget"), P_SystemControl_10, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("enable_gpu_timing"), P_SystemControl_11, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("execute_command"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("system_control"), TEXT("execute_python"), P_SystemControl_13, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("generate_memory_report"), P_SystemControl_14, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("generate_test_stub"), P_SystemControl_15, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("get_build_status"), P_SystemControl_16, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("get_log"), P_SystemControl_17, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("get_perf_stats"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("system_control"), TEXT("get_profile"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("system_control"), TEXT("get_project_settings"), P_SystemControl_20, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("get_test_results"), P_SystemControl_21, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("list_tests"), P_SystemControl_22, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("live_coding_compile"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("system_control"), TEXT("lumen_update_scene"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("system_control"), TEXT("merge_actors"), P_SystemControl_25, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("optimize_draw_calls"), P_SystemControl_26, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("optimize_shaders"), P_SystemControl_27, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("run_benchmark"), P_SystemControl_28, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("run_tests"), P_SystemControl_29, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("run_ubt"), P_SystemControl_30, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("screenshot"), P_SystemControl_31, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("set_cvar"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("system_control"), TEXT("set_frame_rate_limit"), P_SystemControl_33, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("set_project_setting"), P_SystemControl_34, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("set_resolution_scale"), P_SystemControl_35, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("set_scalability"), P_SystemControl_36, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("set_vsync"), P_SystemControl_37, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("show_fps"), P_SystemControl_38, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("show_stats"), P_SystemControl_39, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("spawn_category"), P_SystemControl_40, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("start_profiling"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("system_control"), TEXT("start_session"), P_SystemControl_42, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("stop_profiling"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("system_control"), TEXT("subscribe"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("system_control"), TEXT("tail_log"), P_SystemControl_45, EMcpCallFlags::None },
	{ TEXT("system_control"), TEXT("unsubscribe"), {}, EMcpCallFlags::UnverifiedDecl },
};
}
