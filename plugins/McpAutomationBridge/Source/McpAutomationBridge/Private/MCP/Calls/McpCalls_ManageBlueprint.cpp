// LINT-TOOL: manage_blueprint
// manage_blueprint as FMcpCall classes — twenty-first and final classed
// family (docs/action-declarations.md), completing the FMcpCall classing
// migration. This is the one DELEGATION-wired family: unlike the extracted
// families, its four route dispatchers cannot be retired — HandleBlueprintAction
// recurses into HandleBlueprintGraphAction/HandleSCSAction and is called
// externally by EditorFunctionHandlers.cpp, so all four survive as members.
// Each class Run() therefore delegates to its route's surviving dispatcher,
// passing EXACTLY the args the retired registration lambda passed (byte-
// behaviour-identical for canonical inputs); no branch body was extracted:
//   Core     -> HandleBlueprintAction(RequestId, "manage_blueprint", ...)
//   Graph    -> HandleBlueprintGraphAction(RequestId, "manage_blueprint", ...)
//   Widget   -> HandleManageWidgetAuthoringAction(RequestId, "manage_widget_authoring", ...)
//   CommonUi -> HandleCommonUiAction(RequestId, "manage_common_ui", ...)
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageBlueprint
{

// --- Param contracts --------------------------------------------------------
// Ported VERBATIM from this family's retired shim declarations
// (McpDecl_ManageBlueprint.h GManageBlueprint table) — the only change is the
// array name (P_ManageBlueprint_N -> P_<Action>). Zero decl fixes: the shim rows
// reconciled clean against the surviving dispatchers, which still own the reads.
// The five zero-param actions (bind_color/bind_enabled/bind_text/bind_visibility/
// create_property_binding) pass {} at the macro instead of an array.

// Core (HandleBlueprintAction)
inline const FMcpParamDecl P_Create[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false }, { TEXT("blueprintType"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("waitForCompletion"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_Get[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Compile[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("saveAfterCompile"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddComponent[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("componentType"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetDefault[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("propertyName"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::Any, true } };
inline const FMcpParamDecl P_GetDefault[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("propertyName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ListFunctions[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_ModifyScs[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("operations"), EMcpParamKind::Array, true }, { TEXT("compile"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetScs[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_AddScsComponent[] = { { TEXT("blueprint_path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("component_class"), EMcpParamKind::String, false }, { TEXT("componentClass"), EMcpParamKind::String, false }, { TEXT("component_name"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("parent_component"), EMcpParamKind::String, false }, { TEXT("parentComponent"), EMcpParamKind::String, false }, { TEXT("mesh_path"), EMcpParamKind::String, false }, { TEXT("meshPath"), EMcpParamKind::String, false }, { TEXT("material_path"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_RemoveScsComponent[] = { { TEXT("blueprint_path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("component_name"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ReparentScsComponent[] = { { TEXT("blueprint_path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("component_name"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("new_parent"), EMcpParamKind::String, false }, { TEXT("newParent"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetScsTransform[] = { { TEXT("blueprint_path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("component_name"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Any, false }, { TEXT("rotation"), EMcpParamKind::Any, false }, { TEXT("scale"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_SetScsProperty[] = { { TEXT("blueprint_path"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("component_name"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("property_name"), EMcpParamKind::String, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("property_value"), EMcpParamKind::Any, false }, { TEXT("value"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_EnsureExists[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("parentClass"), EMcpParamKind::String, false }, { TEXT("createIfMissing"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ProbeHandle[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_AddVariable[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("variableName"), EMcpParamKind::String, true }, { TEXT("variableType"), EMcpParamKind::String, false }, { TEXT("defaultValue"), EMcpParamKind::Any, false }, { TEXT("category"), EMcpParamKind::String, false }, { TEXT("isReplicated"), EMcpParamKind::Bool, false }, { TEXT("isPublic"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_RemoveVariable[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("variableName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_RenameVariable[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("oldName"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddFunction[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("functionName"), EMcpParamKind::String, false }, { TEXT("memberName"), EMcpParamKind::String, false }, { TEXT("inputs"), EMcpParamKind::Array, false }, { TEXT("outputs"), EMcpParamKind::Array, false }, { TEXT("isPublic"), EMcpParamKind::Bool, false }, { TEXT("override"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_RemoveFunction[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("functionName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddEvent[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("eventType"), EMcpParamKind::String, false }, { TEXT("customEventName"), EMcpParamKind::String, false }, { TEXT("eventName"), EMcpParamKind::String, false }, { TEXT("parameters"), EMcpParamKind::Array, false }, { TEXT("posX"), EMcpParamKind::Number, false }, { TEXT("posY"), EMcpParamKind::Number, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_RemoveEvent[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("eventName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddConstructionScript[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_SetVariableMetadata[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("variableName"), EMcpParamKind::String, true }, { TEXT("metadata"), EMcpParamKind::Object, true } };
inline const FMcpParamDecl P_SetMetadata[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("metadata"), EMcpParamKind::Object, true } };
inline const FMcpParamDecl P_AddNode[] = { { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("nodeType"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("functionName"), EMcpParamKind::String, false }, { TEXT("variableName"), EMcpParamKind::String, false }, { TEXT("nodeName"), EMcpParamKind::String, false }, { TEXT("posX"), EMcpParamKind::Number, false }, { TEXT("posY"), EMcpParamKind::Number, false }, { TEXT("memberName"), EMcpParamKind::String, false }, { TEXT("autoConnect"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Any, false }, { TEXT("x"), EMcpParamKind::Any, false }, { TEXT("y"), EMcpParamKind::Any, false } };

// BlueprintGraph (HandleBlueprintGraphAction)
inline const FMcpParamDecl P_CreateNode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("nodeType"), EMcpParamKind::String, true }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("posX"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("posY"), EMcpParamKind::Number, false }, { TEXT("variableName"), EMcpParamKind::String, false }, { TEXT("memberClass"), EMcpParamKind::String, false }, { TEXT("memberName"), EMcpParamKind::String, false }, { TEXT("eventName"), EMcpParamKind::String, false }, { TEXT("parameters"), EMcpParamKind::Array, false }, { TEXT("targetClass"), EMcpParamKind::String, false }, { TEXT("inputAxisName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_DeleteNode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConnectPins[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("sourceNodeId"), EMcpParamKind::String, true }, { TEXT("sourcePinName"), EMcpParamKind::String, true }, { TEXT("targetNodeId"), EMcpParamKind::String, true }, { TEXT("targetPinName"), EMcpParamKind::String, true }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("scriptType"), EMcpParamKind::String, false }, { TEXT("autoConnect"), EMcpParamKind::Bool, false }, { TEXT("inputName"), EMcpParamKind::String, false }, { TEXT("fromExpression"), EMcpParamKind::Any, false }, { TEXT("toExpression"), EMcpParamKind::Any, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("requestedPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("blueprintCandidates"), EMcpParamKind::Array, false }, { TEXT("candidates"), EMcpParamKind::Array, false }, { TEXT("sourceNodeGuid"), EMcpParamKind::String, true }, { TEXT("targetNodeGuid"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_BreakPinLinks[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, true }, { TEXT("pinName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetNodeProperty[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_CreateRerouteNode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("posX"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("posY"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_GetNodeDetails[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, true }, { TEXT("expressionIndex"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_GetGraphDetails[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("includePinLinks"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetPinDetails[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, true }, { TEXT("pinName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ListNodeTypes[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetPinDefaultValue[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, true }, { TEXT("pinName"), EMcpParamKind::String, true }, { TEXT("value"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ArrangeGraph[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ListAnimbpGraphs[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetTransitionRuleGraph[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("graphName"), EMcpParamKind::String, false }, { TEXT("fromState"), EMcpParamKind::String, false }, { TEXT("toState"), EMcpParamKind::String, false }, { TEXT("transitionName"), EMcpParamKind::String, false }, { TEXT("stateMachine"), EMcpParamKind::String, false }, { TEXT("transitionIndex"), EMcpParamKind::Number, false } };

// WidgetAuthoring (HandleManageWidgetAuthoringAction)
inline const FMcpParamDecl P_CreateWidgetBlueprint[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("parentClass"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetWidgetParentClass[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("parentClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddCanvasPanel[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddHorizontalBox[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddVerticalBox[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddOverlay[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddGridPanel[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddUniformGrid[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("slotPadding"), EMcpParamKind::Object, false }, { TEXT("minDesiredSlotWidth"), EMcpParamKind::Number, false }, { TEXT("minDesiredSlotHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddWrapBox[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("innerSlotPadding"), EMcpParamKind::Object, false }, { TEXT("wrapSize"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddScrollBox[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("orientation"), EMcpParamKind::String, false }, { TEXT("scrollBarVisibility"), EMcpParamKind::String, false }, { TEXT("alwaysShowScrollbar"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddSizeBox[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("widthOverride"), EMcpParamKind::Number, false }, { TEXT("heightOverride"), EMcpParamKind::Number, false }, { TEXT("minDesiredWidth"), EMcpParamKind::Number, false }, { TEXT("minDesiredHeight"), EMcpParamKind::Number, false }, { TEXT("maxDesiredWidth"), EMcpParamKind::Number, false }, { TEXT("maxDesiredHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddScaleBox[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("stretch"), EMcpParamKind::String, false }, { TEXT("userSpecifiedScale"), EMcpParamKind::Number, false }, { TEXT("stretchDirection"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddBorder[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("brushColor"), EMcpParamKind::Object, false }, { TEXT("contentColorAndOpacity"), EMcpParamKind::Object, false }, { TEXT("padding"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_AddTextBlock[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("text"), EMcpParamKind::String, false }, { TEXT("fontSize"), EMcpParamKind::Number, false }, { TEXT("colorAndOpacity"), EMcpParamKind::Object, false }, { TEXT("autoWrap"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddRichTextBlock[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("text"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddImage[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("texturePath"), EMcpParamKind::String, false }, { TEXT("colorAndOpacity"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_AddButton[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("isEnabled"), EMcpParamKind::Bool, false }, { TEXT("colorAndOpacity"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_AddCheckBox[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("isChecked"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddSlider[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("value"), EMcpParamKind::Number, false }, { TEXT("minValue"), EMcpParamKind::Number, false }, { TEXT("maxValue"), EMcpParamKind::Number, false }, { TEXT("stepSize"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddProgressBar[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("percent"), EMcpParamKind::Number, false }, { TEXT("fillColorAndOpacity"), EMcpParamKind::Object, false }, { TEXT("isMarquee"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddTextInput[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("multiLine"), EMcpParamKind::Bool, false }, { TEXT("hintText"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddComboBox[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("options"), EMcpParamKind::Array, false }, { TEXT("selectedOption"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddSpinBox[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("value"), EMcpParamKind::Number, false }, { TEXT("minValue"), EMcpParamKind::Number, false }, { TEXT("maxValue"), EMcpParamKind::Number, false }, { TEXT("delta"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddListView[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddTreeView[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetAnchor[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("anchorMin"), EMcpParamKind::Object, false }, { TEXT("anchorMax"), EMcpParamKind::Object, false }, { TEXT("anchorMinX"), EMcpParamKind::Number, false }, { TEXT("anchorMinY"), EMcpParamKind::Number, false }, { TEXT("anchorMaxX"), EMcpParamKind::Number, false }, { TEXT("anchorMaxY"), EMcpParamKind::Number, false }, { TEXT("preset"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetAlignment[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("alignment"), EMcpParamKind::Object, false }, { TEXT("alignmentX"), EMcpParamKind::Number, false }, { TEXT("alignmentY"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetPosition[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("position"), EMcpParamKind::Object, false }, { TEXT("posX"), EMcpParamKind::Number, false }, { TEXT("posY"), EMcpParamKind::Number, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetSize[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("size"), EMcpParamKind::Object, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("fill"), EMcpParamKind::Bool, false }, { TEXT("fillWeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetPadding[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("padding"), EMcpParamKind::Object, false }, { TEXT("pad"), EMcpParamKind::Number, false }, { TEXT("padLeft"), EMcpParamKind::Number, false }, { TEXT("padTop"), EMcpParamKind::Number, false }, { TEXT("padRight"), EMcpParamKind::Number, false }, { TEXT("padBottom"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetZOrder[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("zOrder"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetRenderTransform[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("translation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("shear"), EMcpParamKind::Object, false }, { TEXT("angle"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetVisibility[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("visibility"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetStyle[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, false }, { TEXT("style"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_SetClipping[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("clipping"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BindOnClicked[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("delegateName"), EMcpParamKind::String, false }, { TEXT("targetFunction"), EMcpParamKind::String, false }, { TEXT("functionName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BindOnHovered[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("delegateName"), EMcpParamKind::String, false }, { TEXT("targetFunction"), EMcpParamKind::String, false }, { TEXT("functionName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BindOnValueChanged[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("delegateName"), EMcpParamKind::String, false }, { TEXT("targetText"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BindEventToDelegate[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("eventName"), EMcpParamKind::String, true }, { TEXT("ownerPin"), EMcpParamKind::String, true }, { TEXT("delegateName"), EMcpParamKind::String, true }, { TEXT("handlerEventName"), EMcpParamKind::String, false }, { TEXT("handlerTargetWidget"), EMcpParamKind::String, false }, { TEXT("handlerFunction"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateWidgetAnimation[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("animationName"), EMcpParamKind::String, false }, { TEXT("duration"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddAnimationTrack[] = { { TEXT("sequencePath"), EMcpParamKind::String, true }, { TEXT("bindingGuid"), EMcpParamKind::String, true }, { TEXT("animSequencePath"), EMcpParamKind::String, true }, { TEXT("startTime"), EMcpParamKind::Number, false }, { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("animationName"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("propertyName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddAnimationKeyframe[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("animationName"), EMcpParamKind::String, true }, { TEXT("time"), EMcpParamKind::Number, false }, { TEXT("value"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetAnimationLoop[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("animationName"), EMcpParamKind::String, true }, { TEXT("loop"), EMcpParamKind::Bool, false }, { TEXT("loopCount"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateMainMenu[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("title"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreatePauseMenu[] = { { TEXT("widgetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateSettingsMenu[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("folder"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateLoadingScreen[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("folder"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateHudWidget[] = { { TEXT("widgetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddHealthBar[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("parentName"), EMcpParamKind::String, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("height"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddAmmoCounter[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("parentName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddMinimap[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("size"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddCrosshair[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("parentName"), EMcpParamKind::String, false }, { TEXT("size"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddCompass[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddInteractionPrompt[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("text"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddObjectiveTracker[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddDamageIndicator[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateInventoryUi[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("columns"), EMcpParamKind::Number, false }, { TEXT("rows"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateDialogWidget[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("folder"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateRadialMenu[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("segments"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_GetWidgetInfo[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("widgetName"), EMcpParamKind::String, false }, { TEXT("propertyNames"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_PreviewWidget[] = { { TEXT("widgetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_RemoveWidget[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ReparentWidget[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("newParent"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_RenameWidget[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("oldName"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("newName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_GetWidgetSlotInfo[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ReplaceWidgetClass[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("newWidgetClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddWidget[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("widgetClass"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("parentSlot"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_WrapRoot[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("panelType"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ShowWidget[] = { { TEXT("widgetPath"), EMcpParamKind::String, false }, { TEXT("widgetId"), EMcpParamKind::String, false }, { TEXT("message"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddWidgetComponent[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("componentType"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("parentName"), EMcpParamKind::String, false }, { TEXT("positionX"), EMcpParamKind::Number, false }, { TEXT("positionY"), EMcpParamKind::Number, false }, { TEXT("sizeX"), EMcpParamKind::Number, false }, { TEXT("sizeY"), EMcpParamKind::Number, false }, { TEXT("text"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddSafeZone[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddSpacer[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("sizeX"), EMcpParamKind::Number, false }, { TEXT("sizeY"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddWidgetSwitcher[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("activeIndex"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetFont[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("font"), EMcpParamKind::String, false }, { TEXT("fontSize"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetMargin[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("left"), EMcpParamKind::Number, false }, { TEXT("top"), EMcpParamKind::Number, false }, { TEXT("right"), EMcpParamKind::Number, false }, { TEXT("bottom"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateWidgetStyle[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("styleName"), EMcpParamKind::String, false }, { TEXT("styleType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ApplyStyleToWidget[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("styleName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetWidgetBinding[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("targetWidget"), EMcpParamKind::String, true }, { TEXT("property"), EMcpParamKind::String, true }, { TEXT("functionName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BindLocalizedText[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("stringTableId"), EMcpParamKind::String, true }, { TEXT("stringKey"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetLocalizationKey[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, true }, { TEXT("namespace"), EMcpParamKind::String, false }, { TEXT("key"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_GetAnimationInfo[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("animationName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetAnimationSpeed[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("animationName"), EMcpParamKind::String, true }, { TEXT("speed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_DeleteAnimation[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("animationName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateCreditsScreen[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("folder"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateShopUi[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("columns"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddQuestTracker[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false } };

// CommonUi (HandleCommonUiAction)
inline const FMcpParamDecl P_AddCommonButton[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("buttonClass"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("styleClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddCommonText[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("text"), EMcpParamKind::String, false }, { TEXT("styleClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddCommonBorder[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("slotName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("parentSlot"), EMcpParamKind::String, false }, { TEXT("styleClass"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetCommonButtonStyle[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("widgetName"), EMcpParamKind::String, true }, { TEXT("styleClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetCommonTextStyle[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("widgetName"), EMcpParamKind::String, true }, { TEXT("styleClass"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetCommonButtonInputAction[] = { { TEXT("widgetPath"), EMcpParamKind::String, true }, { TEXT("widgetName"), EMcpParamKind::String, true }, { TEXT("dataTable"), EMcpParamKind::String, true }, { TEXT("rowName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateCommonButtonStyle[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateCommonTextStyle[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };

// --- Classes ----------------------------------------------------------------
// Flags authored per action. RequiresEditor on Core + Graph: BlueprintHandlers.cpp
// (HandleBlueprintAction, whole-body #if WITH_EDITOR) and BlueprintGraphHandlers.cpp
// (HandleBlueprintGraphAction, whole-body #if WITH_EDITOR) are editor-gated. NOT on
// Widget (WidgetAuthoringHandlers.cpp carries no editor gate) or CommonUi
// (CommonUIHandlers.cpp gates on MCP_HAS_COMMON_UI, not WITH_EDITOR) — flagging
// either would newly reject the GEditor-less runs the shim served (networking/
// asset-Texture precedent). Mutating on every action except the pure
// get_*/list_*/preview_*/probe_* reads.
//
// MCP_BP_ACTION_CALL delegates to a surviving 4-arg dispatcher, passing the route's
// Action-arg literal (the manufactured tool-name for Widget/CommonUi, the
// "manage_blueprint" tool name for Core/Graph — each dispatcher gates on it and
// reads the real sub-action from the payload). No body is owned here.

#define MCP_BP_ACTION_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ActionArg, Flags) \
class FMcpCall_ManageBlueprint_##ClassSuffix final : public FMcpCall                     \
{                                                                                        \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl Decl{ TEXT("manage_blueprint"), TEXT(ActionLiteral),      \
			ParamsArray, (Flags) };                                                          \
		return Decl;                                                                       \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, TEXT(ActionArg), Payload, Socket);                   \
	}                                                                                    \
};

// Core (HandleBlueprintAction, Action arg "manage_blueprint")
MCP_BP_ACTION_CALL(Create, "create", P_Create, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(Get, "get", P_Get, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(Compile, "compile", P_Compile, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddComponent, "add_component", P_AddComponent, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetDefault, "set_default", P_SetDefault, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetDefault, "get_default", P_GetDefault, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(ListFunctions, "list_functions", P_ListFunctions, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(ModifyScs, "modify_scs", P_ModifyScs, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetScs, "get_scs", P_GetScs, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(AddScsComponent, "add_scs_component", P_AddScsComponent, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RemoveScsComponent, "remove_scs_component", P_RemoveScsComponent, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ReparentScsComponent, "reparent_scs_component", P_ReparentScsComponent, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetScsTransform, "set_scs_transform", P_SetScsTransform, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetScsProperty, "set_scs_property", P_SetScsProperty, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(EnsureExists, "ensure_exists", P_EnsureExists, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ProbeHandle, "probe_handle", P_ProbeHandle, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(AddVariable, "add_variable", P_AddVariable, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RemoveVariable, "remove_variable", P_RemoveVariable, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RenameVariable, "rename_variable", P_RenameVariable, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddFunction, "add_function", P_AddFunction, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RemoveFunction, "remove_function", P_RemoveFunction, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddEvent, "add_event", P_AddEvent, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RemoveEvent, "remove_event", P_RemoveEvent, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddConstructionScript, "add_construction_script", P_AddConstructionScript, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetVariableMetadata, "set_variable_metadata", P_SetVariableMetadata, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetMetadata, "set_metadata", P_SetMetadata, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddNode, "add_node", P_AddNode, HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// BlueprintGraph (HandleBlueprintGraphAction, Action arg "manage_blueprint")
MCP_BP_ACTION_CALL(CreateNode, "create_node", P_CreateNode, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(DeleteNode, "delete_node", P_DeleteNode, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ConnectPins, "connect_pins", P_ConnectPins, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BreakPinLinks, "break_pin_links", P_BreakPinLinks, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetNodeProperty, "set_node_property", P_SetNodeProperty, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateRerouteNode, "create_reroute_node", P_CreateRerouteNode, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetNodeDetails, "get_node_details", P_GetNodeDetails, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(GetGraphDetails, "get_graph_details", P_GetGraphDetails, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(GetPinDetails, "get_pin_details", P_GetPinDetails, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(ListNodeTypes, "list_node_types", P_ListNodeTypes, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(SetPinDefaultValue, "set_pin_default_value", P_SetPinDefaultValue, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ArrangeGraph, "arrange_graph", P_ArrangeGraph, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ListAnimbpGraphs, "list_animbp_graphs", P_ListAnimbpGraphs, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(GetTransitionRuleGraph, "get_transition_rule_graph", P_GetTransitionRuleGraph, HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)

// WidgetAuthoring (HandleManageWidgetAuthoringAction, Action arg "manage_widget_authoring")
MCP_BP_ACTION_CALL(CreateWidgetBlueprint, "create_widget_blueprint", P_CreateWidgetBlueprint, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetWidgetParentClass, "set_widget_parent_class", P_SetWidgetParentClass, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCanvasPanel, "add_canvas_panel", P_AddCanvasPanel, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddHorizontalBox, "add_horizontal_box", P_AddHorizontalBox, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddVerticalBox, "add_vertical_box", P_AddVerticalBox, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddOverlay, "add_overlay", P_AddOverlay, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddGridPanel, "add_grid_panel", P_AddGridPanel, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddUniformGrid, "add_uniform_grid", P_AddUniformGrid, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddWrapBox, "add_wrap_box", P_AddWrapBox, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddScrollBox, "add_scroll_box", P_AddScrollBox, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddSizeBox, "add_size_box", P_AddSizeBox, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddScaleBox, "add_scale_box", P_AddScaleBox, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddBorder, "add_border", P_AddBorder, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddTextBlock, "add_text_block", P_AddTextBlock, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddRichTextBlock, "add_rich_text_block", P_AddRichTextBlock, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddImage, "add_image", P_AddImage, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddButton, "add_button", P_AddButton, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCheckBox, "add_check_box", P_AddCheckBox, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddSlider, "add_slider", P_AddSlider, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddProgressBar, "add_progress_bar", P_AddProgressBar, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddTextInput, "add_text_input", P_AddTextInput, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddComboBox, "add_combo_box", P_AddComboBox, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddSpinBox, "add_spin_box", P_AddSpinBox, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddListView, "add_list_view", P_AddListView, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddTreeView, "add_tree_view", P_AddTreeView, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetAnchor, "set_anchor", P_SetAnchor, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetAlignment, "set_alignment", P_SetAlignment, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetPosition, "set_position", P_SetPosition, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetSize, "set_size", P_SetSize, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetPadding, "set_padding", P_SetPadding, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetZOrder, "set_z_order", P_SetZOrder, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetRenderTransform, "set_render_transform", P_SetRenderTransform, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetVisibility, "set_visibility", P_SetVisibility, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetStyle, "set_style", P_SetStyle, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetClipping, "set_clipping", P_SetClipping, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreatePropertyBinding, "create_property_binding", {}, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindText, "bind_text", {}, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindVisibility, "bind_visibility", {}, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindColor, "bind_color", {}, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindEnabled, "bind_enabled", {}, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindOnClicked, "bind_on_clicked", P_BindOnClicked, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindOnHovered, "bind_on_hovered", P_BindOnHovered, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindOnValueChanged, "bind_on_value_changed", P_BindOnValueChanged, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindEventToDelegate, "bind_event_to_delegate", P_BindEventToDelegate, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateWidgetAnimation, "create_widget_animation", P_CreateWidgetAnimation, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddAnimationTrack, "add_animation_track", P_AddAnimationTrack, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddAnimationKeyframe, "add_animation_keyframe", P_AddAnimationKeyframe, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetAnimationLoop, "set_animation_loop", P_SetAnimationLoop, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateMainMenu, "create_main_menu", P_CreateMainMenu, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreatePauseMenu, "create_pause_menu", P_CreatePauseMenu, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateSettingsMenu, "create_settings_menu", P_CreateSettingsMenu, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateLoadingScreen, "create_loading_screen", P_CreateLoadingScreen, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateHudWidget, "create_hud_widget", P_CreateHudWidget, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddHealthBar, "add_health_bar", P_AddHealthBar, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddAmmoCounter, "add_ammo_counter", P_AddAmmoCounter, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddMinimap, "add_minimap", P_AddMinimap, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCrosshair, "add_crosshair", P_AddCrosshair, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCompass, "add_compass", P_AddCompass, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddInteractionPrompt, "add_interaction_prompt", P_AddInteractionPrompt, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddObjectiveTracker, "add_objective_tracker", P_AddObjectiveTracker, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddDamageIndicator, "add_damage_indicator", P_AddDamageIndicator, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateInventoryUi, "create_inventory_ui", P_CreateInventoryUi, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateDialogWidget, "create_dialog_widget", P_CreateDialogWidget, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateRadialMenu, "create_radial_menu", P_CreateRadialMenu, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetWidgetInfo, "get_widget_info", P_GetWidgetInfo, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::None)
MCP_BP_ACTION_CALL(PreviewWidget, "preview_widget", P_PreviewWidget, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::None)
MCP_BP_ACTION_CALL(RemoveWidget, "remove_widget", P_RemoveWidget, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ReparentWidget, "reparent_widget", P_ReparentWidget, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RenameWidget, "rename_widget", P_RenameWidget, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetWidgetSlotInfo, "get_widget_slot_info", P_GetWidgetSlotInfo, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::None)
MCP_BP_ACTION_CALL(ReplaceWidgetClass, "replace_widget_class", P_ReplaceWidgetClass, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddWidget, "add_widget", P_AddWidget, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(WrapRoot, "wrap_root", P_WrapRoot, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ShowWidget, "show_widget", P_ShowWidget, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddWidgetComponent, "add_widget_component", P_AddWidgetComponent, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddSafeZone, "add_safe_zone", P_AddSafeZone, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddSpacer, "add_spacer", P_AddSpacer, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddWidgetSwitcher, "add_widget_switcher", P_AddWidgetSwitcher, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetFont, "set_font", P_SetFont, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetMargin, "set_margin", P_SetMargin, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateWidgetStyle, "create_widget_style", P_CreateWidgetStyle, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ApplyStyleToWidget, "apply_style_to_widget", P_ApplyStyleToWidget, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetWidgetBinding, "set_widget_binding", P_SetWidgetBinding, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindLocalizedText, "bind_localized_text", P_BindLocalizedText, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetLocalizationKey, "set_localization_key", P_SetLocalizationKey, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetAnimationInfo, "get_animation_info", P_GetAnimationInfo, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::None)
MCP_BP_ACTION_CALL(SetAnimationSpeed, "set_animation_speed", P_SetAnimationSpeed, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(DeleteAnimation, "delete_animation", P_DeleteAnimation, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateCreditsScreen, "create_credits_screen", P_CreateCreditsScreen, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateShopUi, "create_shop_ui", P_CreateShopUi, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddQuestTracker, "add_quest_tracker", P_AddQuestTracker, HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)

// CommonUi (HandleCommonUiAction, Action arg "manage_common_ui")
MCP_BP_ACTION_CALL(AddCommonButton, "add_common_button", P_AddCommonButton, HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCommonText, "add_common_text", P_AddCommonText, HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCommonBorder, "add_common_border", P_AddCommonBorder, HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetCommonButtonStyle, "set_common_button_style", P_SetCommonButtonStyle, HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetCommonTextStyle, "set_common_text_style", P_SetCommonTextStyle, HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetCommonButtonInputAction, "set_common_button_input_action", P_SetCommonButtonInputAction, HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateCommonButtonStyle, "create_common_button_style", P_CreateCommonButtonStyle, HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateCommonTextStyle, "create_common_text_style", P_CreateCommonTextStyle, HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)

#undef MCP_BP_ACTION_CALL

} // namespace McpCalls::ManageBlueprint

void McpRegisterManageBlueprintCalls()
{
	using namespace McpCalls::ManageBlueprint;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_Create>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_Get>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_Compile>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetDefault>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetDefault>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ListFunctions>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ModifyScs>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetScs>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddScsComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RemoveScsComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ReparentScsComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetScsTransform>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetScsProperty>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_EnsureExists>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ProbeHandle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddVariable>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RemoveVariable>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RenameVariable>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddFunction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RemoveFunction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddEvent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RemoveEvent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddConstructionScript>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetVariableMetadata>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetMetadata>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_DeleteNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ConnectPins>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BreakPinLinks>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetNodeProperty>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateRerouteNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetNodeDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetGraphDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetPinDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ListNodeTypes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetPinDefaultValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ArrangeGraph>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ListAnimbpGraphs>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetTransitionRuleGraph>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateWidgetBlueprint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetWidgetParentClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCanvasPanel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddHorizontalBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddVerticalBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddOverlay>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddGridPanel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddUniformGrid>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddWrapBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddScrollBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddSizeBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddScaleBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddBorder>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddTextBlock>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddRichTextBlock>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddImage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddButton>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCheckBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddSlider>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddProgressBar>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddTextInput>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddComboBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddSpinBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddListView>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddTreeView>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetAnchor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetAlignment>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetPosition>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetSize>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetPadding>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetZOrder>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetRenderTransform>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetVisibility>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetStyle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetClipping>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreatePropertyBinding>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindText>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindVisibility>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindColor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindEnabled>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindOnClicked>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindOnHovered>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindOnValueChanged>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindEventToDelegate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateWidgetAnimation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddAnimationTrack>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddAnimationKeyframe>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetAnimationLoop>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateMainMenu>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreatePauseMenu>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateSettingsMenu>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateLoadingScreen>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateHudWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddHealthBar>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddAmmoCounter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddMinimap>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCrosshair>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCompass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddInteractionPrompt>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddObjectiveTracker>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddDamageIndicator>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateInventoryUi>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateDialogWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateRadialMenu>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetWidgetInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_PreviewWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RemoveWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ReparentWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RenameWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetWidgetSlotInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ReplaceWidgetClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_WrapRoot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ShowWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddWidgetComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddSafeZone>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddSpacer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddWidgetSwitcher>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetFont>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetMargin>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateWidgetStyle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ApplyStyleToWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetWidgetBinding>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindLocalizedText>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetLocalizationKey>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetAnimationInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetAnimationSpeed>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_DeleteAnimation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateCreditsScreen>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateShopUi>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddQuestTracker>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCommonButton>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCommonText>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCommonBorder>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetCommonButtonStyle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetCommonTextStyle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetCommonButtonInputAction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateCommonButtonStyle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateCommonTextStyle>());
}
