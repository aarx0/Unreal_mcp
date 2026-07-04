// Action declarations for manage_ai — the server's contract: which params
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
inline const FMcpParamDecl P_ManageAi_0[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAi_1[] = { { TEXT("blackboardPath"), EMcpParamKind::String, true }, { TEXT("keyName"), EMcpParamKind::String, false }, { TEXT("keyType"), EMcpParamKind::String, false }, { TEXT("baseObjectClass"), EMcpParamKind::String, false }, { TEXT("isInstanceSynced"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAi_2[] = { { TEXT("compositeType"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAi_3[] = { { TEXT("parentNodeId"), EMcpParamKind::String, true }, { TEXT("decoratorType"), EMcpParamKind::String, true }, { TEXT("nodeClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_4[] = { { TEXT("queryPath"), EMcpParamKind::String, true }, { TEXT("contextType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_5[] = { { TEXT("queryPath"), EMcpParamKind::String, true }, { TEXT("generatorType"), EMcpParamKind::String, true }, { TEXT("generatorSettings"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ManageAi_6[] = { { TEXT("queryPath"), EMcpParamKind::String, true }, { TEXT("testType"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAi_7[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("configPath"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("spawnCount"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAi_8[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("nodeType"), EMcpParamKind::String, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("parentNodeId"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::Any, false }, { TEXT("save"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ManageAi_9[] = { { TEXT("parentNodeId"), EMcpParamKind::String, true }, { TEXT("serviceType"), EMcpParamKind::String, true }, { TEXT("nodeClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_10[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("definitionPath"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_11[] = { { TEXT("definitionPath"), EMcpParamKind::String, true }, { TEXT("offset"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAi_12[] = { { TEXT("stateTreePath"), EMcpParamKind::String, true }, { TEXT("stateName"), EMcpParamKind::String, true }, { TEXT("parentStateName"), EMcpParamKind::String, false }, { TEXT("stateType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_13[] = { { TEXT("stateTreePath"), EMcpParamKind::String, true }, { TEXT("fromState"), EMcpParamKind::String, true }, { TEXT("toState"), EMcpParamKind::String, true }, { TEXT("triggerType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_14[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("parentNodeId"), EMcpParamKind::String, true }, { TEXT("subnodeType"), EMcpParamKind::String, true }, { TEXT("nodeClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAi_15[] = { { TEXT("taskType"), EMcpParamKind::String, true }, { TEXT("nodeClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_16[] = { { TEXT("controllerPath"), EMcpParamKind::String, true }, { TEXT("behaviorTreePath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAi_17[] = { { TEXT("controllerPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("blackboardPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAi_18[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("pinName"), EMcpParamKind::String, false }, { TEXT("expressionIndex"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAi_19[] = { { TEXT("controllerPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAi_20[] = { { TEXT("behaviorTreePath"), EMcpParamKind::String, true }, { TEXT("nodeId"), EMcpParamKind::String, true }, { TEXT("properties"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ManageAi_21[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("damageConfig"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ManageAi_22[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("hearingRange"), EMcpParamKind::Number, false }, { TEXT("hearingConfig"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ManageAi_23[] = { { TEXT("configPath"), EMcpParamKind::String, true }, { TEXT("parentConfigPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_24[] = { { TEXT("areaClass"), EMcpParamKind::String, true }, { TEXT("areaCost"), EMcpParamKind::Number, false }, { TEXT("fixedAreaEnteringCost"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAi_25[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("startPoint"), EMcpParamKind::Object, false }, { TEXT("endPoint"), EMcpParamKind::Object, false }, { TEXT("direction"), EMcpParamKind::String, false }, { TEXT("snapRadius"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAi_26[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("tileSizeUU"), EMcpParamKind::Number, false }, { TEXT("minRegionArea"), EMcpParamKind::Number, false }, { TEXT("mergeRegionSize"), EMcpParamKind::Number, false }, { TEXT("maxSimplificationError"), EMcpParamKind::Number, false }, { TEXT("cellSize"), EMcpParamKind::Number, false }, { TEXT("cellHeight"), EMcpParamKind::Number, false }, { TEXT("agentStepHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAi_27[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("sightRadius"), EMcpParamKind::Number, false }, { TEXT("loseSightRadius"), EMcpParamKind::Number, false }, { TEXT("peripheralVisionAngle"), EMcpParamKind::Number, false }, { TEXT("maxAge"), EMcpParamKind::Number, false }, { TEXT("autoSuccessRange"), EMcpParamKind::Number, false }, { TEXT("pointOfViewBackwardOffset"), EMcpParamKind::Number, false }, { TEXT("nearClippingRadius"), EMcpParamKind::Number, false }, { TEXT("enemies"), EMcpParamKind::Bool, false }, { TEXT("neutrals"), EMcpParamKind::Bool, false }, { TEXT("friendlies"), EMcpParamKind::Bool, false }, { TEXT("detectionByAffiliation"), EMcpParamKind::Object, false }, { TEXT("sightConfig"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ManageAi_28[] = { { TEXT("definitionPath"), EMcpParamKind::String, true }, { TEXT("slotIndex"), EMcpParamKind::Number, false }, { TEXT("behaviorType"), EMcpParamKind::String, false }, { TEXT("activityTags"), EMcpParamKind::Array, false }, { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAi_29[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("linkEnabled"), EMcpParamKind::Bool, false }, { TEXT("enabledAreaClass"), EMcpParamKind::String, false }, { TEXT("disabledAreaClass"), EMcpParamKind::String, false }, { TEXT("broadcastRadius"), EMcpParamKind::Number, false }, { TEXT("broadcastInterval"), EMcpParamKind::Number, false }, { TEXT("bCreateBoxObstacle"), EMcpParamKind::Bool, false }, { TEXT("obstacleAreaClass"), EMcpParamKind::String, false }, { TEXT("obstacleExtent"), EMcpParamKind::Object, false }, { TEXT("obstacleOffset"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ManageAi_30[] = { { TEXT("stateTreePath"), EMcpParamKind::String, true }, { TEXT("stateName"), EMcpParamKind::String, true }, { TEXT("taskType"), EMcpParamKind::String, false }, { TEXT("selectionBehavior"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_31[] = { { TEXT("queryPath"), EMcpParamKind::String, true }, { TEXT("testIndex"), EMcpParamKind::Number, false }, { TEXT("testSettings"), EMcpParamKind::Object, false }, { TEXT("scoringEquation"), EMcpParamKind::String, false }, { TEXT("filterType"), EMcpParamKind::String, false }, { TEXT("clampMin"), EMcpParamKind::Number, false }, { TEXT("clampMax"), EMcpParamKind::Number, false }, { TEXT("floatMin"), EMcpParamKind::Number, false }, { TEXT("floatMax"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAi_32[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("parentNodeId"), EMcpParamKind::String, false }, { TEXT("childNodeId"), EMcpParamKind::String, false }, { TEXT("fromExpression"), EMcpParamKind::Any, false }, { TEXT("inputName"), EMcpParamKind::Any, false }, { TEXT("save"), EMcpParamKind::Any, false }, { TEXT("sourceNodeId"), EMcpParamKind::Any, false }, { TEXT("targetNodeId"), EMcpParamKind::Any, false }, { TEXT("toExpression"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ManageAi_33[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("blackboardPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_34[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_35[] = { { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_36[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_37[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_38[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_39[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_40[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, true }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("startPoint"), EMcpParamKind::Object, true }, { TEXT("endPoint"), EMcpParamKind::Object, true }, { TEXT("direction"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_41[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("areaClass"), EMcpParamKind::String, false }, { TEXT("failsafeExtent"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAi_42[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, true }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("startPoint"), EMcpParamKind::Object, true }, { TEXT("endPoint"), EMcpParamKind::Object, true }, { TEXT("direction"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_43[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_44[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("schemaType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_45[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("controllerPath"), EMcpParamKind::String, false }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("blackboardPath"), EMcpParamKind::String, false }, { TEXT("queryPath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_46[] = { { TEXT("blackboardPath"), EMcpParamKind::String, true }, { TEXT("keyName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAi_47[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_48[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_49[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("scriptType"), EMcpParamKind::String, false }, { TEXT("expressionIndex"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAi_50[] = { { TEXT("controllerPath"), EMcpParamKind::String, true }, { TEXT("behaviorTreePath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAi_51[] = { { TEXT("blackboardPath"), EMcpParamKind::String, true }, { TEXT("keyName"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_52[] = { { TEXT("controllerPath"), EMcpParamKind::String, true }, { TEXT("focusActorName"), EMcpParamKind::String, false }, { TEXT("targetActor"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_53[] = { { TEXT("blackboardPath"), EMcpParamKind::String, true }, { TEXT("keyName"), EMcpParamKind::String, true }, { TEXT("isInstanceSynced"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAi_54[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("agentRadius"), EMcpParamKind::Number, false }, { TEXT("agentHeight"), EMcpParamKind::Number, false }, { TEXT("agentMaxSlope"), EMcpParamKind::Number, false }, { TEXT("agentStepHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAi_55[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("areaClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAi_56[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("linkType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_57[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("behaviorTreePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false }, { TEXT("comment"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ManageAi_58[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("teamId"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAi_59[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("controllerPath"), EMcpParamKind::String, false }, { TEXT("enableSight"), EMcpParamKind::Bool, false }, { TEXT("sightRadius"), EMcpParamKind::Number, false }, { TEXT("loseSightRadius"), EMcpParamKind::Number, false }, { TEXT("peripheralVisionAngle"), EMcpParamKind::Number, false }, { TEXT("enableHearing"), EMcpParamKind::Bool, false }, { TEXT("hearingRange"), EMcpParamKind::Number, false }, { TEXT("enableDamage"), EMcpParamKind::Bool, false }, { TEXT("dominantSense"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAi_60[] = { { TEXT("controllerPath"), EMcpParamKind::String, true } };

inline const FMcpCallDecl GManageAi[] =
{
	{ TEXT("manage_ai"), TEXT("add_ai_perception_component"), P_ManageAi_0, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_blackboard_key"), P_ManageAi_1, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_composite_node"), P_ManageAi_2, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_ai"), TEXT("add_decorator"), P_ManageAi_3, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_ai"), TEXT("add_eqs_context"), P_ManageAi_4, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_eqs_generator"), P_ManageAi_5, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_eqs_test"), P_ManageAi_6, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_mass_spawner"), P_ManageAi_7, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_node"), P_ManageAi_8, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_service"), P_ManageAi_9, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_ai"), TEXT("add_smart_object_component"), P_ManageAi_10, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_smart_object_slot"), P_ManageAi_11, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_state_tree_state"), P_ManageAi_12, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_state_tree_transition"), P_ManageAi_13, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_subnode"), P_ManageAi_14, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("add_task_node"), P_ManageAi_15, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_ai"), TEXT("assign_behavior_tree"), P_ManageAi_16, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("assign_blackboard"), P_ManageAi_17, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("break_connections"), P_ManageAi_18, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("clear_focus"), P_ManageAi_19, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_bt_node"), P_ManageAi_20, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_damage_sense_config"), P_ManageAi_21, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_hearing_config"), P_ManageAi_22, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_mass_entity"), P_ManageAi_23, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_nav_area_cost"), P_ManageAi_24, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_nav_link"), P_ManageAi_25, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_nav_mesh_settings"), P_ManageAi_26, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_sight_config"), P_ManageAi_27, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_slot_behavior"), P_ManageAi_28, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_smart_link_behavior"), P_ManageAi_29, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_state_tree_task"), P_ManageAi_30, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("configure_test_scoring"), P_ManageAi_31, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("connect_nodes"), P_ManageAi_32, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("create"), P_ManageAi_33, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("create_ai_controller"), P_ManageAi_34, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("create_behavior_tree"), P_ManageAi_35, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_ai"), TEXT("create_blackboard"), P_ManageAi_36, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("create_blackboard_asset"), P_ManageAi_37, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("create_eqs_query"), P_ManageAi_38, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("create_mass_entity_config"), P_ManageAi_39, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("create_nav_link_proxy"), P_ManageAi_40, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("create_nav_modifier_component"), P_ManageAi_41, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("create_smart_link"), P_ManageAi_42, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("create_smart_object_definition"), P_ManageAi_43, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("create_state_tree"), P_ManageAi_44, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("get_ai_info"), P_ManageAi_45, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("get_blackboard_value"), P_ManageAi_46, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("get_navigation_info"), P_ManageAi_47, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("rebuild_navigation"), P_ManageAi_48, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("remove_node"), P_ManageAi_49, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("run_behavior_tree"), P_ManageAi_50, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("set_blackboard_value"), P_ManageAi_51, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("set_focus"), P_ManageAi_52, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("set_key_instance_synced"), P_ManageAi_53, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("set_nav_agent_properties"), P_ManageAi_54, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("set_nav_area_class"), P_ManageAi_55, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("set_nav_link_type"), P_ManageAi_56, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("set_node_properties"), P_ManageAi_57, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("set_perception_team"), P_ManageAi_58, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("setup_perception"), P_ManageAi_59, EMcpCallFlags::None },
	{ TEXT("manage_ai"), TEXT("stop_behavior_tree"), P_ManageAi_60, EMcpCallFlags::None },
};
}
