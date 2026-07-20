// LINT-TOOL: manage_sequence
// LINT-SCHEMA-DERIVED
// manage_sequence as FMcpCall classes — second classed family
// (docs/action-declarations.md). Adopts schema-from-decls: each class AUTHORS
// its schema fragment in a S_<Suffix>() function; the published facade schema
// folds those fragments and GetDecl() derives the validation decl from the same
// fragment via McpDeriveDecl(), so schema and decl are one source and cannot
// drift. Run() delegates to a de-membered McpHandlers::Sequence free function,
// or to the UMcpAutomationBridgeSubsystem member for handlers still touching the
// private ResolveSequencePath / FindActorByName helpers.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_SequenceHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageSequence
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. `path` is required on
// actions that hard-error without a sequence; on create it names the save
// folder. Several actions accept a prior create's current-sequence fallback in
// path's place — the decl vocabulary cannot express that either-or, so the
// handler enforces it.

static void S_List(FMcpSchemaBuilder&) {}
static void S_ListTrackTypes(FMcpSchemaBuilder&) {}

static void S_Create(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("name")});
}

static void S_Open(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_Duplicate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
	 .Required({TEXT("destinationPath")});
}

static void S_Rename(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .String(TEXT("newName"), TEXT("New name for renaming."))
	 .Required({TEXT("newName")});
}

static void S_Delete(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_GetBindings(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_GetMetadata(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_GetProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_SetProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .String(TEXT("frameRate"), TEXT("Frame/display rate in fps: an integer string (e.g. \"30\"), an exact NTSC rational \"numerator/denominator\" (e.g. \"24000/1001\"), or a plain JSON number (rounded to a whole-number rate)."))
	 .Integer(TEXT("lengthInFrames"), TEXT("set_properties: playback-range length in display-rate frames from playbackStart (ignored when playbackEnd is given)."))
	 .Integer(TEXT("playbackStart"), TEXT("set_properties: playback range start in display-rate frames (same units as add_keyframe)."))
	 .Integer(TEXT("playbackEnd"), TEXT("set_properties: playback range end in display-rate frames (wins over lengthInFrames)."));
}

static void S_AddCamera(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_AddActor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .Required({TEXT("actorName")});
}

static void S_AddActors(FMcpSchemaBuilder& B)
{
	B.Array(TEXT("actorNames"), TEXT("add_actors: level actor names (label/name/UAID) to bind as possessables; remove_actors: binding names to remove (case-insensitive)."))
	 .String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .Required({TEXT("actorNames")});
}

static void S_RemoveActors(FMcpSchemaBuilder& B)
{
	B.Array(TEXT("actorNames"), TEXT("add_actors: level actor names (label/name/UAID) to bind as possessables; remove_actors: binding names to remove (case-insensitive)."))
	 .String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .Required({TEXT("actorNames")});
}

static void S_AddSpawnableFromClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("className"), TEXT("add_spawnable_from_class: class for the new spawnable; a native class name or a /Game Blueprint asset path (its generated class is used)."))
	 .String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .Required({TEXT("className")});
}

