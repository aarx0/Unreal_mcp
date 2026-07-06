// LINT-TOOL: manage_networking
// manage_networking as FMcpCall classes — fourteenth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers — HandleNetworking* (NetworkingHandlers.cpp) for the 27
// core actions, HandleGameFramework* (GameFrameworkHandlers.cpp) for the 20
// game-framework actions, HandleSessions* (SessionsHandlers.cpp) for the 16
// session actions — until the module split de-members those bodies. The 9
// Enhanced Input actions split out into manage_input
// (MCP/Calls/McpCalls_ManageInput.cpp).
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageNetworking
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations
// (McpDecl_ManageNetworking.h) and re-verified against the extracted member
// bodies. Thirteen GameFramework rows shipped with decl fixes at classing:
// the retired dispatcher resolved gameModeBlueprint through the
// blueprintPath fallback (first present spelling wins) and each body rejects
// only when BOTH are absent, so the shim rows that declared the pair
// all-required false-rejected every one-spelling payload the handler serves
// — both spellings are optional now with the at-least-one requirement
// handler-enforced. set_default_pawn_class needed the same fix twice: its
// pawnClass/defaultPawnClass pair is resolved and joint-rejected the same
// way. The single class-path spellings that stay required
// (gameStateClass/playerControllerClass/playerStateClass) have their own
// per-spelling rejects. The dispatcher-prologue spellings a body does not
// read (name/path/save on the GameFramework rows) stay declared-optional so
// payloads the family accepts today keep validating.

