// LINT-TOOL: manage_networking
// LINT-SCHEMA-DERIVED
// manage_networking as FMcpCall classes — adopts schema-from-decls
// (docs/action-declarations.md). Each class AUTHORS its schema fragment in a
// S_<Suffix>() function; the published facade schema folds those fragments and
// GetDecl() derives the validation decl from the same fragment via
// McpDeriveDecl(), so schema and decl are one source and cannot drift. Run()
// delegates to the subsystem member handlers — HandleNetworking*
// (NetworkingHandlers.cpp) for the 27 core actions, HandleGameFramework*
// (GameFrameworkHandlers.cpp) for the 20 game-framework actions, HandleSessions*
// (SessionsHandlers.cpp) for the 6 session actions — until the module split
// de-members those bodies. The 9 Enhanced Input actions split out into
// manage_input (MCP/Calls/McpCalls_ManageInput.cpp).
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_NetworkingHandlers.h"
#include "McpAutomationBridge_GameFrameworkHandlers.h"
#include "McpAutomationBridge_SessionsHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageNetworking
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. GameFramework actions
// resolve gameModeBlueprint/blueprintPath first-present-wins and reject only
// when both are absent, so both stay optional (the at-least-one requirement is
// handler-enforced); set_default_pawn_class resolves pawnClass/defaultPawnClass
// the same way. The dispatcher-prologue spellings a body does not read
// (name/path/save on the Set*/Configure* GameFramework rows) stay declared so
// payloads the family accepts keep validating. Nine Sessions echoes and the
// readers take one-of-several params optional and enforce it themselves.

// Replication (NetworkingHandlers.cpp)
static void S_SetPropertyReplicated(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .Bool(TEXT("replicated"), TEXT("Whether property should be replicated."))
	 .Required({TEXT("blueprintPath"), TEXT("propertyName")});
}

static void S_SetReplicationCondition(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
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
	 .Required({TEXT("blueprintPath"), TEXT("propertyName"), TEXT("condition")});
}

static void S_ConfigureNetUpdateFrequency(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("netUpdateFrequency"), TEXT("How often actor replicates (Hz, default 100)."))
	 .Number(TEXT("minNetUpdateFrequency"), TEXT("Minimum update frequency when idle (Hz, default 2)."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureNetPriority(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("netPriority"), TEXT("Network priority for bandwidth (default 1.0)."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetNetDormancy(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .StringEnum(TEXT("dormancy"), {
		TEXT("DORM_Never"),
		TEXT("DORM_Awake"),
		TEXT("DORM_DormantAll"),
		TEXT("DORM_DormantPartial"),
		TEXT("DORM_Initial")
	 }, TEXT("Net dormancy mode."))
	 .Required({TEXT("blueprintPath"), TEXT("dormancy")});
}

static void S_ConfigureReplicationGraph(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("spatiallyLoaded"), TEXT("Spatially loaded for replication graph."))
	 .Bool(TEXT("netLoadOnClient"), TEXT("Net load on client for replication graph."))
	 .String(TEXT("replicationPolicy"), TEXT("Replication policy for replication graph."))
	 .Required({TEXT("blueprintPath")});
}

// RPCs
static void S_CreateRpcFunction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("functionName"), TEXT("Name of the function."))
	 .StringEnum(TEXT("rpcType"), {
		TEXT("Server"),
		TEXT("Client"),
		TEXT("NetMulticast")
	 }, TEXT("Type of RPC."))
	 .Bool(TEXT("reliable"), TEXT("Whether the operation is reliable."))
	 .Required({TEXT("blueprintPath"), TEXT("functionName"), TEXT("rpcType")});
}

static void S_ConfigureRpcValidation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("functionName"), TEXT("Name of the function."))
	 .Bool(TEXT("withValidation"), TEXT("Enable RPC validation."))
	 .Required({TEXT("blueprintPath"), TEXT("functionName")});
}

