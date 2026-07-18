// =============================================================================
// McpGraphLayoutUtils.h
// =============================================================================
// Pure, UE-agnostic graph-layout core (Option A).
//
// A layered Sugiyama-ish layout extracted from the Blueprint graph arranger so
// that both the Blueprint and Material adapters can share one implementation.
// The core knows nothing about EdGraph / Material / editor types: callers map
// their own nodes to dense int32 ids (FNodeId), describe edges + per-node sizes,
// and receive integral top-left positions back.
//
// This header is intentionally NOT wrapped in WITH_EDITOR — it only depends on
// Core (CoreMinimal + FVector2D) so it can be unit-tested headlessly.
//
// Copyright (c) 2025 MCP Automation Bridge Contributors
// SPDX-License-Identifier: MIT
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "Math/Vector2D.h"

namespace McpGraphLayout
{
    // Adapter owns the int32<->UObject* map; ids are call-local and dense.
    using FNodeId = int32;

    // Horizontal (default): column -> X, row -> Y. Vertical swaps the axes.
    enum class ELayoutAxis : uint8 { Horizontal, Vertical };

    // Source = Blueprint (root in column 0). Sink = Material (root at max column).
    enum class ERootRole : uint8 { Source, Sink };

    struct FLayoutOrientation
    {
        ELayoutAxis Axis     = ELayoutAxis::Horizontal;
        ERootRole   RootRole = ERootRole::Source;
    };

    struct FLayoutEdge
    {
        FNodeId FromId = INDEX_NONE;   // one column closer to source
        FNodeId ToId   = INDEX_NONE;   // one column further from source
        // Y tree subset: BP exec-successor + pure-feeder only; Material: ALL true.
        bool bContributesToRowCentering = true;
        // Centering-parent override: false (default) => parent = FromId (exec /
        // source behaviour); true => parent = ToId, i.e. the consumer centers over
        // this feeder. Lets the Source role mix exec (parent=source) and pure-data
        // (parent=consumer) the way the original Blueprint arranger did. Ignored
        // for the Sink role (RootRole==Sink already forces parent=ToId).
        bool bCenterParentIsTo = false;
    };

    struct FLayoutNode
    {
        FNodeId   Id   = INDEX_NONE;
        FVector2D Size = FVector2D(224.f, 64.f);   // adapter-estimated
        FString   Label;                            // diagnostics only
    };

    struct FMcpGraphLayoutInput
    {
        TArray<FLayoutNode> Nodes;
        TArray<FLayoutEdge> Edges;
        FNodeId RootId = INDEX_NONE;                // optional; INDEX_NONE => core derives roots structurally
        TArray<FNodeId> AdditionalRoots;
        FLayoutOrientation Orientation;
        float RowStep  = 180.f;   // ArrangeRowStepY
        float GapMajor = 96.f;    // ArrangeGapX (between columns)
        float GapMinor = 48.f;    // ArrangeGapY (within a column)
    };

    struct FMcpGraphLayoutResult
    {
        TMap<FNodeId, FVector2D> Positions;   // node -> integral top-left
        int32 NumPlaced = 0;
        int32 MaxColumn = 0;
    };

    FMcpGraphLayoutResult LayoutGraph(const FMcpGraphLayoutInput& In);

    // ---- Estimated-geometry passes (overlap report + scoped arrange) --------

    // Axis-aligned node extent: Pos is the top-left, Size the adapter's
    // estimate (no Slate geometry exists headless, so all of this is
    // approximate by construction).
    struct FNodeRect
    {
        FNodeId   Id   = INDEX_NONE;
        FVector2D Pos  = FVector2D::ZeroVector;
        FVector2D Size = FVector2D::ZeroVector;
    };

    struct FOverlapPair
    {
        FNodeId A = INDEX_NONE;
        FNodeId B = INDEX_NONE;
    };

    // Pairwise AABB interpenetration. Each rect is deflated by Slack on every
    // side first: with estimated sizes a near-touching pair is measurement
    // noise, not a finding. Exact edge contact never counts. Pairs preserve
    // input order (A earlier than B).
    TArray<FOverlapPair> DetectOverlaps(const TArray<FNodeRect>& Rects, float Slack);

    // Rigid-block placement for scoped arrange. Translates Block (as one
    // unit) so its bounding box's top-left lands on AnchorTopLeft, then
    // pushes it straight down until the bbox clears every obstacle by at
    // least ClearGap. Returns the integral translation to add to each block
    // Pos; obstacles are never moved.
    FVector2D PlaceBlock(const TArray<FNodeRect>& Block,
                         const TArray<FNodeRect>& Obstacles,
                         const FVector2D& AnchorTopLeft,
                         float ClearGap);
}