static void S_AddKeyframe(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .String(TEXT("bindingId"), TEXT("add_keyframe: sequence binding GUID (alternative to actorName)."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("property"), TEXT("Name of the property."))
	 .Integer(TEXT("frame"), TEXT("add_keyframe: keyframe frame number at the sequence display rate."))
	 // Transform tracks: structValue {location,rotation,scale}. Float property tracks: floatValue.
	 .Number(TEXT("floatValue"), TEXT("Keyframe value for a float property track."))
	 .Object(TEXT("structValue"), TEXT("Keyframe value for a transform track: {location:{x,y,z}, rotation:{roll,pitch,yaw}, scale:{x,y,z}}."))
	 .Required({TEXT("frame")});
}

static void S_AddSection(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .String(TEXT("trackName"), TEXT("add_section/remove_track/set_track_muted/set_track_solo/set_track_locked: substring matched against track names to pick the target track; add_track: display name for the new track (the name the other track actions match on)."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Integer(TEXT("startFrame"), TEXT("add_section: section start in display-rate frames (default 0; same units as add_keyframe)."))
	 .Integer(TEXT("endFrame"), TEXT("add_section: section end in display-rate frames (default 100; same units as add_keyframe)."));
}

static void S_AddTrack(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .String(TEXT("trackType"), TEXT("add_track: track class to add; a UMovieSceneTrack class name or short form expanded to MovieScene<Type>Track (e.g. Audio, Event; see list_track_types)."))
	 .String(TEXT("trackName"), TEXT("add_section/remove_track/set_track_muted/set_track_solo/set_track_locked: substring matched against track names to pick the target track; add_track: display name for the new track (the name the other track actions match on)."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Required({TEXT("trackType")});
}

static void S_RemoveTrack(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .String(TEXT("trackName"), TEXT("add_section/remove_track/set_track_muted/set_track_solo/set_track_locked: substring matched against track names to pick the target track; add_track: display name for the new track (the name the other track actions match on)."));
}

static void S_ListTracks(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_Play(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_Pause(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_Stop(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_SetPlaybackSpeed(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("speed"), TEXT("set_playback_speed: playback speed multiplier applied to the open Sequencer (default 1; must be > 0)."))
	 .String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_SetDisplayRate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .String(TEXT("frameRate"), TEXT("Frame/display rate in fps: an integer string (e.g. \"30\"), an exact NTSC rational \"numerator/denominator\" (e.g. \"24000/1001\"), or a plain JSON number (rounded to a whole-number rate)."))
	 .Required({TEXT("frameRate")});
}

static void S_SetTickResolution(FMcpSchemaBuilder& B)
{
	B.String(TEXT("resolution"), TEXT("set_tick_resolution: tick resolution as 'numerator/denominator' (e.g. 24000/1001) or a whole number per second; unrecognized formats keep the current resolution."))
	 .String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_SetViewRange(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("start"), TEXT("set_view_range/set_work_range: range start in seconds (default 0)."))
	 .Number(TEXT("end"), TEXT("set_view_range/set_work_range: range end in seconds (default 10 for view, 0 for work)."))
	 .String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."));
}

static void S_SetWorkRange(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .Number(TEXT("start"), TEXT("set_view_range/set_work_range: range start in seconds (default 0)."))
	 .Number(TEXT("end"), TEXT("set_view_range/set_work_range: range end in seconds (default 10 for view, 0 for work)."));
}

static void S_SetTrackMuted(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .String(TEXT("trackName"), TEXT("add_section/remove_track/set_track_muted/set_track_solo/set_track_locked: substring matched against track names to pick the target track; add_track: display name for the new track (the name the other track actions match on)."))
	 .Bool(TEXT("muted"), TEXT("set_track_muted: true disables evaluation of the track, false re-enables it (default true)."));
}

static void S_SetTrackSolo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .String(TEXT("trackName"), TEXT("add_section/remove_track/set_track_muted/set_track_solo/set_track_locked: substring matched against track names to pick the target track; add_track: display name for the new track (the name the other track actions match on)."))
	 .Bool(TEXT("solo"), TEXT("set_track_solo: true solos by disabling evaluation on all other tracks, false re-enables all (default true)."));
}

static void S_SetTrackLocked(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Level Sequence asset path."))
	 .String(TEXT("path"), TEXT("Level Sequence asset path (alias of assetPath)."))
	 .String(TEXT("trackName"), TEXT("add_section/remove_track/set_track_muted/set_track_solo/set_track_locked: substring matched against track names to pick the target track; add_track: display name for the new track (the name the other track actions match on)."))
	 .Bool(TEXT("locked"), TEXT("set_track_locked: true locks all the track's sections, false unlocks them (default true)."));
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are explicit per action (unlike control_actor's macro, which bakes
// RequiresEditor in) because list_track_types is pure reflection — no editor,
// no mutation.

#define MCP_MS_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                      \
class FMcpCall_ManageSequence_##ClassSuffix final : public FMcpCall                       \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_sequence"),             \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                             \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return HandlerFn(S, RequestId, Payload, Socket);                                  \
	}                                                                                     \
};

MCP_MS_CALL(Create, "create", McpHandlers::Sequence::HandleSequenceCreate, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(Open, "open", McpHandlers::Sequence::HandleSequenceOpen, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(List, "list", McpHandlers::Sequence::HandleSequenceList, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(Duplicate, "duplicate", McpHandlers::Sequence::HandleSequenceDuplicate, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(Rename, "rename", McpHandlers::Sequence::HandleSequenceRename, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(Delete, "delete", McpHandlers::Sequence::HandleSequenceDelete, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(GetBindings, "get_bindings", McpHandlers::Sequence::HandleSequenceGetBindings, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(GetMetadata, "get_metadata", McpHandlers::Sequence::HandleSequenceGetMetadata, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(GetProperties, "get_properties", McpHandlers::Sequence::HandleSequenceGetProperties, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(SetProperties, "set_properties", McpHandlers::Sequence::HandleSequenceSetProperties, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddCamera, "add_camera", McpHandlers::Sequence::HandleSequenceAddCamera, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddActor, "add_actor", McpHandlers::Sequence::HandleSequenceAddActor, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddActors, "add_actors", McpHandlers::Sequence::HandleSequenceAddActors, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(RemoveActors, "remove_actors", McpHandlers::Sequence::HandleSequenceRemoveActors, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddSpawnableFromClass, "add_spawnable_from_class", McpHandlers::Sequence::HandleSequenceAddSpawnable, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddKeyframe, "add_keyframe", McpHandlers::Sequence::HandleSequenceAddKeyframe, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddSection, "add_section", McpHandlers::Sequence::HandleSequenceAddSection, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddTrack, "add_track", McpHandlers::Sequence::HandleSequenceAddTrack, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(RemoveTrack, "remove_track", McpHandlers::Sequence::HandleSequenceRemoveTrack, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(ListTracks, "list_tracks", McpHandlers::Sequence::HandleSequenceListTracks, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(ListTrackTypes, "list_track_types", McpHandlers::Sequence::HandleSequenceListTrackTypes, EMcpCallFlags::None)
MCP_MS_CALL(Play, "play", McpHandlers::Sequence::HandleSequencePlay, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(Pause, "pause", McpHandlers::Sequence::HandleSequencePause, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(Stop, "stop", McpHandlers::Sequence::HandleSequenceStop, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(SetPlaybackSpeed, "set_playback_speed", McpHandlers::Sequence::HandleSequenceSetPlaybackSpeed, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(SetDisplayRate, "set_display_rate", McpHandlers::Sequence::HandleSequenceSetDisplayRate, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetTickResolution, "set_tick_resolution", McpHandlers::Sequence::HandleSequenceSetTickResolution, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetViewRange, "set_view_range", McpHandlers::Sequence::HandleSequenceSetViewRange, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetWorkRange, "set_work_range", McpHandlers::Sequence::HandleSequenceSetWorkRange, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetTrackMuted, "set_track_muted", McpHandlers::Sequence::HandleSequenceSetTrackMuted, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetTrackSolo, "set_track_solo", McpHandlers::Sequence::HandleSequenceSetTrackSolo, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetTrackLocked, "set_track_locked", McpHandlers::Sequence::HandleSequenceSetTrackLocked, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

#undef MCP_MS_CALL

} // namespace McpCalls::ManageSequence

void McpRegisterManageSequenceCalls()
{
	using namespace McpCalls::ManageSequence;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_Create>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_Open>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_List>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_Duplicate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_Rename>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_Delete>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_GetBindings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_GetMetadata>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_GetProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_SetProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_AddCamera>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_AddActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_AddActors>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_RemoveActors>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_AddSpawnableFromClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_AddKeyframe>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_AddSection>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_AddTrack>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_RemoveTrack>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_ListTracks>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_ListTrackTypes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_Play>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_Pause>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_Stop>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_SetPlaybackSpeed>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_SetDisplayRate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_SetTickResolution>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_SetViewRange>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_SetWorkRange>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_SetTrackMuted>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_SetTrackSolo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageSequence_SetTrackLocked>());
}
