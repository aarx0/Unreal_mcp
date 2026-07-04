// LINT-TOOL: manage_sequence
// manage_sequence as FMcpCall classes — second classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageSequence
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// bRequired marks params the handler hard-errors without. `path` is required
// on actions that hard-error without a sequence; on create it names the save
// folder. bRequired is not transport-enforced yet (migration rule 3), and the
// handlers accept a prior create's current-sequence fallback in path's place —
// the decl vocabulary cannot express that either-or.

inline const FMcpParamDecl P_PathOnly[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Create[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddActor[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddActors[] = { { TEXT("actorNames"), EMcpParamKind::Array, true }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_RemoveActors[] = { { TEXT("actorNames"), EMcpParamKind::Array, true }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddSpawnableFromClass[] = { { TEXT("className"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddKeyframe[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("bindingId"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("property"), EMcpParamKind::String, false }, { TEXT("frame"), EMcpParamKind::Number, true }, { TEXT("value"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_AddSection[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("startFrame"), EMcpParamKind::Number, false }, { TEXT("endFrame"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddTrack[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackType"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_RemoveTrack[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Duplicate[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("destinationPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Rename[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetDisplayRate[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("frameRate"), EMcpParamKind::Any, true } };
inline const FMcpParamDecl P_SetPlaybackSpeed[] = { { TEXT("speed"), EMcpParamKind::Number, false }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetProperties[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("frameRate"), EMcpParamKind::Number, false }, { TEXT("lengthInFrames"), EMcpParamKind::Number, false }, { TEXT("playbackStart"), EMcpParamKind::Number, false }, { TEXT("playbackEnd"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetTickResolution[] = { { TEXT("resolution"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetTrackLocked[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false }, { TEXT("locked"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetTrackMuted[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false }, { TEXT("muted"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetTrackSolo[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("trackName"), EMcpParamKind::String, false }, { TEXT("solo"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetViewRange[] = { { TEXT("start"), EMcpParamKind::Number, false }, { TEXT("end"), EMcpParamKind::Number, false }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetWorkRange[] = { { TEXT("path"), EMcpParamKind::String, true }, { TEXT("start"), EMcpParamKind::Number, false }, { TEXT("end"), EMcpParamKind::Number, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are explicit per action (unlike control_actor's macro, which bakes
// RequiresEditor in) because list_track_types is pure reflection — no editor,
// no mutation.

#define MCP_MS_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, Flags)            \
class FMcpCall_ManageSequence_##ClassSuffix final : public FMcpCall                       \
{                                                                                         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl Decl{ TEXT("manage_sequence"), TEXT(ActionLiteral),     \
			ParamsArray, (Flags) };                                                       \
		return Decl;                                                                      \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return S.HandlerFn(RequestId, Payload, Socket);                                   \
	}                                                                                     \
};

MCP_MS_CALL(Create, "create", P_Create, HandleSequenceCreate, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(Open, "open", P_PathOnly, HandleSequenceOpen, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(List, "list", {}, HandleSequenceList, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(Duplicate, "duplicate", P_Duplicate, HandleSequenceDuplicate, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(Rename, "rename", P_Rename, HandleSequenceRename, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(Delete, "delete", P_PathOnly, HandleSequenceDelete, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(GetBindings, "get_bindings", P_PathOnly, HandleSequenceGetBindings, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(GetMetadata, "get_metadata", P_PathOnly, HandleSequenceGetMetadata, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(GetProperties, "get_properties", P_PathOnly, HandleSequenceGetProperties, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(SetProperties, "set_properties", P_SetProperties, HandleSequenceSetProperties, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddCamera, "add_camera", P_PathOnly, HandleSequenceAddCamera, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddActor, "add_actor", P_AddActor, HandleSequenceAddActor, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddActors, "add_actors", P_AddActors, HandleSequenceAddActors, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(RemoveActors, "remove_actors", P_RemoveActors, HandleSequenceRemoveActors, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddSpawnableFromClass, "add_spawnable_from_class", P_AddSpawnableFromClass, HandleSequenceAddSpawnable, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddKeyframe, "add_keyframe", P_AddKeyframe, HandleSequenceAddKeyframe, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddSection, "add_section", P_AddSection, HandleSequenceAddSection, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(AddTrack, "add_track", P_AddTrack, HandleSequenceAddTrack, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(RemoveTrack, "remove_track", P_RemoveTrack, HandleSequenceRemoveTrack, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(ListTracks, "list_tracks", P_PathOnly, HandleSequenceListTracks, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(ListTrackTypes, "list_track_types", {}, HandleSequenceListTrackTypes, EMcpCallFlags::None)
MCP_MS_CALL(Play, "play", P_PathOnly, HandleSequencePlay, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(Pause, "pause", P_PathOnly, HandleSequencePause, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(Stop, "stop", P_PathOnly, HandleSequenceStop, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(SetPlaybackSpeed, "set_playback_speed", P_SetPlaybackSpeed, HandleSequenceSetPlaybackSpeed, EMcpCallFlags::RequiresEditor)
MCP_MS_CALL(SetDisplayRate, "set_display_rate", P_SetDisplayRate, HandleSequenceSetDisplayRate, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetTickResolution, "set_tick_resolution", P_SetTickResolution, HandleSequenceSetTickResolution, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetViewRange, "set_view_range", P_SetViewRange, HandleSequenceSetViewRange, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetWorkRange, "set_work_range", P_SetWorkRange, HandleSequenceSetWorkRange, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetTrackMuted, "set_track_muted", P_SetTrackMuted, HandleSequenceSetTrackMuted, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetTrackSolo, "set_track_solo", P_SetTrackSolo, HandleSequenceSetTrackSolo, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_MS_CALL(SetTrackLocked, "set_track_locked", P_SetTrackLocked, HandleSequenceSetTrackLocked, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

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