inline const FMcpParamDecl P_SetPropertyReplicated[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, true }, { TEXT("replicated"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetReplicationCondition[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, true }, { TEXT("condition"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureNetUpdateFrequency[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("netUpdateFrequency"), EMcpParamKind::Number, false }, { TEXT("minNetUpdateFrequency"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureNetPriority[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("netPriority"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetNetDormancy[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("dormancy"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureReplicationGraph[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("spatiallyLoaded"), EMcpParamKind::Bool, false }, { TEXT("netLoadOnClient"), EMcpParamKind::Bool, false }, { TEXT("replicationPolicy"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateRpcFunction[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("rpcType"), EMcpParamKind::String, true }, { TEXT("reliable"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureRpcValidation[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("withValidation"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetRpcReliability[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("reliable"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetOwner[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("ownerActorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetAutonomousProxy[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("isAutonomousProxy"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CheckHasAuthority[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CheckIsLocallyControlled[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureNetCullDistance[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("netCullDistanceSquared"), EMcpParamKind::Number, false }, { TEXT("useOwnerNetRelevancy"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetAlwaysRelevant[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("alwaysRelevant"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetOnlyRelevantToOwner[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("onlyRelevantToOwner"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureNetSerialization[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("structName"), EMcpParamKind::String, false }, { TEXT("customSerialization"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetReplicatedUsing[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, true }, { TEXT("repNotifyFunc"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigurePushModel[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("usePushModel"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureClientPrediction[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("enablePrediction"), EMcpParamKind::Bool, false }, { TEXT("predictionThreshold"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureServerCorrection[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("correctionThreshold"), EMcpParamKind::Number, false }, { TEXT("smoothingRate"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddNetworkPredictionData[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("dataType"), EMcpParamKind::String, true }, { TEXT("variableName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureMovementPrediction[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("networkSmoothingMode"), EMcpParamKind::String, false }, { TEXT("networkMaxSmoothUpdateDistance"), EMcpParamKind::Number, false }, { TEXT("networkNoSmoothUpdateDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureNetDriver[] = { { TEXT("maxClientRate"), EMcpParamKind::Number, false }, { TEXT("maxInternetClientRate"), EMcpParamKind::Number, false }, { TEXT("netServerMaxTickRate"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetNetRole[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("role"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureReplicatedMovement[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("replicateMovement"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetNetworkingInfo[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateGameMode[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false }, { TEXT("defaultPawnClass"), EMcpParamKind::String, false }, { TEXT("playerControllerClass"), EMcpParamKind::String, false }, { TEXT("gameStateClass"), EMcpParamKind::String, false }, { TEXT("playerStateClass"), EMcpParamKind::String, false }, { TEXT("hudClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateGameState[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreatePlayerController[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreatePlayerState[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateGameInstance[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateHudClass[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetDefaultPawnClass[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("pawnClass"), EMcpParamKind::String, false }, { TEXT("defaultPawnClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetPlayerControllerClass[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("playerControllerClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetGameStateClass[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("gameStateClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetPlayerStateClass[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("playerStateClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureGameRules[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("bDelayedStart"), EMcpParamKind::Bool, false }, { TEXT("startPlayersNeeded"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_SetupMatchStates[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("states"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_ConfigureRoundSystem[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("numRounds"), EMcpParamKind::Number, false }, { TEXT("roundTime"), EMcpParamKind::Number, false }, { TEXT("intermissionTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureTeamSystem[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("numTeams"), EMcpParamKind::Number, false }, { TEXT("teamSize"), EMcpParamKind::Number, false }, { TEXT("autoBalance"), EMcpParamKind::Bool, false }, { TEXT("friendlyFire"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureScoringSystem[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("scorePerKill"), EMcpParamKind::Number, false }, { TEXT("scorePerObjective"), EMcpParamKind::Number, false }, { TEXT("scorePerAssist"), EMcpParamKind::Number, false }, { TEXT("winScore"), EMcpParamKind::Number, false }, { TEXT("scorePerDeath"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureSpawnSystem[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("spawnSelectionMethod"), EMcpParamKind::String, false }, { TEXT("respawnDelay"), EMcpParamKind::Number, false }, { TEXT("usePlayerStarts"), EMcpParamKind::Bool, false }, { TEXT("canRespawn"), EMcpParamKind::Bool, false }, { TEXT("maxRespawns"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigurePlayerStart[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("teamIndex"), EMcpParamKind::Number, false }, { TEXT("bPlayerOnly"), EMcpParamKind::Bool, false }, { TEXT("playerStartName"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("playerStartTag"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetRespawnRules[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("respawnDelay"), EMcpParamKind::Number, false }, { TEXT("respawnLocation"), EMcpParamKind::String, false }, { TEXT("forceRespawn"), EMcpParamKind::Bool, false }, { TEXT("respawnLives"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureSpectating[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("spectatorClass"), EMcpParamKind::String, false }, { TEXT("allowSpectating"), EMcpParamKind::Bool, false }, { TEXT("spectatorViewMode"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetGameFrameworkInfo[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureLocalSessionSettings[] = { { TEXT("sessionName"), EMcpParamKind::String, false }, { TEXT("maxPlayers"), EMcpParamKind::Number, false }, { TEXT("bIsLANMatch"), EMcpParamKind::Bool, false }, { TEXT("bAllowJoinInProgress"), EMcpParamKind::Bool, false }, { TEXT("bAllowInvites"), EMcpParamKind::Bool, false }, { TEXT("bUsesPresence"), EMcpParamKind::Bool, false }, { TEXT("bUseLobbiesIfAvailable"), EMcpParamKind::Bool, false }, { TEXT("bShouldAdvertise"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureSessionInterface[] = { { TEXT("interfaceType"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureSplitScreen[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("splitScreenType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetSplitScreenType[] = { { TEXT("splitScreenType"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddLocalPlayer[] = { { TEXT("controllerId"), EMcpParamKind::Number, true } };
inline const FMcpParamDecl P_RemoveLocalPlayer[] = { { TEXT("playerIndex"), EMcpParamKind::Number, true } };
inline const FMcpParamDecl P_ConfigureLanPlay[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("serverPort"), EMcpParamKind::Number, false }, { TEXT("serverPassword"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_HostLanServer[] = { { TEXT("serverName"), EMcpParamKind::String, false }, { TEXT("mapName"), EMcpParamKind::String, true }, { TEXT("maxPlayers"), EMcpParamKind::Number, false }, { TEXT("travelOptions"), EMcpParamKind::String, false }, { TEXT("executeTravel"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_JoinLanServer[] = { { TEXT("serverAddress"), EMcpParamKind::String, true }, { TEXT("serverPort"), EMcpParamKind::Number, false }, { TEXT("serverPassword"), EMcpParamKind::String, false }, { TEXT("travelOptions"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_EnableVoiceChat[] = { { TEXT("voiceEnabled"), EMcpParamKind::Bool, true } };
inline const FMcpParamDecl P_ConfigureVoiceSettings[] = { { TEXT("voiceSettings"), EMcpParamKind::Object, true } };
inline const FMcpParamDecl P_SetVoiceChannel[] = { { TEXT("channelName"), EMcpParamKind::String, true }, { TEXT("channelType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_MutePlayer[] = { { TEXT("playerName"), EMcpParamKind::String, false }, { TEXT("targetPlayerId"), EMcpParamKind::String, false }, { TEXT("muted"), EMcpParamKind::Bool, false }, { TEXT("localPlayerNum"), EMcpParamKind::Number, false }, { TEXT("systemWide"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetVoiceAttenuation[] = { { TEXT("attenuationRadius"), EMcpParamKind::Number, true }, { TEXT("attenuationFalloff"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigurePushToTalk[] = { { TEXT("pushToTalkEnabled"), EMcpParamKind::Bool, true }, { TEXT("pushToTalkKey"), EMcpParamKind::String, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are authored per action and deliberately mixed. RequiresEditor on
// the 36 GameFramework/Sessions actions — those two chains were
// whole-editor-gated and the members answer their editor-build stubs in
// non-editor builds; NOT on the 27 core networking actions —
// NetworkingHandlers.cpp carries no editor gate, and flagging them would
// newly reject GEditor-less runs the shim served (the world-dependent
// bodies keep their handler-enforced NO_WORLD errors). Mutating on the
// writers only. Nine actions parse and acknowledge without writing
// anything and stay unflagged alongside the readers: the nine Sessions
// echoes (configure_local_session_settings, configure_session_interface,
// set_split_screen_type, configure_lan_play, join_lan_server — builds the
// travel URL but never travels — configure_voice_settings,
// set_voice_channel, set_voice_attenuation, configure_push_to_talk);
// deepen-or-retire is TODO'd for Aaron. The readers are check_has_authority,
// check_is_locally_controlled, get_info, get_game_framework_info, and
// get_sessions_info.

#define MCP_NW_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, Flags)           \
class FMcpCall_ManageNetworking_##ClassSuffix final : public FMcpCall                    \
{                                                                                        \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl Decl{ TEXT("manage_networking"), TEXT(ActionLiteral),  \
			ParamsArray, (Flags) };                                                      \
		return Decl;                                                                     \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

// Replication (NetworkingHandlers.cpp)
MCP_NW_CALL(SetPropertyReplicated, "set_property_replicated", P_SetPropertyReplicated, HandleNetworkingSetPropertyReplicated, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetReplicationCondition, "set_replication_condition", P_SetReplicationCondition, HandleNetworkingSetReplicationCondition, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureNetUpdateFrequency, "configure_net_update_frequency", P_ConfigureNetUpdateFrequency, HandleNetworkingConfigureNetUpdateFrequency, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureNetPriority, "configure_net_priority", P_ConfigureNetPriority, HandleNetworkingConfigureNetPriority, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetNetDormancy, "set_net_dormancy", P_SetNetDormancy, HandleNetworkingSetNetDormancy, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureReplicationGraph, "configure_replication_graph", P_ConfigureReplicationGraph, HandleNetworkingConfigureReplicationGraph, EMcpCallFlags::Mutating)

// RPCs
MCP_NW_CALL(CreateRpcFunction, "create_rpc_function", P_CreateRpcFunction, HandleNetworkingCreateRpcFunction, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureRpcValidation, "configure_rpc_validation", P_ConfigureRpcValidation, HandleNetworkingConfigureRpcValidation, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetRpcReliability, "set_rpc_reliability", P_SetRpcReliability, HandleNetworkingSetRpcReliability, EMcpCallFlags::Mutating)

// Authority & ownership
MCP_NW_CALL(SetOwner, "set_owner", P_SetOwner, HandleNetworkingSetOwner, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetAutonomousProxy, "set_autonomous_proxy", P_SetAutonomousProxy, HandleNetworkingSetAutonomousProxy, EMcpCallFlags::Mutating)
MCP_NW_CALL(CheckHasAuthority, "check_has_authority", P_CheckHasAuthority, HandleNetworkingCheckHasAuthority, EMcpCallFlags::None)
MCP_NW_CALL(CheckIsLocallyControlled, "check_is_locally_controlled", P_CheckIsLocallyControlled, HandleNetworkingCheckIsLocallyControlled, EMcpCallFlags::None)

// Network relevancy
MCP_NW_CALL(ConfigureNetCullDistance, "configure_net_cull_distance", P_ConfigureNetCullDistance, HandleNetworkingConfigureNetCullDistance, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetAlwaysRelevant, "set_always_relevant", P_SetAlwaysRelevant, HandleNetworkingSetAlwaysRelevant, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetOnlyRelevantToOwner, "set_only_relevant_to_owner", P_SetOnlyRelevantToOwner, HandleNetworkingSetOnlyRelevantToOwner, EMcpCallFlags::Mutating)

// Net serialization
MCP_NW_CALL(ConfigureNetSerialization, "configure_net_serialization", P_ConfigureNetSerialization, HandleNetworkingConfigureNetSerialization, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetReplicatedUsing, "set_replicated_using", P_SetReplicatedUsing, HandleNetworkingSetReplicatedUsing, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigurePushModel, "configure_push_model", P_ConfigurePushModel, HandleNetworkingConfigurePushModel, EMcpCallFlags::Mutating)

// Network prediction
MCP_NW_CALL(ConfigureClientPrediction, "configure_client_prediction", P_ConfigureClientPrediction, HandleNetworkingConfigureClientPrediction, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureServerCorrection, "configure_server_correction", P_ConfigureServerCorrection, HandleNetworkingConfigureServerCorrection, EMcpCallFlags::Mutating)
MCP_NW_CALL(AddNetworkPredictionData, "add_network_prediction_data", P_AddNetworkPredictionData, HandleNetworkingAddNetworkPredictionData, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureMovementPrediction, "configure_movement_prediction", P_ConfigureMovementPrediction, HandleNetworkingConfigureMovementPrediction, EMcpCallFlags::Mutating)

// Connection & net driver
MCP_NW_CALL(ConfigureNetDriver, "configure_net_driver", P_ConfigureNetDriver, HandleNetworkingConfigureNetDriver, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetNetRole, "set_net_role", P_SetNetRole, HandleNetworkingSetNetRole, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureReplicatedMovement, "configure_replicated_movement", P_ConfigureReplicatedMovement, HandleNetworkingConfigureReplicatedMovement, EMcpCallFlags::Mutating)

// Utility
MCP_NW_CALL(GetInfo, "get_info", P_GetNetworkingInfo, HandleNetworkingGetInfo, EMcpCallFlags::None)

// Game framework (GameFrameworkHandlers.cpp)
MCP_NW_CALL(CreateGameMode, "create_game_mode", P_CreateGameMode, HandleGameFrameworkCreateGameMode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(CreateGameState, "create_game_state", P_CreateGameState, HandleGameFrameworkCreateGameState, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(CreatePlayerController, "create_player_controller", P_CreatePlayerController, HandleGameFrameworkCreatePlayerController, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(CreatePlayerState, "create_player_state", P_CreatePlayerState, HandleGameFrameworkCreatePlayerState, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(CreateGameInstance, "create_game_instance", P_CreateGameInstance, HandleGameFrameworkCreateGameInstance, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(CreateHudClass, "create_hud_class", P_CreateHudClass, HandleGameFrameworkCreateHudClass, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetDefaultPawnClass, "set_default_pawn_class", P_SetDefaultPawnClass, HandleGameFrameworkSetDefaultPawnClass, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetPlayerControllerClass, "set_player_controller_class", P_SetPlayerControllerClass, HandleGameFrameworkSetPlayerControllerClass, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetGameStateClass, "set_game_state_class", P_SetGameStateClass, HandleGameFrameworkSetGameStateClass, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetPlayerStateClass, "set_player_state_class", P_SetPlayerStateClass, HandleGameFrameworkSetPlayerStateClass, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureGameRules, "configure_game_rules", P_ConfigureGameRules, HandleGameFrameworkConfigureGameRules, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetupMatchStates, "setup_match_states", P_SetupMatchStates, HandleGameFrameworkSetupMatchStates, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureRoundSystem, "configure_round_system", P_ConfigureRoundSystem, HandleGameFrameworkConfigureRoundSystem, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureTeamSystem, "configure_team_system", P_ConfigureTeamSystem, HandleGameFrameworkConfigureTeamSystem, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureScoringSystem, "configure_scoring_system", P_ConfigureScoringSystem, HandleGameFrameworkConfigureScoringSystem, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureSpawnSystem, "configure_spawn_system", P_ConfigureSpawnSystem, HandleGameFrameworkConfigureSpawnSystem, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigurePlayerStart, "configure_player_start", P_ConfigurePlayerStart, HandleGameFrameworkConfigurePlayerStart, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetRespawnRules, "set_respawn_rules", P_SetRespawnRules, HandleGameFrameworkSetRespawnRules, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureSpectating, "configure_spectating", P_ConfigureSpectating, HandleGameFrameworkConfigureSpectating, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(GetGameFrameworkInfo, "get_game_framework_info", P_GetGameFrameworkInfo, HandleGameFrameworkGetGameFrameworkInfo, EMcpCallFlags::RequiresEditor)

// Sessions (SessionsHandlers.cpp)
MCP_NW_CALL(ConfigureLocalSessionSettings, "configure_local_session_settings", P_ConfigureLocalSessionSettings, HandleSessionsConfigureLocalSessionSettings, EMcpCallFlags::RequiresEditor)
MCP_NW_CALL(ConfigureSessionInterface, "configure_session_interface", P_ConfigureSessionInterface, HandleSessionsConfigureSessionInterface, EMcpCallFlags::RequiresEditor)
MCP_NW_CALL(ConfigureSplitScreen, "configure_split_screen", P_ConfigureSplitScreen, HandleSessionsConfigureSplitScreen, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetSplitScreenType, "set_split_screen_type", P_SetSplitScreenType, HandleSessionsSetSplitScreenType, EMcpCallFlags::RequiresEditor)
MCP_NW_CALL(AddLocalPlayer, "add_local_player", P_AddLocalPlayer, HandleSessionsAddLocalPlayer, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(RemoveLocalPlayer, "remove_local_player", P_RemoveLocalPlayer, HandleSessionsRemoveLocalPlayer, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureLanPlay, "configure_lan_play", P_ConfigureLanPlay, HandleSessionsConfigureLanPlay, EMcpCallFlags::RequiresEditor)
MCP_NW_CALL(HostLanServer, "host_lan_server", P_HostLanServer, HandleSessionsHostLanServer, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(JoinLanServer, "join_lan_server", P_JoinLanServer, HandleSessionsJoinLanServer, EMcpCallFlags::RequiresEditor)
MCP_NW_CALL(EnableVoiceChat, "enable_voice_chat", P_EnableVoiceChat, HandleSessionsEnableVoiceChat, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureVoiceSettings, "configure_voice_settings", P_ConfigureVoiceSettings, HandleSessionsConfigureVoiceSettings, EMcpCallFlags::RequiresEditor)
MCP_NW_CALL(SetVoiceChannel, "set_voice_channel", P_SetVoiceChannel, HandleSessionsSetVoiceChannel, EMcpCallFlags::RequiresEditor)
MCP_NW_CALL(MutePlayer, "mute_player", P_MutePlayer, HandleSessionsMutePlayer, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetVoiceAttenuation, "set_voice_attenuation", P_SetVoiceAttenuation, HandleSessionsSetVoiceAttenuation, EMcpCallFlags::RequiresEditor)
MCP_NW_CALL(ConfigurePushToTalk, "configure_push_to_talk", P_ConfigurePushToTalk, HandleSessionsConfigurePushToTalk, EMcpCallFlags::RequiresEditor)
MCP_NW_CALL(GetSessionsInfo, "get_sessions_info", {}, HandleSessionsGetSessionsInfo, EMcpCallFlags::RequiresEditor)

#undef MCP_NW_CALL

} // namespace McpCalls::ManageNetworking

void McpRegisterManageNetworkingCalls()
{
	using namespace McpCalls::ManageNetworking;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetPropertyReplicated>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetReplicationCondition>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureNetUpdateFrequency>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureNetPriority>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetNetDormancy>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureReplicationGraph>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_CreateRpcFunction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureRpcValidation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetRpcReliability>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetOwner>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetAutonomousProxy>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_CheckHasAuthority>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_CheckIsLocallyControlled>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureNetCullDistance>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetAlwaysRelevant>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetOnlyRelevantToOwner>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureNetSerialization>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetReplicatedUsing>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigurePushModel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureClientPrediction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureServerCorrection>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_AddNetworkPredictionData>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureMovementPrediction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureNetDriver>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetNetRole>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureReplicatedMovement>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_GetInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_CreateGameMode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_CreateGameState>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_CreatePlayerController>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_CreatePlayerState>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_CreateGameInstance>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_CreateHudClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetDefaultPawnClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetPlayerControllerClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetGameStateClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetPlayerStateClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureGameRules>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetupMatchStates>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureRoundSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureTeamSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureScoringSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureSpawnSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigurePlayerStart>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetRespawnRules>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureSpectating>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_GetGameFrameworkInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureLocalSessionSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureSessionInterface>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureSplitScreen>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetSplitScreenType>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_AddLocalPlayer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_RemoveLocalPlayer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureLanPlay>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_HostLanServer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_JoinLanServer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_EnableVoiceChat>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureVoiceSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetVoiceChannel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_MutePlayer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetVoiceAttenuation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigurePushToTalk>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_GetSessionsInfo>());
}