static void S_SetRpcReliability(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("functionName"), TEXT("Name of the function."))
	 .Bool(TEXT("reliable"), TEXT("Whether the operation is reliable."))
	 .Required({TEXT("blueprintPath"), TEXT("functionName")});
}

// Authority & ownership
static void S_SetOwner(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("ownerActorName"), TEXT("Name of owner actor (null to clear)."))
	 .Required({TEXT("actorName")});
}

static void S_SetAutonomousProxy(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("isAutonomousProxy"), TEXT("Configure as autonomous proxy."))
	 .Required({TEXT("blueprintPath")});
}

static void S_CheckHasAuthority(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Required({TEXT("actorName")});
}

static void S_CheckIsLocallyControlled(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Required({TEXT("actorName")});
}

// Network relevancy
static void S_ConfigureNetCullDistance(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("netCullDistanceSquared"), TEXT("Network cull distance squared."))
	 .Bool(TEXT("useOwnerNetRelevancy"), TEXT("Use owner relevancy."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetAlwaysRelevant(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("alwaysRelevant"), TEXT("Always relevant to all clients."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetOnlyRelevantToOwner(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("onlyRelevantToOwner"), TEXT("Only relevant to owner."))
	 .Required({TEXT("blueprintPath")});
}

// Net serialization

static void S_SetReplicatedUsing(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .String(TEXT("repNotifyFunc"), TEXT("RepNotify function name."))
	 .Required({TEXT("blueprintPath"), TEXT("propertyName"), TEXT("repNotifyFunc")});
}

static void S_ConfigurePushModel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("usePushModel"), TEXT("Use push-model replication."))
	 .Required({TEXT("blueprintPath")});
}

// Network prediction
static void S_ConfigureClientPrediction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("enablePrediction"), TEXT("Enable client-side prediction."))
	 .Number(TEXT("predictionThreshold"), TEXT("Prediction threshold for client prediction."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureServerCorrection(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("correctionThreshold"), TEXT("Server correction threshold."))
	 .Number(TEXT("smoothingRate"), TEXT("Smoothing rate for corrections."))
	 .Required({TEXT("blueprintPath")});
}

static void S_AddNetworkPredictionData(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .StringEnum(TEXT("dataType"), {
		TEXT("Transform"),
		TEXT("Vector"),
		TEXT("Rotator"),
		TEXT("Float")
	 }, TEXT("Network prediction data type."))
	 .String(TEXT("variableName"), TEXT("add_network_prediction_data: name for the created replicated variable (default PredictionData_<dataType>)."))
	 .Required({TEXT("blueprintPath"), TEXT("dataType")});
}

static void S_ConfigureMovementPrediction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .StringEnum(TEXT("networkSmoothingMode"), {
		TEXT("Disabled"),
		TEXT("Linear"),
		TEXT("Exponential")
	 }, TEXT("Movement smoothing mode."))
	 .Number(TEXT("networkMaxSmoothUpdateDistance"), TEXT("Max smooth update distance."))
	 .Number(TEXT("networkNoSmoothUpdateDistance"), TEXT("No smooth update distance."))
	 .Required({TEXT("blueprintPath")});
}

// Connection & net driver
static void S_ConfigureNetDriver(FMcpSchemaBuilder& B)
{
	B.Integer(TEXT("maxClientRate"), TEXT("Max client rate."))
	 .Integer(TEXT("maxInternetClientRate"), TEXT("Max internet client rate."))
	 .Integer(TEXT("netServerMaxTickRate"), TEXT("Server max tick rate."));
}

static void S_SetNetRole(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .StringEnum(TEXT("role"), {
		TEXT("ROLE_None"),
		TEXT("ROLE_SimulatedProxy"),
		TEXT("ROLE_AutonomousProxy"),
		TEXT("ROLE_Authority")
	 }, TEXT("Net role."))
	 .Required({TEXT("blueprintPath"), TEXT("role")});
}

static void S_ConfigureReplicatedMovement(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("replicateMovement"), TEXT("Replicate movement."))
	 .Required({TEXT("blueprintPath")});
}

