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
    // Tree gap: bump the cursor only when a root actually consumed rows —
    // explicit-root mode appends every node as a fallback root, and a cached
    // root must not widen the lane it already lives in.
    TMap<FNodeId, float> Row;
    {
        TSet<FNodeId> Visited;
        float Cursor = 0.0f;
        for (FNodeId R : Roots)
        {
            const float Before = Cursor;
            ArrangeRow(R, Children, Row, Visited, Cursor);
            if (Cursor > Before) { Cursor += In.TreeGapRows; }
        }
        for (FNodeId N : Nodes)
        {
            if (Row.Contains(N)) { continue; }
            const float Before = Cursor;
            ArrangeRow(N, Children, Row, Visited, Cursor);
            if (Cursor > Before) { Cursor += In.TreeGapRows; }
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

TArray<FOverlapPair> DetectOverlaps(const TArray<FNodeRect>& Rects, float Slack)
{
    TArray<FOverlapPair> Out;
    for (int32 I = 0; I < Rects.Num(); ++I)
    {
        const FVector2D AMin = Rects[I].Pos + FVector2D(Slack, Slack);
        const FVector2D AMax = Rects[I].Pos + Rects[I].Size - FVector2D(Slack, Slack);
        if (AMax.X <= AMin.X || AMax.Y <= AMin.Y) { continue; }   // deflated to nothing
        for (int32 J = I + 1; J < Rects.Num(); ++J)
        {
            const FVector2D BMin = Rects[J].Pos + FVector2D(Slack, Slack);
            const FVector2D BMax = Rects[J].Pos + Rects[J].Size - FVector2D(Slack, Slack);
            if (BMax.X <= BMin.X || BMax.Y <= BMin.Y) { continue; }
            if (AMin.X < BMax.X && BMin.X < AMax.X &&
                AMin.Y < BMax.Y && BMin.Y < AMax.Y)
            {
                Out.Add({Rects[I].Id, Rects[J].Id});
            }
        }
    }
    return Out;
}

FVector2D PlaceBlock(const TArray<FNodeRect>& Block,
                     const TArray<FNodeRect>& Obstacles,
                     const FVector2D& AnchorTopLeft,
                     float ClearGap)
{
    if (Block.Num() == 0) { return FVector2D::ZeroVector; }

    FVector2D Min(FLT_MAX, FLT_MAX);
    FVector2D Max(-FLT_MAX, -FLT_MAX);
    for (const FNodeRect& R : Block)
    {
        Min.X = FMath::Min(Min.X, R.Pos.X);
        Min.Y = FMath::Min(Min.Y, R.Pos.Y);
        Max.X = FMath::Max(Max.X, R.Pos.X + R.Size.X);
        Max.Y = FMath::Max(Max.Y, R.Pos.Y + R.Size.Y);
    }

    FVector2D T = AnchorTopLeft - Min;

    // Downward-only clearing: each pass jumps the block top below the bottom
    // of the lowest currently-intersecting obstacle, so a cleared obstacle can
    // never re-intersect and the loop runs at most Obstacles.Num() passes.
    bool bMoved = true;
    while (bMoved)
    {
        bMoved = false;
        const float BlockTop = Min.Y + T.Y;
        const FVector2D BMin = Min + T - FVector2D(ClearGap, ClearGap);
        const FVector2D BMax = Max + T + FVector2D(ClearGap, ClearGap);
        float RequiredTop = BlockTop;
        for (const FNodeRect& O : Obstacles)
        {
            if (O.Pos.X < BMax.X && BMin.X < O.Pos.X + O.Size.X &&
                O.Pos.Y < BMax.Y && BMin.Y < O.Pos.Y + O.Size.Y)
            {
                RequiredTop = FMath::Max(RequiredTop, O.Pos.Y + O.Size.Y + ClearGap);
            }
        }
        if (RequiredTop > BlockTop)
        {
            T.Y += RequiredTop - BlockTop;
            bMoved = true;
        }
    }

    T.X = (float)FMath::RoundToInt(T.X);
    T.Y = (float)FMath::RoundToInt(T.Y);
    return T;
}

namespace
{
// Liang-Barsky clip of segment [A,B] to rect [RMin,RMax]. True when a
// non-degenerate portion lies inside; T0/T1 bound that portion parametrically.
bool ClipSegmentToRect(const FVector2D& A, const FVector2D& B,
                       const FVector2D& RMin, const FVector2D& RMax,
                       double& T0, double& T1)
{
    T0 = 0.0;
    T1 = 1.0;
    const double DX = B.X - A.X;
    const double DY = B.Y - A.Y;
    const double P[4] = { -DX, DX, -DY, DY };
    const double Q[4] = { A.X - RMin.X, RMax.X - A.X, A.Y - RMin.Y, RMax.Y - A.Y };
    for (int32 I = 0; I < 4; ++I)
    {
        if (P[I] == 0.0)
        {
            if (Q[I] < 0.0) { return false; }   // parallel and outside this slab
        }
        else
        {
            const double R = Q[I] / P[I];
            if (P[I] < 0.0)
            {
                if (R > T1) { return false; }
                if (R > T0) { T0 = R; }
            }
            else
            {
                if (R < T0) { return false; }
                if (R < T1) { T1 = R; }
            }
        }
    }
    return T1 > T0;
}

double Orient(const FVector2D& A, const FVector2D& B, const FVector2D& C)
{
    return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
}

// Proper crossing only: each segment strictly separates the other's endpoints.
// Any collinearity or endpoint touch (an orientation of exactly 0) is not a
// crossing — chords touching at shared coordinates are layout noise.
bool SegmentsProperlyCross(const FVector2D& A, const FVector2D& B,
                           const FVector2D& C, const FVector2D& D)
{
    const double O1 = Orient(A, B, C);
    const double O2 = Orient(A, B, D);
    const double O3 = Orient(C, D, A);
    const double O4 = Orient(C, D, B);
    if (O1 == 0.0 || O2 == 0.0 || O3 == 0.0 || O4 == 0.0) { return false; }
    return ((O1 > 0.0) != (O2 > 0.0)) && ((O3 > 0.0) != (O4 > 0.0));
}
} // namespace

FWireQualityReport AnalyzeWires(const TArray<FWireSegment>& Wires,
                                const TArray<FNodeRect>& Rects,
                                float Slack)
{
    FWireQualityReport Report;

    for (int32 W = 0; W < Wires.Num(); ++W)
    {
        const FWireSegment& Wire = Wires[W];
        for (const FNodeRect& R : Rects)
        {
            if (R.Id == Wire.FromNode || R.Id == Wire.ToNode) { continue; }
            const FVector2D RMin = R.Pos + FVector2D(Slack, Slack);
            const FVector2D RMax = R.Pos + R.Size - FVector2D(Slack, Slack);
            if (RMax.X <= RMin.X || RMax.Y <= RMin.Y) { continue; }   // deflated away
            double T0, T1;
            if (ClipSegmentToRect(Wire.FromAnchor, Wire.ToAnchor, RMin, RMax, T0, T1))
            {
                const double Cut = (Wire.ToAnchor - Wire.FromAnchor).Size() * (T1 - T0);
                if (Cut > 0.0)
                {
                    Report.ThroughNodes.Add({W, R.Id, (float)Cut});
                }
            }
        }
    }
    Report.ThroughNodes.Sort([](const FWireThroughNode& A, const FWireThroughNode& B)
    {
        if (A.CutLength != B.CutLength) { return A.CutLength > B.CutLength; }
        if (A.WireIndex != B.WireIndex) { return A.WireIndex < B.WireIndex; }
        return A.NodeId < B.NodeId;
    });

    for (int32 I = 0; I < Wires.Num(); ++I)
    {
        for (int32 J = I + 1; J < Wires.Num(); ++J)
        {
            const FWireSegment& A = Wires[I];
            const FWireSegment& B = Wires[J];
            if (A.FromNode == B.FromNode || A.FromNode == B.ToNode ||
                A.ToNode == B.FromNode || A.ToNode == B.ToNode)
            {
                continue;   // wires meeting at a node fan out, they don't "cross"
            }
            if (SegmentsProperlyCross(A.FromAnchor, A.ToAnchor, B.FromAnchor, B.ToAnchor))
            {
                ++Report.NumCrossings;
            }
        }
    }
    return Report;
}

FVector2D EstimatePinAnchor(const FNodeRect& Node, int32 VisibleRowIndex,
                            bool bOutput, const FPinAnchorParams& Params)
{
    const double X = bOutput ? (Node.Pos.X + Node.Size.X) : Node.Pos.X;
    double Y = Node.Pos.Y + Params.TitleBand +
               VisibleRowIndex * Params.RowPitch + Params.RowPitch * 0.5;
    Y = FMath::Clamp(Y, (double)Node.Pos.Y, Node.Pos.Y + Node.Size.Y);
    return FVector2D(X, Y);
}

} // namespace McpGraphLayout
