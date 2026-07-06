// LINT-TOOL: manage_ai
// manage_ai as FMcpCall classes — sixteenth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers — HandleAi* (AIHandlers.cpp) for the 42 core actions,
// HandleBehaviorTree* (BehaviorTreeHandlers.cpp) for the 7 Behavior Tree
// graph actions, HandleNavigation* (NavigationHandlers.cpp) for the 12
// navigation actions — until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageAi
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations (McpDecl_ManageAi.h)
// and re-verified against the member bodies. Seven rows shipped with decl
// fixes at classing. The six Behavior Tree graph rows (add_node, add_subnode,
// break_connections, connect_nodes, remove_node, set_node_properties)
// declared assetPath required, but the retired dispatcher resolves it through
// the behaviorTreePath/path fallbacks and joint-rejects only when all three
// are absent ("Missing 'assetPath' (or 'behaviorTreePath'/'path')"), so
// requiring assetPath false-rejected every fallback-spelling payload —
// optional now with the at-least-one requirement handler-enforced.
// create_nav_link_proxy dropped blueprintPath/name/path: those spellings
// were read only by the dispatcher's shadowed inline copy (deleted at this
// classing — the Navigation router always won the name), and the live
// NavigationHandlers body reads actorName/location/rotation/startPoint/
// endPoint/direction, so the row now matches create_smart_link's. Its
// location/startPoint/endPoint required flags STAY — the body rejects when
// any of the three is missing (the scoping sweep's one-of suspicion did not
// survive the body evidence; create_smart_link's row is unchanged for the
// same reason).

