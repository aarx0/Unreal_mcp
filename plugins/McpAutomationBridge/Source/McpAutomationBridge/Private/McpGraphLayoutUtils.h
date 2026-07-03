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
}
