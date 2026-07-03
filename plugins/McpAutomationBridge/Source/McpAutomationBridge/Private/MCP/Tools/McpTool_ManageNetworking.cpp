// McpTool_ManageNetworking.cpp — manage_networking tool definition (27 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageNetworking : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_networking"); }

	FString GetDescription() const override
	{
		return TEXT("Multiplayer and input setup: property replication (conditions, "
			"RepNotify, push model, dormancy, relevancy), RPCs (Server/Client/Multicast), "
			"authority/ownership, network prediction, net driver settings; Enhanced Input "
			"authoring (InputActions, mapping contexts, triggers, modifiers); game framework "
			"classes (GameMode/GameState/PlayerController/PlayerState/GameInstance/HUD, match "
			"and scoring rules); split-screen/local players, LAN host/join, and voice chat.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
				.StringEnum(TEXT("action"), McpConsolidatedActions::ManageNetworking(),
					TEXT("Networking action to perform"))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("propertyName"), TEXT("Name of the property."))
			.Bool(TEXT("replicated"), TEXT("Whether property should be replicated."))
			.StringEnum(TEXT("condition"), {
				TEXT("COND_None"),
				TEXT("COND_InitialOnly"),
				TEXT("COND_OwnerOnly"),
				TEXT("COND_SkipOwner"),
				TEXT("COND_SimulatedOnly"),
				TEXT("COND_AutonomousOnly"),
				TEXT("COND_SimulatedOrPhysics"),
				TEXT("COND_InitialOrOwner"),
				TEXT("COND_Custom"),
				TEXT("COND_ReplayOrOwner"),
				TEXT("COND_ReplayOnly"),
				TEXT("COND_SimulatedOnlyNoReplay"),
				TEXT("COND_SimulatedOrPhysicsNoReplay"),
				TEXT("COND_SkipReplay"),
				TEXT("COND_Never")
			}, TEXT("Replication condition."))
			.String(TEXT("repNotifyFunc"), TEXT("RepNotify function name."))
			.Number(TEXT("netUpdateFrequency"), TEXT("How often actor replicates (Hz, default 100)."))
			.Number(TEXT("minNetUpdateFrequency"), TEXT("Minimum update frequency when idle (Hz, default 2)."))
			.Number(TEXT("netPriority"), TEXT("Network priority for bandwidth (default 1.0)."))
			.StringEnum(TEXT("dormancy"), {
				TEXT("DORM_Never"),
				TEXT("DORM_Awake"),
				TEXT("DORM_DormantAll"),
				TEXT("DORM_DormantPartial"),
				TEXT("DORM_Initial")
			}, TEXT("Net dormancy mode."))
			.String(TEXT("functionName"), TEXT("Name of the function."))
			.StringEnum(TEXT("rpcType"), {
				TEXT("Server"),
				TEXT("Client"),
				TEXT("NetMulticast")
			}, TEXT("Type of RPC."))
			.Bool(TEXT("reliable"), TEXT("Whether the operation is reliable."))
			.Bool(TEXT("withValidation"), TEXT("Enable RPC validation."))
			.String(TEXT("ownerActorName"), TEXT("Name of owner actor (null to clear)."))
			.Bool(TEXT("isAutonomousProxy"), TEXT("Configure as autonomous proxy."))
			.Number(TEXT("netCullDistanceSquared"), TEXT("Network cull distance squared."))
			.Bool(TEXT("useOwnerNetRelevancy"), TEXT("Use owner relevancy."))
			.Bool(TEXT("alwaysRelevant"), TEXT("Always relevant to all clients."))
			.Bool(TEXT("onlyRelevantToOwner"), TEXT("Only relevant to owner."))
			.String(TEXT("structName"), TEXT("Name of struct for custom serialization."))
			.Bool(TEXT("usePushModel"), TEXT("Use push-model replication."))
			.Bool(TEXT("enablePrediction"), TEXT("Enable client-side prediction."))
			.Number(TEXT("correctionThreshold"), TEXT("Server correction threshold."))
			.Number(TEXT("smoothingRate"), TEXT("Smoothing rate for corrections."))
			.StringEnum(TEXT("dataType"), {
				TEXT("Transform"),
				TEXT("Vector"),
				TEXT("Rotator"),
				TEXT("Float")
			}, TEXT("Network prediction data type."))
			.String(TEXT("variableName"), TEXT("add_network_prediction_data: name for the created replicated variable (default PredictionData_<dataType>)."))
			.StringEnum(TEXT("networkSmoothingMode"), {
				TEXT("Disabled"),
				TEXT("Linear"),
				TEXT("Exponential")
			}, TEXT("Movement smoothing mode."))
			.Number(TEXT("networkMaxSmoothUpdateDistance"), TEXT("Max smooth update distance."))
			.Number(TEXT("networkNoSmoothUpdateDistance"), TEXT("No smooth update distance."))
			.Number(TEXT("maxClientRate"), TEXT("Max client rate."))
			.Number(TEXT("maxInternetClientRate"), TEXT("Max internet client rate."))
			.Number(TEXT("netServerMaxTickRate"), TEXT("Server max tick rate."))
			.StringEnum(TEXT("role"), {
				TEXT("ROLE_None"),
				TEXT("ROLE_SimulatedProxy"),
				TEXT("ROLE_AutonomousProxy"),
				TEXT("ROLE_Authority")
			}, TEXT("Net role."))
			.Bool(TEXT("replicateMovement"), TEXT("Replicate movement."))
			.Bool(TEXT("spatiallyLoaded"), TEXT("Spatially loaded for replication graph."))
			.Bool(TEXT("netLoadOnClient"), TEXT("Net load on client for replication graph."))
			.String(TEXT("replicationPolicy"), TEXT("Replication policy for replication graph."))
			.Bool(TEXT("customSerialization"), TEXT("Use custom serialization."))
			.Number(TEXT("predictionThreshold"), TEXT("Prediction threshold for client prediction."))
			// Enhanced Input authoring (this tool re-dispatches the Input action group).
			// Handlers read these from the payload regardless; declared here so the params
			// are discoverable / passable through a schema-validating client.
			.String(TEXT("name"), TEXT("Asset name (create_input_action / create_input_mapping_context)."))
			.String(TEXT("path"), TEXT("Destination folder for a created input asset."))
			.String(TEXT("assetPath"), TEXT("Input asset to read (get_input_info)."))
			.StringEnum(TEXT("valueType"), {
				TEXT("Boolean"), TEXT("Axis1D"), TEXT("Axis2D"), TEXT("Axis3D")
			}, TEXT("InputAction value type (create_input_action; default Boolean)."))
			.String(TEXT("actionPath"), TEXT("InputAction asset path (mappings / triggers)."))
			.String(TEXT("contextPath"), TEXT("InputMappingContext asset path (add_mapping / remove_mapping)."))
			.String(TEXT("key"), TEXT("Key id for a mapping (e.g. SpaceBar, Gamepad_FaceButton_Bottom)."))
			.StringEnum(TEXT("triggerType"), {
				TEXT("Pressed"), TEXT("Released"), TEXT("Down"), TEXT("Tap"), TEXT("Hold"),
				TEXT("HoldAndRelease"), TEXT("Pulse"), TEXT("RepeatedTap")
			}, TEXT("Input trigger class (set_input_trigger)."))
			.String(TEXT("modifierType"), TEXT("Input modifier class (set_input_modifier, e.g. Negate, SwizzleAxis)."))
			.Bool(TEXT("replace"), TEXT("set_input_trigger: clear existing triggers before adding (default false = idempotent dedupe)."))
			.Number(TEXT("priority"), TEXT("enable_input_mapping: mapping context priority (default 0)."))
			// Game framework authoring (this tool re-dispatches the GameFramework action
			// group): GameMode/GameState/PlayerController/PlayerState/GameInstance/HUD
			// creation, match rules, spawn/respawn/spectating.
			.Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
			.String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
			.String(TEXT("parentClass"), TEXT("create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class: parent class path (defaults to the native base class)."))
			.String(TEXT("defaultPawnClass"), TEXT("create_game_mode: default pawn class path. Alias: pawnClass (set_default_pawn_class)."))
			.String(TEXT("playerControllerClass"), TEXT("create_game_mode / set_player_controller_class: PlayerController class path."))
			.String(TEXT("gameStateClass"), TEXT("create_game_mode / set_game_state_class: GameState class path."))
			.String(TEXT("playerStateClass"), TEXT("create_game_mode / set_player_state_class: PlayerState class path."))
			.String(TEXT("hudClass"), TEXT("create_game_mode: HUD class path."))
			.Bool(TEXT("bDelayedStart"), TEXT("configure_game_rules: sets AGameMode::bDelayedStart when the property exists on the class."))
			.Array(TEXT("states"), TEXT("setup_match_states: match state names to record on the Blueprint."))
			.Number(TEXT("numRounds"), TEXT("configure_round_system: total rounds in the match."))
			.Number(TEXT("roundTime"), TEXT("configure_round_system: seconds per round."))
			.Number(TEXT("intermissionTime"), TEXT("configure_round_system: seconds between rounds."))
			.Number(TEXT("numTeams"), TEXT("configure_team_system: number of teams (default 2)."))
			.Number(TEXT("teamSize"), TEXT("configure_team_system: maximum players per team."))
			.Bool(TEXT("autoBalance"), TEXT("configure_team_system: auto-balance team sizes."))
			.Bool(TEXT("friendlyFire"), TEXT("configure_team_system: allow damage to teammates."))
			.Number(TEXT("scorePerKill"), TEXT("configure_scoring_system: points awarded per kill."))
			.Number(TEXT("scorePerObjective"), TEXT("configure_scoring_system: points awarded per objective completion."))
			.Number(TEXT("scorePerAssist"), TEXT("configure_scoring_system: points awarded per assist."))
			.Number(TEXT("winScore"), TEXT("configure_scoring_system: score required to win the match."))
			.Number(TEXT("scorePerDeath"), TEXT("configure_scoring_system: score penalty for dying."))
			.String(TEXT("spawnSelectionMethod"), TEXT("configure_spawn_system: how spawn points are chosen (default Random)."))
			.Number(TEXT("respawnDelay"), TEXT("configure_spawn_system / set_respawn_rules: seconds before a player respawns (default 5)."))
			.Bool(TEXT("usePlayerStarts"), TEXT("configure_spawn_system: spawn at PlayerStart actors."))
			.Bool(TEXT("canRespawn"), TEXT("configure_spawn_system: whether respawning is allowed."))
			.Number(TEXT("maxRespawns"), TEXT("configure_spawn_system: maximum respawns per player (-1 = unlimited)."))
			.String(TEXT("playerStartName"), TEXT("configure_player_start: specific PlayerStart actor to configure (falls back to actorName)."))
			.String(TEXT("playerStartTag"), TEXT("configure_player_start: PlayerStartTag to assign (default derived from teamIndex)."))
			.Number(TEXT("teamIndex"), TEXT("configure_player_start: team index used to build the default PlayerStartTag."))
			.String(TEXT("respawnLocation"), TEXT("set_respawn_rules: named respawn location strategy (default PlayerStart)."))
			.Bool(TEXT("forceRespawn"), TEXT("set_respawn_rules: whether respawn is forced rather than optional."))
			.Number(TEXT("respawnLives"), TEXT("set_respawn_rules: lives per player (-1 = unlimited)."))
			.String(TEXT("spectatorClass"), TEXT("configure_spectating: SpectatorPawn class path."))
			// Sessions authoring (this tool re-dispatches the Sessions action group):
			// local session settings, split-screen, LAN host/join, voice chat.
			.String(TEXT("sessionName"), TEXT("configure_local_session_settings: session name (default DefaultSession)."))
			.Number(TEXT("maxPlayers"), TEXT("configure_local_session_settings / host_lan_server: max player count (default 4)."))
			.Bool(TEXT("bIsLANMatch"), TEXT("configure_local_session_settings: mark the session as a LAN match."))
			.Bool(TEXT("bAllowJoinInProgress"), TEXT("configure_local_session_settings: allow players to join after start."))
			.Bool(TEXT("bAllowInvites"), TEXT("configure_local_session_settings: allow session invites."))
			.Bool(TEXT("bUsesPresence"), TEXT("configure_local_session_settings: advertise via presence."))
			.Bool(TEXT("bUseLobbiesIfAvailable"), TEXT("configure_local_session_settings: prefer lobbies when supported."))
			.Bool(TEXT("bShouldAdvertise"), TEXT("configure_local_session_settings: advertise the session publicly."))
			.StringEnum(TEXT("interfaceType"), {
				TEXT("Default"), TEXT("LAN"), TEXT("Null")
			}, TEXT("configure_session_interface: online subsystem interface to use."))
			.Bool(TEXT("enabled"), TEXT("configure_split_screen / configure_lan_play: enable flag for the feature being configured."))
			.StringEnum(TEXT("splitScreenType"), {
				TEXT("None"), TEXT("TwoPlayer_Horizontal"), TEXT("TwoPlayer_Vertical"),
				TEXT("ThreePlayer_FavorTop"), TEXT("ThreePlayer_FavorBottom"), TEXT("FourPlayer_Grid")
			}, TEXT("configure_split_screen / set_split_screen_type: split-screen layout."))
			.Number(TEXT("controllerId"), TEXT("add_local_player: controller id for the new local player."))
			.Number(TEXT("playerIndex"), TEXT("remove_local_player: local player index to remove (cannot be 0)."))
			.Number(TEXT("serverPort"), TEXT("configure_lan_play / join_lan_server: port number (default 7777)."))
			.String(TEXT("serverPassword"), TEXT("configure_lan_play / join_lan_server: LAN server password."))
			.String(TEXT("serverName"), TEXT("host_lan_server: display name for the hosted server."))
			.String(TEXT("mapName"), TEXT("host_lan_server: map to travel to when hosting."))
			.String(TEXT("travelOptions"), TEXT("host_lan_server / join_lan_server: extra URL options appended to the travel string."))
			.Bool(TEXT("executeTravel"), TEXT("host_lan_server: actually perform ServerTravel instead of just building the URL (default false)."))
			.String(TEXT("serverAddress"), TEXT("join_lan_server: address of the LAN server to connect to."))
			.Bool(TEXT("voiceEnabled"), TEXT("enable_voice_chat: enable or disable voice chat."))
			.Object(TEXT("voiceSettings"), TEXT("configure_voice_settings: nested settings object ({volume, noiseGateThreshold, noiseSuppression, echoCancellation, sampleRate})."))
			.String(TEXT("channelName"), TEXT("set_voice_channel: voice channel name."))
			.StringEnum(TEXT("channelType"), {
				TEXT("Team"), TEXT("Global"), TEXT("Proximity"), TEXT("Party")
			}, TEXT("set_voice_channel: channel type (default Global)."))
			.String(TEXT("playerName"), TEXT("mute_player: player name to mute/unmute (or use targetPlayerId)."))
			.String(TEXT("targetPlayerId"), TEXT("mute_player: player id to mute/unmute (or use playerName)."))
			.Bool(TEXT("muted"), TEXT("mute_player: mute (true) or unmute (false), default true."))
			.Number(TEXT("localPlayerNum"), TEXT("mute_player: local player index applying the mute."))
			.Bool(TEXT("systemWide"), TEXT("mute_player: apply the mute system-wide rather than just locally."))
			.Number(TEXT("attenuationRadius"), TEXT("set_voice_attenuation: distance at which voice starts attenuating (default 2000)."))
			.Number(TEXT("attenuationFalloff"), TEXT("set_voice_attenuation: falloff curve exponent, 0.1-10.0 (default 1.0)."))
			.Bool(TEXT("pushToTalkEnabled"), TEXT("configure_push_to_talk: enable push-to-talk."))
			.String(TEXT("pushToTalkKey"), TEXT("configure_push_to_talk: key bound to push-to-talk (default V)."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageNetworking);
