// =============================================================================
// McpAutomationBridge_BlueprintGraphHandlers.cpp
// =============================================================================
// Blueprint Graph Manipulation Handlers
//
// Implements node creation, connection, and graph inspection for Blueprint graphs.
//
// HANDLERS IMPLEMENTED (13 subActions):
// ================================
//
// NODE OPERATIONS:
//   - create_node         : Create K2Node of specified type at position
//   - delete_node         : Remove a node from the graph
//   - create_reroute_node : Create K2Node_Knot (reroute node)
//
// PIN OPERATIONS:
//   - connect_pins        : Link two pins together
//   - break_pin_links     : Disconnect all links from a pin
//   - set_pin_default_value : Set default value for an input pin
//
// INSPECTION:
//   - get_nodes          : List all nodes in a graph
//   - get_node_details   : Get detailed info about a specific node
//   - get_graph_details  : Get graph metadata (name, node count, etc.)
//   - get_pin_details    : Get pin information (type, connections, value)
//   - list_node_types    : List all available K2Node subclasses (global)
//
// PROPERTY:
//   - set_node_property  : Set a property on a node (comment text, etc.)
//
// SUPPORTED NODE TYPES (partial list):
//   - K2Node_CallFunction, K2Node_VariableGet, K2Node_VariableSet
//   - K2Node_IfThenElse, K2Node_ExecutionSequence, K2Node_Knot
//   - K2Node_CustomEvent, K2Node_Event, K2Node_Literal
//   - K2Node_MakeArray, K2Node_MakeStruct, K2Node_BreakStruct
//   - K2Node_DynamicCast, K2Node_Select, K2Node_Timeline
//   - And all other registered UK2Node subclasses
//
// VERSION COMPATIBILITY:
//   - UE 5.0-5.7: All handlers supported
//   - K2Node header locations vary by version (handled via __has_include)
//   - Uses ScopedTransaction for undo/redo support
//
// Copyright (c) 2025 MCP Automation Bridge Contributors
// SPDX-License-Identifier: MIT
// =============================================================================

// Include version compatibility FIRST
#include "McpVersionCompatibility.h"

#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpGraphLayoutUtils.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
// Graph Framework
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"   // PC_Exec — classify exec vs pure/data pins for arrange_graph
#include "EdGraphNode_Comment.h"

// Blueprint Core
#include "Engine/Blueprint.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

// K2Node Types
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_InputAxisEvent.h"
#include "K2Node_Knot.h"
#include "K2Node_Literal.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_Select.h"
#include "K2Node_Self.h"
#include "K2Node_Timeline.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

// Anim graph readback (list_animbp_graphs / get_transition_rule_graph)
#include "AnimationGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateGraph.h"
#include "AnimationTransitionGraph.h"
#include "AnimationCustomTransitionGraph.h"
#include "AnimStateNodeBase.h"
#include "AnimStateTransitionNode.h"

// Blueprint Editor
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"

#endif

/**
 * Process a canonical "manage_blueprint" graph request to inspect or modify a
 * Blueprint graph.
 *
 * The Payload JSON controls the specific operation via the "subAction" field
 * (examples: create_node, connect_pins, get_nodes, break_pin_links,
 * delete_node, create_reroute_node, set_node_property, get_node_details,
 * get_graph_details, get_pin_details). In editor builds this function performs
 * graph/blueprint lookups and edits; in non-editor builds it reports an
 * editor-only error.
 *
 * @param RequestId Unique identifier for the automation request (used in
 * responses).
 * @param Action The requested action name; this handler only processes
 * "manage_blueprint".
 * @param Payload JSON object containing action options such as
 * "assetPath"/"blueprintPath", "graphName", "subAction" and subaction-specific
 * fields (nodeType, nodeId, pin names, positions, etc.).
 * @param RequestingSocket WebSocket used to send responses and errors back to
 * the requester.
 * @return `true` if the request was handled by this function, `false` otherwise.
 */
#if WITH_EDITOR
// =============================================================================
// arrange_graph — Blueprint graph auto-layout (see docs/graph-auto-layout.md)
// =============================================================================
// Sugiyama-flavoured layout specialised for Blueprint graphs. Dependency-free,
// no crossing-minimisation ("fine, not perfect" — a throwaway authoring aid):
//   X = longest-path columns over exec wires + data wires (so a consumer sits
//       right of both its exec predecessor and its data feeders); lone pure
//       getters are then nudged to just-left-of-their-consumer.
//   Y = a single non-resetting row cursor with parent-centering (Reingold-
//       Tilford): each leaf takes the next row, each parent centers on its
//       children (pure feeders + exec successors), so feeders stack on adjacent
//       rows and independent event trees stack below one another for free.
//   Placement is size-aware: columns advance by the widest node in each column,
//   and a per-column collision pass pushes nodes down until vertical extents
//   clear — parent-centering produces unconstrained averages that can land
//   exactly on another node's row, so a post-pass is required, not optional.
namespace
{
constexpr int32 ArrangeRowStepY = 180; // target row spacing (px) for the Y cursor
constexpr float ArrangeGapX = 96.f;    // horizontal gap between columns
constexpr float ArrangeGapY = 48.f;    // minimum vertical clearance within a column

// Headless node-extent estimate (no Slate geometry exists for unopened graphs).
// Height: the engine's canonical estimator (exists for exactly this reason —
// "we don't know the actual size of the node until the next Slate tick").
// Width: title length at ~7 px/char (Bold 10pt), floored at the engine's own
// K2 node-width guess of 224 (BlueprintFunctionNodeSpawner's EstimatedVarNodeWidth).
FVector2D ArrangeEstimateNodeSize(UEdGraphNode* Node)
{
    const float Height = UEdGraphSchema_K2::EstimateNodeHeight(Node);
    const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
    const float Width = FMath::Max(224.f, 32.f + 7.f * Title.Len());
    return FVector2D(Width, Height);
}

bool ArrangePinIsExec(const UEdGraphPin* Pin)
{
    return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}

bool ArrangeNodeIsPure(const UEdGraphNode* Node)
{
    if (!Node) { return true; }
    for (const UEdGraphPin* P : Node->Pins)
    {
        if (ArrangePinIsExec(P)) { return false; }
    }
    return true; // no exec pins at all => pure/data node
}

// Which nodes actually contribute to executed behavior. Two passes:
//   1. Exec reachability — BFS from true entry nodes (an exec output and *no*
//      exec input pin: events, custom/input events, function & macro entries)
//      along exec wires. The distinction that matters: an exec node whose exec
//      input merely happens to be *unconnected* is NOT an entry, so a Push/Set
//      node left off the chain stays unreached. linkCount/isOrphan can't catch
//      that — they count the node's data + "then" wires and read as connected.
//   2. Data liveness — a pure node is live iff it feeds (transitively, through
//      other pure nodes) a live node; a getter wired only into a dead exec node
//      is itself dead. An unreached exec node is never resurrected this way: a
//      node that doesn't run can't make its data feeders matter.
// OutLive is the union. Connectivity only — enable-state is reported separately.
void ComputeGraphLiveness(const UEdGraph* Graph,
                          TSet<const UEdGraphNode*>& OutExecReachable,
                          TSet<const UEdGraphNode*>& OutLive)
{
    OutExecReachable.Reset();
    OutLive.Reset();
    if (!Graph) { return; }

    TArray<const UEdGraphNode*> Queue; // exec BFS frontier (index-walked)
    for (const UEdGraphNode* N : Graph->Nodes)
    {
        if (!N) { continue; }
        bool bHasExecOut = false;
        bool bHasExecInPin = false;
        for (const UEdGraphPin* P : N->Pins)
        {
            if (!ArrangePinIsExec(P)) { continue; }
            if (P->Direction == EGPD_Output) { bHasExecOut = true; }
            else if (P->Direction == EGPD_Input) { bHasExecInPin = true; }
        }
        if (bHasExecOut && !bHasExecInPin) // entry: emits exec, consumes none
        {
            OutExecReachable.Add(N);
            Queue.Add(N);
        }
    }
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        for (const UEdGraphPin* P : Queue[Head]->Pins)
        {
            if (!ArrangePinIsExec(P) || P->Direction != EGPD_Output) { continue; }
            for (const UEdGraphPin* L : P->LinkedTo)
            {
                const UEdGraphNode* M = L ? L->GetOwningNode() : nullptr;
                if (M && !OutExecReachable.Contains(M))
                {
                    OutExecReachable.Add(M);
                    Queue.Add(M);
                }
            }
        }
    }

    OutLive = OutExecReachable;
    bool bChanged = true;
    while (bChanged) // backward data-liveness fixpoint over pure nodes
    {
        bChanged = false;
        for (const UEdGraphNode* N : Graph->Nodes)
        {
            if (!N || OutLive.Contains(N) || !ArrangeNodeIsPure(N)) { continue; }
            bool bFeedsLive = false;
            for (const UEdGraphPin* P : N->Pins)
            {
                if (!P || P->Direction != EGPD_Output) { continue; }
                for (const UEdGraphPin* L : P->LinkedTo)
                {
                    const UEdGraphNode* M = L ? L->GetOwningNode() : nullptr;
                    if (M && OutLive.Contains(M)) { bFeedsLive = true; break; }
                }
                if (bFeedsLive) { break; }
            }
            if (bFeedsLive) { OutLive.Add(N); bChanged = true; }
        }
    }
}

