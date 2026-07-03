// McpTool_ManageSequence.cpp — manage_sequence tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageSequence : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_sequence"); }

	FString GetDescription() const override
	{
		return TEXT("Edit Level Sequences: add tracks, bind actors, set keyframes, "
			"control playback, and record camera.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ManageSequence(), TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.Array(TEXT("actorNames"), TEXT(""))
			.String(TEXT("bindingId"), TEXT("add_keyframe: sequence binding GUID (alternative to actorName)."))
			.Number(TEXT("frame"), TEXT(""))
			.FreeformObject(TEXT("value"), TEXT(""))
			.String(TEXT("property"), TEXT("Name of the property."))
			.String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.Number(TEXT("speed"), TEXT(""))
			.Number(TEXT("startTime"), TEXT(""))
			.String(TEXT("className"), TEXT(""))
			.String(TEXT("trackType"), TEXT(""))
			.String(TEXT("trackName"), TEXT(""))
			.Bool(TEXT("muted"), TEXT(""))
			.Bool(TEXT("solo"), TEXT(""))
			.Bool(TEXT("locked"), TEXT(""))
			.Number(TEXT("startFrame"), TEXT(""))
			.Number(TEXT("endFrame"), TEXT(""))
			.String(TEXT("frameRate"), TEXT(""))
			.String(TEXT("resolution"), TEXT(""))
			.Number(TEXT("start"), TEXT(""))
			.Number(TEXT("end"), TEXT(""))
			.Number(TEXT("lengthInFrames"), TEXT(""))
			.Number(TEXT("playbackStart"), TEXT(""))
			.Number(TEXT("playbackEnd"), TEXT(""))
			.FreeformObject(TEXT("metadata"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageSequence);
