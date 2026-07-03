// =============================================================================
// McpGraphLayoutUtils.cpp
// =============================================================================
// Pure layout core. Ported from ArrangeBlueprintGraph
// (McpAutomationBridge_BlueprintGraphHandlers.cpp:259-447), retyped from
// UEdGraphNode* to McpGraphLayout::FNodeId. Same layered layout: longest-path
// columns (major axis); DFS row-centering (minor axis) whose parent is From for
// exec edges and To for pure-data feeders / the Sink role (see bCenterParentIsTo
// / RootRole); per-column size-aware collision pass. Two determinism fixes:
//   1. Columns are iterated by integer index over a TArray<TArray<FNodeId>>
//      (never TMap hash order).
//   2. Within-column ordering is a TOTAL order: sort by Row, then by FNodeId.
//
// Root derivation polarity is selectable: Source (no incoming edge, root at
// column 0 — Blueprint) or Sink (no outgoing edge, root at the max column —
// Material). The Sink role does NOT flip the X axis; longest-path already lands
// the output node at the rightmost column. Sink only (a) changes root derivation
// and (b) skips the Source-only "lone-getter pull" X pass.
//
// The core NEVER reads existing positions: positions are a pure function of
// (ordered edges, sizes, tunables), so re-running is idempotent.
//
// Copyright (c) 2025 MCP Automation Bridge Contributors
// SPDX-License-Identifier: MIT
// =============================================================================

#include "McpGraphLayoutUtils.h"