// Utility
static void S_GetInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."));
}

// Game framework (GameFrameworkHandlers.cpp)
static void S_CreateGameMode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("parentClass"), TEXT("create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class: parent class path (defaults to the native base class)."))
	 .String(TEXT("defaultPawnClass"), TEXT("create_game_mode: default pawn class path. Alias: pawnClass (set_default_pawn_class)."))
	 .String(TEXT("playerControllerClass"), TEXT("create_game_mode / set_player_controller_class: PlayerController class path."))
	 .String(TEXT("gameStateClass"), TEXT("create_game_mode / set_game_state_class: GameState class path."))
	 .String(TEXT("playerStateClass"), TEXT("create_game_mode / set_player_state_class: PlayerState class path."))
	 .String(TEXT("hudClass"), TEXT("create_game_mode: HUD class path."))
	 .Required({TEXT("name")});
}

static void S_CreateGameState(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("parentClass"), TEXT("create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class: parent class path (defaults to the native base class)."))
	 .Required({TEXT("name")});
}

static void S_CreatePlayerController(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("parentClass"), TEXT("create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class: parent class path (defaults to the native base class)."))
	 .Required({TEXT("name")});
}

static void S_CreatePlayerState(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("parentClass"), TEXT("create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class: parent class path (defaults to the native base class)."))
	 .Required({TEXT("name")});
}

static void S_CreateGameInstance(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("parentClass"), TEXT("create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class: parent class path (defaults to the native base class)."))
	 .Required({TEXT("name")});
}

static void S_CreateHudClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("parentClass"), TEXT("create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class: parent class path (defaults to the native base class)."))
	 .Required({TEXT("name")});
}

static void S_SetDefaultPawnClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("pawnClass"), TEXT("set_default_pawn_class: default pawn class path (alias of defaultPawnClass; first present wins)."))
	 .String(TEXT("defaultPawnClass"), TEXT("create_game_mode: default pawn class path. Alias: pawnClass (set_default_pawn_class)."));
}

static void S_SetPlayerControllerClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("playerControllerClass"), TEXT("create_game_mode / set_player_controller_class: PlayerController class path."))
	 .Required({TEXT("playerControllerClass")});
}

static void S_SetGameStateClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("gameStateClass"), TEXT("create_game_mode / set_game_state_class: GameState class path."))
	 .Required({TEXT("gameStateClass")});
}

static void S_SetPlayerStateClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("playerStateClass"), TEXT("create_game_mode / set_player_state_class: PlayerState class path."))
	 .Required({TEXT("playerStateClass")});
}

static void S_ConfigureGameRules(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("bDelayedStart"), TEXT("configure_game_rules: sets AGameMode::bDelayedStart when the property exists on the class."))
	 .Number(TEXT("startPlayersNeeded"), TEXT("configure_game_rules: NOT IMPLEMENTED — rejected as an unsupported field (not a native GameMode property, not a generated Blueprint variable)."));
}






static void S_ConfigurePlayerStart(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Object(TEXT("location"), TEXT("configure_player_start: PlayerStart world location (x, y, z)."))
	 .Object(TEXT("rotation"), TEXT("configure_player_start: PlayerStart world rotation (pitch, yaw, roll)."))
	 .Integer(TEXT("teamIndex"), TEXT("configure_player_start: team index used to build the default PlayerStartTag."))
	 .Bool(TEXT("bPlayerOnly"), TEXT("configure_player_start: restrict the PlayerStart to player pawns only."))
	 .String(TEXT("playerStartName"), TEXT("configure_player_start: specific PlayerStart actor to configure (falls back to actorName)."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("playerStartTag"), TEXT("configure_player_start: PlayerStartTag to assign (default derived from teamIndex)."));
}

static void S_SetRespawnRules(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("respawnDelay"), TEXT("configure_spawn_system / set_respawn_rules: seconds before a player respawns (default 5)."))
	 .String(TEXT("respawnLocation"), TEXT("set_respawn_rules: named respawn location strategy (default PlayerStart)."))
	 .Bool(TEXT("forceRespawn"), TEXT("set_respawn_rules: whether respawn is forced rather than optional."))
	 .Integer(TEXT("respawnLives"), TEXT("set_respawn_rules: lives per player (-1 = unlimited)."));
}