// Shared node dump for get_graph_details and get_transition_rule_graph — one
// copy, or the two drift. bIncludePinLinks adds per-node pins with defaults
// and "<nodeGuid>:<pinName>" link refs; rule graphs are small enough that
// get_transition_rule_graph always opts in.
static TSharedPtr<FJsonObject> BuildGraphDetailsJson(UEdGraph* Graph, bool bIncludePinLinks)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (!Graph) { return Result; }
    Result->SetStringField(TEXT("graphName"), Graph->GetName());
    Result->SetNumberField(TEXT("nodeCount"), Graph->Nodes.Num());

    TSet<const UEdGraphNode*> ExecReachable;
    TSet<const UEdGraphNode*> LiveNodes;
    ComputeGraphLiveness(Graph, ExecReachable, LiveNodes);

    // Pose-dataflow graphs (AnimGraph and state-machine inner graphs) have no
    // exec pins anywhere, so exec-reachability is meaningless — it used to
    // flag every anim node as dead. Detect and skip liveness for those.
    bool bGraphHasExecPins = false;
    for (const UEdGraphNode* N : Graph->Nodes)
    {
        if (!N) { continue; }
        for (const UEdGraphPin* P : N->Pins)
        {
            if (ArrangePinIsExec(P)) { bGraphHasExecPins = true; break; }
        }
        if (bGraphHasExecPins) { break; }
    }

    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node) { continue; }
        TSharedPtr<FJsonObject> NodeObj = McpHandlerUtils::CreateResultObject();
        NodeObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
        NodeObj->SetStringField(TEXT("nodeName"), Node->GetName());
        NodeObj->SetStringField(
            TEXT("nodeTitle"),
            Node->GetNodeTitle(ENodeTitleType::ListView).ToString());

        // Hygiene-pass fields: position (overlap detection), enabled state
        // (disabled-node clutter), link count (orphan detection), exec
        // reachability (dead-node detection) — so a graph review is one call
        // instead of N get_node_details round-trips.
        NodeObj->SetNumberField(TEXT("x"), Node->NodePosX);
        NodeObj->SetNumberField(TEXT("y"), Node->NodePosY);

        const TCHAR* EnabledStateName = nullptr;
        switch (Node->GetDesiredEnabledState())
        {
        case ENodeEnabledState::Enabled:
            EnabledStateName = TEXT("Enabled");
            break;
        case ENodeEnabledState::Disabled:
            EnabledStateName = TEXT("Disabled");
            break;
        case ENodeEnabledState::DevelopmentOnly:
            EnabledStateName = TEXT("DevelopmentOnly");
            break;
        default:
            EnabledStateName = TEXT("Unknown");
            break;
        }
        NodeObj->SetStringField(TEXT("enabledState"), EnabledStateName);

        int32 LinkCount = 0;
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin) { LinkCount += Pin->LinkedTo.Num(); }
        }
        NodeObj->SetNumberField(TEXT("linkCount"), LinkCount);
        NodeObj->SetNumberField(TEXT("pinCount"), Node->Pins.Num());
        // Pin-bearing node with zero links = orphan. Pinless nodes (comments)
        // are not flagged — they are connected by placement, not pins.
        NodeObj->SetBoolField(TEXT("isOrphan"),
                              Node->Pins.Num() > 0 && LinkCount == 0);
        // execReachable: exec node reachable from an entry along exec wires, or a
        // pure node feeding one. False = the node never runs (see deadNodes).
        // Dataflow graphs evaluate on demand — everything counts as reachable.
        NodeObj->SetBoolField(TEXT("execReachable"),
                              !bGraphHasExecPins || LiveNodes.Contains(Node));

        if (bIncludePinLinks)
        {
            TArray<TSharedPtr<FJsonValue>> Pins;
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin) { continue; }
                TSharedPtr<FJsonObject> PinObj = McpHandlerUtils::CreateResultObject();
                PinObj->SetStringField(TEXT("pinName"), Pin->PinName.ToString());
                PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input
                                                              ? TEXT("Input")
                                                              : TEXT("Output"));
                PinObj->SetStringField(TEXT("pinType"),
                                       Pin->PinType.PinCategory.ToString());
                if (Pin->LinkedTo.Num() > 0)
                {
                    TArray<TSharedPtr<FJsonValue>> Links;
                    for (const UEdGraphPin* Linked : Pin->LinkedTo)
                    {
                        if (!Linked) { continue; }
                        const UEdGraphNode* Owner = Linked->GetOwningNode();
                        Links.Add(MakeShared<FJsonValueString>(FString::Printf(
                            TEXT("%s:%s"),
                            Owner ? *Owner->NodeGuid.ToString() : TEXT("?"),
                            *Linked->PinName.ToString())));
                    }
                    PinObj->SetArrayField(TEXT("linkedTo"), Links);
                }
                else if (!Pin->DefaultValue.IsEmpty())
                {
                    PinObj->SetStringField(TEXT("defaultValue"), Pin->DefaultValue);
                }
                Pins.Add(MakeShared<FJsonValueObject>(PinObj));
            }
            NodeObj->SetArrayField(TEXT("pins"), Pins);
        }

        Nodes.Add(MakeShared<FJsonValueObject>(NodeObj));
    }
    Result->SetArrayField(TEXT("nodes"), Nodes);

    // Actionable dead-node summary: pin-bearing nodes that never run. Comments
    // and other pinless nodes are connected by placement, not exec, so skip.
    if (!bGraphHasExecPins)
    {
        Result->SetStringField(
            TEXT("livenessNote"),
            TEXT("dataflow graph (no exec pins) — exec-liveness not applicable"));
    }
    TArray<TSharedPtr<FJsonValue>> DeadNodes;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!bGraphHasExecPins || !Node || Node->Pins.Num() == 0 ||
            LiveNodes.Contains(Node))
        {
            continue;
        }
        TSharedPtr<FJsonObject> DeadObj = McpHandlerUtils::CreateResultObject();
        DeadObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
        DeadObj->SetStringField(
            TEXT("nodeTitle"),
            Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
        DeadObj->SetStringField(TEXT("reason"),
                                ArrangeNodeIsPure(Node)
                                    ? TEXT("feeds-nothing-live")
                                    : TEXT("exec-unreachable"));
        DeadNodes.Add(MakeShared<FJsonValueObject>(DeadObj));
    }
    Result->SetNumberField(TEXT("deadNodeCount"), DeadNodes.Num());
    Result->SetArrayField(TEXT("deadNodes"), DeadNodes);

    return Result;
}

// Best-effort graph kind for list_animbp_graphs. graphClass rides alongside in
// the response, so an unclassified graph is still identifiable verbatim.
static FString ClassifyBlueprintGraph(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (Cast<UAnimationStateMachineGraph>(Graph)) { return TEXT("StateMachine"); }
    if (Cast<UAnimationCustomTransitionGraph>(Graph)) { return TEXT("CustomTransitionBlend"); }
    if (Cast<UAnimationTransitionGraph>(Graph))
    {
        // Conduits reuse the transition graph class; only the schema differs.
        if (Graph->Schema && Graph->Schema->GetName() == TEXT("AnimationConduitGraphSchema"))
        {
            return TEXT("Conduit");
        }
        return TEXT("TransitionRule");
    }
    if (Cast<UAnimationStateGraph>(Graph)) { return TEXT("State"); }
    if (Cast<UAnimationGraph>(Graph)) { return TEXT("AnimGraph"); }
    for (const TObjectPtr<UEdGraph>& G : Blueprint->UbergraphPages)
    {
        if (G == Graph) { return TEXT("EventGraph"); }
    }
    for (const TObjectPtr<UEdGraph>& G : Blueprint->FunctionGraphs)
    {
        if (G == Graph) { return TEXT("Function"); }
    }
    for (const TObjectPtr<UEdGraph>& G : Blueprint->MacroGraphs)
    {
        if (G == Graph) { return TEXT("Macro"); }
    }
    for (const TObjectPtr<UEdGraph>& G : Blueprint->DelegateSignatureGraphs)
    {
        if (G == Graph) { return TEXT("DelegateSignature"); }
    }
    return Graph->GetClass()->GetName();
}

