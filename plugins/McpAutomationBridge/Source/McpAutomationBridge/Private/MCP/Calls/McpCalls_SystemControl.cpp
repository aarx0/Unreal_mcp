// LINT-TOOL: system_control
// system_control as FMcpCall classes — fifth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers until the module split de-members those bodies. Unlike
// earlier families the implementations span six handler TUs (Performance /
// SystemControl / Ui / Log / Debug / Render handlers), and execute_command /
// set_cvar delegate to HandleConsoleCommandAction (which has other owners
// and keeps its internal "console_command" literal).
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::SystemControl
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations (fleet-verified against
// the handler bodies), except: configure_texture_streaming and
// generate_memory_report drop the bogus required `functionName` neither
// handler reads (mega-bag residue). get_log/tail_log share one contract like
// they share one implementation.

inline const FMcpParamDecl P_AddWidgetChild[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("childClass"), EMcpParamKind::String, true }, { TEXT("parentName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ApplyBaselineSettings[] = { { TEXT("profile"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureLod[] = { { TEXT("lodBias"), EMcpParamKind::Number, false }, { TEXT("forceLOD"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureNanite[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureOcclusionCulling[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("slop"), EMcpParamKind::Number, false }, { TEXT("minScreenRadius"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureTextureStreaming[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("poolSize"), EMcpParamKind::Number, false }, { TEXT("boostPlayerLocation"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureWorldPartition[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("cellSize"), EMcpParamKind::Number, false }, { TEXT("loadingRange"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateWidget[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("widgetType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_EnableGpuTiming[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ExecuteCommand[] = { { TEXT("command"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ExecutePython[] = { { TEXT("code"), EMcpParamKind::String, false }, { TEXT("file"), EMcpParamKind::String, false }, { TEXT("allowModalApis"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GenerateMemoryReport[] = { { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("detailed"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GenerateTestStub[] = { { TEXT("testName"), EMcpParamKind::String, true }, { TEXT("testType"), EMcpParamKind::String, false }, { TEXT("className"), EMcpParamKind::String, false }, { TEXT("flagsExpr"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("overwrite"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetBuildStatus[] = { { TEXT("buildId"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetProjectSettings[] = { { TEXT("section"), EMcpParamKind::String, false }, { TEXT("category"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetTestResults[] = { { TEXT("runId"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ListTests[] = { { TEXT("filter"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_LogQuery[] = { { TEXT("count"), EMcpParamKind::Number, false }, { TEXT("sinceSeq"), EMcpParamKind::Number, false }, { TEXT("verbosity"), EMcpParamKind::String, false }, { TEXT("category"), EMcpParamKind::String, false }, { TEXT("contains"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_MergeActors[] = { { TEXT("actors"), EMcpParamKind::Array, true }, { TEXT("packageName"), EMcpParamKind::String, false }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("replaceSourceActors"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_OptimizeDrawCalls[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("instancing"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_OptimizeShaders[] = { { TEXT("mode"), EMcpParamKind::String, false }, { TEXT("forceRecompile"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_RunBenchmark[] = { { TEXT("duration"), EMcpParamKind::Number, false }, { TEXT("type"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_RunTests[] = { { TEXT("suite"), EMcpParamKind::String, false }, { TEXT("spec"), EMcpParamKind::String, false }, { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("test"), EMcpParamKind::String, false }, { TEXT("maxTests"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_RunUbt[] = { { TEXT("target"), EMcpParamKind::String, true }, { TEXT("platform"), EMcpParamKind::String, false }, { TEXT("configuration"), EMcpParamKind::String, false }, { TEXT("additionalArgs"), EMcpParamKind::String, false }, { TEXT("arguments"), EMcpParamKind::String, false }, { TEXT("noUBA"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_Screenshot[] = { { TEXT("path"), EMcpParamKind::String, false }, { TEXT("filename"), EMcpParamKind::String, false }, { TEXT("returnBase64"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetCvar[] = { { TEXT("key"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetFrameRateLimit[] = { { TEXT("maxFPS"), EMcpParamKind::Number, true } };
inline const FMcpParamDecl P_SetProjectSetting[] = { { TEXT("section"), EMcpParamKind::String, true }, { TEXT("key"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetResolutionScale[] = { { TEXT("scale"), EMcpParamKind::Number, true } };
inline const FMcpParamDecl P_SetScalability[] = { { TEXT("level"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetVsync[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ShowFps[] = { { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ShowStats[] = { { TEXT("category"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SpawnCategory[] = { { TEXT("categoryName"), EMcpParamKind::String, false }, { TEXT("category"), EMcpParamKind::String, false }, { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_StartSession[] = { { TEXT("channels"), EMcpParamKind::String, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are explicit and complete per action — this family is mixed: the log
// actions and the console/config delegations run in non-editor builds (their
// TUs are not editor-gated), so RequiresEditor is NOT baked into the macro.

#define MCP_SC_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, Flags)             \
class FMcpCall_SystemControl_##ClassSuffix final : public FMcpCall                         \
{                                                                                          \
	const FMcpCallDecl& GetDecl() const override                                           \
	{                                                                                      \
		static const FMcpCallDecl Decl{ TEXT("system_control"), TEXT(ActionLiteral),       \
			ParamsArray, (Flags) };                                                        \
		return Decl;                                                                       \
	}                                                                                      \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                   \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override   \
	{                                                                                      \
		return S.HandlerFn(RequestId, Payload, Socket);                                    \
	}                                                                                      \
};

// Build / test pipeline (McpAutomationBridge_SystemControlHandlers.cpp)
MCP_SC_CALL(GenerateTestStub, "generate_test_stub", P_GenerateTestStub, HandleSysGenerateTestStub, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(LiveCodingCompile, "live_coding_compile", {}, HandleSysLiveCodingCompile, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(RunUbt, "run_ubt", P_RunUbt, HandleSysRunUbt, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(GetBuildStatus, "get_build_status", P_GetBuildStatus, HandleSysGetBuildStatus, EMcpCallFlags::RequiresEditor)
MCP_SC_CALL(ListTests, "list_tests", P_ListTests, HandleSysListTests, EMcpCallFlags::RequiresEditor)
MCP_SC_CALL(RunTests, "run_tests", P_RunTests, HandleSysRunTests, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(GetTestResults, "get_test_results", P_GetTestResults, HandleSysGetTestResults, EMcpCallFlags::RequiresEditor)
MCP_SC_CALL(ExecutePython, "execute_python", P_ExecutePython, HandleSysExecutePython, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(StartSession, "start_session", P_StartSession, HandleSysStartSession, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Console delegations (McpAutomationBridge_SystemControlHandlers.cpp →
// HandleConsoleCommandAction; handler-enforced availability, all builds)
MCP_SC_CALL(ExecuteCommand, "execute_command", P_ExecuteCommand, HandleSysExecuteCommand, EMcpCallFlags::Mutating)
MCP_SC_CALL(SetCvar, "set_cvar", P_SetCvar, HandleSysSetCvar, EMcpCallFlags::Mutating)

// Widgets / screenshot / project settings (McpAutomationBridge_UiHandlers.cpp).
// screenshot needs a game viewport and get/set_project_settings walk
// GEngine/GConfig with fallbacks — handler-enforced, not RequiresEditor.
MCP_SC_CALL(CreateWidget, "create_widget", P_CreateWidget, HandleUiCreateWidget, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(AddWidgetChild, "add_widget_child", P_AddWidgetChild, HandleUiAddWidgetChild, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(Screenshot, "screenshot", P_Screenshot, HandleUiScreenshot, EMcpCallFlags::Mutating)
MCP_SC_CALL(GetProjectSettings, "get_project_settings", P_GetProjectSettings, HandleUiGetProjectSettings, EMcpCallFlags::None)
MCP_SC_CALL(SetProjectSetting, "set_project_setting", P_SetProjectSetting, HandleUiSetProjectSetting, EMcpCallFlags::Mutating)

// Log ring (McpAutomationBridge_LogHandlers.cpp). Deliberately NOT
// RequiresEditor: the ring and its members work in non-editor builds.
MCP_SC_CALL(GetLog, "get_log", P_LogQuery, HandleLogQuery, EMcpCallFlags::None)
MCP_SC_CALL(TailLog, "tail_log", P_LogQuery, HandleLogQuery, EMcpCallFlags::None)
MCP_SC_CALL(ClearLog, "clear_log", {}, HandleLogClear, EMcpCallFlags::Mutating)
MCP_SC_CALL(Subscribe, "subscribe", {}, HandleLogSubscribe, EMcpCallFlags::Mutating)
MCP_SC_CALL(Unsubscribe, "unsubscribe", {}, HandleLogUnsubscribe, EMcpCallFlags::Mutating)

// Gameplay debugger (McpAutomationBridge_DebugHandlers.cpp; not editor-gated)
MCP_SC_CALL(SpawnCategory, "spawn_category", P_SpawnCategory, HandleDebugSpawnCategory, EMcpCallFlags::Mutating)

// Lumen (McpAutomationBridge_RenderHandlers.cpp)
MCP_SC_CALL(LumenUpdateScene, "lumen_update_scene", {}, HandleRenderLumenUpdateScene, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Performance (McpAutomationBridge_PerformanceHandlers.cpp)
MCP_SC_CALL(GenerateMemoryReport, "generate_memory_report", P_GenerateMemoryReport, HandlePerfGenerateMemoryReport, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(GetPerfStats, "get_perf_stats", {}, HandlePerfGetStats, EMcpCallFlags::RequiresEditor)
MCP_SC_CALL(StartProfiling, "start_profiling", {}, HandlePerfStartProfiling, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(StopProfiling, "stop_profiling", {}, HandlePerfStopProfiling, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ShowFps, "show_fps", P_ShowFps, HandlePerfShowFps, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ShowStats, "show_stats", P_ShowStats, HandlePerfShowStats, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(SetScalability, "set_scalability", P_SetScalability, HandlePerfSetScalability, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(SetResolutionScale, "set_resolution_scale", P_SetResolutionScale, HandlePerfSetResolutionScale, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(SetVsync, "set_vsync", P_SetVsync, HandlePerfSetVsync, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(SetFrameRateLimit, "set_frame_rate_limit", P_SetFrameRateLimit, HandlePerfSetFrameRateLimit, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(EnableGpuTiming, "enable_gpu_timing", P_EnableGpuTiming, HandlePerfEnableGpuTiming, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ConfigureNanite, "configure_nanite", P_ConfigureNanite, HandlePerfConfigureNanite, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ConfigureLod, "configure_lod", P_ConfigureLod, HandlePerfConfigureLod, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ConfigureTextureStreaming, "configure_texture_streaming", P_ConfigureTextureStreaming, HandlePerfConfigureTextureStreaming, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ApplyBaselineSettings, "apply_baseline_settings", P_ApplyBaselineSettings, HandlePerfApplyBaselineSettings, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(OptimizeDrawCalls, "optimize_draw_calls", P_OptimizeDrawCalls, HandlePerfOptimizeDrawCalls, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ConfigureOcclusionCulling, "configure_occlusion_culling", P_ConfigureOcclusionCulling, HandlePerfConfigureOcclusionCulling, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(OptimizeShaders, "optimize_shaders", P_OptimizeShaders, HandlePerfOptimizeShaders, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(ConfigureWorldPartition, "configure_world_partition", P_ConfigureWorldPartition, HandlePerfConfigureWorldPartition, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(MergeActors, "merge_actors", P_MergeActors, HandlePerfMergeActors, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_SC_CALL(RunBenchmark, "run_benchmark", P_RunBenchmark, HandlePerfRunBenchmark, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

#undef MCP_SC_CALL

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
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_CreateWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_AddWidgetChild>());
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_Screenshot>());
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
	Registry.RegisterCall(MakeUnique<FMcpCall_SystemControl_ShowStats>());
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
