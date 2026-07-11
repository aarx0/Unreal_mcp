// LINT-TOOL: manage_ai
// LINT-SCHEMA-DERIVED
// manage_ai as FMcpCall classes — sixteenth classed family, now schema-from-decls
// (docs/action-declarations.md). Each class AUTHORS its schema fragment in a
// S_<Suffix>() function; the published facade schema folds those fragments and
// GetDecl() derives the validation decl from the same fragment via
// McpDeriveDecl(), so schema and decl are one source and cannot drift. Run()
// delegates to the subsystem member handlers — HandleAi* (AIHandlers.cpp) for the
// core actions, HandleBehaviorTree* (BehaviorTreeHandlers.cpp) for the 7 Behavior
// Tree graph actions, HandleNavigation* (NavigationHandlers.cpp) for the 12
// navigation actions — until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_AIHandlers.h"
#include "McpAutomationBridge_BehaviorTreeHandlers.h"
#include "McpAutomationBridge_NavigationHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageAi
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions + builder methods
// are copied verbatim from the retired facade; McpDeriveDecl() reads the param
// kinds + required-set back out of these to build the transport validation decl.
//
// The path spellings (assetPath/behaviorTreePath/path for the BT graph actions,
// controllerPath/behaviorTreePath/blackboardPath for assign_blackboard,
// blueprintPath/controllerPath for get_info) stay optional: the handler resolves
// the alias fallback itself, so the "at least one" requirement is handler-enforced,
// not declared. setup_perception is the exception — it declares the
// {blueprintPath, controllerPath} any-of group via RequiredAnyOf() (a
// validation-only side-channel; nothing reaches the published JSON), so the
// transport rejects a request that supplies neither before the handler runs.
//
// Drift-corrections vs the retired flat facade + P_* decls, re-verified against
// the member bodies at this classing:
//   • add_node/connect_nodes/remove_node/break_connections dropped the
//     material/Niagara graph params carried in from the old shared graph
//     dispatcher (fromExpression/toExpression/inputName/sourceNodeId/
//     targetNodeId/emitterName/scriptType/expressionIndex/pinName) plus a
//     'name'/'save' the BT handlers never read (HandleBehaviorTree* only read
//     assetPath|behaviorTreePath|path + nodeType/x/y/nodeId/parentNodeId/
//     childNodeId).
//   • configure_sight_config authors maxAge/autoSuccessRange/
//     pointOfViewBackwardOffset/nearClippingRadius/enemies/neutrals/friendlies/
//     detectionByAffiliation top-level — the facade only exposed them nested
//     under sightConfig, but ReadSightParams(Payload) reads them top-level too
//     (dual API; nested wins when both are given).
//   • configure_test_scoring authors scoringEquation/filterType/clampMin/
//     clampMax/floatMin/floatMax top-level for the same reason (Settings falls
//     back to Payload when testSettings is absent).
//   • configure_nav_area_cost authors fixedAreaEnteringCost — read by the handler
//     but absent from the old facade entirely.
//   • create_nav_link_proxy/create_smart_link keep location/startPoint/endPoint
//     required (the body rejects when any is missing).
// configure_state_tree_task shares the 'taskType' param name with add_task_node;
// both author the add_task_node StringEnum verbatim so the folded facade keeps one
// consistent taskType entry (the State-Tree task string is not enum-constrained at
// the transport gate — the decl only checks kind=string).

static void S_CreateAiController(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Required({TEXT("name")});
}