inline const FMcpParamDecl P_CreateAiController[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AssignBehaviorTree[] = { { TEXT("controllerPath"), EMcpParamKind::String, true }, { TEXT("behaviorTreePath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AssignBlackboard[] = { { TEXT("controllerPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("blackboardPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateBlackboardAsset[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddBlackboardKey[] = { { TEXT("blackboardPath"), EMcpParamKind::String, true }, { TEXT("keyName"), EMcpParamKind::String, false }, { TEXT("keyType"), EMcpParamKind::String, false }, { TEXT("baseObjectClass"), EMcpParamKind::String, false }, { TEXT("isInstanceSynced"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetKeyInstanceSynced[] = { { TEXT("blackboardPath"), EMcpParamKind::String, true }, { TEXT("keyName"), EMcpParamKind::String, true }, { TEXT("isInstanceSynced"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateBehaviorTree[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blackboardPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddCompositeNode[] = { { TEXT("compositeType"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("parentNodeId"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddTaskNode[] = { { TEXT("taskType"), EMcpParamKind::String, true }, { TEXT("nodeClass"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("parentNodeId"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddDecorator[] = { { TEXT("parentNodeId"), EMcpParamKind::String, true }, { TEXT("decoratorType"), EMcpParamKind::String, true }, { TEXT("nodeClass"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddService[] = { { TEXT("parentNodeId"), EMcpParamKind::String, true }, { TEXT("serviceType"), EMcpParamKind::String, true }, { TEXT("nodeClass"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureBtNode[] = { { TEXT("behaviorTreePath"), EMcpParamKind::String, true }, { TEXT("nodeId"), EMcpParamKind::String, true }, { TEXT("properties"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateEqsQuery[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddEqsGenerator[] = { { TEXT("queryPath"), EMcpParamKind::String, true }, { TEXT("generatorType"), EMcpParamKind::String, true }, { TEXT("generatorSettings"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_AddEqsContext[] = { { TEXT("queryPath"), EMcpParamKind::String, true }, { TEXT("contextType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddEqsTest[] = { { TEXT("queryPath"), EMcpParamKind::String, true }, { TEXT("testType"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureTestScoring[] = { { TEXT("queryPath"), EMcpParamKind::String, true }, { TEXT("testIndex"), EMcpParamKind::Number, false }, { TEXT("testSettings"), EMcpParamKind::Object, false }, { TEXT("scoringEquation"), EMcpParamKind::String, false }, { TEXT("filterType"), EMcpParamKind::String, false }, { TEXT("clampMin"), EMcpParamKind::Number, false }, { TEXT("clampMax"), EMcpParamKind::Number, false }, { TEXT("floatMin"), EMcpParamKind::Number, false }, { TEXT("floatMax"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddAiPerceptionComponent[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureSightConfig[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("sightRadius"), EMcpParamKind::Number, false }, { TEXT("loseSightRadius"), EMcpParamKind::Number, false }, { TEXT("peripheralVisionAngle"), EMcpParamKind::Number, false }, { TEXT("maxAge"), EMcpParamKind::Number, false }, { TEXT("autoSuccessRange"), EMcpParamKind::Number, false }, { TEXT("pointOfViewBackwardOffset"), EMcpParamKind::Number, false }, { TEXT("nearClippingRadius"), EMcpParamKind::Number, false }, { TEXT("enemies"), EMcpParamKind::Bool, false }, { TEXT("neutrals"), EMcpParamKind::Bool, false }, { TEXT("friendlies"), EMcpParamKind::Bool, false }, { TEXT("detectionByAffiliation"), EMcpParamKind::Object, false }, { TEXT("sightConfig"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ConfigureHearingConfig[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("hearingRange"), EMcpParamKind::Number, false }, { TEXT("hearingConfig"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ConfigureDamageSenseConfig[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("damageConfig"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_SetPerceptionTeam[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("teamId"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateStateTree[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("schemaType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddStateTreeState[] = { { TEXT("stateTreePath"), EMcpParamKind::String, true }, { TEXT("stateName"), EMcpParamKind::String, true }, { TEXT("parentStateName"), EMcpParamKind::String, false }, { TEXT("stateType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddStateTreeTransition[] = { { TEXT("stateTreePath"), EMcpParamKind::String, true }, { TEXT("fromState"), EMcpParamKind::String, true }, { TEXT("toState"), EMcpParamKind::String, true }, { TEXT("triggerType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureStateTreeTask[] = { { TEXT("stateTreePath"), EMcpParamKind::String, true }, { TEXT("stateName"), EMcpParamKind::String, true }, { TEXT("taskType"), EMcpParamKind::String, false }, { TEXT("selectionBehavior"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateSmartObjectDefinition[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddSmartObjectSlot[] = { { TEXT("definitionPath"), EMcpParamKind::String, true }, { TEXT("offset"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureSlotBehavior[] = { { TEXT("definitionPath"), EMcpParamKind::String, true }, { TEXT("slotIndex"), EMcpParamKind::Number, false }, { TEXT("behaviorType"), EMcpParamKind::String, false }, { TEXT("activityTags"), EMcpParamKind::Array, false }, { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddSmartObjectComponent[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("definitionPath"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateMassEntityConfig[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureMassEntity[] = { { TEXT("configPath"), EMcpParamKind::String, true }, { TEXT("parentConfigPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddMassSpawner[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("configPath"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("spawnCount"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_GetAiInfo[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("controllerPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("blackboardPath"), EMcpParamKind::String, false }, { TEXT("queryPath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetupPerception[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("controllerPath"), EMcpParamKind::String, false }, { TEXT("enableSight"), EMcpParamKind::Bool, false }, { TEXT("sightRadius"), EMcpParamKind::Number, false }, { TEXT("loseSightRadius"), EMcpParamKind::Number, false }, { TEXT("peripheralVisionAngle"), EMcpParamKind::Number, false }, { TEXT("enableHearing"), EMcpParamKind::Bool, false }, { TEXT("hearingRange"), EMcpParamKind::Number, false }, { TEXT("enableDamage"), EMcpParamKind::Bool, false }, { TEXT("dominantSense"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetFocus[] = { { TEXT("controllerPath"), EMcpParamKind::String, true }, { TEXT("focusActorName"), EMcpParamKind::String, false }, { TEXT("targetActor"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ClearFocus[] = { { TEXT("controllerPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetBlackboardValue[] = { { TEXT("blackboardPath"), EMcpParamKind::String, true }, { TEXT("keyName"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetBlackboardValue[] = { { TEXT("blackboardPath"), EMcpParamKind::String, true }, { TEXT("keyName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_RunBehaviorTree[] = { { TEXT("controllerPath"), EMcpParamKind::String, true }, { TEXT("behaviorTreePath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_StopBehaviorTree[] = { { TEXT("controllerPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Create[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blackboardPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddNode[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("nodeType"), EMcpParamKind::String, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("parentNodeId"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::Any, false }, { TEXT("save"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ConnectNodes[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("parentNodeId"), EMcpParamKind::String, false }, { TEXT("childNodeId"), EMcpParamKind::String, false }, { TEXT("fromExpression"), EMcpParamKind::Any, false }, { TEXT("inputName"), EMcpParamKind::Any, false }, { TEXT("save"), EMcpParamKind::Any, false }, { TEXT("sourceNodeId"), EMcpParamKind::Any, false }, { TEXT("targetNodeId"), EMcpParamKind::Any, false }, { TEXT("toExpression"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_RemoveNode[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("scriptType"), EMcpParamKind::String, false }, { TEXT("expressionIndex"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BreakConnections[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("pinName"), EMcpParamKind::String, false }, { TEXT("expressionIndex"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetNodeProperties[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("comment"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_AddSubnode[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("parentNodeId"), EMcpParamKind::String, true }, { TEXT("subnodeType"), EMcpParamKind::String, true }, { TEXT("nodeClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureNavMeshSettings[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("tileSizeUU"), EMcpParamKind::Number, false }, { TEXT("minRegionArea"), EMcpParamKind::Number, false }, { TEXT("mergeRegionSize"), EMcpParamKind::Number, false }, { TEXT("maxSimplificationError"), EMcpParamKind::Number, false }, { TEXT("cellSize"), EMcpParamKind::Number, false }, { TEXT("cellHeight"), EMcpParamKind::Number, false }, { TEXT("agentStepHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetNavAgentProperties[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("agentRadius"), EMcpParamKind::Number, false }, { TEXT("agentHeight"), EMcpParamKind::Number, false }, { TEXT("agentMaxSlope"), EMcpParamKind::Number, false }, { TEXT("agentStepHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_RebuildNavigation[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateNavModifierComponent[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("areaClass"), EMcpParamKind::String, false }, { TEXT("failsafeExtent"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetNavAreaClass[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("areaClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureNavAreaCost[] = { { TEXT("areaClass"), EMcpParamKind::String, true }, { TEXT("areaCost"), EMcpParamKind::Number, false }, { TEXT("fixedAreaEnteringCost"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateNavLinkProxy[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, true }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("startPoint"), EMcpParamKind::Object, true }, { TEXT("endPoint"), EMcpParamKind::Object, true }, { TEXT("direction"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureNavLink[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("startPoint"), EMcpParamKind::Object, false }, { TEXT("endPoint"), EMcpParamKind::Object, false }, { TEXT("direction"), EMcpParamKind::String, false }, { TEXT("snapRadius"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetNavLinkType[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("linkType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateSmartLink[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, true }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("startPoint"), EMcpParamKind::Object, true }, { TEXT("endPoint"), EMcpParamKind::Object, true }, { TEXT("direction"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureSmartLinkBehavior[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("linkEnabled"), EMcpParamKind::Bool, false }, { TEXT("enabledAreaClass"), EMcpParamKind::String, false }, { TEXT("disabledAreaClass"), EMcpParamKind::String, false }, { TEXT("broadcastRadius"), EMcpParamKind::Number, false }, { TEXT("broadcastInterval"), EMcpParamKind::Number, false }, { TEXT("bCreateBoxObstacle"), EMcpParamKind::Bool, false }, { TEXT("obstacleAreaClass"), EMcpParamKind::String, false }, { TEXT("obstacleExtent"), EMcpParamKind::Object, false }, { TEXT("obstacleOffset"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_GetNavigationInfo[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor on all 61: the three retired dispatchers were whole-editor-
// gated and every member answers its chain's EDITOR_ONLY stub in non-editor
// builds. Mutating on the writers; the readers are get_ai_info,
// get_blackboard_value, and get_navigation_info — add_eqs_context is also
// unflagged (its body is an honest NOT_IMPLEMENTED error that writes
// nothing).

#define MCP_AI_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, Flags)           \
class FMcpCall_ManageAi_##ClassSuffix final : public FMcpCall                            \
{                                                                                        \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl Decl{ TEXT("manage_ai"), TEXT(ActionLiteral),          \
			ParamsArray, (Flags) };                                                      \
		return Decl;                                                                     \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

// AI Controllers & Blackboard assets (AIHandlers.cpp)
MCP_AI_CALL(CreateAiController, "create_ai_controller", P_CreateAiController, HandleAiCreateAiController, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AssignBehaviorTree, "assign_behavior_tree", P_AssignBehaviorTree, HandleAiAssignBehaviorTree, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AssignBlackboard, "assign_blackboard", P_AssignBlackboard, HandleAiAssignBlackboard, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(CreateBlackboardAsset, "create_blackboard_asset", P_CreateBlackboardAsset, HandleAiCreateBlackboardAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddBlackboardKey, "add_blackboard_key", P_AddBlackboardKey, HandleAiAddBlackboardKey, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(SetKeyInstanceSynced, "set_key_instance_synced", P_SetKeyInstanceSynced, HandleAiSetKeyInstanceSynced, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Behavior Tree conveniences (AIHandlers.cpp wrappers over the graph members)
MCP_AI_CALL(CreateBehaviorTree, "create_behavior_tree", P_CreateBehaviorTree, HandleAiCreateBehaviorTree, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddCompositeNode, "add_composite_node", P_AddCompositeNode, HandleAiAddCompositeNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddTaskNode, "add_task_node", P_AddTaskNode, HandleAiAddTaskNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddDecorator, "add_decorator", P_AddDecorator, HandleAiAddDecorator, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddService, "add_service", P_AddService, HandleAiAddService, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureBtNode, "configure_bt_node", P_ConfigureBtNode, HandleAiConfigureBtNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// EQS
MCP_AI_CALL(CreateEqsQuery, "create_eqs_query", P_CreateEqsQuery, HandleAiCreateEqsQuery, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddEqsGenerator, "add_eqs_generator", P_AddEqsGenerator, HandleAiAddEqsGenerator, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddEqsContext, "add_eqs_context", P_AddEqsContext, HandleAiAddEqsContext, EMcpCallFlags::RequiresEditor)
MCP_AI_CALL(AddEqsTest, "add_eqs_test", P_AddEqsTest, HandleAiAddEqsTest, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureTestScoring, "configure_test_scoring", P_ConfigureTestScoring, HandleAiConfigureTestScoring, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Perception
MCP_AI_CALL(AddAiPerceptionComponent, "add_ai_perception_component", P_AddAiPerceptionComponent, HandleAiAddAiPerceptionComponent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureSightConfig, "configure_sight_config", P_ConfigureSightConfig, HandleAiConfigureSightConfig, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureHearingConfig, "configure_hearing_config", P_ConfigureHearingConfig, HandleAiConfigureHearingConfig, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureDamageSenseConfig, "configure_damage_sense_config", P_ConfigureDamageSenseConfig, HandleAiConfigureDamageSenseConfig, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(SetPerceptionTeam, "set_perception_team", P_SetPerceptionTeam, HandleAiSetPerceptionTeam, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// State Trees (UE 5.3+)
MCP_AI_CALL(CreateStateTree, "create_state_tree", P_CreateStateTree, HandleAiCreateStateTree, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddStateTreeState, "add_state_tree_state", P_AddStateTreeState, HandleAiAddStateTreeState, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddStateTreeTransition, "add_state_tree_transition", P_AddStateTreeTransition, HandleAiAddStateTreeTransition, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureStateTreeTask, "configure_state_tree_task", P_ConfigureStateTreeTask, HandleAiConfigureStateTreeTask, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Smart Objects
MCP_AI_CALL(CreateSmartObjectDefinition, "create_smart_object_definition", P_CreateSmartObjectDefinition, HandleAiCreateSmartObjectDefinition, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddSmartObjectSlot, "add_smart_object_slot", P_AddSmartObjectSlot, HandleAiAddSmartObjectSlot, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureSlotBehavior, "configure_slot_behavior", P_ConfigureSlotBehavior, HandleAiConfigureSlotBehavior, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddSmartObjectComponent, "add_smart_object_component", P_AddSmartObjectComponent, HandleAiAddSmartObjectComponent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Mass AI
MCP_AI_CALL(CreateMassEntityConfig, "create_mass_entity_config", P_CreateMassEntityConfig, HandleAiCreateMassEntityConfig, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureMassEntity, "configure_mass_entity", P_ConfigureMassEntity, HandleAiConfigureMassEntity, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddMassSpawner, "add_mass_spawner", P_AddMassSpawner, HandleAiAddMassSpawner, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Utility & conveniences
MCP_AI_CALL(GetAiInfo, "get_ai_info", P_GetAiInfo, HandleAiGetAiInfo, EMcpCallFlags::RequiresEditor)
MCP_AI_CALL(SetupPerception, "setup_perception", P_SetupPerception, HandleAiSetupPerception, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(SetFocus, "set_focus", P_SetFocus, HandleAiSetFocus, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ClearFocus, "clear_focus", P_ClearFocus, HandleAiClearFocus, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(SetBlackboardValue, "set_blackboard_value", P_SetBlackboardValue, HandleAiSetBlackboardValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(GetBlackboardValue, "get_blackboard_value", P_GetBlackboardValue, HandleAiGetBlackboardValue, EMcpCallFlags::RequiresEditor)
MCP_AI_CALL(RunBehaviorTree, "run_behavior_tree", P_RunBehaviorTree, HandleAiRunBehaviorTree, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(StopBehaviorTree, "stop_behavior_tree", P_StopBehaviorTree, HandleAiStopBehaviorTree, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Behavior Tree graph (BehaviorTreeHandlers.cpp)
MCP_AI_CALL(Create, "create", P_Create, HandleBehaviorTreeCreate, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddNode, "add_node", P_AddNode, HandleBehaviorTreeAddNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConnectNodes, "connect_nodes", P_ConnectNodes, HandleBehaviorTreeConnectNodes, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(RemoveNode, "remove_node", P_RemoveNode, HandleBehaviorTreeRemoveNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(BreakConnections, "break_connections", P_BreakConnections, HandleBehaviorTreeBreakConnections, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(SetNodeProperties, "set_node_properties", P_SetNodeProperties, HandleBehaviorTreeSetNodeProperties, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(AddSubnode, "add_subnode", P_AddSubnode, HandleBehaviorTreeAddSubnode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Navigation (NavigationHandlers.cpp)
MCP_AI_CALL(ConfigureNavMeshSettings, "configure_nav_mesh_settings", P_ConfigureNavMeshSettings, HandleNavigationConfigureNavMeshSettings, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(SetNavAgentProperties, "set_nav_agent_properties", P_SetNavAgentProperties, HandleNavigationSetNavAgentProperties, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(RebuildNavigation, "rebuild_navigation", P_RebuildNavigation, HandleNavigationRebuildNavigation, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(CreateNavModifierComponent, "create_nav_modifier_component", P_CreateNavModifierComponent, HandleNavigationCreateNavModifierComponent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(SetNavAreaClass, "set_nav_area_class", P_SetNavAreaClass, HandleNavigationSetNavAreaClass, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureNavAreaCost, "configure_nav_area_cost", P_ConfigureNavAreaCost, HandleNavigationConfigureNavAreaCost, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(CreateNavLinkProxy, "create_nav_link_proxy", P_CreateNavLinkProxy, HandleNavigationCreateNavLinkProxy, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureNavLink, "configure_nav_link", P_ConfigureNavLink, HandleNavigationConfigureNavLink, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(SetNavLinkType, "set_nav_link_type", P_SetNavLinkType, HandleNavigationSetNavLinkType, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(CreateSmartLink, "create_smart_link", P_CreateSmartLink, HandleNavigationCreateSmartLink, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(ConfigureSmartLinkBehavior, "configure_smart_link_behavior", P_ConfigureSmartLinkBehavior, HandleNavigationConfigureSmartLinkBehavior, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_AI_CALL(GetNavigationInfo, "get_navigation_info", P_GetNavigationInfo, HandleNavigationGetNavigationInfo, EMcpCallFlags::RequiresEditor)

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
