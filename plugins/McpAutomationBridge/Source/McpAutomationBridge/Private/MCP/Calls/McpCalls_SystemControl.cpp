// LINT-TOOL: system_control
// LINT-SCHEMA-DERIVED
// system_control as FMcpCall classes — schema-from-decls
// (docs/action-declarations.md). Each class AUTHORS its schema fragment in a
// S_<Suffix>() function; the published facade schema folds those fragments and
// GetDecl() derives the validation decl from the same fragment via
// McpDeriveDecl(), so schema and decl are one source and cannot drift. Run()
// delegates to the subsystem member handlers until the module split de-members
// those bodies. Unlike earlier families the implementations span six handler
// TUs (Performance / SystemControl / Ui / Log / Debug / Render handlers), and
// execute_command / set_cvar delegate to HandleConsoleCommandAction (which has
// other owners and keeps its internal "console_command" literal).
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_SystemControlHandlers.h"
#include "McpAutomationBridge_PerformanceHandlers.h"
#include "McpAutomationBridge_UiHandlers.h"
#include "McpAutomationBridge_LogHandlers.h"
#include "McpAutomationBridge_DebugHandlers.h"
#include "McpAutomationBridge_RenderHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::SystemControl
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. Ported from this
// family's retired param arrays (fleet-verified against the handler bodies),
// except: configure_texture_streaming and generate_memory_report never carried
// the bogus required functionName (mega-bag residue); the facade's dead
// `resolution` param (no handler reads it) is dropped. Shared param names (enabled, category,
// section, key, value, outputPath, filter, the get_log/tail_log set) author an
// identical description in every fragment so the fold is unambiguous.

static void S_LiveCodingCompile(FMcpSchemaBuilder&) {}
static void S_ClearLog(FMcpSchemaBuilder&) {}
static void S_Subscribe(FMcpSchemaBuilder&) {}
static void S_Unsubscribe(FMcpSchemaBuilder&) {}
static void S_LumenUpdateScene(FMcpSchemaBuilder&) {}
static void S_GetPerfStats(FMcpSchemaBuilder&) {}
static void S_StartProfiling(FMcpSchemaBuilder&) {}
static void S_StopProfiling(FMcpSchemaBuilder&) {}

static void S_GenerateTestStub(FMcpSchemaBuilder& B)
{
	B.String(TEXT("testName"), TEXT("generate_test_stub: the registered test path, e.g. 'Combat.DamageApplies' (also the run_tests filter)."))
	 .String(TEXT("testType"), TEXT("generate_test_stub: simple (default) | complex (parameterized) | latent (multi-frame)."))
	 .String(TEXT("className"), TEXT("generate_test_stub: C++ class symbol (default derived from testName, e.g. FCombatDamageAppliesTest)."))
	 .String(TEXT("flagsExpr"), TEXT("generate_test_stub: override the EAutomationTestFlags expression (default EditorContext | EngineFilter)."))
	 .String(TEXT("outputPath"), TEXT("generate_test_stub: .cpp path to write (default: the bridge plugin's GeneratedTests dir)."))
	 .Bool(TEXT("overwrite"), TEXT("generate_test_stub: replace the file if it already exists (default false)."))
	 .Required({TEXT("testName")});
}

static void S_RunUbt(FMcpSchemaBuilder& B)
{
	B.String(TEXT("target"), TEXT("run_ubt: build target (e.g. MyProjectEditor). Non-blocking: returns a buildId immediately; poll get_build_status."))
	 .String(TEXT("platform"), TEXT(""))
	 .String(TEXT("configuration"), TEXT(""))
	 .String(TEXT("additionalArgs"), TEXT("run_ubt: extra UBT command-line arguments (alias: arguments)."))
	 .String(TEXT("arguments"), TEXT(""))
	 .Bool(TEXT("noUBA"), TEXT("run_ubt: pass -NoUBA (default true — UBA-built binaries break later Live Coding patches). Set false for faster UBA builds when no LC patching will follow."))
	 .Required({TEXT("target")});
}

static void S_GetBuildStatus(FMcpSchemaBuilder& B)
{
	B.String(TEXT("buildId"), TEXT("get_build_status: buildId from run_ubt (default: most recent build this session). Returns status running|succeeded|failed, [N/M] progress, parsed errors, warningCount, resultLine, logPath."));
}