static void S_AssignBehaviorTree(FMcpSchemaBuilder& B)
{
	B.String(TEXT("controllerPath"), TEXT("Path to controller blueprint."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .Required({TEXT("controllerPath"), TEXT("behaviorTreePath")});
}

static void S_AssignBlackboard(FMcpSchemaBuilder& B)
{
	B.String(TEXT("controllerPath"), TEXT("Path to controller blueprint."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("blackboardPath"), TEXT("Path to blackboard asset. On 'create'/'create_behavior_tree' it is assigned to the new Behavior Tree (omit = unassigned)."))
	 .Required({TEXT("blackboardPath")});
}

static void S_CreateBlackboardAsset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Required({TEXT("name")});
}

static void S_AddBlackboardKey(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blackboardPath"), TEXT("Path to blackboard asset. On 'create'/'create_behavior_tree' it is assigned to the new Behavior Tree (omit = unassigned)."))
	 .String(TEXT("keyName"), TEXT("Name of the key."))
	 .StringEnum(TEXT("keyType"), {
		TEXT("Bool"),
		TEXT("Int"),
		TEXT("Float"),
		TEXT("Vector"),
		TEXT("Rotator"),
		TEXT("Object"),
		TEXT("Class"),
		TEXT("Enum"),
		TEXT("Name"),
		TEXT("String")
	 }, TEXT("Blackboard key data type."))
	 .String(TEXT("baseObjectClass"), TEXT("add_blackboard_key: base class for Object/Class key types (default 'Actor')."))
	 .Bool(TEXT("isInstanceSynced"), TEXT("Sync key across instances."))
	 .Required({TEXT("blackboardPath")});
}

static void S_SetKeyInstanceSynced(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blackboardPath"), TEXT("Path to blackboard asset. On 'create'/'create_behavior_tree' it is assigned to the new Behavior Tree (omit = unassigned)."))
	 .String(TEXT("keyName"), TEXT("Name of the key."))
	 .Bool(TEXT("isInstanceSynced"), TEXT("Sync key across instances."))
	 .Required({TEXT("blackboardPath"), TEXT("keyName")});
}

static void S_CreateBehaviorTree(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blackboardPath"), TEXT("Path to blackboard asset. On 'create'/'create_behavior_tree' it is assigned to the new Behavior Tree (omit = unassigned)."))
	 .Required({TEXT("name")});
}

static void S_AddCompositeNode(FMcpSchemaBuilder& B)
{
	B.StringEnum(TEXT("compositeType"), {
		TEXT("Selector"),
		TEXT("Sequence"),
		TEXT("SimpleParallel")
	 }, TEXT("Composite node type for add_composite_node."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Number(TEXT("x"), TEXT("Graph node X coordinate."))
	 .Number(TEXT("y"), TEXT("Graph node Y coordinate."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("parentNodeId"), TEXT("Parent node id: 'root' or a node GUID. add_node/add_composite_node/add_task_node connect the new node under it; add_subnode/add_decorator/add_service attach to it."))
	 .Required({TEXT("compositeType")});
}

static void S_AddTaskNode(FMcpSchemaBuilder& B)
{
	B.StringEnum(TEXT("taskType"), {
		TEXT("MoveTo"),
		TEXT("MoveDirectlyToward"),
		TEXT("RotateToFaceBBEntry"),
		TEXT("Wait"),
		TEXT("WaitBlackboardTime"),
		TEXT("PlayAnimation"),
		TEXT("PlaySound"),
		TEXT("RunEQSQuery"),
		TEXT("RunBehavior"),
		TEXT("RunBehaviorDynamic"),
		TEXT("SetTagCooldown"),
		TEXT("FinishWithResult"),
		TEXT("MakeNoise"),
		TEXT("Custom")
	 }, TEXT("Task node type for add_task_node (expands to BTTask_<type>; 'Custom' uses nodeClass)."))
	 .String(TEXT("nodeClass"), TEXT("Behavior Tree node class: engine name (e.g. 'BTDecorator_Blackboard'), a short name that expands to one, or a Blueprint class path."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Number(TEXT("x"), TEXT("Graph node X coordinate."))
	 .Number(TEXT("y"), TEXT("Graph node Y coordinate."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("parentNodeId"), TEXT("Parent node id: 'root' or a node GUID. add_node/add_composite_node/add_task_node connect the new node under it; add_subnode/add_decorator/add_service attach to it."))
	 .Required({TEXT("taskType")});
}

static void S_AddDecorator(FMcpSchemaBuilder& B)
{
	B.String(TEXT("parentNodeId"), TEXT("Parent node id: 'root' or a node GUID. add_node/add_composite_node/add_task_node connect the new node under it; add_subnode/add_decorator/add_service attach to it."))
	 .StringEnum(TEXT("decoratorType"), {
		TEXT("Blackboard"),
		TEXT("CheckGameplayTagsOnActor"),
		TEXT("CompareBBEntries"),
		TEXT("Cooldown"),
		TEXT("ConeCheck"),
		TEXT("DoesPathExist"),
		TEXT("IsAtLocation"),
		TEXT("IsBBEntryOfClass"),
		TEXT("KeepInCone"),
		TEXT("Loop"),
		TEXT("SetTagCooldown"),
		TEXT("TagCooldown"),
		TEXT("TimeLimit"),
		TEXT("ForceSuccess"),
		TEXT("ConditionalLoop"),
		TEXT("Custom")
	 }, TEXT("Decorator node type for add_decorator (expands to BTDecorator_<type>; 'Custom' uses nodeClass)."))
	 .String(TEXT("nodeClass"), TEXT("Behavior Tree node class: engine name (e.g. 'BTDecorator_Blackboard'), a short name that expands to one, or a Blueprint class path."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Required({TEXT("parentNodeId"), TEXT("decoratorType")});
}

static void S_AddService(FMcpSchemaBuilder& B)
{
	B.String(TEXT("parentNodeId"), TEXT("Parent node id: 'root' or a node GUID. add_node/add_composite_node/add_task_node connect the new node under it; add_subnode/add_decorator/add_service attach to it."))
	 .StringEnum(TEXT("serviceType"), {
		TEXT("DefaultFocus"),
		TEXT("RunEQS"),
		TEXT("Custom")
	 }, TEXT("Service node type for add_service (expands to BTService_<type>; 'Custom' uses nodeClass)."))
	 .String(TEXT("nodeClass"), TEXT("Behavior Tree node class: engine name (e.g. 'BTDecorator_Blackboard'), a short name that expands to one, or a Blueprint class path."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Required({TEXT("parentNodeId"), TEXT("serviceType")});
}

static void S_ConfigureBtNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Object(TEXT("properties"), TEXT("Properties to set on a graph node."))
	 .Required({TEXT("behaviorTreePath"), TEXT("nodeId")});
}

static void S_CreateEqsQuery(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Required({TEXT("name")});
}

static void S_AddEqsGenerator(FMcpSchemaBuilder& B)
{
	B.String(TEXT("queryPath"), TEXT("Path to EQS query asset."))
	 .StringEnum(TEXT("generatorType"), {
		TEXT("ActorsOfClass"),
		TEXT("CurrentLocation"),
		TEXT("Donut"),
		TEXT("OnCircle"),
		TEXT("PathingGrid"),
		TEXT("SimpleGrid"),
		TEXT("Composite"),
		TEXT("Custom")
	 }, TEXT("EQS generator type."))
	 .Object(TEXT("generatorSettings"), TEXT("Generator-specific settings."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("searchRadius"))
		 .String(TEXT("searchCenter"), TEXT(""))
		 .String(TEXT("actorClass"), TEXT(""))
		 .Number(TEXT("gridSize"))
		 .Number(TEXT("spacesBetween"))
		 .Number(TEXT("innerRadius"))
		 .Number(TEXT("outerRadius"));
	})
	 .Required({TEXT("queryPath"), TEXT("generatorType")});
}

static void S_AddEqsContext(FMcpSchemaBuilder& B)
{
	B.String(TEXT("queryPath"), TEXT("Path to EQS query asset."))
	 .StringEnum(TEXT("contextType"), {
		TEXT("Querier"),
		TEXT("Item"),
		TEXT("EnvQueryContext_BlueprintBase"),
		TEXT("Custom")
	 }, TEXT("EQS context type."))
	 .Required({TEXT("queryPath")});
}

static void S_AddEqsTest(FMcpSchemaBuilder& B)
{
	B.String(TEXT("queryPath"), TEXT("Path to EQS query asset."))
	 .StringEnum(TEXT("testType"), {
		TEXT("Distance"),
		TEXT("Dot"),
		TEXT("GameplayTags"),
		TEXT("Overlap"),
		TEXT("Pathfinding"),
		TEXT("PathfindingBatch"),
		TEXT("Project"),
		TEXT("Random"),
		TEXT("Trace"),
		TEXT("Custom")
	 }, TEXT("EQS test type."))
	 .Required({TEXT("queryPath"), TEXT("testType")});
}

static void S_ConfigureTestScoring(FMcpSchemaBuilder& B)
{
	B.String(TEXT("queryPath"), TEXT("Path to EQS query asset."))
	 .Integer(TEXT("testIndex"), TEXT("Index of test to configure."))
	 .Object(TEXT("testSettings"), TEXT("Test scoring and filter settings."),
		[](FMcpSchemaBuilder& S) {
		S.StringEnum(TEXT("scoringEquation"), {
			TEXT("Linear"),
			TEXT("Square"),
			TEXT("InverseLinear"),
			TEXT("Constant")
		}, TEXT(""))
		 .Number(TEXT("clampMin"))
		 .Number(TEXT("clampMax"))
		 .StringEnum(TEXT("filterType"), {
			TEXT("Minimum"),
			TEXT("Maximum"),
			TEXT("Range")
		}, TEXT(""))
		 .Number(TEXT("floatMin"))
		 .Number(TEXT("floatMax"));
	})
	 .StringEnum(TEXT("scoringEquation"), {
		TEXT("Linear"),
		TEXT("Square"),
		TEXT("InverseLinear"),
		TEXT("Constant")
	 }, TEXT("configure_test_scoring: score curve, top-level alias of testSettings.scoringEquation."))
	 .StringEnum(TEXT("filterType"), {
		TEXT("Minimum"),
		TEXT("Maximum"),
		TEXT("Range")
	 }, TEXT("configure_test_scoring: filter mode, top-level alias of testSettings.filterType."))
	 .Number(TEXT("clampMin"), TEXT("configure_test_scoring: lower score clamp, top-level alias of testSettings.clampMin."))
	 .Number(TEXT("clampMax"), TEXT("configure_test_scoring: upper score clamp, top-level alias of testSettings.clampMax."))
	 .Number(TEXT("floatMin"), TEXT("configure_test_scoring: lower range-filter bound, top-level alias of testSettings.floatMin."))
	 .Number(TEXT("floatMax"), TEXT("configure_test_scoring: upper range-filter bound, top-level alias of testSettings.floatMax."))
	 .Required({TEXT("queryPath")});
}

static void S_AddAiPerceptionComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureSightConfig(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("sightRadius"), TEXT("AI sight radius."))
	 .Number(TEXT("loseSightRadius"), TEXT("AI sight lose radius."))
	 .Number(TEXT("peripheralVisionAngle"), TEXT("Sight peripheral vision angle."))
	 .Number(TEXT("maxAge"), TEXT("configure_sight_config: perception forget time in seconds (0 = never expires); top-level alias of sightConfig.maxAge."))
	 .Number(TEXT("autoSuccessRange"), TEXT("configure_sight_config: range within which the target is always seen (-1 disables); top-level alias of sightConfig.autoSuccessRange."))
	 .Number(TEXT("pointOfViewBackwardOffset"), TEXT("configure_sight_config: eye-point backward offset; top-level alias of sightConfig.pointOfViewBackwardOffset."))
	 .Number(TEXT("nearClippingRadius"), TEXT("configure_sight_config: near clipping radius; top-level alias of sightConfig.nearClippingRadius."))
	 .Bool(TEXT("enemies"), TEXT("configure_sight_config: detect enemy-affiliated targets; top-level alias of sightConfig.detectionByAffiliation.enemies."))
	 .Bool(TEXT("neutrals"), TEXT("configure_sight_config: detect neutral-affiliated targets; top-level alias of sightConfig.detectionByAffiliation.neutrals."))
	 .Bool(TEXT("friendlies"), TEXT("configure_sight_config: detect friendly-affiliated targets; top-level alias of sightConfig.detectionByAffiliation.friendlies."))
	 .Object(TEXT("detectionByAffiliation"), TEXT("configure_sight_config: which affiliations to detect; top-level alias of sightConfig.detectionByAffiliation."),
		[](FMcpSchemaBuilder& S) {
		S.Bool(TEXT("enemies"), TEXT(""))
		 .Bool(TEXT("neutrals"), TEXT(""))
		 .Bool(TEXT("friendlies"), TEXT(""));
	})
	 .Object(TEXT("sightConfig"), TEXT("AI sight sense configuration."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("sightRadius"))
		 .Number(TEXT("loseSightRadius"))
		 .Number(TEXT("peripheralVisionAngle"))
		 .Number(TEXT("pointOfViewBackwardOffset"))
		 .Number(TEXT("nearClippingRadius"))
		 .Number(TEXT("autoSuccessRange"))
		 .Number(TEXT("maxAge"))
		 .Object(TEXT("detectionByAffiliation"), TEXT(""),
			[](FMcpSchemaBuilder& Inner) {
			Inner.Bool(TEXT("enemies"), TEXT(""))
				 .Bool(TEXT("neutrals"), TEXT(""))
				 .Bool(TEXT("friendlies"), TEXT(""));
		});
	})
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureHearingConfig(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("hearingRange"), TEXT("AI hearing range."))
	 .Object(TEXT("hearingConfig"), TEXT("AI hearing sense configuration."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("hearingRange"))
		 .Bool(TEXT("detectFriendly"), TEXT(""))
		 .Number(TEXT("maxAge"));
	})
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureDamageSenseConfig(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Object(TEXT("damageConfig"), TEXT("AI damage sense configuration."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("maxAge"));
	})
	 .Required({TEXT("blueprintPath")});
}

static void S_SetPerceptionTeam(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Integer(TEXT("teamId"), TEXT("Team ID for perception affiliation (0=Neutral, 1=Player, 2=Enemy, etc.)."))
	 .Required({TEXT("blueprintPath")});
}

static void S_CreateStateTree(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("schemaType"), TEXT("create_state_tree: schema type (default 'Component')."))
	 .Required({TEXT("name")});
}

static void S_AddStateTreeState(FMcpSchemaBuilder& B)
{
	B.String(TEXT("stateTreePath"), TEXT("Path to State Tree asset."))
	 .String(TEXT("stateName"), TEXT("Name of the state."))
	 .String(TEXT("parentStateName"), TEXT("Parent state name."))
	 .String(TEXT("stateType"), TEXT("State Tree state type."))
	 .Required({TEXT("stateTreePath"), TEXT("stateName")});
}

static void S_AddStateTreeTransition(FMcpSchemaBuilder& B)
{
	B.String(TEXT("stateTreePath"), TEXT("Path to State Tree asset."))
	 .String(TEXT("fromState"), TEXT("Source state name."))
	 .String(TEXT("toState"), TEXT("Target state name."))
	 .String(TEXT("triggerType"), TEXT("State Tree transition trigger type."))
	 .Required({TEXT("stateTreePath"), TEXT("fromState"), TEXT("toState")});
}

static void S_ConfigureStateTreeTask(FMcpSchemaBuilder& B)
{
	B.String(TEXT("stateTreePath"), TEXT("Path to State Tree asset."))
	 .String(TEXT("stateName"), TEXT("Name of the state."))
	 .StringEnum(TEXT("taskType"), {
		TEXT("MoveTo"),
		TEXT("MoveDirectlyToward"),
		TEXT("RotateToFaceBBEntry"),
		TEXT("Wait"),
		TEXT("WaitBlackboardTime"),
		TEXT("PlayAnimation"),
		TEXT("PlaySound"),
		TEXT("RunEQSQuery"),
		TEXT("RunBehavior"),
		TEXT("RunBehaviorDynamic"),
		TEXT("SetTagCooldown"),
		TEXT("FinishWithResult"),
		TEXT("MakeNoise"),
		TEXT("Custom")
	 }, TEXT("Task node type for add_task_node (expands to BTTask_<type>; 'Custom' uses nodeClass)."))
	 .String(TEXT("selectionBehavior"), TEXT("configure_state_tree_task: state selection behavior."))
	 .Required({TEXT("stateTreePath"), TEXT("stateName")});
}

static void S_CreateSmartObjectDefinition(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Required({TEXT("name")});
}

static void S_AddSmartObjectSlot(FMcpSchemaBuilder& B)
{
	B.String(TEXT("definitionPath"), TEXT("Path to definition asset."))
	 .Object(TEXT("offset"), TEXT("Generic offset vector."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Object(TEXT("rotation"), TEXT("Rotation for nav link proxy."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
	})
	 .Bool(TEXT("enabled"), TEXT("Generic enabled flag."))
	 .Required({TEXT("definitionPath")});
}

static void S_ConfigureSlotBehavior(FMcpSchemaBuilder& B)
{
	B.String(TEXT("definitionPath"), TEXT("Path to definition asset."))
	 .Integer(TEXT("slotIndex"), TEXT("Index of slot to configure."))
	 .String(TEXT("behaviorType"), TEXT("configure_slot_behavior: behavior definition type for the slot."))
	 .Array(TEXT("activityTags"), TEXT("configure_slot_behavior: gameplay tags describing slot activity."))
	 .Bool(TEXT("enabled"), TEXT("Generic enabled flag."))
	 .Required({TEXT("definitionPath")});
}

static void S_AddSmartObjectComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("definitionPath"), TEXT("Path to definition asset."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Required({TEXT("blueprintPath")});
}

static void S_CreateMassEntityConfig(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Required({TEXT("name")});
}

static void S_ConfigureMassEntity(FMcpSchemaBuilder& B)
{
	B.String(TEXT("configPath"), TEXT("Path to config asset."))
	 .String(TEXT("parentConfigPath"), TEXT("configure_mass_entity: optional parent config to inherit from."))
	 .Required({TEXT("configPath")});
}

static void S_AddMassSpawner(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("configPath"), TEXT("Path to config asset."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Integer(TEXT("spawnCount"), TEXT("Mass spawner entity count."))
	 .Required({TEXT("blueprintPath")});
}

static void S_GetAiInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("controllerPath"), TEXT("Path to controller blueprint."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("blackboardPath"), TEXT("Path to blackboard asset. On 'create'/'create_behavior_tree' it is assigned to the new Behavior Tree (omit = unassigned)."))
	 .String(TEXT("queryPath"), TEXT("Path to EQS query asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."));
}

static void S_SetupPerception(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("controllerPath"), TEXT("Path to controller blueprint."))
	 .Bool(TEXT("enableSight"), TEXT("Enable sight sense."))
	 .Number(TEXT("sightRadius"), TEXT("AI sight radius."))
	 .Number(TEXT("loseSightRadius"), TEXT("AI sight lose radius."))
	 .Number(TEXT("peripheralVisionAngle"), TEXT("Sight peripheral vision angle."))
	 .Bool(TEXT("enableHearing"), TEXT("Enable hearing sense."))
	 .Number(TEXT("hearingRange"), TEXT("AI hearing range."))
	 .Bool(TEXT("enableDamage"), TEXT("Enable damage sense."))
	 .StringEnum(TEXT("dominantSense"), {
		TEXT("Sight"),
		TEXT("Hearing"),
		TEXT("Damage"),
		TEXT("Touch"),
		TEXT("None")
	 }, TEXT("Dominant sense for perception prioritization."))
	 .RequiredAnyOf({TEXT("blueprintPath"), TEXT("controllerPath")});
}

static void S_SetFocus(FMcpSchemaBuilder& B)
{
	B.String(TEXT("controllerPath"), TEXT("Path to controller blueprint."))
	 .String(TEXT("focusActorName"), TEXT("Actor name to focus."))
	 .String(TEXT("targetActor"), TEXT("set_focus: alias for focusActorName (checked as fallback)."))
	 .Required({TEXT("controllerPath")});
}

static void S_ClearFocus(FMcpSchemaBuilder& B)
{
	B.String(TEXT("controllerPath"), TEXT("Path to controller blueprint."))
	 .Required({TEXT("controllerPath")});
}

static void S_SetBlackboardValue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blackboardPath"), TEXT("Path to blackboard asset. On 'create'/'create_behavior_tree' it is assigned to the new Behavior Tree (omit = unassigned)."))
	 .String(TEXT("keyName"), TEXT("Name of the key."))
	 .String(TEXT("stringValue"), TEXT("Blackboard value, parsed per key type (bool true/false, int, float, vector \"X= Y= Z=\", name)."))
	 .Required({TEXT("blackboardPath"), TEXT("keyName")});
}

static void S_GetBlackboardValue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blackboardPath"), TEXT("Path to blackboard asset. On 'create'/'create_behavior_tree' it is assigned to the new Behavior Tree (omit = unassigned)."))
	 .String(TEXT("keyName"), TEXT("Name of the key."))
	 .Required({TEXT("blackboardPath"), TEXT("keyName")});
}

static void S_RunBehaviorTree(FMcpSchemaBuilder& B)
{
	B.String(TEXT("controllerPath"), TEXT("Path to controller blueprint."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .Required({TEXT("controllerPath"), TEXT("behaviorTreePath")});
}

static void S_StopBehaviorTree(FMcpSchemaBuilder& B)
{
	B.String(TEXT("controllerPath"), TEXT("Path to controller blueprint."))
	 .Required({TEXT("controllerPath")});
}

static void S_Create(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("blackboardPath"), TEXT("Path to blackboard asset. On 'create'/'create_behavior_tree' it is assigned to the new Behavior Tree (omit = unassigned)."))
	 .Required({TEXT("name")});
}

static void S_AddNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("nodeType"), TEXT("Behavior Tree graph node type."))
	 .Number(TEXT("x"), TEXT("Graph node X coordinate."))
	 .Number(TEXT("y"), TEXT("Graph node Y coordinate."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("parentNodeId"), TEXT("Parent node id: 'root' or a node GUID. add_node/add_composite_node/add_task_node connect the new node under it; add_subnode/add_decorator/add_service attach to it."))
	 .Required({TEXT("nodeType")});
}

static void S_ConnectNodes(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("parentNodeId"), TEXT("Parent node id: 'root' or a node GUID. add_node/add_composite_node/add_task_node connect the new node under it; add_subnode/add_decorator/add_service attach to it."))
	 .String(TEXT("childNodeId"), TEXT("Child node ID."))
	 .Required({TEXT("parentNodeId"), TEXT("childNodeId")});
}

static void S_RemoveNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Required({TEXT("nodeId")});
}

static void S_BreakConnections(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Required({TEXT("nodeId")});
}

static void S_SetNodeProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("comment"), TEXT("Node comment."))
	 .Object(TEXT("properties"), TEXT("Properties to set on a graph node."))
	 .Required({TEXT("nodeId")});
}

static void S_AddSubnode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("parentNodeId"), TEXT("Parent node id: 'root' or a node GUID. add_node/add_composite_node/add_task_node connect the new node under it; add_subnode/add_decorator/add_service attach to it."))
	 .String(TEXT("subnodeType"), TEXT("Behavior Tree graph subnode type."))
	 .String(TEXT("nodeClass"), TEXT("Behavior Tree node class: engine name (e.g. 'BTDecorator_Blackboard'), a short name that expands to one, or a Blueprint class path."))
	 .Required({TEXT("parentNodeId"), TEXT("subnodeType"), TEXT("nodeClass")});
}