static void S_ConfigureSpectating(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("spectatorClass"), TEXT("configure_spectating: SpectatorPawn class path."))
	 .Bool(TEXT("allowSpectating"), TEXT("configure_spectating: allow players to spectate (default true)."))
	 .String(TEXT("spectatorViewMode"), TEXT("configure_spectating: spectator view mode (default FreeCam)."));
}

static void S_GetGameFrameworkInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Asset name (create_game_mode / create_game_state / create_player_controller / create_player_state / create_game_instance / create_hud_class)."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset."))
	 .Bool(TEXT("save"), TEXT("Game framework actions: persist the created/modified asset (or level, for configure_player_start) after applying changes (default false)."))
	 .String(TEXT("gameModeBlueprint"), TEXT("Alias of blueprintPath for GameFramework actions (set_default_pawn_class, set_player_controller_class, set_game_state_class, set_player_state_class, configure_game_rules, setup_match_states, configure_round_system, configure_team_system, configure_scoring_system, configure_spawn_system, set_respawn_rules, configure_spectating, get_game_framework_info)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."));
}

// Sessions (SessionsHandlers.cpp)
static void S_AddLocalPlayer(FMcpSchemaBuilder& B)
{
	B.Number(TEXT("controllerId"), TEXT("add_local_player: controller id for the new local player."))
	 .Required({TEXT("controllerId")});
}

static void S_RemoveLocalPlayer(FMcpSchemaBuilder& B)
{
	B.Integer(TEXT("playerIndex"), TEXT("remove_local_player: local player index to remove (cannot be 0)."))
	 .Required({TEXT("playerIndex")});
}


static void S_HostLanServer(FMcpSchemaBuilder& B)
{
	B.String(TEXT("serverName"), TEXT("host_lan_server: display name for the hosted server."))
	 .String(TEXT("mapName"), TEXT("host_lan_server: map to travel to when hosting."))
	 .Integer(TEXT("maxPlayers"), TEXT("host_lan_server: max player count (default 4)."))
	 .String(TEXT("travelOptions"), TEXT("host_lan_server: extra URL options appended to the travel string."))
	 .Bool(TEXT("executeTravel"), TEXT("host_lan_server: actually perform ServerTravel instead of just building the URL (default false)."))
	 .Required({TEXT("mapName")});
}

static void S_EnableVoiceChat(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("voiceEnabled"), TEXT("enable_voice_chat: enable or disable voice chat."))
	 .Required({TEXT("voiceEnabled")});
}


static void S_MutePlayer(FMcpSchemaBuilder& B)
{
	B.String(TEXT("playerName"), TEXT("mute_player: player name to mute/unmute (or use targetPlayerId)."))
	 .String(TEXT("targetPlayerId"), TEXT("mute_player: player id to mute/unmute (or use playerName)."))
	 .Bool(TEXT("muted"), TEXT("mute_player: mute (true) or unmute (false), default true."))
	 .Integer(TEXT("localPlayerNum"), TEXT("mute_player: local player index applying the mute."))
	 .Bool(TEXT("systemWide"), TEXT("mute_player: apply the mute system-wide rather than just locally."));
}

static void S_GetSessionsInfo(FMcpSchemaBuilder&) {}

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are authored per action and deliberately mixed. RequiresEditor on
// the GameFramework/Sessions actions — those two chains were
// whole-editor-gated and the members answer their editor-build stubs in
// non-editor builds; NOT on the core networking actions —
// NetworkingHandlers.cpp carries no editor gate, and flagging them would
// newly reject GEditor-less runs the shim served (the world-dependent
// bodies keep their handler-enforced NO_WORLD errors). Mutating on the
// writers only. The readers are check_has_authority,
// check_is_locally_controlled, get_info, get_game_framework_info, and
// get_sessions_info.