// Lay out every node in Graph; returns the count repositioned.
// ArrangeColumn / ArrangeRow and the placement algorithm now live in the shared,
// UE-agnostic core (McpGraphLayoutUtils). This adapter only translates the K2
// exec/data pin topology into McpGraphLayout::FLayoutEdge / FLayoutNode, runs
// the core, then writes the integral positions back onto the EdGraph nodes.
int32 ArrangeBlueprintGraph(UEdGraph* Graph)
{
    if (!Graph) { return 0; }

    // Call-local dense id map, in Graph->Nodes order (see resolution: preserve
    // Graph->Nodes order rather than sorting by NodeGuid).
    TArray<UEdGraphNode*> IdToNode;
    TMap<UEdGraphNode*, McpGraphLayout::FNodeId> NodeToId;
    for (UEdGraphNode* N : Graph->Nodes)
    {
        if (!N) { continue; }
        NodeToId.Add(N, IdToNode.Num());
        IdToNode.Add(N);
    }
    if (IdToNode.Num() == 0) { return 0; }

    McpGraphLayout::FMcpGraphLayoutInput In;
    In.Orientation = { McpGraphLayout::ELayoutAxis::Horizontal, McpGraphLayout::ERootRole::Source };
    In.RowStep  = (float)ArrangeRowStepY;
    In.GapMajor = ArrangeGapX;
    In.GapMinor = ArrangeGapY;
    In.Nodes.Reserve(IdToNode.Num());
    for (McpGraphLayout::FNodeId Id = 0; Id < IdToNode.Num(); ++Id)
    {
        UEdGraphNode* N = IdToNode[Id];
        McpGraphLayout::FLayoutNode LN;
        LN.Id    = Id;
        LN.Size  = ArrangeEstimateNodeSize(N);
        LN.Label = N->GetNodeTitle(ENodeTitleType::ListView).ToString();
        In.Nodes.Add(LN);
    }

    // Pin-walk: emit FLayoutEdge (From=upstream, To=downstream) + roots.
    //   exec output -> successor : centering=true (exec successor).
    //   data input  <- feeder    : centering=ArrangeNodeIsPure(feeder) (pure
    //                              feeders are Y-children; non-pure data sources
    //                              still push N right but don't center).
    //   roots (!bPure && no connected exec input) -> AdditionalRoots.
    for (McpGraphLayout::FNodeId Id = 0; Id < IdToNode.Num(); ++Id)
    {
        UEdGraphNode* N = IdToNode[Id];
        const bool bPure = ArrangeNodeIsPure(N);
        bool bHasExecInput = false;
        for (UEdGraphPin* P : N->Pins)
        {
            if (!P) { continue; }
            const bool bExec = ArrangePinIsExec(P);
            if (bExec && P->Direction == EGPD_Output)
            {
                for (UEdGraphPin* L : P->LinkedTo)
                {
                    UEdGraphNode* M = L ? L->GetOwningNode() : nullptr;
                    if (M && M != N)
                    {
                        const McpGraphLayout::FNodeId* MId = NodeToId.Find(M);
                        if (MId)
                        {
                            McpGraphLayout::FLayoutEdge E;
                            E.FromId = Id; E.ToId = *MId;
                            E.bContributesToRowCentering = true;   // exec successor
                            In.Edges.Add(E);
                        }
                    }
                }
            }
            else if (bExec && P->Direction == EGPD_Input)
            {
                for (UEdGraphPin* L : P->LinkedTo)
                {
                    if (L && L->GetOwningNode()) { bHasExecInput = true; }
                }
            }
            else if (!bExec && P->Direction == EGPD_Input)
            {
                for (UEdGraphPin* L : P->LinkedTo)
                {
                    UEdGraphNode* F = L ? L->GetOwningNode() : nullptr;
                    if (F && F != N)
                    {
                        const McpGraphLayout::FNodeId* FId = NodeToId.Find(F);
                        if (FId)
                        {
                            McpGraphLayout::FLayoutEdge E;
                            E.FromId = *FId; E.ToId = Id;          // feeder pushes N right (X)
                            E.bContributesToRowCentering = ArrangeNodeIsPure(F);
                            E.bCenterParentIsTo = true;            // ...but N centers over F (Y), matching original
                            In.Edges.Add(E);
                        }
                    }
                }
            }
        }
        if (!bPure && !bHasExecInput) { In.AdditionalRoots.Add(Id); }
    }

    const McpGraphLayout::FMcpGraphLayoutResult R = McpGraphLayout::LayoutGraph(In);

    // Mutation loop (editor-only): write the core's integral positions back.
    int32 Count = 0;
    for (const TPair<McpGraphLayout::FNodeId, FVector2D>& Pair : R.Positions)
    {
        if (!IdToNode.IsValidIndex(Pair.Key)) { continue; }
        UEdGraphNode* N = IdToNode[Pair.Key];
        if (!N) { continue; }
        N->Modify();
        N->NodePosX = FMath::RoundToInt(Pair.Value.X);
        N->NodePosY = FMath::RoundToInt(Pair.Value.Y);
        ++Count;
    }
    return Count;
}
} // namespace
#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleBlueprintGraphAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  if (Action != TEXT("manage_blueprint")) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Missing payload for blueprint graph action."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Extract subAction early to handle actions that don't require a blueprint
  const FString EarlySubAction = GetJsonStringField(Payload, TEXT("subAction"));

  // SECURITY: Validate any provided path even for actions that don't require a blueprint
  // This prevents false negatives in security tests where malicious paths should still be rejected
  {
    FString AssetPathParam;
    FString BlueprintPathParam;
    
    if (Payload->TryGetStringField(TEXT("assetPath"), AssetPathParam) && !AssetPathParam.IsEmpty()) {
      FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPathParam);
      if (SanitizedAssetPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Invalid assetPath: contains traversal sequences or invalid characters."),
                            TEXT("INVALID_PATH"));
        return true;
      }
    }
    
    if (Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPathParam) && !BlueprintPathParam.IsEmpty()) {
      FString SanitizedBlueprintPath = SanitizeProjectRelativePath(BlueprintPathParam);
      if (SanitizedBlueprintPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Invalid blueprintPath: contains traversal sequences or invalid characters."),
                            TEXT("INVALID_PATH"));
        return true;
      }
    }
  }

  // Special case: list_node_types doesn't require a blueprint - it lists all UK2Node types globally
  if (EarlySubAction == TEXT("list_node_types")) {
    TArray<TSharedPtr<FJsonValue>> NodeTypes;
    for (TObjectIterator<UClass> It; It; ++It) {
      if (!It->IsChildOf(UK2Node::StaticClass()))
        continue;
      if (It->HasAnyClassFlags(CLASS_Abstract))
        continue;

      TSharedPtr<FJsonObject> TypeObj = McpHandlerUtils::CreateResultObject();
      TypeObj->SetStringField(TEXT("className"), It->GetName());
      TypeObj->SetStringField(TEXT("displayName"),
                              It->GetDisplayNameText().ToString());
      NodeTypes.Add(MakeShared<FJsonValueObject>(TypeObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("nodeTypes"), NodeTypes);
    Result->SetNumberField(TEXT("count"), NodeTypes.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Node types listed."), Result);
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.IsEmpty()) {
    // Allow callers to use "blueprintPath" (as exposed by the consolidated
    // tool schema) as an alias for assetPath so tests and tools do not need
    // to duplicate the same value under two keys.
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (!BlueprintPath.IsEmpty()) {
      AssetPath = BlueprintPath;
    }
  }
  
  // SECURITY: Sanitize the path before loading
  FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SanitizedAssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Invalid asset path: contains traversal sequences or invalid characters."),
                        TEXT("INVALID_PATH"));
    return true;
  }
  AssetPath = SanitizedAssetPath;

  if (AssetPath.IsEmpty()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Missing 'assetPath' or 'blueprintPath' in payload."),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // CRITICAL FIX: Use LoadBlueprintAsset instead of LoadObject to properly
  // find in-memory blueprints first. This prevents reloading stale versions
  // from disk when the blueprint has been modified in memory (e.g., after
  // create_node adds nodes that haven't been saved to disk yet).
  FString NormalizedPath;
  FString LoadError;
  UBlueprint *Blueprint = LoadBlueprintAsset(AssetPath, NormalizedPath, LoadError);
  if (!Blueprint) {
    SendAutomationError(
        RequestingSocket, RequestId,
        LoadError.IsEmpty()
            ? FString::Printf(TEXT("Could not load blueprint at path: %s"),
                              *AssetPath)
            : LoadError,
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString GraphName;
  Payload->TryGetStringField(TEXT("graphName"), GraphName);
  UEdGraph *TargetGraph = nullptr;

  // Find the target graph
  if (GraphName.IsEmpty() ||
      GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase)) {
    // Default to the main ubergraph/event graph
    if (Blueprint->UbergraphPages.Num() > 0) {
      TargetGraph = Blueprint->UbergraphPages[0];
    }
  } else {
    // Search in FunctionGraphs and UbergraphPages
    for (UEdGraph *Graph : Blueprint->FunctionGraphs) {
      if (Graph->GetName() == GraphName) {
        TargetGraph = Graph;
        break;
      }
    }
    if (!TargetGraph) {
      for (UEdGraph *Graph : Blueprint->UbergraphPages) {
        if (Graph->GetName() == GraphName) {
          TargetGraph = Graph;
          break;
        }
      }
    }
  }

  if (!TargetGraph) {
    // Fallback: try finding by name in all graphs
    TArray<UEdGraph *> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);
    for (UEdGraph *Graph : AllGraphs) {
      if (Graph->GetName() == GraphName) {
        TargetGraph = Graph;
        break;
      }
    }
  }

  if (!TargetGraph) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Could not find graph '%s' in blueprint."),
                        *GraphName),
        TEXT("GRAPH_NOT_FOUND"));
    return true;
  }

  const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

  // Node identifier interoperability:
  // - Prefer NodeGuid strings for stable references.
  // - Accept node UObject names (e.g. "K2Node_Event_0") for clients that
  //   mistakenly pass nodeName where nodeId is expected.
  auto FindNodeByIdOrName = [&](const FString &Id) -> UEdGraphNode * {
    if (Id.IsEmpty()) {
      return nullptr;
    }

    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      if (!Node) {
        continue;
      }

      if (Node->NodeGuid.ToString().Equals(Id, ESearchCase::IgnoreCase) ||
          Node->GetName().Equals(Id, ESearchCase::IgnoreCase)) {
        return Node;
      }
    }

    return nullptr;
  };

  auto FindPinByName = [](UEdGraphNode *Node, const FString &PinName) -> UEdGraphPin * {
    if (!Node || PinName.IsEmpty()) {
      return nullptr;
    }

    FString CleanPinName;
    if (!PinName.Split(TEXT("."), nullptr, &CleanPinName)) {
      CleanPinName = PinName;
    }

    auto MatchPin = [Node](const FString &Candidate) -> UEdGraphPin * {
      if (UEdGraphPin *Pin = Node->FindPin(*Candidate)) {
        return Pin;
      }

      for (UEdGraphPin *Pin : Node->Pins) {
        if (!Pin) {
          continue;
        }
        if (Pin->PinName.ToString().Equals(Candidate, ESearchCase::IgnoreCase) ||
            Pin->GetDisplayName().ToString().Equals(Candidate, ESearchCase::IgnoreCase)) {
          return Pin;
        }
      }

      return nullptr;
    };

    if (UEdGraphPin *Pin = MatchPin(CleanPinName)) {
      return Pin;
    }

    FString UnderscorePinName = CleanPinName;
    UnderscorePinName.ReplaceCharInline(TEXT(' '), TEXT('_'));
    if (!UnderscorePinName.Equals(CleanPinName, ESearchCase::CaseSensitive)) {
      return MatchPin(UnderscorePinName);
    }

    return nullptr;
  };

  if (SubAction == TEXT("create_node")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Create Blueprint Node")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString NodeType;
    Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    float X = 0.0f;
    float Y = 0.0f;
    // Schema advertises both x/y and posX/posY — honor either spelling.
    if (!Payload->TryGetNumberField(TEXT("x"), X)) { Payload->TryGetNumberField(TEXT("posX"), X); }
    if (!Payload->TryGetNumberField(TEXT("y"), Y)) { Payload->TryGetNumberField(TEXT("posY"), Y); }

    // Helper to finalize and report
    auto FinalizeAndReport = [&](auto &NodeCreator, UEdGraphNode *NewNode) {
      if (NewNode) {
        // Set position BEFORE finalization per FGraphNodeCreator pattern
        NewNode->NodePosX = X;
        NewNode->NodePosY = Y;

        // Finalize() calls CreateNewGuid(), PostPlacedNewNode(), and
        // AllocateDefaultPins() if pins are empty. Do NOT call
        // AllocateDefaultPins() again after this!
        NodeCreator.Finalize();

        McpFinalizeBlueprint(Blueprint, /*bStructural=*/false, /*bSave=*/true);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
        Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Node created."), Result);
      } else {
        SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("Failed to create node (unsupported type or internal error)."),
            TEXT("CREATE_FAILED"));
      }
    };

    // Map common Blueprint node names to their CallFunction equivalents
    // This allows users to use nodeType="PrintString" instead of CallFunction
    static TMap<FString, TTuple<FString, FString>> CommonFunctionNodes = {
        {TEXT("PrintString"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("PrintString"))},
        {TEXT("Print"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("PrintString"))},
        {TEXT("PrintText"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("PrintText"))},
        {TEXT("SetActorLocation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_SetActorLocation"))},
        {TEXT("GetActorLocation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_GetActorLocation"))},
        {TEXT("SetActorRotation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_SetActorRotation"))},
        {TEXT("GetActorRotation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_GetActorRotation"))},
        {TEXT("SetActorTransform"),
         MakeTuple(TEXT("AActor"), TEXT("K2_SetActorTransform"))},
        {TEXT("GetActorTransform"),
         MakeTuple(TEXT("AActor"), TEXT("K2_GetActorTransform"))},
        {TEXT("AddActorLocalOffset"),
         MakeTuple(TEXT("AActor"), TEXT("K2_AddActorLocalOffset"))},
        {TEXT("Delay"), MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("Delay"))},
        {TEXT("DestroyActor"),
         MakeTuple(TEXT("AActor"), TEXT("K2_DestroyActor"))},
        {TEXT("SpawnActor"),
         MakeTuple(TEXT("UGameplayStatics"),
                   TEXT("BeginDeferredActorSpawnFromClass"))},
        {TEXT("GetPlayerPawn"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("GetPlayerPawn"))},
        {TEXT("GetPlayerController"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("GetPlayerController"))},
        {TEXT("PlaySound"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("PlaySound2D"))},
        {TEXT("PlaySound2D"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("PlaySound2D"))},
        {TEXT("PlaySoundAtLocation"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("PlaySoundAtLocation"))},
        {TEXT("GetWorldDeltaSeconds"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("GetWorldDeltaSeconds"))},
        {TEXT("SetTimerByFunctionName"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("K2_SetTimer"))},
        {TEXT("ClearTimer"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("K2_ClearTimer"))},
        {TEXT("IsValid"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("IsValid"))},
        {TEXT("IsValidClass"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("IsValidClass"))},
        // Math Nodes
        {TEXT("Add_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Add_IntInt"))},
        {TEXT("Subtract_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Subtract_IntInt"))},
        {TEXT("Multiply_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Multiply_IntInt"))},
        {TEXT("Divide_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Divide_IntInt"))},
        {TEXT("Add_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Add_DoubleDouble"))},
        {TEXT("Subtract_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Subtract_DoubleDouble"))},
        {TEXT("Multiply_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Multiply_DoubleDouble"))},
        {TEXT("Divide_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Divide_DoubleDouble"))},
        {TEXT("FTrunc"), MakeTuple(TEXT("UKismetMathLibrary"), TEXT("FTrunc"))},
        // Vector Ops
        {TEXT("MakeVector"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("MakeVector"))},
        {TEXT("BreakVector"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("BreakVector"))},
        // Actor/Component Ops
        {TEXT("GetComponentByClass"),
         MakeTuple(TEXT("AActor"), TEXT("GetComponentByClass"))},
        // Timer
        {TEXT("GetWorldTimerManager"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("K2_GetTimerManager"))}};

    // Check if this is a common function node shortcut
    if (const auto *FuncInfo = CommonFunctionNodes.Find(NodeType)) {
      FString ClassName = FuncInfo->Get<0>();
      FString FuncName = FuncInfo->Get<1>();

      // Find the class and function BEFORE creating NodeCreator
      // (FGraphNodeCreator asserts in destructor if not finalized)
      UClass *Class = nullptr;
      if (ClassName == TEXT("UKismetSystemLibrary")) {
        Class = UKismetSystemLibrary::StaticClass();
      } else if (ClassName == TEXT("UGameplayStatics")) {
        Class = UGameplayStatics::StaticClass();
      } else if (ClassName == TEXT("AActor")) {
        Class = AActor::StaticClass();
      } else if (ClassName == TEXT("UKismetMathLibrary")) {
        Class = UKismetMathLibrary::StaticClass();
      } else {
        Class = ResolveUClass(ClassName);
      }

      UFunction *Func = nullptr;
      if (Class) {
        Func = Class->FindFunctionByName(*FuncName);
      }

      // Return early with error if function not found (before NodeCreator)
      if (!Func) {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(
                TEXT("Could not find function '%s::%s' for node type '%s'"),
                *ClassName, *FuncName, *NodeType),
            TEXT("FUNCTION_NOT_FOUND"));
        return true;
      }

      // Now safe to create NodeCreator since we know we'll finalize it
      FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*TargetGraph);
      UK2Node_CallFunction *CallFuncNode = NodeCreator.CreateNode(false);
      CallFuncNode->SetFromFunction(Func);
      FinalizeAndReport(NodeCreator, CallFuncNode);
      return true;
    }

    // ========================================================================
    // DYNAMIC NODE CREATION - Find node classes at runtime
    // ========================================================================

    // Map user-friendly node names to their K2Node class names
    static TMap<FString, FString> NodeTypeAliases = {
        // Flow Control
        {TEXT("Branch"), TEXT("K2Node_IfThenElse")},
        {TEXT("IfThenElse"), TEXT("K2Node_IfThenElse")},
        {TEXT("Sequence"), TEXT("K2Node_ExecutionSequence")},
        {TEXT("ExecutionSequence"), TEXT("K2Node_ExecutionSequence")},
        {TEXT("Select"), TEXT("K2Node_Select")},
        {TEXT("Switch"), TEXT("K2Node_SwitchInteger")},
        {TEXT("SwitchOnInt"), TEXT("K2Node_SwitchInteger")},
        {TEXT("SwitchOnEnum"), TEXT("K2Node_SwitchEnum")},
        {TEXT("SwitchOnString"), TEXT("K2Node_SwitchString")},
        {TEXT("SwitchOnName"), TEXT("K2Node_SwitchName")},
        // Flow Control
        {TEXT("DoOnce"), TEXT("K2Node_DoOnce")},
        {TEXT("DoN"), TEXT("K2Node_DoN")},
        {TEXT("FlipFlop"), TEXT("K2Node_FlipFlop")},
        {TEXT("Gate"), TEXT("K2Node_Gate")},
        {TEXT("MultiGate"), TEXT("K2Node_MultiGate")},
        // Loops
        {TEXT("ForLoop"), TEXT("K2Node_ForLoop")},
        {TEXT("ForLoopWithBreak"), TEXT("K2Node_ForLoopWithBreak")},
        {TEXT("ForEachLoop"), TEXT("K2Node_ForEachElementInEnum")},
        {TEXT("WhileLoop"), TEXT("K2Node_WhileLoop")},
        // Data
        {TEXT("MakeArray"), TEXT("K2Node_MakeArray")},
        {TEXT("MakeStruct"), TEXT("K2Node_MakeStruct")},
        {TEXT("BreakStruct"), TEXT("K2Node_BreakStruct")},
        {TEXT("MakeMap"), TEXT("K2Node_MakeMap")},
        {TEXT("MakeSet"), TEXT("K2Node_MakeSet")},
        // Actor/Component
        {TEXT("SpawnActorFromClass"), TEXT("K2Node_SpawnActorFromClass")},
        {TEXT("GetAllActorsOfClass"), TEXT("K2Node_GetAllActorsOfClass")},
        // Misc
        {TEXT("Self"), TEXT("K2Node_Self")},
        {TEXT("GetSelf"), TEXT("K2Node_Self")},
        {TEXT("Timeline"), TEXT("K2Node_Timeline")},
        {TEXT("Knot"), TEXT("K2Node_Knot")},
        {TEXT("Reroute"), TEXT("K2Node_Knot")},
        {TEXT("Comment"), TEXT("EdGraphNode_Comment")},
        // Literals
        {TEXT("Literal"), TEXT("K2Node_Literal")},
    };

    // Helper: Try to find a UK2Node subclass by name
    auto FindNodeClassByName = [](const FString &TypeName) -> UClass * {
      // First check for aliases
      FString ResolvedName = TypeName;
      if (const FString *Alias = NodeTypeAliases.Find(TypeName)) {
        ResolvedName = *Alias;
      }

      TArray<FString> NamesToTry;
      NamesToTry.Add(ResolvedName);
      NamesToTry.Add(FString::Printf(TEXT("K2Node_%s"), *ResolvedName));
      NamesToTry.Add(FString::Printf(TEXT("UK2Node_%s"), *ResolvedName));
      // Also try the original name if different
      if (ResolvedName != TypeName) {
        NamesToTry.Add(TypeName);
        NamesToTry.Add(FString::Printf(TEXT("K2Node_%s"), *TypeName));
        NamesToTry.Add(FString::Printf(TEXT("UK2Node_%s"), *TypeName));
      }

      for (TObjectIterator<UClass> It; It; ++It) {
        if (!It->IsChildOf(UEdGraphNode::StaticClass()))
          continue;
        if (It->HasAnyClassFlags(CLASS_Abstract))
          continue;

        FString ClassName = It->GetName();
        for (const FString &NameToMatch : NamesToTry) {
          if (ClassName.Equals(NameToMatch, ESearchCase::IgnoreCase)) {
            return *It;
          }
        }
      }
      return nullptr;
    };

    // Special nodes requiring extra parameters
    if (NodeType == TEXT("VariableGet") ||
        NodeType == TEXT("K2Node_VariableGet") ||
        NodeType == TEXT("VariableSet") ||
        NodeType == TEXT("K2Node_VariableSet")) {
      const bool bIsSet = (NodeType == TEXT("VariableSet") ||
                           NodeType == TEXT("K2Node_VariableSet"));
      FString VarName;
      Payload->TryGetStringField(TEXT("variableName"), VarName);
      FName VarFName(*VarName);

      // Support inherited UPROPERTY on parent / SCS component class
      // (e.g. UCharacterMovementComponent::MaxWalkSpeed). When memberClass is
      // provided, look the property up *only* there; otherwise check the
      // Blueprint's own NewVariables (BP-declared but possibly uncompiled
      // member vars) and then walk its GeneratedClass parent chain.
      FString MemberClassStr;
      Payload->TryGetStringField(TEXT("memberClass"), MemberClassStr);

      FProperty *FoundProp = nullptr;
      UClass *ResolvedOwnerClass = nullptr;
      bool bFoundAsBPDeclaredVar = false;

      if (!MemberClassStr.IsEmpty()) {
        // Explicit memberClass — restrict lookup to that class's parent chain.
        // Do NOT fall back to Blueprint->NewVariables / GeneratedClass: if the
        // caller asserted "this variable lives on UCharacterMovementComponent"
        // and it doesn't, fail with VARIABLE_NOT_FOUND rather than silently
        // matching a same-name property on this BP's own class chain (which
        // would generate a wrong-target self-context node).
        if (UClass *OwnerClass = ResolveUClass(MemberClassStr)) {
          FoundProp = McpFindPropertyRecursive(OwnerClass, VarFName);
          if (FoundProp) {
            ResolvedOwnerClass = OwnerClass;
          }
        }
      } else {
        // No explicit memberClass — preserve the original lookup order:
        //   1. Blueprint->NewVariables (BP-declared vars that may be present
        //      here but not yet on GeneratedClass if the BP hasn't been
        //      recompiled since add_variable).
        //   2. Blueprint->GeneratedClass parent chain (compiled BP-declared
        //      vars and inherited UPROPERTY on parent BP/native classes).
        for (const FBPVariableDescription &VarDesc : Blueprint->NewVariables) {
          if (VarDesc.VarName == VarFName) {
            bFoundAsBPDeclaredVar = true;
            ResolvedOwnerClass = Blueprint->GeneratedClass;
            break;
          }
        }
        if (!bFoundAsBPDeclaredVar && Blueprint->GeneratedClass) {
          FoundProp =
              McpFindPropertyRecursive(Blueprint->GeneratedClass, VarFName);
          if (FoundProp) {
            ResolvedOwnerClass = FoundProp->GetOwnerClass();
          }
        }
      }

      if (!FoundProp && !bFoundAsBPDeclaredVar) {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(
                TEXT("Variable '%s' not found in Blueprint or any parent class "
                     "(memberClass='%s')"),
                *VarName, *MemberClassStr),
            TEXT("VARIABLE_NOT_FOUND"));
        return true;
      }

      // Treat the variable as "self context" when the Blueprint's class IS-A
      // owner class — i.e. the property lives on this BP or any of its
      // ancestors (BP-declared vars, parent BP/native ACharacter components,
      // etc.). For UPROPERTY on an unrelated class (e.g. accessing
      // UCharacterMovementComponent::MaxWalkSpeed through an external Target
      // component) we want external-member semantics so the K2Node exposes
      // a Target pin that the caller must wire to a component reference.
      // NewVariables hits are always self-context by construction.
      const bool bIsSelfContext =
          bFoundAsBPDeclaredVar ||
          (Blueprint->GeneratedClass &&
           Blueprint->GeneratedClass->IsChildOf(ResolvedOwnerClass));

      if (bIsSet) {
        FGraphNodeCreator<UK2Node_VariableSet> NodeCreator(*TargetGraph);
        UK2Node_VariableSet *VarSet = NodeCreator.CreateNode(false);
        if (bIsSelfContext) {
          VarSet->VariableReference.SetSelfMember(VarFName);
        } else {
          VarSet->VariableReference.SetFromField<FProperty>(
              FoundProp, /*bIsConsideredSelfContext=*/false, ResolvedOwnerClass);
        }
        FinalizeAndReport(NodeCreator, VarSet);
      } else {
        FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*TargetGraph);
        UK2Node_VariableGet *VarGet = NodeCreator.CreateNode(false);
        if (bIsSelfContext) {
          VarGet->VariableReference.SetSelfMember(VarFName);
        } else {
          VarGet->VariableReference.SetFromField<FProperty>(
              FoundProp, /*bIsConsideredSelfContext=*/false, ResolvedOwnerClass);
        }
        FinalizeAndReport(NodeCreator, VarGet);
      }
      return true;
    }

    if (NodeType == TEXT("CallFunction") ||
        NodeType == TEXT("K2Node_CallFunction") ||
        NodeType == TEXT("FunctionCall")) {
      FString MemberName, MemberClass;
      Payload->TryGetStringField(TEXT("memberName"), MemberName);
      Payload->TryGetStringField(TEXT("memberClass"), MemberClass);
      UFunction *Func = nullptr;
      if (!MemberClass.IsEmpty()) {
        if (UClass *Class = ResolveUClass(MemberClass))
          Func = Class->FindFunctionByName(*MemberName);
      } else {
        Func = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindFunctionByName(*MemberName) : nullptr;
        if (!Func) {
          if (UClass *KSL = UKismetSystemLibrary::StaticClass())
            Func = KSL->FindFunctionByName(*MemberName);
          if (!Func)
            if (UClass *GPS = UGameplayStatics::StaticClass())
              Func = GPS->FindFunctionByName(*MemberName);
          if (!Func)
            if (UClass *KML = UKismetMathLibrary::StaticClass())
              Func = KML->FindFunctionByName(*MemberName);
        }
      }
      if (Func) {
        FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*TargetGraph);
        UK2Node_CallFunction *CallFuncNode = NodeCreator.CreateNode(false);
        CallFuncNode->SetFromFunction(Func);
        FinalizeAndReport(NodeCreator, CallFuncNode);
      } else {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(
                TEXT("No UFunction named '%s' found. Pass the function's C++ "
                     "UFUNCTION name (e.g. 'SetActorLocation'), not its editor "
                     "display name. Set 'memberClass' to the owning class to "
                     "disambiguate; without it only the blueprint's own class, "
                     "KismetSystemLibrary, GameplayStatics and KismetMathLibrary "
                     "are searched."),
                *MemberName),
            TEXT("FUNCTION_NOT_FOUND"));
      }
      return true;
    }

    if (NodeType == TEXT("Event") || NodeType == TEXT("K2Node_Event")) {
      FString EventName, MemberClass;
      Payload->TryGetStringField(TEXT("eventName"), EventName);
      Payload->TryGetStringField(TEXT("memberClass"), MemberClass);
      if (EventName.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("eventName required"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      static TMap<FString, FString> Aliases = {
          {TEXT("BeginPlay"), TEXT("ReceiveBeginPlay")},
          {TEXT("Tick"), TEXT("ReceiveTick")},
          {TEXT("EndPlay"), TEXT("ReceiveEndPlay")}};
      if (const FString *A = Aliases.Find(EventName))
        EventName = *A;

      UClass *TargetClass = nullptr;
      UFunction *EventFunc = nullptr;
      if (!MemberClass.IsEmpty()) {
        TargetClass = ResolveUClass(MemberClass);
        if (TargetClass)
          EventFunc = TargetClass->FindFunctionByName(*EventName);
      } else {
        for (UClass *C = Blueprint->ParentClass; C && !EventFunc;
             C = C->GetSuperClass()) {
          EventFunc = C->FindFunctionByName(*EventName,
                                            EIncludeSuperFlag::ExcludeSuper);
          if (EventFunc)
            TargetClass = C;
        }
      }
      if (EventFunc && TargetClass) {
        FGraphNodeCreator<UK2Node_Event> NodeCreator(*TargetGraph);
        UK2Node_Event *EventNode = NodeCreator.CreateNode(false);
        EventNode->EventReference.SetFromField<UFunction>(EventFunc, false);
        EventNode->bOverrideFunction = true;
        FinalizeAndReport(NodeCreator, EventNode);
      } else {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Event '%s' not found"), *EventName),
            TEXT("EVENT_NOT_FOUND"));
      }
      return true;
    }

    if (NodeType == TEXT("CustomEvent") ||
        NodeType == TEXT("K2Node_CustomEvent")) {
      FString EventName;
      Payload->TryGetStringField(TEXT("eventName"), EventName);
      
      // Helper lambda to convert a type string into an FEdGraphPinType
      auto ResolvePinType = [&](const FString& TypeStr) -> FEdGraphPinType {
        FEdGraphPinType PinType;
        FString CleanType = TypeStr;
        CleanType.TrimStartAndEndInline();
        CleanType.RemoveFromEnd(TEXT("*"));

        // Built‑in types
        if (CleanType.Equals(TEXT("float"), ESearchCase::IgnoreCase)) {
          // PC_Float as the *category* compiles to an int (silent truncation);
          // a float pin is PC_Real + the float sub-category (cf. the double case).
          PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
          PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
          return PinType;
        }
        if (CleanType.Equals(TEXT("double"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
          PinType.PinSubCategory = NAME_Double;
          return PinType;
        }
        if (CleanType.Equals(TEXT("int"), ESearchCase::IgnoreCase) ||
            CleanType.Equals(TEXT("int32"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
          return PinType;
        }
        if (CleanType.Equals(TEXT("int64"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
          return PinType;
        }
        if (CleanType.Equals(TEXT("bool"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
          return PinType;
        }
        if (CleanType.Equals(TEXT("string"), ESearchCase::IgnoreCase) ||
            CleanType.Equals(TEXT("FString"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_String;
          return PinType;
        }
        if (CleanType.Equals(TEXT("name"), ESearchCase::IgnoreCase) ||
            CleanType.Equals(TEXT("FName"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
          return PinType;
        }
        if (CleanType.Equals(TEXT("text"), ESearchCase::IgnoreCase) ||
            CleanType.Equals(TEXT("FText"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
          return PinType;
        }
        if (CleanType.Equals(TEXT("byte"), ESearchCase::IgnoreCase) ||
            CleanType.Equals(TEXT("uint8"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
          return PinType;
        }
        if (CleanType.Equals(TEXT("object"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
          return PinType;
        }
        if (CleanType.Equals(TEXT("class"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
          return PinType;
        }
        if (CleanType.Equals(TEXT("softobject"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
          return PinType;
        }
        if (CleanType.Equals(TEXT("softclass"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
          return PinType;
        }
        if (CleanType.Equals(TEXT("interface"), ESearchCase::IgnoreCase)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Interface;
          return PinType;
        }

        // Structs (FVector, etc.) – try qualified path first, then short name, then iterate
        UScriptStruct* Struct = nullptr;
        
        // 1) Try full path (e.g., /Script/CoreUObject.Vector)
        if (CleanType.Contains(TEXT("/Script/")))
        {
          Struct = LoadObject<UScriptStruct>(nullptr, *CleanType);
        }
        
        // 2) Try short name with optional leading 'F'
        if (!Struct)
        {
          FString StructName = CleanType;
          if (StructName.StartsWith(TEXT("F")))
          {
            StructName = StructName.Mid(1);
          }
          Struct = FindObject<UScriptStruct>(nullptr, *StructName);
          if (!Struct)
          {
            Struct = FindObject<UScriptStruct>(nullptr, *CleanType);
          }
        }
        
        // 3) Fallback: iterate over all structs and match by name (case-insensitive)
        if (!Struct)
        {
          for (TObjectIterator<UScriptStruct> It; It; ++It)
          {
            if (It->GetName().Equals(CleanType, ESearchCase::IgnoreCase))
            {
              Struct = *It;
              break;
            }
          }
        }
        
        if (Struct)
        {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
          PinType.PinSubCategoryObject = Struct;
          return PinType;
        }

        // Enums
        if (UEnum* Enum = FindObject<UEnum>(nullptr, *CleanType)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
          PinType.PinSubCategoryObject = Enum;
          return PinType;
        }

        // UObject derived classes
        if (UClass* Class = ResolveUClass(CleanType)) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
          PinType.PinSubCategoryObject = Class;
          return PinType;
        }

        // Containers (fallback to wildcard)
        if (CleanType.StartsWith(TEXT("array<")) ||
            CleanType.StartsWith(TEXT("set<")) ||
            CleanType.StartsWith(TEXT("map<"))) {
          PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
          if (CleanType.StartsWith(TEXT("array<"))) PinType.ContainerType = EPinContainerType::Array;
          else if (CleanType.StartsWith(TEXT("set<"))) PinType.ContainerType = EPinContainerType::Set;
          else PinType.ContainerType = EPinContainerType::Map;
          return PinType;
        }

        // Unknown fallback
        PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
        return PinType;
      };

      const TArray<TSharedPtr<FJsonValue>>* ParamsArray = nullptr;
      const bool bHasParams = Payload->TryGetArrayField(TEXT("parameters"), ParamsArray) && ParamsArray->Num() > 0;

      // No parameters → simple custom event
      if (!bHasParams) {
        FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*TargetGraph);
        UK2Node_CustomEvent* EventNode = NodeCreator.CreateNode(false);
        EventNode->CustomFunctionName = FName(*EventName);
        FinalizeAndReport(NodeCreator, EventNode);
        return true;
      }

      // --- With parameters: use engine's UK2Node_CustomEvent::CreateFromFunction ---
      // Remove any existing custom event with the same name to avoid conflicts.
      // Must also remove the stale UFunction and its associated graph, otherwise the compiler
      // will see two functions with the same name and the new node will remain out-of-date.
      TArray<UK2Node_CustomEvent*> ExistingEvents;
      FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_CustomEvent>(Blueprint, ExistingEvents);
      for (UK2Node_CustomEvent* Existing : ExistingEvents) {
        if (Existing && Existing->CustomFunctionName.ToString() == EventName) {
          FName FuncName = Existing->CustomFunctionName;
          // 1. Remove the function graph associated with this custom event
          UEdGraph* FuncGraph = nullptr;
          for (UEdGraph* Graph : Blueprint->FunctionGraphs)
          {
            if (Graph && Graph->GetFName() == FuncName)
            {
              FuncGraph = Graph;
              break;
            }
          }
          if (FuncGraph)
          {
            FBlueprintEditorUtils::RemoveGraph(Blueprint, FuncGraph, EGraphRemoveFlags::Default);
          }
          // 2. Remove the node from the graph
          FBlueprintEditorUtils::RemoveNode(Blueprint, Existing, true);
        }
      }

      // Ensure the blueprint has a GeneratedClass (must be compiled)
      if (!Blueprint->GeneratedClass)
      {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Blueprint has no GeneratedClass. Compile it first."), TEXT("INVALID_STATE"));
        return true;
      }

      // Create a unique temporary function name
      FName TempFuncName = MakeUniqueObjectName(Blueprint->GeneratedClass, UFunction::StaticClass(), FName(*FString::Printf(TEXT("__TempCustomEventFunc_%s"), *EventName)));

      // Create the temporary UFunction
      UFunction* TempFunc = NewObject<UFunction>(Blueprint->GeneratedClass, TempFuncName, RF_Public);
      TempFunc->FunctionFlags = FUNC_Public | FUNC_BlueprintCallable;

      // Build parameter properties
      TArray<FProperty*> Params;
      for (const auto& ParamValue : *ParamsArray) {
        const TSharedPtr<FJsonObject>& ParamObj = ParamValue->AsObject();
        if (!ParamObj.IsValid()) continue;

        FString ParamName, ParamType;
        if (!ParamObj->TryGetStringField(TEXT("name"), ParamName) ||
            !ParamObj->TryGetStringField(TEXT("type"), ParamType))
        {
          SendAutomationError(RequestingSocket, RequestId,
              TEXT("Missing 'name' or 'type' in parameter definition."), TEXT("INVALID_PARAMETER"));
          return true;
        }
        FEdGraphPinType PinType = ResolvePinType(ParamType);

        FProperty* Prop = nullptr;

        // Map PinType to FProperty subclass
        if (PinType.PinCategory == UEdGraphSchema_K2::PC_Float) {
          Prop = new FFloatProperty(TempFunc, FName(*ParamName), RF_Public);
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real) {
          // double precision float
          Prop = new FDoubleProperty(TempFunc, FName(*ParamName), RF_Public);
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int) {
          Prop = new FIntProperty(TempFunc, FName(*ParamName), RF_Public);
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64) {
          Prop = new FInt64Property(TempFunc, FName(*ParamName), RF_Public);
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean) {
          FBoolProperty* BoolProp = new FBoolProperty(TempFunc, FName(*ParamName), RF_Public);
          BoolProp->SetBoolSize(0, true);  // 0 = single bit, not byte size
          Prop = BoolProp;
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String) {
          Prop = new FStrProperty(TempFunc, FName(*ParamName), RF_Public);
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name) {
          Prop = new FNameProperty(TempFunc, FName(*ParamName), RF_Public);
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text) {
          Prop = new FTextProperty(TempFunc, FName(*ParamName), RF_Public);
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte && PinType.PinSubCategoryObject.IsValid()) {
          FByteProperty* ByteProp = new FByteProperty(TempFunc, FName(*ParamName), RF_Public);
          ByteProp->Enum = Cast<UEnum>(PinType.PinSubCategoryObject);
          Prop = ByteProp;
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject.IsValid()) {
          FStructProperty* StructProp = new FStructProperty(TempFunc, FName(*ParamName), RF_Public);
          StructProp->Struct = Cast<UScriptStruct>(PinType.PinSubCategoryObject);
          Prop = StructProp;
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object && PinType.PinSubCategoryObject.IsValid()) {
          FObjectProperty* ObjProp = new FObjectProperty(TempFunc, FName(*ParamName), RF_Public);
          ObjProp->PropertyClass = Cast<UClass>(PinType.PinSubCategoryObject);
          Prop = ObjProp;
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class && PinType.PinSubCategoryObject.IsValid()) {
          FClassProperty* ClassProp = new FClassProperty(TempFunc, FName(*ParamName), RF_Public);
          ClassProp->PropertyClass = UClass::StaticClass();
          ClassProp->MetaClass = Cast<UClass>(PinType.PinSubCategoryObject);
          Prop = ClassProp;
        } else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject && PinType.PinSubCategoryObject.IsValid()) {
          FSoftObjectProperty* SoftProp = new FSoftObjectProperty(TempFunc, FName(*ParamName), RF_Public);
          SoftProp->PropertyClass = Cast<UClass>(PinType.PinSubCategoryObject);
          Prop = SoftProp;
        } else {
          // Unknown type: log warning and skip this parameter
          UE_LOG(LogTemp, Warning, TEXT("Unsupported parameter type '%s' for parameter '%s' – skipping"), *ParamType, *ParamName);
          continue;
        }

        if (Prop) {
          Prop->SetFlags(RF_Public);
          Prop->PropertyFlags |= CPF_Parm;
          Params.Add(Prop);
        }
      }

      // Link properties into the function
      if (Params.Num() > 0) {
        TempFunc->ChildProperties = Params[0];
        for (int32 i = 0; i < Params.Num() - 1; ++i) {
          Params[i]->Next = Params[i + 1];
        }
      }
      TempFunc->Bind();   // Finalize function signature

      // Create the custom event node using the engine's official API
      FVector2D NodePos(X, Y);
      UK2Node_CustomEvent* EventNode = UK2Node_CustomEvent::CreateFromFunction(
          NodePos, TargetGraph, EventName, TempFunc, false);

      if (!EventNode) {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to create custom event from function."), TEXT("INTERNAL_ERROR"));
        return true;
      }

      // Ensure the node is fully initialized with a stable GUID
      // CreateFromFunction does NOT call PostPlacedNewNode or CreateNewGuid
      EventNode->CreateNewGuid();
      EventNode->PostPlacedNewNode();

      // Clean up the temporary function
      TempFunc->MarkAsGarbage();

      McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, /*bSave=*/true);

      // Report success
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("nodeId"), EventNode->NodeGuid.ToString());
      Result->SetStringField(TEXT("nodeName"), EventNode->GetName());
      McpHandlerUtils::AddVerification(Result, Blueprint);
      SendAutomationResponse(RequestingSocket, RequestId, true,
          TEXT("Custom event with parameters created using engine API."), Result);
      return true;
    }

    // Accept the engine UClass names ("DynamicCast"/"K2Node_DynamicCast" — what
    // get_graph_details reports) here too, not just "Cast"/"CastTo<Class>". Without
    // this they fell through to the generic registered-node path, which spawns a
    // UK2Node_DynamicCast but never sets TargetType → a "Bad cast node" (wildcard
    // Object pin, no As<Class> output) even with a full targetClass. These names
    // carry no inline class, so targetClass is required (empty → CLASS_NOT_FOUND).
    if (NodeType == TEXT("Cast") || NodeType.StartsWith(TEXT("CastTo")) ||
        NodeType == TEXT("DynamicCast") ||
        NodeType == TEXT("K2Node_DynamicCast")) {
      FString TargetClassName;
      Payload->TryGetStringField(TEXT("targetClass"), TargetClassName);
      if (TargetClassName.IsEmpty() && NodeType.StartsWith(TEXT("CastTo")))
        TargetClassName = NodeType.Mid(6);
      UClass *TargetClass = ResolveUClass(TargetClassName);
      if (!TargetClass) {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Class '%s' not found"), *TargetClassName),
            TEXT("CLASS_NOT_FOUND"));
        return true;
      }
      FGraphNodeCreator<UK2Node_DynamicCast> NodeCreator(*TargetGraph);
      UK2Node_DynamicCast *CastNode = NodeCreator.CreateNode(false);
      CastNode->TargetType = TargetClass;
      FinalizeAndReport(NodeCreator, CastNode);
      return true;
    }

    if (NodeType == TEXT("InputAxisEvent") ||
        NodeType == TEXT("K2Node_InputAxisEvent")) {
      FString InputAxisName;
      Payload->TryGetStringField(TEXT("inputAxisName"), InputAxisName);
      if (InputAxisName.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("inputAxisName required"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FGraphNodeCreator<UK2Node_InputAxisEvent> NodeCreator(*TargetGraph);
      UK2Node_InputAxisEvent *InputNode = NodeCreator.CreateNode(false);
      InputNode->InputAxisName = FName(*InputAxisName);
      FinalizeAndReport(NodeCreator, InputNode);
      return true;
    }

    // ========== DYNAMIC FALLBACK: Create ANY node class by name ==========
    UClass *NodeClass = FindNodeClassByName(NodeType);

    // The ConstructObjectFromClass family (SpawnActorFromClass, ConstructObject,
    // CreateWidget, AddComponentByClass, ...) derives its pins from a Class pin and
    // requires AllocateDefaultPins to run BEFORE PostPlacedNewNode: e.g.
    // UK2Node_SpawnActorFromClass::PostPlacedNewNode() calls GetScaleMethodPin()
    // (FindPinChecked) on a pin that AllocateDefaultPins creates. The generic path
    // below runs PostPlacedNewNode before AllocateDefaultPins, so FindPinChecked
    // asserts (EdGraphNode.h:586) and hard-crashes the editor for these nodes.
    // Allocate first, then PostPlaced; if a class was supplied (targetClass), set it
    // and reconstruct so the class's expose-on-spawn pins appear.
    if (NodeClass &&
        NodeClass->IsChildOf(UK2Node_ConstructObjectFromClass::StaticClass())) {
      UClass *SpawnClass = nullptr;
      FString SpawnClassName;
      if (Payload->TryGetStringField(TEXT("targetClass"), SpawnClassName) &&
          !SpawnClassName.IsEmpty()) {
        SpawnClass = ResolveUClass(SpawnClassName);
        if (!SpawnClass) {
          SendAutomationError(
              RequestingSocket, RequestId,
              FString::Printf(TEXT("Class '%s' not found"), *SpawnClassName),
              TEXT("CLASS_NOT_FOUND"));
          return true;
        }
      }
      UEdGraphNode *NewNode = NewObject<UEdGraphNode>(TargetGraph, NodeClass);
      TargetGraph->Modify();
      TargetGraph->AddNode(NewNode, false, false);
      NewNode->CreateNewGuid();
      NewNode->AllocateDefaultPins();
      NewNode->PostPlacedNewNode();
      if (SpawnClass) {
        if (UK2Node_ConstructObjectFromClass *ConstructNode =
                Cast<UK2Node_ConstructObjectFromClass>(NewNode)) {
          if (UEdGraphPin *ClassPin = ConstructNode->GetClassPin()) {
            ClassPin->DefaultObject = SpawnClass;
            NewNode->ReconstructNode();
          }
        }
      }
      NewNode->NodePosX = X;
      NewNode->NodePosY = Y;
      McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, /*bSave=*/true);

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
      Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
      Result->SetStringField(TEXT("nodeClass"), NodeClass->GetName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Node created."), Result);
      return true;
    }

    if (NodeClass) {
      UEdGraphNode *NewNode = NewObject<UEdGraphNode>(TargetGraph, NodeClass);
      if (NewNode) {
        // Record the graph mutation for undo (the other create_node branches go
        // through FGraphNodeCreator, which Modify()s; this hand-rolled fallback didn't).
        TargetGraph->Modify();
        TargetGraph->AddNode(NewNode, false, false);
        NewNode->CreateNewGuid();
        NewNode->PostPlacedNewNode();
        NewNode->AllocateDefaultPins();
        NewNode->NodePosX = X;
        NewNode->NodePosY = Y;
        McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, /*bSave=*/true);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
        Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
        Result->SetStringField(TEXT("nodeClass"), NodeClass->GetName());
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Node created."), Result);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to instantiate node."),
                            TEXT("CREATE_FAILED"));
      }
    } else {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Node type '%s' not found. Use list_node_types "
                               "to see available types."),
                          *NodeType),
          TEXT("NODE_TYPE_NOT_FOUND"));
    }
    return true;
  } else if (SubAction == TEXT("connect_pins")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Connect Blueprint Pins")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString FromNodeId, FromPinName, ToNodeId, ToPinName;
    Payload->TryGetStringField(TEXT("sourceNodeId"), FromNodeId);
    Payload->TryGetStringField(TEXT("sourcePinName"), FromPinName);
    Payload->TryGetStringField(TEXT("targetNodeId"), ToNodeId);
    Payload->TryGetStringField(TEXT("targetPinName"), ToPinName);

    UEdGraphNode *FromNode = FindNodeByIdOrName(FromNodeId);
    UEdGraphNode *ToNode = FindNodeByIdOrName(ToNodeId);

    if (!FromNode || !ToNode) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Could not find source or target node."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Mark nodes for modification BEFORE any mutations so undo captures AllocateDefaultPins as well
    FromNode->Modify();
    ToNode->Modify();

    // Ensure nodes have allocated pins (some node types may not have pins yet)
    if (FromNode->Pins.Num() == 0) {
      UE_LOG(LogTemp, Warning, TEXT("connect_pins: FromNode '%s' has no pins, calling AllocateDefaultPins"), *FromNode->GetName());
      FromNode->AllocateDefaultPins();
    }
    if (ToNode->Pins.Num() == 0) {
      UE_LOG(LogTemp, Warning, TEXT("connect_pins: ToNode '%s' has no pins, calling AllocateDefaultPins"), *ToNode->GetName());
      ToNode->AllocateDefaultPins();
    }

    // Handle PinName in format "NodeName.PinName"
    FString FromPinClean;
    if (!FromPinName.Split(TEXT("."), nullptr, &FromPinClean)) {
      FromPinClean = FromPinName;
    }
    FString ToPinClean;
    if (!ToPinName.Split(TEXT("."), nullptr, &ToPinClean)) {
      ToPinClean = ToPinName;
    }

    UEdGraphPin *FromPin = FindPinByName(FromNode, FromPinClean);
    UEdGraphPin *ToPin = FindPinByName(ToNode, ToPinClean);

    if (!FromPin || !ToPin) {
      // Log the available pins for debugging, skipping null pins
      FString FromPinsList, ToPinsList;
      for (UEdGraphPin* P : FromNode->Pins) {
        if (P) FromPinsList += P->PinName.ToString() + TEXT(", ");
      }
      for (UEdGraphPin* P : ToNode->Pins) {
        if (P) ToPinsList += P->PinName.ToString() + TEXT(", ");
      }
      UE_LOG(LogTemp, Warning, TEXT("connect_pins: FromNode '%s' pins: %s"), *FromNode->GetName(), *FromPinsList);
      UE_LOG(LogTemp, Warning, TEXT("connect_pins: ToNode '%s' pins: %s"), *ToNode->GetName(), *ToPinsList);
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Could not find source or target pin."),
                          TEXT("PIN_NOT_FOUND"));
      return true;
    }

    if (TargetGraph->GetSchema()->TryCreateConnection(FromPin, ToPin)) {
      McpFinalizeBlueprint(Blueprint, /*bStructural=*/false, /*bSave=*/true);
      
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      McpHandlerUtils::AddVerification(Result, Blueprint);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Pins connected."), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to connect pins (schema rejection)."),
                          TEXT("CONNECTION_FAILED"));
    }
    return true;
  } else if (SubAction == TEXT("break_pin_links")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Break Blueprint Pin Links")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString NodeId, PinName;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    UEdGraphNode *TargetNode = FindNodeByIdOrName(NodeId);

    if (!TargetNode) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    UEdGraphPin *Pin = FindPinByName(TargetNode, PinName);
    if (!Pin) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Pin not found."),
                          TEXT("PIN_NOT_FOUND"));
      return true;
    }

    TargetNode->Modify();
    TargetGraph->GetSchema()->BreakPinLinks(*Pin, true);
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/false, /*bSave=*/true);
    
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pin links broken."), Result);
    return true;
  }

  else if (SubAction == TEXT("delete_node")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Delete Blueprint Node")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    UEdGraphNode *TargetNode = FindNodeByIdOrName(NodeId);

    if (TargetNode) {
      FBlueprintEditorUtils::RemoveNode(Blueprint, TargetNode, true);
      
      // CRITICAL: Save the blueprint to persist changes.
      SaveLoadedAssetThrottled(Blueprint);
      
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      McpHandlerUtils::AddVerification(Result, Blueprint);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Node deleted."), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
    }
    return true;
  } else if (SubAction == TEXT("create_reroute_node")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Create Reroute Node")));
    Blueprint->Modify();
    TargetGraph->Modify();

    float X = 0.0f;
    float Y = 0.0f;
    if (!Payload->TryGetNumberField(TEXT("x"), X)) { Payload->TryGetNumberField(TEXT("posX"), X); }
    if (!Payload->TryGetNumberField(TEXT("y"), Y)) { Payload->TryGetNumberField(TEXT("posY"), Y); }

    FGraphNodeCreator<UK2Node_Knot> NodeCreator(*TargetGraph);
    UK2Node_Knot *RerouteNode = NodeCreator.CreateNode(false);

    RerouteNode->NodePosX = X;
    RerouteNode->NodePosY = Y;

    NodeCreator.Finalize();

    McpFinalizeBlueprint(Blueprint, /*bStructural=*/false, /*bSave=*/true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), RerouteNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), RerouteNode->GetName());
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Reroute node created."), Result);
    return true;
  } else if (SubAction == TEXT("set_node_property")) {
    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Set Blueprint Node Property")));
    Blueprint->Modify();
    TargetGraph->Modify();

    // Generic property setter for common node properties used by tools.
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    FString PropertyName;
    Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
    FString Value;
    Payload->TryGetStringField(TEXT("stringValue"), Value);

    UEdGraphNode *TargetNode = FindNodeByIdOrName(NodeId);

    if (TargetNode) {
      TargetNode->Modify();
      bool bHandled = false;
      bool bStructuralChange = false; // structural writes need the stronger mark

      if (PropertyName.Equals(TEXT("Comment"), ESearchCase::IgnoreCase) ||
          PropertyName.Equals(TEXT("NodeComment"), ESearchCase::IgnoreCase)) {
        TargetNode->NodeComment = Value;
        bHandled = true;
      } else if (PropertyName.Equals(TEXT("X"), ESearchCase::IgnoreCase) ||
                 PropertyName.Equals(TEXT("NodePosX"),
                                     ESearchCase::IgnoreCase)) {
        double NumValue = 0.0;
        if (!Payload->TryGetNumberField(TEXT("value"), NumValue)) {
          NumValue = FCString::Atod(*Value);
        }
        TargetNode->NodePosX = static_cast<float>(NumValue);
        bHandled = true;
      } else if (PropertyName.Equals(TEXT("Y"), ESearchCase::IgnoreCase) ||
                 PropertyName.Equals(TEXT("NodePosY"),
                                     ESearchCase::IgnoreCase)) {
        double NumValue = 0.0;
        if (!Payload->TryGetNumberField(TEXT("value"), NumValue)) {
          NumValue = FCString::Atod(*Value);
        }
        TargetNode->NodePosY = static_cast<float>(NumValue);
        bHandled = true;
      } else if (PropertyName.Equals(TEXT("bCommentBubbleVisible"),
                                     ESearchCase::IgnoreCase)) {
        TargetNode->bCommentBubbleVisible = Value.ToBool();
        bHandled = true;
      } else if (PropertyName.Equals(TEXT("bCommentBubblePinned"),
                                     ESearchCase::IgnoreCase)) {
        TargetNode->bCommentBubblePinned = Value.ToBool();
        bHandled = true;
      } else if (PropertyName.Equals(TEXT("EnabledState"),
                                     ESearchCase::IgnoreCase)) {
        // Flip ghost/disabled nodes (e.g. the stock Disabled `Event BeginPlay`)
        // without the delete-and-recreate dance. Strict parse, fail fast.
        ENodeEnabledState NewState;
        if (Value.Equals(TEXT("Enabled"), ESearchCase::IgnoreCase)) {
          NewState = ENodeEnabledState::Enabled;
        } else if (Value.Equals(TEXT("Disabled"), ESearchCase::IgnoreCase)) {
          NewState = ENodeEnabledState::Disabled;
        } else if (Value.Equals(TEXT("DevelopmentOnly"),
                                ESearchCase::IgnoreCase)) {
          NewState = ENodeEnabledState::DevelopmentOnly;
        } else {
          SendAutomationError(
              RequestingSocket, RequestId,
              FString::Printf(
                  TEXT("Invalid EnabledState '%s' (expected "
                       "Enabled|Disabled|DevelopmentOnly)"),
                  *Value),
              TEXT("INVALID_VALUE"));
          return true;
        }
        TargetNode->SetEnabledState(NewState); // bUserAction=true: mirrors a user toggle
        bStructuralChange = true;
        bHandled = true;
      }

      if (bHandled) {
        TargetGraph->NotifyGraphChanged();
        if (bStructuralChange) {
          // Enabled state changes what gets COMPILED (disabled nodes are
          // compiled out) — the editor's own toggle marks structurally
          // (FBlueprintEditor::OnSetEnabledStateForSelectedNodes); plain
          // MarkBlueprintAsModified would leave the old state compiled in.
          FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        } else {
          FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        }

        // CRITICAL: Save the blueprint to persist changes.
        SaveLoadedAssetThrottled(Blueprint);
        
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("nodeId"), TargetNode->NodeGuid.ToString());
        Result->SetStringField(TEXT("nodeName"), TargetNode->GetName());
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Node property updated."), Result);
      } else {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Unsupported node property '%s'"),
                            *PropertyName),
            TEXT("PROPERTY_NOT_SUPPORTED"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
    }
    return true;
  } else if (SubAction == TEXT("get_node_details")) {
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    UEdGraphNode *TargetNode = FindNodeByIdOrName(NodeId);

    if (TargetNode) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("nodeName"), TargetNode->GetName());
      Result->SetStringField(
          TEXT("nodeTitle"),
          TargetNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
      Result->SetStringField(TEXT("nodeComment"), TargetNode->NodeComment);
      Result->SetNumberField(TEXT("x"), TargetNode->NodePosX);
      Result->SetNumberField(TEXT("y"), TargetNode->NodePosY);

      TArray<TSharedPtr<FJsonValue>> Pins;
      for (UEdGraphPin *Pin : TargetNode->Pins) {
        if (!Pin)
          continue;

        TSharedPtr<FJsonObject> PinObj = McpHandlerUtils::CreateResultObject();
        PinObj->SetStringField(TEXT("pinName"), Pin->PinName.ToString());
        PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input
                                                      ? TEXT("Input")
                                                      : TEXT("Output"));
        PinObj->SetStringField(TEXT("pinType"),
                               Pin->PinType.PinCategory.ToString());

        // Add pin sub-category object type if applicable
        if (Pin->PinType.PinCategory == TEXT("object") ||
            Pin->PinType.PinCategory == TEXT("class") ||
            Pin->PinType.PinCategory == TEXT("struct")) {
          if (Pin->PinType.PinSubCategoryObject.IsValid()) {
            PinObj->SetStringField(
                TEXT("pinSubType"),
                Pin->PinType.PinSubCategoryObject->GetName());
          }
        }

        // Serialize linked pins as JSON objects (consistent with get_nodes)
        TArray<TSharedPtr<FJsonValue>> LinkedToArray;
        for (UEdGraphPin *LinkedPin : Pin->LinkedTo) {
          if (LinkedPin && LinkedPin->GetOwningNode()) {
            TSharedPtr<FJsonObject> LinkObj =
                McpHandlerUtils::CreateResultObject();
            LinkObj->SetStringField(
                TEXT("nodeId"),
                LinkedPin->GetOwningNode()->NodeGuid.ToString());
            LinkObj->SetStringField(TEXT("pinName"),
                                    LinkedPin->PinName.ToString());
            LinkedToArray.Add(MakeShared<FJsonValueObject>(LinkObj));
          }
        }
        PinObj->SetArrayField(TEXT("linkedTo"), LinkedToArray);

        if (!Pin->DefaultValue.IsEmpty()) {
          PinObj->SetStringField(TEXT("defaultValue"), Pin->DefaultValue);
        } else if (!Pin->DefaultTextValue.IsEmptyOrWhitespace()) {
          PinObj->SetStringField(TEXT("defaultTextValue"),
                                 Pin->DefaultTextValue.ToString());
        } else if (Pin->DefaultObject) {
          PinObj->SetStringField(TEXT("defaultObjectPath"),
                                 Pin->DefaultObject->GetPathName());
        }

        Pins.Add(MakeShared<FJsonValueObject>(PinObj));
      }
      Result->SetArrayField(TEXT("pins"), Pins);
      McpHandlerUtils::AddVerification(Result, Blueprint);

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Node details retrieved."), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
    }
    return true;
  } else if (SubAction == TEXT("get_graph_details")) {
    const bool bIncludePinLinks =
        GetJsonBoolField(Payload, TEXT("includePinLinks"), false);
    TSharedPtr<FJsonObject> Result =
        BuildGraphDetailsJson(TargetGraph, bIncludePinLinks);
    McpHandlerUtils::AddVerification(Result, Blueprint);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Graph details retrieved."), Result);
    return true;
  } else if (SubAction == TEXT("arrange_graph")) {
    // Auto-layout every node in TargetGraph: exec-flow columns for X, a
    // non-resetting row cursor with parent-centering for Y. Works on any graph
    // (generated or hand-edited), not just the chains the binders emit.
    const int32 Arranged = ArrangeBlueprintGraph(TargetGraph);
    TargetGraph->NotifyGraphChanged();
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/false, /*bSave=*/true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("graphName"), TargetGraph->GetName());
    Result->SetNumberField(TEXT("arrangedNodes"), Arranged);
    TArray<TSharedPtr<FJsonValue>> NodePositions;
    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      if (!Node) {
        continue;
      }
      TSharedPtr<FJsonObject> NodeObj = McpHandlerUtils::CreateResultObject();
      NodeObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
      NodeObj->SetStringField(
          TEXT("nodeTitle"),
          Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
      NodeObj->SetNumberField(TEXT("x"), Node->NodePosX);
      NodeObj->SetNumberField(TEXT("y"), Node->NodePosY);
      NodePositions.Add(MakeShared<FJsonValueObject>(NodeObj));
    }
    Result->SetArrayField(TEXT("nodes"), NodePositions);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Graph arranged."), Result);
    return true;
  } else if (SubAction == TEXT("get_pin_details")) {
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    FString PinName;
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    UEdGraphNode *TargetNode = FindNodeByIdOrName(NodeId);

    if (!TargetNode) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    TArray<UEdGraphPin *> PinsToReport;
    if (!PinName.IsEmpty()) {
      UEdGraphPin *Pin = FindPinByName(TargetNode, PinName);
      if (!Pin) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Pin not found."),
                            TEXT("PIN_NOT_FOUND"));
        return true;
      }
      PinsToReport.Add(Pin);
    } else {
      PinsToReport = TargetNode->Pins;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NodeId);

    TArray<TSharedPtr<FJsonValue>> PinsJson;
    for (UEdGraphPin *Pin : PinsToReport) {
      if (!Pin) {
        continue;
      }

      TSharedPtr<FJsonObject> PinObj = McpHandlerUtils::CreateResultObject();
      PinObj->SetStringField(TEXT("pinName"), Pin->PinName.ToString());
      PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input
                                                    ? TEXT("Input")
                                                    : TEXT("Output"));
      PinObj->SetStringField(TEXT("pinType"),
                             Pin->PinType.PinCategory.ToString());

      // Add pin sub-category object type if applicable
      if (Pin->PinType.PinCategory == TEXT("object") ||
          Pin->PinType.PinCategory == TEXT("class") ||
          Pin->PinType.PinCategory == TEXT("struct")) {
        if (Pin->PinType.PinSubCategoryObject.IsValid()) {
          PinObj->SetStringField(
              TEXT("pinSubType"),
              Pin->PinType.PinSubCategoryObject->GetName());
        }
      }

      // Serialize linked pins as JSON objects (consistent with get_nodes)
      TArray<TSharedPtr<FJsonValue>> LinkedToArray;
      for (UEdGraphPin *LinkedPin : Pin->LinkedTo) {
        if (LinkedPin && LinkedPin->GetOwningNode()) {
          TSharedPtr<FJsonObject> LinkObj =
              McpHandlerUtils::CreateResultObject();
          LinkObj->SetStringField(
              TEXT("nodeId"),
              LinkedPin->GetOwningNode()->NodeGuid.ToString());
          LinkObj->SetStringField(TEXT("pinName"),
                                  LinkedPin->PinName.ToString());
          LinkedToArray.Add(MakeShared<FJsonValueObject>(LinkObj));
        }
      }
      PinObj->SetArrayField(TEXT("linkedTo"), LinkedToArray);

      if (!Pin->DefaultValue.IsEmpty()) {
        PinObj->SetStringField(TEXT("defaultValue"), Pin->DefaultValue);
      } else if (!Pin->DefaultTextValue.IsEmptyOrWhitespace()) {
        PinObj->SetStringField(TEXT("defaultTextValue"),
                               Pin->DefaultTextValue.ToString());
      } else if (Pin->DefaultObject) {
        PinObj->SetStringField(TEXT("defaultObjectPath"),
                               Pin->DefaultObject->GetPathName());
      }

      PinsJson.Add(MakeShared<FJsonValueObject>(PinObj));
    }

    Result->SetArrayField(TEXT("pins"), PinsJson);
    McpHandlerUtils::AddVerification(Result, Blueprint);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pin details retrieved."), Result);
    return true;
  } else if (SubAction == TEXT("list_node_types")) {
    // List all available UK2Node types for AI discoverability
    TArray<TSharedPtr<FJsonValue>> NodeTypes;
    for (TObjectIterator<UClass> It; It; ++It) {
      if (!It->IsChildOf(UK2Node::StaticClass()))
        continue;
      if (It->HasAnyClassFlags(CLASS_Abstract))
        continue;

      TSharedPtr<FJsonObject> TypeObj = McpHandlerUtils::CreateResultObject();
      TypeObj->SetStringField(TEXT("className"), It->GetName());
      TypeObj->SetStringField(TEXT("displayName"),
                              It->GetDisplayNameText().ToString());
      NodeTypes.Add(MakeShared<FJsonValueObject>(TypeObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("nodeTypes"), NodeTypes);
    Result->SetNumberField(TEXT("count"), NodeTypes.Num());
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Node types listed."), Result);
    return true;
  } else if (SubAction == TEXT("set_pin_default_value")) {
    // Set a default value on a node's input pin
    FString NodeId, PinName, Value;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("pinName"), PinName);
    Payload->TryGetStringField(TEXT("stringValue"), Value);

    UEdGraphNode *TargetNode = FindNodeByIdOrName(NodeId);
    if (!TargetNode) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    UEdGraphPin *Pin = FindPinByName(TargetNode, PinName);
    if (!Pin) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Pin not found."),
                          TEXT("PIN_NOT_FOUND"));
      return true;
    }

    if (Pin->Direction != EGPD_Input) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Can only set default values on input pins."),
                          TEXT("INVALID_PIN_DIRECTION"));
      return true;
    }

    const UEdGraphSchema *Schema = TargetGraph->GetSchema();
    if (!Schema) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Graph has no schema; cannot set pin default."),
                          TEXT("NO_SCHEMA"));
      return true;
    }

    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Set Pin Default Value")));
    Blueprint->Modify();
    TargetGraph->Modify();
    TargetNode->Modify();

    // Use the schema to set the default. Object/class/soft pins store their
    // default in DefaultObject, NOT the string DefaultValue — TrySetDefaultValue
    // alone silently no-ops on them (e.g. CreateSaveGameObject's SaveGameClass).
    // Resolve the value to a UObject/UClass and route through TrySetDefaultObject.
    const FName PinCat = Pin->PinType.PinCategory;
    const bool bObjectLikePin =
        PinCat == UEdGraphSchema_K2::PC_Object ||
        PinCat == UEdGraphSchema_K2::PC_Class ||
        PinCat == UEdGraphSchema_K2::PC_Interface ||
        PinCat == UEdGraphSchema_K2::PC_SoftObject ||
        PinCat == UEdGraphSchema_K2::PC_SoftClass;
    UObject *Resolved = nullptr;
    if (bObjectLikePin && !Value.IsEmpty())
    {
        Resolved = StaticLoadObject(UObject::StaticClass(), nullptr, *Value);
        if (!Resolved)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Could not resolve object/class '%s' for pin '%s'."), *Value, *PinName),
                TEXT("OBJECT_NOT_FOUND"));
            return true;
        }
        Schema->TrySetDefaultObject(*Pin, Resolved);
    }
    else
    {
        Schema->TrySetDefaultValue(*Pin, Value);
    }

    // TrySetDefault* is void and silently no-ops on invalid/incompatible values.
    // Read the pin back and confirm it reflects the request before reporting
    // success — the schema may canonically reformat (e.g. "1" -> "1.0"), so a
    // requested non-empty value must leave a non-empty stored default rather
    // than matching byte-for-byte.
    const FString StoredValue = Pin->DefaultValue;
    const FString StoredText = Pin->DefaultTextValue.ToString();
    bool bStored = false;
    if (bObjectLikePin && Resolved)
    {
        // Soft pins store the resolved path in DefaultValue and leave DefaultObject
        // null; only hard object/class/interface pins populate DefaultObject. Verify
        // each by the slot the schema actually writes, or a valid soft set false-fails.
        const bool bSoftPin = PinCat == UEdGraphSchema_K2::PC_SoftObject || PinCat == UEdGraphSchema_K2::PC_SoftClass;
        bStored = bSoftPin ? !Pin->DefaultValue.IsEmpty() : (Pin->DefaultObject == Resolved);
    }
    else if (Value.IsEmpty())
    {
        // Resetting to the pin's autogenerated default is always honored.
        bStored = true;
    }
    else
    {
        bStored = !StoredValue.IsEmpty() || !StoredText.IsEmpty();
    }
    if (!bStored)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(
                TEXT("Schema rejected default value '%s' for pin '%s' (incompatible type or invalid format)."),
                *Value, *PinName),
            TEXT("SET_FAILED"));
        return true;
    }

    McpFinalizeBlueprint(Blueprint, /*bStructural=*/false, /*bSave=*/true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetStringField(TEXT("nodeName"), TargetNode->GetName());
    Result->SetStringField(TEXT("pinName"), PinName);
    Result->SetStringField(TEXT("value"), Value);
    if (Pin->DefaultObject) {
      Result->SetStringField(TEXT("storedObjectPath"),
                             Pin->DefaultObject->GetPathName());
    } else if (!StoredText.IsEmpty()) {
      Result->SetStringField(TEXT("storedValue"), StoredText);
    } else {
      Result->SetStringField(TEXT("storedValue"), StoredValue);
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pin default value set."), Result);
    return true;
  } else if (SubAction == TEXT("list_animbp_graphs")) {
    // Every graph in the blueprint, including anim subgraphs (state machines,
    // state bodies, transition rules) that FunctionGraphs/UbergraphPages don't
    // surface. Each graphName here is addressable by get_graph_details.
    TArray<UEdGraph *> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);

    // Child→parent from each graph's SubGraphs list (authoritative; a bound
    // graph's Outer is the owning node, not the parent graph).
    TMap<UEdGraph *, UEdGraph *> ParentOf;
    for (UEdGraph *G : AllGraphs) {
      if (!G) {
        continue;
      }
      for (const TObjectPtr<UEdGraph> &Child : G->SubGraphs) {
        if (Child) {
          ParentOf.Add(Child, G);
        }
      }
    }

    TArray<TSharedPtr<FJsonValue>> GraphsJson;
    for (UEdGraph *G : AllGraphs) {
      if (!G) {
        continue;
      }
      TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
      Obj->SetStringField(TEXT("graphName"), G->GetName());
      Obj->SetStringField(TEXT("type"), ClassifyBlueprintGraph(Blueprint, G));
      Obj->SetStringField(TEXT("graphClass"), G->GetClass()->GetName());
      Obj->SetNumberField(TEXT("nodeCount"), G->Nodes.Num());
      if (UEdGraph *const *Parent = ParentOf.Find(G)) {
        Obj->SetStringField(TEXT("parentGraph"), (*Parent)->GetName());
      }
      // Rule graphs share generated names (AnimationTransitionGraph_0 ×N);
      // the owning transition node's endpoints are the only useful identity.
      // Addressing one by name is first-match — use get_transition_rule_graph.
      if (UAnimStateTransitionNode *OwnerTrans =
              Cast<UAnimStateTransitionNode>(G->GetOuter())) {
        UAnimStateNodeBase *Prev = OwnerTrans->GetPreviousState();
        UAnimStateNodeBase *Next = OwnerTrans->GetNextState();
        Obj->SetStringField(
            TEXT("transition"),
            FString::Printf(TEXT("%s -> %s"),
                            Prev ? *Prev->GetStateName() : TEXT("?"),
                            Next ? *Next->GetStateName() : TEXT("?")));
      }
      GraphsJson.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("graphs"), GraphsJson);
    Result->SetNumberField(TEXT("count"), GraphsJson.Num());
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        TEXT("Graphs listed. Pass any graphName to get_graph_details."),
        Result);
    return true;
  } else if (SubAction == TEXT("get_transition_rule_graph")) {
    const FString FromState = GetJsonStringField(Payload, TEXT("fromState"));
    const FString ToState = GetJsonStringField(Payload, TEXT("toState"));
    const FString TransitionName =
        GetJsonStringField(Payload, TEXT("transitionName"));
    const FString StateMachineName =
        GetJsonStringField(Payload, TEXT("stateMachine"));
    if (TransitionName.IsEmpty() && (FromState.IsEmpty() || ToState.IsEmpty())) {
      SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Provide fromState + toState (optionally stateMachine to "
               "disambiguate) or transitionName."),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TArray<UEdGraph *> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);
    // Parallel transitions between the same two states are legal (e.g. an
    // automatic rule alongside a conditional one, ordered by priority), so
    // collect every match; transitionIndex selects among them.
    TArray<UAnimStateTransitionNode *> Matches;
    TArray<FString> MatchStateMachines;
    TArray<FString> Candidates;
    for (UEdGraph *G : AllGraphs) {
      UAnimationStateMachineGraph *SMGraph =
          Cast<UAnimationStateMachineGraph>(G);
      if (!SMGraph) {
        continue;
      }
      if (!StateMachineName.IsEmpty() &&
          !SMGraph->GetName().Equals(StateMachineName,
                                     ESearchCase::IgnoreCase)) {
        continue;
      }
      for (UEdGraphNode *Node : SMGraph->Nodes) {
        UAnimStateTransitionNode *Trans = Cast<UAnimStateTransitionNode>(Node);
        if (!Trans) {
          continue;
        }
        UAnimStateNodeBase *Prev = Trans->GetPreviousState();
        UAnimStateNodeBase *Next = Trans->GetNextState();
        const FString PrevName = Prev ? Prev->GetStateName() : FString();
        const FString NextName = Next ? Next->GetStateName() : FString();
        Candidates.Add(FString::Printf(TEXT("%s: %s -> %s"),
                                       *SMGraph->GetName(), *PrevName,
                                       *NextName));
        bool bMatch;
        if (!TransitionName.IsEmpty()) {
          const FString Title =
              Trans->GetNodeTitle(ENodeTitleType::ListView).ToString();
          bMatch = Title.Equals(TransitionName, ESearchCase::IgnoreCase) ||
                   (Trans->BoundGraph &&
                    Trans->BoundGraph->GetName().Equals(
                        TransitionName, ESearchCase::IgnoreCase));
        } else {
          bMatch = PrevName.Equals(FromState, ESearchCase::IgnoreCase) &&
                   NextName.Equals(ToState, ESearchCase::IgnoreCase);
        }
        if (bMatch) {
          Matches.Add(Trans);
          MatchStateMachines.Add(SMGraph->GetName());
        }
      }
    }

    if (Matches.Num() == 0) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(
              TEXT("No transition matched. Transitions present: %s"),
              Candidates.Num() > 0 ? *FString::Join(Candidates, TEXT("; "))
                                   : TEXT("(none — no state machine graphs "
                                          "found in this blueprint)")),
          TEXT("TRANSITION_NOT_FOUND"));
      return true;
    }

    const int32 TransitionIndex =
        GetJsonIntField(Payload, TEXT("transitionIndex"), 0);
    if (TransitionIndex < 0 || TransitionIndex >= Matches.Num()) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("transitionIndex %d out of range: %d parallel "
                               "transition(s) matched."),
                          TransitionIndex, Matches.Num()),
          TEXT("INDEX_OUT_OF_RANGE"));
      return true;
    }
    UAnimStateTransitionNode *Found = Matches[TransitionIndex];
    const FString FoundStateMachine = MatchStateMachines[TransitionIndex];

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("matchCount"), Matches.Num());
    if (Matches.Num() > 1) {
      Result->SetNumberField(TEXT("transitionIndex"), TransitionIndex);
    }
    Result->SetStringField(TEXT("stateMachine"), FoundStateMachine);
    UAnimStateNodeBase *Prev = Found->GetPreviousState();
    UAnimStateNodeBase *Next = Found->GetNextState();
    Result->SetStringField(TEXT("fromState"),
                           Prev ? Prev->GetStateName() : FString());
    Result->SetStringField(TEXT("toState"),
                           Next ? Next->GetStateName() : FString());
    Result->SetStringField(
        TEXT("transitionTitle"),
        Found->GetNodeTitle(ENodeTitleType::ListView).ToString());
    Result->SetNumberField(TEXT("crossfadeDuration"),
                           Found->CrossfadeDuration);
    Result->SetNumberField(TEXT("priorityOrder"), Found->PriorityOrder);
    Result->SetBoolField(TEXT("automaticRule"),
                         Found->bAutomaticRuleBasedOnSequencePlayerInState);
    const TCHAR *LogicTypeName;
    switch (Found->LogicType) {
    case ETransitionLogicType::TLT_StandardBlend:
      LogicTypeName = TEXT("StandardBlend");
      break;
    case ETransitionLogicType::TLT_Inertialization:
      LogicTypeName = TEXT("Inertialization");
      break;
    case ETransitionLogicType::TLT_Custom:
      LogicTypeName = TEXT("Custom");
      break;
    default:
      LogicTypeName = TEXT("Unknown");
      break;
    }
    Result->SetStringField(TEXT("logicType"), LogicTypeName);
    if (Found->BoundGraph) {
      Result->SetStringField(TEXT("ruleGraphName"),
                             Found->BoundGraph->GetName());
      Result->SetObjectField(
          TEXT("ruleGraph"),
          BuildGraphDetailsJson(Found->BoundGraph, /*bIncludePinLinks=*/true));
    }
    if (Found->CustomTransitionGraph) {
      Result->SetStringField(TEXT("customBlendGraphName"),
                             Found->CustomTransitionGraph->GetName());
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Transition rule graph retrieved."), Result);
    return true;
  }

  SendAutomationError(
      RequestingSocket, RequestId,
      FString::Printf(TEXT("Unknown subAction: %s"), *SubAction),
      TEXT("INVALID_SUBACTION"));
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Blueprint graph actions are editor-only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