static void S_ListTests(FMcpSchemaBuilder& B)
{
	B.String(TEXT("filter"), TEXT("Substring filter over automation test display name / full path / test name (run_tests, list_tests)."));
}

static void S_RunTests(FMcpSchemaBuilder& B)
{
	B.String(TEXT("suite"), TEXT("run_tests: \"ui-nav\" runs the checked-in CommonUI focus/nav spec suite (tests/ui-nav) via the external pwsh runner, fire-and-poll like run_ubt; omit for engine automation tests. Needs pwsh on PATH and an idle editor."))
	 .String(TEXT("spec"), TEXT("run_tests suite:\"ui-nav\": single spec to run, by name or filename (e.g. \"pause_menu\"); omit to run every spec in tests/ui-nav."))
	 .String(TEXT("filter"), TEXT("Substring filter over automation test display name / full path / test name (run_tests, list_tests)."))
	 .String(TEXT("test"), TEXT("Exact/partial test name to run (run_tests; alias for filter)."))
	 .Number(TEXT("maxTests"), TEXT("Max tests to queue for run_tests (1-500, default 50)."));
}

static void S_GetTestResults(FMcpSchemaBuilder& B)
{
	B.String(TEXT("runId"), TEXT("Run identifier returned by run_tests; pass to get_test_results."));
}

static void S_ExecutePython(FMcpSchemaBuilder& B)
{
	B.String(TEXT("code"), TEXT("Python code to execute inline"))
	 .String(TEXT("file"), TEXT("Path to .py file to execute"))
	 .Bool(TEXT("allowModalApis"), TEXT("execute_python: opt out of the modal-API guard (reload_packages / *_with_dialog / EditorDialog are blocked by default — a modal dialog permanently freezes a headless editor). Only pass true when the exact call cannot prompt."));
}

static void S_StartSession(FMcpSchemaBuilder& B)
{
	B.String(TEXT("channels"), TEXT(""));
}

static void S_ExecuteCommand(FMcpSchemaBuilder& B)
{
	B.String(TEXT("command"), TEXT(""))
	 .Required({TEXT("command")});
}

static void S_SetCvar(FMcpSchemaBuilder& B)
{
	B.String(TEXT("key"), TEXT(""))
	 .String(TEXT("value"), TEXT(""))
	 .Required({TEXT("key")});
}

static void S_GetProjectSettings(FMcpSchemaBuilder& B)
{
	B.String(TEXT("section"), TEXT(""))
	 .String(TEXT("category"), TEXT(""));
}

static void S_SetProjectSetting(FMcpSchemaBuilder& B)
{
	B.String(TEXT("section"), TEXT(""))
	 .String(TEXT("key"), TEXT(""))
	 .String(TEXT("value"), TEXT(""))
	 .Required({TEXT("section"), TEXT("key")});
}

static void S_GetLog(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("count"), TEXT("get_log/tail_log: max lines to return (default 100, max 2000)."))
	 .Number(TEXT("sinceSeq"), TEXT("get_log/tail_log: return only lines with seq > this (incremental polling; response carries nextSeq + dropped)."))
	 .String(TEXT("verbosity"), TEXT("get_log/tail_log: minimum severity to include — Fatal|Error|Warning|Display|Log|Verbose|VeryVerbose (default Verbose = all)."))
	 .String(TEXT("category"), TEXT(""))
	 .String(TEXT("contains"), TEXT("get_log/tail_log: case-insensitive substring the message must contain."));
}

static void S_TailLog(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("count"), TEXT("get_log/tail_log: max lines to return (default 100, max 2000)."))
	 .Number(TEXT("sinceSeq"), TEXT("get_log/tail_log: return only lines with seq > this (incremental polling; response carries nextSeq + dropped)."))
	 .String(TEXT("verbosity"), TEXT("get_log/tail_log: minimum severity to include — Fatal|Error|Warning|Display|Log|Verbose|VeryVerbose (default Verbose = all)."))
	 .String(TEXT("category"), TEXT(""))
	 .String(TEXT("contains"), TEXT("get_log/tail_log: case-insensitive substring the message must contain."));
}