static void S_ConfigureNavMeshSettings(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("tileSizeUU"), TEXT("NavMesh tile size in UU (default: 1000)."))
	 .Number(TEXT("minRegionArea"), TEXT("Minimum region area to keep."))
	 .Number(TEXT("mergeRegionSize"), TEXT("Region merge threshold."))
	 .Number(TEXT("maxSimplificationError"), TEXT("Edge simplification error."))
	 .Number(TEXT("cellSize"), TEXT("NavMesh cell size (default: 19)."))
	 .Number(TEXT("cellHeight"), TEXT("NavMesh cell height (default: 10)."))
	 .Number(TEXT("agentStepHeight"), TEXT("Maximum step height agent can climb (default: 35)."));
}

static void S_SetNavAgentProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("agentRadius"), TEXT("Navigation agent radius (default: 35)."))
	 .Number(TEXT("agentHeight"), TEXT("Navigation agent height (default: 144)."))
	 .Number(TEXT("agentMaxSlope"), TEXT("Maximum slope angle in degrees (default: 44)."))
	 .Number(TEXT("agentStepHeight"), TEXT("Maximum step height agent can climb (default: 35)."));
}

static void S_RebuildNavigation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."));
}

static void S_CreateNavModifierComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("areaClass"), TEXT("Navigation area class path."))
	 .Object(TEXT("failsafeExtent"), TEXT("Failsafe extent for nav modifier when actor has no collision."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetNavAreaClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("areaClass"), TEXT("Navigation area class path."))
	 .Required({TEXT("actorName"), TEXT("areaClass")});
}