namespace McpGraphLayout
{
namespace
{
// Longest-path column: column = 1 + max(predecessor columns). Memoised; the
// Visiting set turns any back-edge in a cycle into a no-op so it terminates.
// (Ported verbatim from ArrangeColumn, BlueprintGraphHandlers.cpp:259-278.)
int32 ArrangeColumn(FNodeId Node,
                    const TMap<FNodeId, TArray<FNodeId>>& ColPred,
                    TMap<FNodeId, int32>& Col,
                    TSet<FNodeId>& Visiting)
{
    if (const int32* Cached = Col.Find(Node)) { return *Cached; }
    if (Visiting.Contains(Node)) { return 0; }
    Visiting.Add(Node);
    int32 Best = 0;
    if (const TArray<FNodeId>* Preds = ColPred.Find(Node))
    {
        for (FNodeId P : *Preds)
        {
            Best = FMath::Max(Best, ArrangeColumn(P, ColPred, Col, Visiting) + 1);
        }
    }
    Visiting.Remove(Node);
    Col.Add(Node, Best);
    return Best;
}

// DFS row assignment: leaves consume the monotonic cursor, parents center on
// their children. Visited guard makes shared feeders / cycles safe (placed once).
// (Ported verbatim from ArrangeRow, BlueprintGraphHandlers.cpp:282-303.)
float ArrangeRow(FNodeId Node,
                 const TMap<FNodeId, TArray<FNodeId>>& Children,
                 TMap<FNodeId, float>& Row,
                 TSet<FNodeId>& Visited,
                 float& Cursor)
{
    if (const float* Cached = Row.Find(Node)) { return *Cached; }
    const TArray<FNodeId>* Kids = Children.Find(Node);
    if (Visited.Contains(Node) || !Kids || Kids->Num() == 0)
    {
        const float Leaf = Cursor; Cursor += 1.0f; Row.Add(Node, Leaf); return Leaf;
    }
    Visited.Add(Node);
    float Sum = 0.0f;
    for (FNodeId K : *Kids)
    {
        Sum += ArrangeRow(K, Children, Row, Visited, Cursor);
    }
    const float Centered = Sum / Kids->Num();
    Row.Add(Node, Centered);
    return Centered;
}
} // namespace

FMcpGraphLayoutResult LayoutGraph(const FMcpGraphLayoutInput& In)
{
    FMcpGraphLayoutResult Result;
    if (In.Nodes.Num() == 0) { return Result; }

    // Stable node-id list in input order, plus a per-node size lookup.
    TArray<FNodeId> Nodes;
    Nodes.Reserve(In.Nodes.Num());
    TMap<FNodeId, FVector2D> Size;
    Size.Reserve(In.Nodes.Num());
    for (const FLayoutNode& N : In.Nodes)
    {
        Nodes.Add(N.Id);
        Size.Add(N.Id, N.Size);
    }

    // Build adjacency from the edge list (replaces the K2 pin-walk).
    //   ColPred  : X — every edge's From is a predecessor of To.
    //   Children : Y — centering-edges only (subset of ColPred).
    //   PureConsumers : structural "lone getter" (From has no incoming edge) ->
    //                   nodes it feeds, used by the Source-only lone-getter pull.
    TMap<FNodeId, TArray<FNodeId>> ColPred;
    TMap<FNodeId, TArray<FNodeId>> Children;
    TMap<FNodeId, TArray<FNodeId>> PureConsumers;

    // First pass: ColPred (so we can tell which From nodes have no incoming edge).
    for (const FLayoutEdge& E : In.Edges)
    {
        if (E.FromId == INDEX_NONE || E.ToId == INDEX_NONE || E.FromId == E.ToId) { continue; }
        ColPred.FindOrAdd(E.ToId).AddUnique(E.FromId);
    }
    // Second pass: Children + PureConsumers (centering edges only).
    // The centering tree must flow AWAY from the root so the root centers over
    // its neighbours instead of becoming a leaf: for a Source root it runs
    // source->consumer (a parent centers over its downstream children); for a
    // Sink root it runs consumer->source (the sink centers over its inputs).
    // Without this flip a Sink root (e.g. a material output) is a leaf in the
    // tree and top-aligns instead of centering over the expressions feeding it.
    const bool bSinkCentering = (In.Orientation.RootRole == ERootRole::Sink);
    for (const FLayoutEdge& E : In.Edges)
    {
        if (E.FromId == INDEX_NONE || E.ToId == INDEX_NONE || E.FromId == E.ToId) { continue; }
        if (!E.bContributesToRowCentering) { continue; }
        const bool bParentIsTo = bSinkCentering || E.bCenterParentIsTo;
        const FNodeId ParentId = bParentIsTo ? E.ToId   : E.FromId;
        const FNodeId ChildId  = bParentIsTo ? E.FromId : E.ToId;
        Children.FindOrAdd(ParentId).AddUnique(ChildId);
        if (E.bCenterParentIsTo)   // pure feeder -> lone-getter candidate for the Source X pass 2
        {
            PureConsumers.FindOrAdd(E.FromId).AddUnique(E.ToId);
        }
    }

    // Root derivation. Explicit roots (RootId + AdditionalRoots) take priority,
    // in supplied order, then remaining nodes in input order. Otherwise derive
    // structurally by polarity: Source = no incoming edge; Sink = no outgoing.
    const bool bExplicitRoots = (In.RootId != INDEX_NONE) || (In.AdditionalRoots.Num() > 0);
    TArray<FNodeId> Roots;
    if (bExplicitRoots)
    {
        if (In.RootId != INDEX_NONE) { Roots.AddUnique(In.RootId); }
        for (FNodeId R : In.AdditionalRoots) { if (R != INDEX_NONE) { Roots.AddUnique(R); } }
        for (FNodeId N : Nodes) { Roots.AddUnique(N); }
    }
    else if (In.Orientation.RootRole == ERootRole::Sink)
    {
        // Sink root: a node that never appears as an edge From (no outgoing edge).
        TSet<FNodeId> HasOutgoing;
        for (const FLayoutEdge& E : In.Edges)
        {
            if (E.FromId != INDEX_NONE && E.ToId != INDEX_NONE && E.FromId != E.ToId)
            {
                HasOutgoing.Add(E.FromId);
            }
        }
        for (FNodeId N : Nodes) { if (!HasOutgoing.Contains(N)) { Roots.Add(N); } }
    }
    else
    {
        // Source root: a node with no incoming edge (no entry in ColPred).
        for (FNodeId N : Nodes) { if (!ColPred.Contains(N)) { Roots.Add(N); } }
    }

    // X pass 1 — longest-path columns over the combined DAG (cycle guard kept).
    TMap<FNodeId, int32> Col;
    {
        TSet<FNodeId> Visiting;
        for (FNodeId N : Nodes) { ArrangeColumn(N, ColPred, Col, Visiting); }
    }

    // X pass 2 — lone-getter pull (Source only). A getter with no inputs would
    // land in column 0 beside the roots; pull it to just-left-of its leftmost
    // consumer instead. Skipped entirely for Sink (no symmetric case, and no
    // X-axis flip — longest-path already anchors the output at the max column).
    TMap<FNodeId, int32> FinalCol = Col;
    if (In.Orientation.RootRole == ERootRole::Source)
    {
        for (FNodeId N : Nodes)
        {
            const TArray<FNodeId>* Cons = PureConsumers.Find(N);
            if (!Cons || Cons->Num() == 0) { continue; }
            int32 MinConsumerCol = MAX_int32;
            for (FNodeId C : *Cons) { MinConsumerCol = FMath::Min(MinConsumerCol, Col.FindRef(C)); }
            FinalCol.Add(N, FMath::Max(Col.FindRef(N), MinConsumerCol - 1));
        }
    }

    // Y — non-resetting cursor + parent-centering from each root, then any nodes
    // not reached from a root (orphans / disconnected) get their own rows.
    TMap<FNodeId, float> Row;
    {
        TSet<FNodeId> Visited;
        float Cursor = 0.0f;
        for (FNodeId R : Roots) { ArrangeRow(R, Children, Row, Visited, Cursor); }
        for (FNodeId N : Nodes)
        {
            if (!Row.Contains(N)) { ArrangeRow(N, Children, Row, Visited, Cursor); }
        }
    }

    // Size-aware placement. Major axis: cumulative column origins from per-column
    // max widths. Minor axis: within each column, nodes keep their computed row
    // as the target but get pushed down until they clear the previous bottom.
    int32 MaxCol = 0;
    for (FNodeId N : Nodes) { MaxCol = FMath::Max(MaxCol, FinalCol.FindRef(N)); }

    TArray<float> ColWidth;
    ColWidth.Init(0.f, MaxCol + 1);
    for (FNodeId N : Nodes)
    {
        const int32 C = FinalCol.FindRef(N);
        ColWidth[C] = FMath::Max(ColWidth[C], (float)Size.FindRef(N).X);
    }
    TArray<float> ColX;
    ColX.Init(0.f, MaxCol + 1);
    for (int32 C = 1; C <= MaxCol; ++C)
    {
        ColX[C] = ColX[C - 1] + ColWidth[C - 1] + In.GapMajor;
    }

    // Determinism fix 1: index columns by integer, never TMap hash order.
    TArray<TArray<FNodeId>> ByCol;
    ByCol.SetNum(MaxCol + 1);
    for (FNodeId N : Nodes) { ByCol[FinalCol.FindRef(N)].Add(N); }

    int32 Count = 0;
    for (int32 C = 0; C <= MaxCol; ++C)
    {
        TArray<FNodeId>& Column = ByCol[C];
        // Determinism fix 2: total order — sort by Row, tie-break on FNodeId.
        Column.Sort([&Row](const FNodeId& A, const FNodeId& B)
        {
            const float RA = Row.FindRef(A);
            const float RB = Row.FindRef(B);
            if (RA != RB) { return RA < RB; }
            return A < B;
        });

        float PrevBottom = -FLT_MAX;
        for (FNodeId N : Column)
        {
            const float TargetY = Row.FindRef(N) * In.RowStep;
            const float Y = FMath::Max(TargetY, PrevBottom);

            // Axis emit (final step): Horizontal => (ColX, Y); Vertical => swap.
            const float Major = ColX[C];
            FVector2D Pos = (In.Orientation.Axis == ELayoutAxis::Horizontal)
                ? FVector2D(Major, Y)
                : FVector2D(Y, Major);
            // Round inside the core so Positions are integral and stable.
            Pos.X = (float)FMath::RoundToInt(Pos.X);
            Pos.Y = (float)FMath::RoundToInt(Pos.Y);
            Result.Positions.Add(N, Pos);

            PrevBottom = Y + (float)Size.FindRef(N).Y + In.GapMinor;
            ++Count;
        }
    }

    Result.NumPlaced = Count;
    Result.MaxColumn = MaxCol;
    return Result;
}

} // namespace McpGraphLayout