#define MCP_NW_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                        \
class FMcpCall_ManageNetworking_##ClassSuffix final : public FMcpCall                    \
{                                                                                        \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }       \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_networking"),          \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                            \
		return D;                                                                        \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return HandlerFn(S, RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

// Replication (NetworkingHandlers.cpp)
MCP_NW_CALL(SetPropertyReplicated, "set_property_replicated", McpHandlers::Networking::HandleNetworkingSetPropertyReplicated, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetReplicationCondition, "set_replication_condition", McpHandlers::Networking::HandleNetworkingSetReplicationCondition, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureNetUpdateFrequency, "configure_net_update_frequency", McpHandlers::Networking::HandleNetworkingConfigureNetUpdateFrequency, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureNetPriority, "configure_net_priority", McpHandlers::Networking::HandleNetworkingConfigureNetPriority, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetNetDormancy, "set_net_dormancy", McpHandlers::Networking::HandleNetworkingSetNetDormancy, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureReplicationGraph, "configure_replication_graph", McpHandlers::Networking::HandleNetworkingConfigureReplicationGraph, EMcpCallFlags::Mutating)

// RPCs
MCP_NW_CALL(CreateRpcFunction, "create_rpc_function", McpHandlers::Networking::HandleNetworkingCreateRpcFunction, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureRpcValidation, "configure_rpc_validation", McpHandlers::Networking::HandleNetworkingConfigureRpcValidation, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetRpcReliability, "set_rpc_reliability", McpHandlers::Networking::HandleNetworkingSetRpcReliability, EMcpCallFlags::Mutating)

// Authority & ownership
MCP_NW_CALL(SetOwner, "set_owner", McpHandlers::Networking::HandleNetworkingSetOwner, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetAutonomousProxy, "set_autonomous_proxy", McpHandlers::Networking::HandleNetworkingSetAutonomousProxy, EMcpCallFlags::Mutating)
MCP_NW_CALL(CheckHasAuthority, "check_has_authority", McpHandlers::Networking::HandleNetworkingCheckHasAuthority, EMcpCallFlags::None)
MCP_NW_CALL(CheckIsLocallyControlled, "check_is_locally_controlled", McpHandlers::Networking::HandleNetworkingCheckIsLocallyControlled, EMcpCallFlags::None)

// Network relevancy
MCP_NW_CALL(ConfigureNetCullDistance, "configure_net_cull_distance", McpHandlers::Networking::HandleNetworkingConfigureNetCullDistance, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetAlwaysRelevant, "set_always_relevant", McpHandlers::Networking::HandleNetworkingSetAlwaysRelevant, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetOnlyRelevantToOwner, "set_only_relevant_to_owner", McpHandlers::Networking::HandleNetworkingSetOnlyRelevantToOwner, EMcpCallFlags::Mutating)

// Net serialization
MCP_NW_CALL(SetReplicatedUsing, "set_replicated_using", McpHandlers::Networking::HandleNetworkingSetReplicatedUsing, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigurePushModel, "configure_push_model", McpHandlers::Networking::HandleNetworkingConfigurePushModel, EMcpCallFlags::Mutating)

// Network prediction
MCP_NW_CALL(ConfigureClientPrediction, "configure_client_prediction", McpHandlers::Networking::HandleNetworkingConfigureClientPrediction, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureServerCorrection, "configure_server_correction", McpHandlers::Networking::HandleNetworkingConfigureServerCorrection, EMcpCallFlags::Mutating)
MCP_NW_CALL(AddNetworkPredictionData, "add_network_prediction_data", McpHandlers::Networking::HandleNetworkingAddNetworkPredictionData, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureMovementPrediction, "configure_movement_prediction", McpHandlers::Networking::HandleNetworkingConfigureMovementPrediction, EMcpCallFlags::Mutating)