static void S_ConfigureNavAreaCost(FMcpSchemaBuilder& B)
{
	B.String(TEXT("areaClass"), TEXT("Navigation area class path."))
	 .Number(TEXT("areaCost"), TEXT("Pathfinding cost multiplier for area (1.0 = normal)."))
	 .Number(TEXT("fixedAreaEnteringCost"), TEXT("configure_nav_area_cost: flat cost added when entering the area (0 = none)."))
	 .Required({TEXT("areaClass")});
}

static void S_CreateNavLinkProxy(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("location"), TEXT("World location for nav link proxy."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Object(TEXT("rotation"), TEXT("Rotation for nav link proxy."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
	})
	 .Object(TEXT("startPoint"), TEXT("Start point of navigation link (relative to actor)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Object(TEXT("endPoint"), TEXT("End point of navigation link (relative to actor)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .StringEnum(TEXT("direction"), {
		TEXT("BothWays"),
		TEXT("LeftToRight"),
		TEXT("RightToLeft")
	 }, TEXT("Link traversal direction."))
	 .Required({TEXT("location"), TEXT("startPoint"), TEXT("endPoint")});
}

static void S_ConfigureNavLink(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("startPoint"), TEXT("Start point of navigation link (relative to actor)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Object(TEXT("endPoint"), TEXT("End point of navigation link (relative to actor)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .StringEnum(TEXT("direction"), {
		TEXT("BothWays"),
		TEXT("LeftToRight"),
		TEXT("RightToLeft")
	 }, TEXT("Link traversal direction."))
	 .Number(TEXT("snapRadius"), TEXT("Snap radius for link endpoints (default: 30)."))
	 .Required({TEXT("actorName")});
}

static void S_SetNavLinkType(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .StringEnum(TEXT("linkType"), {
		TEXT("simple"),
		TEXT("smart")
	 }, TEXT("Type of navigation link."))
	 .Required({TEXT("actorName")});
}

static void S_CreateSmartLink(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("location"), TEXT("World location for nav link proxy."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Object(TEXT("rotation"), TEXT("Rotation for nav link proxy."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
	})
	 .Object(TEXT("startPoint"), TEXT("Start point of navigation link (relative to actor)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Object(TEXT("endPoint"), TEXT("End point of navigation link (relative to actor)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .StringEnum(TEXT("direction"), {
		TEXT("BothWays"),
		TEXT("LeftToRight"),
		TEXT("RightToLeft")
	 }, TEXT("Link traversal direction."))
	 .Required({TEXT("location"), TEXT("startPoint"), TEXT("endPoint")});
}

static void S_ConfigureSmartLinkBehavior(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Bool(TEXT("linkEnabled"), TEXT("Whether the link is enabled."))
	 .String(TEXT("enabledAreaClass"), TEXT("Area class when smart link is enabled."))
	 .String(TEXT("disabledAreaClass"), TEXT("Area class when smart link is disabled."))
	 .Number(TEXT("broadcastRadius"), TEXT("Radius for state change broadcast."))
	 .Number(TEXT("broadcastInterval"), TEXT("Interval for state change broadcast (0 = single)."))
	 .Bool(TEXT("bCreateBoxObstacle"), TEXT("Add box obstacle during nav generation."))
	 .String(TEXT("obstacleAreaClass"), TEXT("Area class for box obstacle."))
	 .Object(TEXT("obstacleExtent"), TEXT("Extent of simple box obstacle."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Object(TEXT("obstacleOffset"), TEXT("Offset of simple box obstacle."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Required({TEXT("actorName")});
}

static void S_GetNavigationInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."));
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor on all rows (baked into the macro): the three retired
// dispatchers were whole-editor-gated and every member answers its chain's
// EDITOR_ONLY stub in non-editor builds. Mutating on the writers; the readers
// are get_info, get_blackboard_value, and get_navigation_info — add_eqs_context
// is also unflagged (its body is an honest NOT_IMPLEMENTED error that writes
// nothing).

#define MCP_AI_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                    \
class FMcpCall_ManageAi_##ClassSuffix final : public FMcpCall                             \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_ai"),                   \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),           \
			&S_##ClassSuffix);                                                           \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return HandlerFn(S, RequestId, Payload, Socket);                                   \
	}                                                                                     \
};

// AI Controllers & Blackboard assets (AIHandlers.cpp)
MCP_AI_CALL(CreateAiController, "create_controller", McpHandlers::Ai::HandleAiCreateAiController, EMcpCallFlags::Mutating)
MCP_AI_CALL(AssignBehaviorTree, "assign_behavior_tree", McpHandlers::Ai::HandleAiAssignBehaviorTree, EMcpCallFlags::Mutating)
MCP_AI_CALL(AssignBlackboard, "assign_blackboard", McpHandlers::Ai::HandleAiAssignBlackboard, EMcpCallFlags::Mutating)
MCP_AI_CALL(CreateBlackboardAsset, "create_blackboard_asset", McpHandlers::Ai::HandleAiCreateBlackboardAsset, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddBlackboardKey, "add_blackboard_key", McpHandlers::Ai::HandleAiAddBlackboardKey, EMcpCallFlags::Mutating)
MCP_AI_CALL(SetKeyInstanceSynced, "set_key_instance_synced", McpHandlers::Ai::HandleAiSetKeyInstanceSynced, EMcpCallFlags::Mutating)

// Behavior Tree conveniences (AIHandlers.cpp wrappers over the graph members)
MCP_AI_CALL(CreateBehaviorTree, "create_behavior_tree", McpHandlers::Ai::HandleAiCreateBehaviorTree, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddCompositeNode, "add_composite_node", McpHandlers::Ai::HandleAiAddCompositeNode, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddTaskNode, "add_task_node", McpHandlers::Ai::HandleAiAddTaskNode, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddDecorator, "add_decorator", McpHandlers::Ai::HandleAiAddDecorator, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddService, "add_service", McpHandlers::Ai::HandleAiAddService, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureBtNode, "configure_bt_node", McpHandlers::Ai::HandleAiConfigureBtNode, EMcpCallFlags::Mutating)

// EQS
MCP_AI_CALL(CreateEqsQuery, "create_eqs_query", McpHandlers::Ai::HandleAiCreateEqsQuery, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddEqsGenerator, "add_eqs_generator", McpHandlers::Ai::HandleAiAddEqsGenerator, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddEqsContext, "add_eqs_context", McpHandlers::Ai::HandleAiAddEqsContext, EMcpCallFlags::None)
MCP_AI_CALL(AddEqsTest, "add_eqs_test", McpHandlers::Ai::HandleAiAddEqsTest, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureTestScoring, "configure_test_scoring", McpHandlers::Ai::HandleAiConfigureTestScoring, EMcpCallFlags::Mutating)

// Perception
MCP_AI_CALL(AddAiPerceptionComponent, "add_perception_component", McpHandlers::Ai::HandleAiAddAiPerceptionComponent, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureSightConfig, "configure_sight_config", McpHandlers::Ai::HandleAiConfigureSightConfig, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureHearingConfig, "configure_hearing_config", McpHandlers::Ai::HandleAiConfigureHearingConfig, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureDamageSenseConfig, "configure_damage_sense_config", McpHandlers::Ai::HandleAiConfigureDamageSenseConfig, EMcpCallFlags::Mutating)
MCP_AI_CALL(SetPerceptionTeam, "set_perception_team", McpHandlers::Ai::HandleAiSetPerceptionTeam, EMcpCallFlags::Mutating)

// State Trees (UE 5.3+)
MCP_AI_CALL(CreateStateTree, "create_state_tree", McpHandlers::Ai::HandleAiCreateStateTree, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddStateTreeState, "add_state_tree_state", McpHandlers::Ai::HandleAiAddStateTreeState, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddStateTreeTransition, "add_state_tree_transition", McpHandlers::Ai::HandleAiAddStateTreeTransition, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureStateTreeTask, "configure_state_tree_task", McpHandlers::Ai::HandleAiConfigureStateTreeTask, EMcpCallFlags::Mutating)

// Smart Objects
MCP_AI_CALL(CreateSmartObjectDefinition, "create_smart_object_definition", McpHandlers::Ai::HandleAiCreateSmartObjectDefinition, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddSmartObjectSlot, "add_smart_object_slot", McpHandlers::Ai::HandleAiAddSmartObjectSlot, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureSlotBehavior, "configure_slot_behavior", McpHandlers::Ai::HandleAiConfigureSlotBehavior, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddSmartObjectComponent, "add_smart_object_component", McpHandlers::Ai::HandleAiAddSmartObjectComponent, EMcpCallFlags::Mutating)

// Mass AI
MCP_AI_CALL(CreateMassEntityConfig, "create_mass_entity_config", McpHandlers::Ai::HandleAiCreateMassEntityConfig, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureMassEntity, "configure_mass_entity", McpHandlers::Ai::HandleAiConfigureMassEntity, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddMassSpawner, "add_mass_spawner", McpHandlers::Ai::HandleAiAddMassSpawner, EMcpCallFlags::Mutating)

// Utility & conveniences
MCP_AI_CALL(GetAiInfo, "get_info", McpHandlers::Ai::HandleAiGetAiInfo, EMcpCallFlags::None)
MCP_AI_CALL(SetupPerception, "setup_perception", McpHandlers::Ai::HandleAiSetupPerception, EMcpCallFlags::Mutating)
MCP_AI_CALL(SetFocus, "set_focus", McpHandlers::Ai::HandleAiSetFocus, EMcpCallFlags::Mutating)
MCP_AI_CALL(ClearFocus, "clear_focus", McpHandlers::Ai::HandleAiClearFocus, EMcpCallFlags::Mutating)
MCP_AI_CALL(SetBlackboardValue, "set_blackboard_value", McpHandlers::Ai::HandleAiSetBlackboardValue, EMcpCallFlags::Mutating)
MCP_AI_CALL(GetBlackboardValue, "get_blackboard_value", McpHandlers::Ai::HandleAiGetBlackboardValue, EMcpCallFlags::None)
MCP_AI_CALL(RunBehaviorTree, "run_behavior_tree", McpHandlers::Ai::HandleAiRunBehaviorTree, EMcpCallFlags::Mutating)
MCP_AI_CALL(StopBehaviorTree, "stop_behavior_tree", McpHandlers::Ai::HandleAiStopBehaviorTree, EMcpCallFlags::Mutating)

// Behavior Tree graph (BehaviorTreeHandlers.cpp)
MCP_AI_CALL(Create, "create", McpHandlers::Ai::HandleBehaviorTreeCreate, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddNode, "add_node", McpHandlers::Ai::HandleBehaviorTreeAddNode, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConnectNodes, "connect_nodes", McpHandlers::Ai::HandleBehaviorTreeConnectNodes, EMcpCallFlags::Mutating)
MCP_AI_CALL(RemoveNode, "remove_node", McpHandlers::Ai::HandleBehaviorTreeRemoveNode, EMcpCallFlags::Mutating)
MCP_AI_CALL(BreakConnections, "break_connections", McpHandlers::Ai::HandleBehaviorTreeBreakConnections, EMcpCallFlags::Mutating)
MCP_AI_CALL(SetNodeProperties, "set_node_properties", McpHandlers::Ai::HandleBehaviorTreeSetNodeProperties, EMcpCallFlags::Mutating)
MCP_AI_CALL(AddSubnode, "add_subnode", McpHandlers::Ai::HandleBehaviorTreeAddSubnode, EMcpCallFlags::Mutating)

// Navigation (NavigationHandlers.cpp)
MCP_AI_CALL(ConfigureNavMeshSettings, "configure_nav_mesh_settings", McpHandlers::Ai::HandleNavigationConfigureNavMeshSettings, EMcpCallFlags::Mutating)
MCP_AI_CALL(SetNavAgentProperties, "set_nav_agent_properties", McpHandlers::Ai::HandleNavigationSetNavAgentProperties, EMcpCallFlags::Mutating)
MCP_AI_CALL(RebuildNavigation, "rebuild_navigation", McpHandlers::Ai::HandleNavigationRebuildNavigation, EMcpCallFlags::Mutating)
MCP_AI_CALL(CreateNavModifierComponent, "create_nav_modifier_component", McpHandlers::Ai::HandleNavigationCreateNavModifierComponent, EMcpCallFlags::Mutating)
MCP_AI_CALL(SetNavAreaClass, "set_nav_area_class", McpHandlers::Ai::HandleNavigationSetNavAreaClass, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureNavAreaCost, "configure_nav_area_cost", McpHandlers::Ai::HandleNavigationConfigureNavAreaCost, EMcpCallFlags::Mutating)
MCP_AI_CALL(CreateNavLinkProxy, "create_nav_link_proxy", McpHandlers::Ai::HandleNavigationCreateNavLinkProxy, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureNavLink, "configure_nav_link", McpHandlers::Ai::HandleNavigationConfigureNavLink, EMcpCallFlags::Mutating)
MCP_AI_CALL(SetNavLinkType, "set_nav_link_type", McpHandlers::Ai::HandleNavigationSetNavLinkType, EMcpCallFlags::Mutating)
MCP_AI_CALL(CreateSmartLink, "create_smart_link", McpHandlers::Ai::HandleNavigationCreateSmartLink, EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureSmartLinkBehavior, "configure_smart_link_behavior", McpHandlers::Ai::HandleNavigationConfigureSmartLinkBehavior, EMcpCallFlags::Mutating)
MCP_AI_CALL(GetNavigationInfo, "get_navigation_info", McpHandlers::Ai::HandleNavigationGetNavigationInfo, EMcpCallFlags::None)

#undef MCP_AI_CALL

} // namespace McpCalls::ManageAi

void McpRegisterManageAiCalls()
{
	using namespace McpCalls::ManageAi;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_CreateAiController>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AssignBehaviorTree>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AssignBlackboard>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_CreateBlackboardAsset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddBlackboardKey>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_SetKeyInstanceSynced>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_CreateBehaviorTree>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddCompositeNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddTaskNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddDecorator>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddService>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureBtNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_CreateEqsQuery>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddEqsGenerator>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddEqsContext>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddEqsTest>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureTestScoring>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddAiPerceptionComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureSightConfig>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureHearingConfig>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureDamageSenseConfig>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_SetPerceptionTeam>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_CreateStateTree>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddStateTreeState>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddStateTreeTransition>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureStateTreeTask>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_CreateSmartObjectDefinition>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddSmartObjectSlot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureSlotBehavior>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddSmartObjectComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_CreateMassEntityConfig>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureMassEntity>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddMassSpawner>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_GetAiInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_SetupPerception>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_SetFocus>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ClearFocus>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_SetBlackboardValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_GetBlackboardValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_RunBehaviorTree>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_StopBehaviorTree>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_Create>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConnectNodes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_RemoveNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_BreakConnections>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_SetNodeProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_AddSubnode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureNavMeshSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_SetNavAgentProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_RebuildNavigation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_CreateNavModifierComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_SetNavAreaClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureNavAreaCost>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_CreateNavLinkProxy>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureNavLink>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_SetNavLinkType>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_CreateSmartLink>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_ConfigureSmartLinkBehavior>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAi_GetNavigationInfo>());
}