static void S_SpawnCategory(FMcpSchemaBuilder& B)
{
	B.String(TEXT("categoryName"), TEXT("spawn_category: gameplay debugger category name (falls back to 'category' if absent)."))
	 .String(TEXT("category"), TEXT(""))
	 .Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."));
}

static void S_GenerateMemoryReport(FMcpSchemaBuilder& B)
{
	B.String(TEXT("outputPath"), TEXT("generate_test_stub: .cpp path to write (default: the bridge plugin's GeneratedTests dir)."))
	 .Bool(TEXT("detailed"), TEXT("generate_memory_report: emit the full/verbose report (memreport -full) instead of the summary."));
}

static void S_ShowFps(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."));
}

static void S_SetScalability(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("level"), TEXT(""));
}

static void S_SetResolutionScale(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("scale"), TEXT("set_resolution_scale: r.ScreenPercentage value (e.g. 100 = native)."))
	 .Required({TEXT("scale")});
}

static void S_SetVsync(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."));
}

static void S_SetFrameRateLimit(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("maxFPS"), TEXT("set_frame_rate_limit: maximum frame rate."))
	 .Required({TEXT("maxFPS")});
}

static void S_EnableGpuTiming(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."));
}

static void S_ConfigureNanite(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."));
}

static void S_ConfigureLod(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("lodBias"), TEXT("configure_lod: r.MipMapLODBias."))
	 .Number(TEXT("forceLOD"), TEXT("configure_lod: r.ForceLOD index (-1 = auto)."));
}

static void S_ConfigureTextureStreaming(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
	 .Number(TEXT("poolSize"), TEXT("configure_texture_streaming: r.Streaming.PoolSize in MB."))
	 .Bool(TEXT("boostPlayerLocation"), TEXT("configure_texture_streaming: boost streaming around the active camera location."));
}

static void S_ApplyBaselineSettings(FMcpSchemaBuilder& B)
{
	B.String(TEXT("profile"), TEXT("apply_baseline_settings: performance | quality | balanced (default balanced)."));
}

static void S_OptimizeDrawCalls(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
	 .Bool(TEXT("instancing"), TEXT("optimize_draw_calls: enable dynamic mesh draw command instancing."));
}

static void S_ConfigureOcclusionCulling(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
	 .Number(TEXT("slop"), TEXT("configure_occlusion_culling: r.OcclusionSlop."))
	 .Number(TEXT("minScreenRadius"), TEXT("configure_occlusion_culling: r.OcclusionCullMinScreenRadius."));
}

static void S_OptimizeShaders(FMcpSchemaBuilder& B)
{
	B.String(TEXT("mode"), TEXT("optimize_shaders: changed (default) | material | global."))
	 .Bool(TEXT("forceRecompile"), TEXT("optimize_shaders: recompile all shaders regardless of mode."));
}

static void S_ConfigureWorldPartition(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
	 .Number(TEXT("cellSize"), TEXT("configure_world_partition: wp.Runtime.RuntimeCellSize."))
	 .Number(TEXT("loadingRange"), TEXT("configure_world_partition: wp.Runtime.RuntimeStreamingRange."));
}

static void S_MergeActors(FMcpSchemaBuilder& B)
{
	B.Array(TEXT("actors"), TEXT("merge_actors: actor names/labels/paths to merge (at least 2)."))
	 .String(TEXT("packageName"), TEXT("merge_actors: destination package path for the merged static mesh (alias: outputPath)."))
	 .String(TEXT("outputPath"), TEXT("generate_test_stub: .cpp path to write (default: the bridge plugin's GeneratedTests dir)."))
	 .Bool(TEXT("replaceSourceActors"), TEXT("merge_actors: destroy the source actors once the merge succeeds (default false)."))
	 .Required({TEXT("actors")});
}

static void S_RunBenchmark(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("duration"), TEXT("run_benchmark: seconds to run before completing (default 60)."))
	 .String(TEXT("type"), TEXT("run_benchmark: benchmark type — all (default) | gpu."));
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are explicit and complete per action — this family is mixed: the log
// actions and the console/config delegations run in non-editor builds (their
// TUs are not editor-gated), so RequiresEditor is NOT baked into the macro and
// each action's Flags pass whole to McpDeriveDecl.

