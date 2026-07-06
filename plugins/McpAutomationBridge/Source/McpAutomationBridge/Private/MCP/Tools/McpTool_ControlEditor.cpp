// McpTool_ControlEditor.cpp — control_editor tool definition 

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ControlEditor : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("control_editor"); }

	FString GetDescription() const override
	{
		return TEXT("Start/stop PIE, control viewport camera, run console commands, "
			"take screenshots, simulate input.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ControlEditor(), TEXT("Editor action. screenshot captures the active "
				"viewport synchronously; the PNG is written to the returned "
				"absolute 'path' before the call returns, so it can be read back "
				"immediately to evaluate the result."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.String(TEXT("viewMode"), TEXT(""))
			.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
			.Number(TEXT("speed"), TEXT(""))
			.String(TEXT("filename"), TEXT("screenshot: output PNG name (saved under Saved/Screenshots; defaults to a timestamp)."))
				.Integer(TEXT("maxWidth"), TEXT("screenshot: downscale the capture to at most this width in px (0/omit = native viewport size)."))
			.Number(TEXT("fov"), TEXT(""))
			.Number(TEXT("width"), TEXT(""))
			.Number(TEXT("height"), TEXT(""))
			.Integer(TEXT("steps"), TEXT(""))
			.Integer(TEXT("id"), TEXT("Bookmark identifier/index."))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("levelPath"), TEXT("Level asset path."))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("mode"), TEXT(""))
			.Number(TEXT("deltaTime"), TEXT(""))
			.String(TEXT("resolution"), TEXT("Resolution setting (e.g., 1024x1024)."))
			.Bool(TEXT("realtime"), TEXT(""))
			.String(TEXT("category"), TEXT(""))
			.FreeformObject(TEXT("preferences"), TEXT(""))
			.String(TEXT("key"), TEXT(""))
			.String(TEXT("type"), TEXT("Input event type for simulate_input: key_down, key_up, mouse_click, mouse_move, analog. "
				"analog injects a 1D axis value (key=Gamepad_LeftY/Gamepad_RightX/MouseX/... + value); the value persists in "
				"PlayerInput until the next sample, so hold = inject once, release = inject 0."))
			.String(TEXT("inputType"), TEXT("Alias for type used by simulate_input."))
			.Number(TEXT("value"), TEXT("simulate_input analog: axis value (typically -1..1; mouse axes take pixel-ish deltas)."))
			.String(TEXT("route"), TEXT("simulate_input analog: 'pie' (default — straight to the PIE player controller, focus-immune) or "
				"'slate' (through FSlateApplication + input preprocessors, exercising the CommonUI layer)."))
			.String(TEXT("inputAction"), TEXT(""))
			.Number(TEXT("x"), TEXT("Mouse X coordinate for simulate_input."))
			.Number(TEXT("y"), TEXT("Mouse Y coordinate for simulate_input."))
			.String(TEXT("button"), TEXT("Mouse button for simulate_input."))
			.String(TEXT("direction"), TEXT("simulate_nav direction: Up/Down/Left/Right/Accept/Back/Next/Previous (PIE-only; faithfully routes a nav key through CommonUI, then returns the post-nav focus snapshot)."))
			.String(TEXT("device"), TEXT("simulate_nav input device: 'gamepad' (default, sends DPad/face-button keys) or 'keyboard' (arrows/Enter/Esc/Tab)."))
			.Bool(TEXT("stabilizeFocus"), TEXT("simulate_nav: default true — before the nav, focus the PIE game viewport if focus isn't already inside it, so the faithful drive routes through CommonUI deterministically (never steals focus from an already-focused in-game widget). Set false to drive from whatever currently has focus."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ControlEditor);
