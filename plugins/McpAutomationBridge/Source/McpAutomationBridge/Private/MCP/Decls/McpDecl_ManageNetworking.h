// Action declarations for manage_networking — the server's contract: which params
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
inline const FMcpParamDecl P_ManageNetworking_0[] = { { TEXT("controllerId"), EMcpParamKind::Number, true } };
inline const FMcpParamDecl P_ManageNetworking_1[] = { { TEXT("contextPath"), EMcpParamKind::String, true }, { TEXT("actionPath"), EMcpParamKind::String, true }, { TEXT("key"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_2[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("dataType"), EMcpParamKind::String, true }, { TEXT("variableName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_3[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_4[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_5[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("enablePrediction"), EMcpParamKind::Bool, false }, { TEXT("predictionThreshold"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_6[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("bDelayedStart"), EMcpParamKind::Bool, false }, { TEXT("startPlayersNeeded"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ManageNetworking_7[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("serverPort"), EMcpParamKind::Number, false }, { TEXT("serverPassword"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_8[] = { { TEXT("sessionName"), EMcpParamKind::String, false }, { TEXT("maxPlayers"), EMcpParamKind::Number, false }, { TEXT("bIsLANMatch"), EMcpParamKind::Bool, false }, { TEXT("bAllowJoinInProgress"), EMcpParamKind::Bool, false }, { TEXT("bAllowInvites"), EMcpParamKind::Bool, false }, { TEXT("bUsesPresence"), EMcpParamKind::Bool, false }, { TEXT("bUseLobbiesIfAvailable"), EMcpParamKind::Bool, false }, { TEXT("bShouldAdvertise"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_9[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("networkSmoothingMode"), EMcpParamKind::String, false }, { TEXT("networkMaxSmoothUpdateDistance"), EMcpParamKind::Number, false }, { TEXT("networkNoSmoothUpdateDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_10[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("netCullDistanceSquared"), EMcpParamKind::Number, false }, { TEXT("useOwnerNetRelevancy"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_11[] = { { TEXT("maxClientRate"), EMcpParamKind::Number, false }, { TEXT("maxInternetClientRate"), EMcpParamKind::Number, false }, { TEXT("netServerMaxTickRate"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_12[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("netPriority"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_13[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("structName"), EMcpParamKind::String, false }, { TEXT("customSerialization"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_14[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("netUpdateFrequency"), EMcpParamKind::Number, false }, { TEXT("minNetUpdateFrequency"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_15[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("teamIndex"), EMcpParamKind::Number, false }, { TEXT("bPlayerOnly"), EMcpParamKind::Bool, false }, { TEXT("playerStartName"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("playerStartTag"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_16[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("usePushModel"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_17[] = { { TEXT("pushToTalkEnabled"), EMcpParamKind::Bool, true }, { TEXT("pushToTalkKey"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_18[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("replicateMovement"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_19[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("spatiallyLoaded"), EMcpParamKind::Bool, false }, { TEXT("netLoadOnClient"), EMcpParamKind::Bool, false }, { TEXT("replicationPolicy"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_20[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("numRounds"), EMcpParamKind::Number, false }, { TEXT("roundTime"), EMcpParamKind::Number, false }, { TEXT("intermissionTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_21[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("withValidation"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_22[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("scorePerKill"), EMcpParamKind::Number, false }, { TEXT("scorePerObjective"), EMcpParamKind::Number, false }, { TEXT("scorePerAssist"), EMcpParamKind::Number, false }, { TEXT("winScore"), EMcpParamKind::Number, false }, { TEXT("scorePerDeath"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_23[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("correctionThreshold"), EMcpParamKind::Number, false }, { TEXT("smoothingRate"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_24[] = { { TEXT("interfaceType"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_25[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("spawnSelectionMethod"), EMcpParamKind::String, false }, { TEXT("respawnDelay"), EMcpParamKind::Number, false }, { TEXT("usePlayerStarts"), EMcpParamKind::Bool, false }, { TEXT("canRespawn"), EMcpParamKind::Bool, false }, { TEXT("maxRespawns"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_26[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("spectatorClass"), EMcpParamKind::String, false }, { TEXT("allowSpectating"), EMcpParamKind::Bool, false }, { TEXT("spectatorViewMode"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_27[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("splitScreenType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_28[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("numTeams"), EMcpParamKind::Number, false }, { TEXT("teamSize"), EMcpParamKind::Number, false }, { TEXT("autoBalance"), EMcpParamKind::Bool, false }, { TEXT("friendlyFire"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_29[] = { { TEXT("voiceSettings"), EMcpParamKind::Object, true } };
inline const FMcpParamDecl P_ManageNetworking_30[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_31[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false }, { TEXT("defaultPawnClass"), EMcpParamKind::String, false }, { TEXT("playerControllerClass"), EMcpParamKind::String, false }, { TEXT("gameStateClass"), EMcpParamKind::String, false }, { TEXT("playerStateClass"), EMcpParamKind::String, false }, { TEXT("hudClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_32[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_33[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_34[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("valueType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_35[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_36[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_37[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_38[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("rpcType"), EMcpParamKind::String, true }, { TEXT("reliable"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_39[] = { { TEXT("actionPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_40[] = { { TEXT("contextPath"), EMcpParamKind::String, true }, { TEXT("priority"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_41[] = { { TEXT("voiceEnabled"), EMcpParamKind::Bool, true } };
inline const FMcpParamDecl P_ManageNetworking_42[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_43[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_44[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_46[] = { { TEXT("serverName"), EMcpParamKind::String, false }, { TEXT("mapName"), EMcpParamKind::String, true }, { TEXT("maxPlayers"), EMcpParamKind::Number, false }, { TEXT("travelOptions"), EMcpParamKind::String, false }, { TEXT("executeTravel"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_47[] = { { TEXT("serverAddress"), EMcpParamKind::String, true }, { TEXT("serverPort"), EMcpParamKind::Number, false }, { TEXT("serverPassword"), EMcpParamKind::String, false }, { TEXT("travelOptions"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_49[] = { { TEXT("playerName"), EMcpParamKind::String, false }, { TEXT("targetPlayerId"), EMcpParamKind::String, false }, { TEXT("muted"), EMcpParamKind::Bool, false }, { TEXT("localPlayerNum"), EMcpParamKind::Number, false }, { TEXT("systemWide"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_50[] = { { TEXT("playerIndex"), EMcpParamKind::Number, true } };
inline const FMcpParamDecl P_ManageNetworking_51[] = { { TEXT("contextPath"), EMcpParamKind::String, true }, { TEXT("actionPath"), EMcpParamKind::String, true }, { TEXT("key"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_52[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("alwaysRelevant"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_53[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("isAutonomousProxy"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_54[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("pawnClass"), EMcpParamKind::String, true }, { TEXT("defaultPawnClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_55[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("gameStateClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_56[] = { { TEXT("contextPath"), EMcpParamKind::String, false }, { TEXT("actionPath"), EMcpParamKind::String, true }, { TEXT("key"), EMcpParamKind::String, false }, { TEXT("modifierType"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_57[] = { { TEXT("actionPath"), EMcpParamKind::String, true }, { TEXT("triggerType"), EMcpParamKind::String, true }, { TEXT("replace"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_58[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("dormancy"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_59[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("role"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_60[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("onlyRelevantToOwner"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_61[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("ownerActorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_62[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("playerControllerClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_63[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("playerStateClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_64[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, true }, { TEXT("replicated"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_65[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, true }, { TEXT("repNotifyFunc"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_66[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, true }, { TEXT("condition"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_67[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("respawnDelay"), EMcpParamKind::Number, false }, { TEXT("respawnLocation"), EMcpParamKind::String, false }, { TEXT("forceRespawn"), EMcpParamKind::Bool, false }, { TEXT("respawnLives"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_68[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("reliable"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageNetworking_69[] = { { TEXT("splitScreenType"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageNetworking_70[] = { { TEXT("attenuationRadius"), EMcpParamKind::Number, true }, { TEXT("attenuationFalloff"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageNetworking_71[] = { { TEXT("channelName"), EMcpParamKind::String, true }, { TEXT("channelType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageNetworking_72[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("gameModeBlueprint"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("states"), EMcpParamKind::Array, false } };

inline const FMcpCallDecl GManageNetworking[] =
{
	{ TEXT("manage_networking"), TEXT("add_local_player"), P_ManageNetworking_0, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("add_mapping"), P_ManageNetworking_1, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("add_network_prediction_data"), P_ManageNetworking_2, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("check_has_authority"), P_ManageNetworking_3, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("check_is_locally_controlled"), P_ManageNetworking_4, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_client_prediction"), P_ManageNetworking_5, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_game_rules"), P_ManageNetworking_6, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_lan_play"), P_ManageNetworking_7, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_local_session_settings"), P_ManageNetworking_8, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_movement_prediction"), P_ManageNetworking_9, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_net_cull_distance"), P_ManageNetworking_10, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_net_driver"), P_ManageNetworking_11, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_net_priority"), P_ManageNetworking_12, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_net_serialization"), P_ManageNetworking_13, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_net_update_frequency"), P_ManageNetworking_14, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_player_start"), P_ManageNetworking_15, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_push_model"), P_ManageNetworking_16, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_push_to_talk"), P_ManageNetworking_17, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_replicated_movement"), P_ManageNetworking_18, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_replication_graph"), P_ManageNetworking_19, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_round_system"), P_ManageNetworking_20, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_rpc_validation"), P_ManageNetworking_21, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_scoring_system"), P_ManageNetworking_22, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_server_correction"), P_ManageNetworking_23, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_session_interface"), P_ManageNetworking_24, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_spawn_system"), P_ManageNetworking_25, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_spectating"), P_ManageNetworking_26, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_split_screen"), P_ManageNetworking_27, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_team_system"), P_ManageNetworking_28, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("configure_voice_settings"), P_ManageNetworking_29, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("create_game_instance"), P_ManageNetworking_30, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("create_game_mode"), P_ManageNetworking_31, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("create_game_state"), P_ManageNetworking_32, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("create_hud_class"), P_ManageNetworking_33, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("create_input_action"), P_ManageNetworking_34, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("create_input_mapping_context"), P_ManageNetworking_35, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("create_player_controller"), P_ManageNetworking_36, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("create_player_state"), P_ManageNetworking_37, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("create_rpc_function"), P_ManageNetworking_38, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("disable_input_action"), P_ManageNetworking_39, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("enable_input_mapping"), P_ManageNetworking_40, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("enable_voice_chat"), P_ManageNetworking_41, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("get_game_framework_info"), P_ManageNetworking_42, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("get_input_info"), P_ManageNetworking_43, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("get_networking_info"), P_ManageNetworking_44, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("get_sessions_info"), {}, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("host_lan_server"), P_ManageNetworking_46, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("join_lan_server"), P_ManageNetworking_47, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("mute_player"), P_ManageNetworking_49, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("remove_local_player"), P_ManageNetworking_50, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("remove_mapping"), P_ManageNetworking_51, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_always_relevant"), P_ManageNetworking_52, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_autonomous_proxy"), P_ManageNetworking_53, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_default_pawn_class"), P_ManageNetworking_54, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_game_state_class"), P_ManageNetworking_55, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_input_modifier"), P_ManageNetworking_56, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_input_trigger"), P_ManageNetworking_57, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_net_dormancy"), P_ManageNetworking_58, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_net_role"), P_ManageNetworking_59, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_only_relevant_to_owner"), P_ManageNetworking_60, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_owner"), P_ManageNetworking_61, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_player_controller_class"), P_ManageNetworking_62, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_player_state_class"), P_ManageNetworking_63, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_property_replicated"), P_ManageNetworking_64, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_replicated_using"), P_ManageNetworking_65, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_replication_condition"), P_ManageNetworking_66, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_respawn_rules"), P_ManageNetworking_67, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_rpc_reliability"), P_ManageNetworking_68, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_split_screen_type"), P_ManageNetworking_69, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_voice_attenuation"), P_ManageNetworking_70, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("set_voice_channel"), P_ManageNetworking_71, EMcpCallFlags::None },
	{ TEXT("manage_networking"), TEXT("setup_match_states"), P_ManageNetworking_72, EMcpCallFlags::None },
};
}