#define MCP_SC_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                          \
class FMcpCall_SystemControl_##ClassSuffix final : public FMcpCall                         \
{                                                                                          \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }         \
	const FMcpCallDecl& GetDecl() const override                                            \
	{                                                                                       \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("system_control"),                \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                               \
		return D;                                                                           \
	}                                                                                       \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                    \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override    \
	{                                                                                       \
		return HandlerFn(S, RequestId, Payload, Socket);                                     \
	}                                                                                       \
};

// Member-form variant: the handler stays a UMcpAutomationBridgeSubsystem member
// (delegates to a private member: HandleInsightsAction / HandleConsoleCommandAction /
// HandleLogSetSubscribed), so Run() dispatches through the instance.
#define MCP_SC_MEMBER_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                          \
class FMcpCall_SystemControl_##ClassSuffix final : public FMcpCall                         \
{                                                                                          \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }         \
	const FMcpCallDecl& GetDecl() const override                                            \
	{                                                                                       \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("system_control"),                \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                               \
		return D;                                                                           \
	}                                                                                       \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                    \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override    \
	{                                                                                       \
		return S.HandlerFn(RequestId, Payload, Socket);                                     \
	}                                                                                       \
};

// Build / test pipeline (McpAutomationBridge_SystemControlHandlers.cpp)
MCP_SC_CALL(GenerateTestStub, "generate_test_stub", McpHandlers::SystemControl::HandleSysGenerateTestStub, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(LiveCodingCompile, "live_coding_compile", McpHandlers::SystemControl::HandleSysLiveCodingCompile, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(RunUbt, "run_ubt", McpHandlers::SystemControl::HandleSysRunUbt, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(GetBuildStatus, "get_build_status", McpHandlers::SystemControl::HandleSysGetBuildStatus, EMcpCallFlags::RequiresEditor)
MCP_SC_CALL(ListTests, "list_tests", McpHandlers::SystemControl::HandleSysListTests, EMcpCallFlags::RequiresEditor)
MCP_SC_CALL(RunTests, "run_tests", McpHandlers::SystemControl::HandleSysRunTests, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(GetTestResults, "get_test_results", McpHandlers::SystemControl::HandleSysGetTestResults, EMcpCallFlags::RequiresEditor)
MCP_SC_CALL(ExecutePython, "execute_python", McpHandlers::SystemControl::HandleSysExecutePython, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_MEMBER_CALL(StartSession, "start_session", HandleSysStartSession, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Console delegations (McpAutomationBridge_SystemControlHandlers.cpp →
// HandleConsoleCommandAction; handler-enforced availability, all builds)
MCP_SC_MEMBER_CALL(ExecuteCommand, "execute_command", HandleSysExecuteCommand, EMcpCallFlags::Mutating)
MCP_SC_MEMBER_CALL(SetCvar, "set_cvar", HandleSysSetCvar, EMcpCallFlags::Mutating)

// Project settings (McpAutomationBridge_UiHandlers.cpp). get/set_project_settings
// walk GEngine/GConfig with fallbacks — handler-enforced, not RequiresEditor.
MCP_SC_CALL(GetProjectSettings, "get_project_settings", McpHandlers::SystemControl::HandleUiGetProjectSettings, EMcpCallFlags::None)
MCP_SC_CALL(SetProjectSetting, "set_project_setting", McpHandlers::SystemControl::HandleUiSetProjectSetting, EMcpCallFlags::Mutating)

// Log ring (McpAutomationBridge_LogHandlers.cpp). Deliberately NOT
// RequiresEditor: the ring and its members work in non-editor builds.
MCP_SC_CALL(GetLog, "get_log", McpHandlers::SystemControl::HandleLogQuery, EMcpCallFlags::None)
MCP_SC_CALL(TailLog, "tail_log", McpHandlers::SystemControl::HandleLogQuery, EMcpCallFlags::None)
MCP_SC_CALL(ClearLog, "clear_log", McpHandlers::SystemControl::HandleLogClear, EMcpCallFlags::Mutating)
MCP_SC_MEMBER_CALL(Subscribe, "subscribe", HandleLogSubscribe, EMcpCallFlags::Mutating)
MCP_SC_MEMBER_CALL(Unsubscribe, "unsubscribe", HandleLogUnsubscribe, EMcpCallFlags::Mutating)

// Gameplay debugger (McpAutomationBridge_DebugHandlers.cpp; not editor-gated)
MCP_SC_CALL(SpawnCategory, "spawn_category", McpHandlers::SystemControl::HandleDebugSpawnCategory, EMcpCallFlags::Mutating)

// Lumen (McpAutomationBridge_RenderHandlers.cpp)
MCP_SC_CALL(LumenUpdateScene, "lumen_update_scene", McpHandlers::SystemControl::HandleRenderLumenUpdateScene, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Performance (McpAutomationBridge_PerformanceHandlers.cpp)
MCP_SC_CALL(GenerateMemoryReport, "generate_memory_report", McpHandlers::SystemControl::HandlePerfGenerateMemoryReport, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(GetPerfStats, "get_perf_stats", McpHandlers::SystemControl::HandlePerfGetStats, EMcpCallFlags::RequiresEditor)
MCP_SC_CALL(StartProfiling, "start_profiling", McpHandlers::SystemControl::HandlePerfStartProfiling, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(StopProfiling, "stop_profiling", McpHandlers::SystemControl::HandlePerfStopProfiling, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ShowFps, "show_fps", McpHandlers::SystemControl::HandlePerfShowFps, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(SetScalability, "set_scalability", McpHandlers::SystemControl::HandlePerfSetScalability, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(SetResolutionScale, "set_resolution_scale", McpHandlers::SystemControl::HandlePerfSetResolutionScale, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(SetVsync, "set_vsync", McpHandlers::SystemControl::HandlePerfSetVsync, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(SetFrameRateLimit, "set_frame_rate_limit", McpHandlers::SystemControl::HandlePerfSetFrameRateLimit, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(EnableGpuTiming, "enable_gpu_timing", McpHandlers::SystemControl::HandlePerfEnableGpuTiming, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ConfigureNanite, "configure_nanite", McpHandlers::SystemControl::HandlePerfConfigureNanite, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ConfigureLod, "configure_lod", McpHandlers::SystemControl::HandlePerfConfigureLod, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ConfigureTextureStreaming, "configure_texture_streaming", McpHandlers::SystemControl::HandlePerfConfigureTextureStreaming, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ApplyBaselineSettings, "apply_baseline_settings", McpHandlers::SystemControl::HandlePerfApplyBaselineSettings, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(OptimizeDrawCalls, "optimize_draw_calls", McpHandlers::SystemControl::HandlePerfOptimizeDrawCalls, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ConfigureOcclusionCulling, "configure_occlusion_culling", McpHandlers::SystemControl::HandlePerfConfigureOcclusionCulling, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(OptimizeShaders, "optimize_shaders", McpHandlers::SystemControl::HandlePerfOptimizeShaders, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ConfigureWorldPartition, "configure_world_partition", McpHandlers::SystemControl::HandlePerfConfigureWorldPartition, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(MergeActors, "merge_actors", McpHandlers::SystemControl::HandlePerfMergeActors, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(RunBenchmark, "run_benchmark", McpHandlers::SystemControl::HandlePerfRunBenchmark, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

#undef MCP_SC_CALL
#undef MCP_SC_MEMBER_CALL

} // namespace McpCalls::SystemControl

void McpRegisterSystemControlCalls()
{
	using namespace McpCalls::SystemControl;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_GenerateTestStub>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_LiveCodingCompile>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_RunUbt>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_GetBuildStatus>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ListTests>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_RunTests>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_GetTestResults>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ExecutePython>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_StartSession>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ExecuteCommand>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_SetCvar>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_GetProjectSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_SetProjectSetting>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_GetLog>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_TailLog>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ClearLog>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_Subscribe>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_Unsubscribe>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_SpawnCategory>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_LumenUpdateScene>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_GenerateMemoryReport>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_GetPerfStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_StartProfiling>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_StopProfiling>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ShowFps>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_SetScalability>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_SetResolutionScale>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_SetVsync>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_SetFrameRateLimit>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_EnableGpuTiming>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ConfigureNanite>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ConfigureLod>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ConfigureTextureStreaming>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ApplyBaselineSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_OptimizeDrawCalls>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ConfigureOcclusionCulling>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_OptimizeShaders>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ConfigureWorldPartition>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_MergeActors>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_RunBenchmark>());
}