// Connection & net driver
MCP_NW_CALL(ConfigureNetDriver, "configure_net_driver", McpHandlers::Networking::HandleNetworkingConfigureNetDriver, EMcpCallFlags::Mutating)
MCP_NW_CALL(SetNetRole, "set_net_role", McpHandlers::Networking::HandleNetworkingSetNetRole, EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureReplicatedMovement, "configure_replicated_movement", McpHandlers::Networking::HandleNetworkingConfigureReplicatedMovement, EMcpCallFlags::Mutating)

// Utility
MCP_NW_CALL(GetInfo, "get_info", McpHandlers::Networking::HandleNetworkingGetInfo, EMcpCallFlags::None)

// Game framework (GameFrameworkHandlers.cpp)
MCP_NW_CALL(CreateGameMode, "create_game_mode", McpHandlers::Networking::HandleGameFrameworkCreateGameMode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(CreateGameState, "create_game_state", McpHandlers::Networking::HandleGameFrameworkCreateGameState, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(CreatePlayerController, "create_player_controller", McpHandlers::Networking::HandleGameFrameworkCreatePlayerController, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(CreatePlayerState, "create_player_state", McpHandlers::Networking::HandleGameFrameworkCreatePlayerState, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(CreateGameInstance, "create_game_instance", McpHandlers::Networking::HandleGameFrameworkCreateGameInstance, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(CreateHudClass, "create_hud_class", McpHandlers::Networking::HandleGameFrameworkCreateHudClass, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetDefaultPawnClass, "set_default_pawn_class", McpHandlers::Networking::HandleGameFrameworkSetDefaultPawnClass, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetPlayerControllerClass, "set_player_controller_class", McpHandlers::Networking::HandleGameFrameworkSetPlayerControllerClass, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetGameStateClass, "set_game_state_class", McpHandlers::Networking::HandleGameFrameworkSetGameStateClass, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetPlayerStateClass, "set_player_state_class", McpHandlers::Networking::HandleGameFrameworkSetPlayerStateClass, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureGameRules, "configure_game_rules", McpHandlers::Networking::HandleGameFrameworkConfigureGameRules, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigurePlayerStart, "configure_player_start", McpHandlers::Networking::HandleGameFrameworkConfigurePlayerStart, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(SetRespawnRules, "set_respawn_rules", McpHandlers::Networking::HandleGameFrameworkSetRespawnRules, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(ConfigureSpectating, "configure_spectating", McpHandlers::Networking::HandleGameFrameworkConfigureSpectating, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(GetGameFrameworkInfo, "get_game_framework_info", McpHandlers::Networking::HandleGameFrameworkGetGameFrameworkInfo, EMcpCallFlags::RequiresEditor)

// Sessions (SessionsHandlers.cpp)
MCP_NW_CALL(AddLocalPlayer, "add_local_player", McpHandlers::Networking::HandleSessionsAddLocalPlayer, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(RemoveLocalPlayer, "remove_local_player", McpHandlers::Networking::HandleSessionsRemoveLocalPlayer, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(HostLanServer, "host_lan_server", McpHandlers::Networking::HandleSessionsHostLanServer, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(EnableVoiceChat, "enable_voice_chat", McpHandlers::Networking::HandleSessionsEnableVoiceChat, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(MutePlayer, "mute_player", McpHandlers::Networking::HandleSessionsMutePlayer, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_NW_CALL(GetSessionsInfo, "get_sessions_info", McpHandlers::Networking::HandleSessionsGetSessionsInfo, EMcpCallFlags::RequiresEditor)

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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigurePlayerStart>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_SetRespawnRules>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_ConfigureSpectating>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_GetGameFrameworkInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_AddLocalPlayer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_RemoveLocalPlayer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_HostLanServer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_EnableVoiceChat>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_MutePlayer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageNetworking_GetSessionsInfo>());
}
