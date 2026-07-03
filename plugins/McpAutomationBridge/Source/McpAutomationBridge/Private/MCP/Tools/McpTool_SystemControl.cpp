// McpTool_SystemControl.cpp — system_control tool definition (25 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_SystemControl : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("system_control"); }

	FString GetDescription() const override
	{
		return TEXT("Run profiling, set quality/CVars, execute console commands, "
			"execute Python scripts, run UBT, and manage widgets.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
				.StringEnum(TEXT("action"), McpConsolidatedActions::SystemControl(),
					TEXT("Action"))
			.String(TEXT("category"), TEXT(""))
			.String(TEXT("categoryName"), TEXT("spawn_category: gameplay debugger category name (falls back to 'category' if absent)."))
			.Number(TEXT("level"), TEXT(""))
			.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
			.String(TEXT("resolution"), TEXT("Resolution setting (e.g., 1024x1024)."))
			.String(TEXT("command"), TEXT(""))
			.String(TEXT("target"), TEXT(""))
			.String(TEXT("platform"), TEXT(""))
			.String(TEXT("configuration"), TEXT(""))
			.String(TEXT("arguments"), TEXT(""))
			.String(TEXT("additionalArgs"), TEXT("run_ubt: extra UBT command-line arguments (alias: arguments)."))
			.String(TEXT("filter"), TEXT("Substring filter over automation test display name / full path / test name (run_tests, list_tests)."))
			.String(TEXT("test"), TEXT("Exact/partial test name to run (run_tests; alias for filter)."))
			.Number(TEXT("maxTests"), TEXT("Max tests to queue for run_tests (1-500, default 50)."))
			.String(TEXT("runId"), TEXT("Run identifier returned by run_tests; pass to get_test_results."))
			.String(TEXT("testName"), TEXT("generate_test_stub: the registered test path, e.g. 'Combat.DamageApplies' (also the run_tests filter)."))
			.String(TEXT("testType"), TEXT("generate_test_stub: simple (default) | complex (parameterized) | latent (multi-frame)."))
			.String(TEXT("className"), TEXT("generate_test_stub: C++ class symbol (default derived from testName, e.g. FCombatDamageAppliesTest)."))
			.String(TEXT("outputPath"), TEXT("generate_test_stub: .cpp path to write (default: the bridge plugin's GeneratedTests dir)."))
			.String(TEXT("flagsExpr"), TEXT("generate_test_stub: override the EAutomationTestFlags expression (default EditorContext | EngineFilter)."))
			.Bool(TEXT("overwrite"), TEXT("generate_test_stub: replace the file if it already exists (default false)."))
			.String(TEXT("channels"), TEXT(""))
			.String(TEXT("widgetPath"), TEXT("Widget blueprint path."))
			.String(TEXT("savePath"), TEXT("create_widget: destination folder for the new widget blueprint (default /Game/UI/Widgets)."))
			.String(TEXT("widgetType"), TEXT("create_widget: informational widget-type tag echoed back in the response."))
			.String(TEXT("childClass"), TEXT(""))
			.String(TEXT("parentName"), TEXT(""))
			.String(TEXT("section"), TEXT(""))
			.String(TEXT("key"), TEXT(""))
			.String(TEXT("value"), TEXT(""))
			.Number(TEXT("count"), TEXT("get_log/tail_log: max lines to return (default 100, max 2000)."))
			.Number(TEXT("sinceSeq"), TEXT("get_log/tail_log: return only lines with seq > this (incremental polling; response carries nextSeq + dropped)."))
			.String(TEXT("verbosity"), TEXT("get_log/tail_log: minimum severity to include — Fatal|Error|Warning|Display|Log|Verbose|VeryVerbose (default Verbose = all)."))
			.String(TEXT("contains"), TEXT("get_log/tail_log: case-insensitive substring the message must contain."))
			.String(TEXT("code"), TEXT("Python code to execute inline"))
			.String(TEXT("file"), TEXT("Path to .py file to execute"))
			.Bool(TEXT("allowModalApis"), TEXT("execute_python: opt out of the modal-API guard (reload_packages / *_with_dialog / EditorDialog are blocked by default — a modal dialog permanently freezes a headless editor). Only pass true when the exact call cannot prompt."))
			.String(TEXT("path"), TEXT("screenshot: output directory, relative to the project (must not escape it); default Saved/Screenshots/WindowsEditor."))
			.String(TEXT("filename"), TEXT("screenshot: output PNG filename (sanitized; default a timestamp)."))
			.Bool(TEXT("returnBase64"), TEXT("screenshot: include the base64-encoded PNG in the response (default true)."))
			.Bool(TEXT("detailed"), TEXT("generate_memory_report: emit the full/verbose report (memreport -full) instead of the summary."))
			.Number(TEXT("scale"), TEXT("set_resolution_scale: r.ScreenPercentage value (e.g. 100 = native)."))
			.Number(TEXT("maxFPS"), TEXT("set_frame_rate_limit: maximum frame rate."))
			.Number(TEXT("lodBias"), TEXT("configure_lod: r.MipMapLODBias."))
			.Number(TEXT("forceLOD"), TEXT("configure_lod: r.ForceLOD index (-1 = auto)."))
			.Number(TEXT("poolSize"), TEXT("configure_texture_streaming: r.Streaming.PoolSize in MB."))
			.Bool(TEXT("boostPlayerLocation"), TEXT("configure_texture_streaming: boost streaming around the active camera location."))
			.Array(TEXT("actors"), TEXT("merge_actors: actor names/labels/paths to merge (at least 2)."))
			.String(TEXT("packageName"), TEXT("merge_actors: destination package path for the merged static mesh (alias: outputPath)."))
			.Bool(TEXT("replaceSourceActors"), TEXT("merge_actors: destroy the source actors once the merge succeeds (default false)."))
			.Number(TEXT("duration"), TEXT("run_benchmark: seconds to run before completing (default 60)."))
			.String(TEXT("type"), TEXT("run_benchmark: benchmark type — all (default) | gpu."))
			.String(TEXT("profile"), TEXT("apply_baseline_settings: performance | quality | balanced (default balanced)."))
			.Bool(TEXT("instancing"), TEXT("optimize_draw_calls: enable dynamic mesh draw command instancing."))
			.Number(TEXT("slop"), TEXT("configure_occlusion_culling: r.OcclusionSlop."))
			.Number(TEXT("minScreenRadius"), TEXT("configure_occlusion_culling: r.OcclusionCullMinScreenRadius."))
			.String(TEXT("mode"), TEXT("optimize_shaders: changed (default) | material | global."))
			.Bool(TEXT("forceRecompile"), TEXT("optimize_shaders: recompile all shaders regardless of mode."))
			.Number(TEXT("cellSize"), TEXT("configure_world_partition: wp.Runtime.RuntimeCellSize."))
			.Number(TEXT("loadingRange"), TEXT("configure_world_partition: wp.Runtime.RuntimeStreamingRange."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_SystemControl);
