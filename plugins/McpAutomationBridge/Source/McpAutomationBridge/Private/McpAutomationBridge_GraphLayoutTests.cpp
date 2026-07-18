// =============================================================================
// McpAutomationBridge_GraphLayoutTests.cpp
// =============================================================================
// Behavioral spec for the estimated-geometry passes in McpGraphLayoutUtils:
// DetectOverlaps (the passive overlap report) and PlaceBlock (scoped
// arrange_graph's rigid-block placement).
//
// Run headlessly with:
//   system_control { action: "run_tests", filter: "McpBridge.GraphLayout" }
// =============================================================================

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "McpGraphLayoutUtils.h"

namespace McpGraphLayoutTestUtil
{
    static McpGraphLayout::FNodeRect Rect(int32 Id, float X, float Y, float W, float H)
    {
        McpGraphLayout::FNodeRect R;
        R.Id = Id;
        R.Pos = FVector2D(X, Y);
        R.Size = FVector2D(W, H);
        return R;
    }

    static McpGraphLayout::FWireSegment Wire(int32 FromNode, int32 ToNode,
                                             float X0, float Y0, float X1, float Y1)
    {
        McpGraphLayout::FWireSegment W;
        W.FromNode = FromNode;
        W.ToNode = ToNode;
        W.FromAnchor = FVector2D(X0, Y0);
        W.ToAnchor = FVector2D(X1, Y1);
        return W;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpGraphLayoutDetectOverlaps, "McpBridge.GraphLayout.DetectOverlaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpGraphLayoutDetectOverlaps::RunTest(const FString& Parameters)
{
    using namespace McpGraphLayout;
    using namespace McpGraphLayoutTestUtil;

    // Disjoint rects never report.
    {
        const TArray<FOverlapPair> P = DetectOverlaps(
            {Rect(0, 0, 0, 100, 50), Rect(1, 200, 0, 100, 50)}, 0.f);
        TestEqual(TEXT("disjoint"), P.Num(), 0);
    }

    // Deep interpenetration reports once, ids in input order.
    {
        const TArray<FOverlapPair> P = DetectOverlaps(
            {Rect(7, 0, 0, 100, 50), Rect(9, 40, 20, 100, 50)}, 0.f);
        TestEqual(TEXT("deep overlap count"), P.Num(), 1);
        if (P.Num() == 1)
        {
            TestEqual(TEXT("deep overlap A"), P[0].A, 7);
            TestEqual(TEXT("deep overlap B"), P[0].B, 9);
        }
    }

    // Exact edge contact is not an overlap even with zero slack.
    {
        const TArray<FOverlapPair> P = DetectOverlaps(
            {Rect(0, 0, 0, 100, 50), Rect(1, 100, 0, 100, 50)}, 0.f);
        TestEqual(TEXT("edge contact"), P.Num(), 0);
    }

    // Slack forgives shallow interpenetration: 4px in X vanishes after each
    // rect deflates 8px per side.
    {
        const TArray<FOverlapPair> P = DetectOverlaps(
            {Rect(0, 0, 0, 100, 50), Rect(1, 96, 0, 100, 50)}, 8.f);
        TestEqual(TEXT("shallow forgiven"), P.Num(), 0);
    }

    // ...but the same pair reports at zero slack.
    {
        const TArray<FOverlapPair> P = DetectOverlaps(
            {Rect(0, 0, 0, 100, 50), Rect(1, 96, 0, 100, 50)}, 0.f);
        TestEqual(TEXT("shallow strict"), P.Num(), 1);
    }

    // A three-node pile-up reports every pair.
    {
        const TArray<FOverlapPair> P = DetectOverlaps(
            {Rect(0, 0, 0, 100, 50), Rect(1, 10, 10, 100, 50), Rect(2, 20, 20, 100, 50)}, 0.f);
        TestEqual(TEXT("pile-up pairs"), P.Num(), 3);
    }

    // A rect deflated to nothing (smaller than 2*Slack) never reports.
    {
        const TArray<FOverlapPair> P = DetectOverlaps(
            {Rect(0, 0, 0, 10, 10), Rect(1, 0, 0, 100, 50)}, 8.f);
        TestEqual(TEXT("degenerate rect"), P.Num(), 0);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpGraphLayoutPlaceBlock, "McpBridge.GraphLayout.PlaceBlock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpGraphLayoutPlaceBlock::RunTest(const FString& Parameters)
{
    using namespace McpGraphLayout;
    using namespace McpGraphLayoutTestUtil;

    // Empty block: no translation.
    {
        const FVector2D T = PlaceBlock({}, {}, FVector2D(500, 100), 10.f);
        TestTrue(TEXT("empty block"), T.Equals(FVector2D::ZeroVector, 0.01f));
    }

    // No obstacles: pure anchor translation (block bbox top-left -> anchor).
    {
        const FVector2D T = PlaceBlock(
            {Rect(0, 0, 0, 100, 50), Rect(1, 150, 80, 100, 50)}, {},
            FVector2D(500, 100), 10.f);
        TestTrue(TEXT("anchor only"), T.Equals(FVector2D(500, 100), 0.01f));
    }

    // One obstacle under the anchor: block slides below its bottom + gap.
    // Block lands at y=100, obstacle spans y=[90,190], gap 10 => top at 200.
    {
        const FVector2D T = PlaceBlock(
            {Rect(0, 0, 0, 100, 50)},
            {Rect(-1, 480, 90, 200, 100)},
            FVector2D(500, 100), 10.f);
        TestTrue(TEXT("single obstacle"), T.Equals(FVector2D(500, 200), 0.01f));
    }

    // Stacked obstacles cascade: after clearing the first (top 200), the
    // second (y=[200,260]) still intersects => top settles at 270.
    {
        const FVector2D T = PlaceBlock(
            {Rect(0, 0, 0, 100, 50)},
            {Rect(-1, 480, 90, 200, 100), Rect(-1, 480, 200, 200, 60)},
            FVector2D(500, 100), 10.f);
        TestTrue(TEXT("cascade"), T.Equals(FVector2D(500, 270), 0.01f));
    }

    // An obstacle outside the block's X range never pushes.
    {
        const FVector2D T = PlaceBlock(
            {Rect(0, 0, 0, 100, 50)},
            {Rect(-1, 900, 90, 200, 100)},
            FVector2D(500, 100), 10.f);
        TestTrue(TEXT("x-disjoint obstacle"), T.Equals(FVector2D(500, 100), 0.01f));
    }

    // The gap counts: an obstacle clearing the block by less than ClearGap
    // still pushes. Obstacle bottom y=95, block top y=100, gap 10 => top 105.
    {
        const FVector2D T = PlaceBlock(
            {Rect(0, 0, 0, 100, 50)},
            {Rect(-1, 480, 45, 200, 50)},
            FVector2D(500, 100), 10.f);
        TestTrue(TEXT("gap enforced"), T.Equals(FVector2D(500, 105), 0.01f));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpGraphLayoutTreeGap, "McpBridge.GraphLayout.TreeGap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpGraphLayoutTreeGap::RunTest(const FString& Parameters)
{
    using namespace McpGraphLayout;

    // Two independent exec chains 0->1 and 2->3 (one row each).
    auto MakeInput = [](float TreeGapRows)
    {
        FMcpGraphLayoutInput In;
        In.TreeGapRows = TreeGapRows;
        for (int32 I = 0; I < 4; ++I)
        {
            FLayoutNode N;
            N.Id = I;
            N.Size = FVector2D(224, 64);
            In.Nodes.Add(N);
        }
        FLayoutEdge E1; E1.FromId = 0; E1.ToId = 1; In.Edges.Add(E1);
        FLayoutEdge E2; E2.FromId = 2; E2.ToId = 3; In.Edges.Add(E2);
        return In;
    };

    // One empty row between trees: the second tree starts two rows down.
    {
        const FMcpGraphLayoutResult R = LayoutGraph(MakeInput(1.f));
        TestEqual(TEXT("tree 1 row"), (int32)R.Positions.FindRef(0).Y, 0);
        TestEqual(TEXT("tree 2 gapped"), (int32)R.Positions.FindRef(2).Y, 360);
        TestEqual(TEXT("tree 2 chain level"), (int32)R.Positions.FindRef(3).Y, 360);
    }

    // Zero = legacy stacking.
    {
        const FMcpGraphLayoutResult R = LayoutGraph(MakeInput(0.f));
        TestEqual(TEXT("legacy tree 2"), (int32)R.Positions.FindRef(2).Y, 180);
    }

    // Explicit-root mode appends every node as a fallback root; roots that
    // place nothing (already reached) must not add gaps.
    {
        FMcpGraphLayoutInput In = MakeInput(1.f);
        In.RootId = 0;
        const FMcpGraphLayoutResult R = LayoutGraph(In);
        TestEqual(TEXT("cached roots add no gap"), (int32)R.Positions.FindRef(2).Y, 360);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpGraphLayoutAnalyzeWires, "McpBridge.GraphLayout.AnalyzeWires",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpGraphLayoutAnalyzeWires::RunTest(const FString& Parameters)
{
    using namespace McpGraphLayout;
    using namespace McpGraphLayoutTestUtil;

    const TArray<FNodeRect> ABC_Clear = {
        Rect(0, 0, 0, 100, 50), Rect(1, 400, 0, 100, 50), Rect(2, 200, 200, 100, 50)};
    const TArray<FNodeRect> ABC_Blocking = {
        Rect(0, 0, 0, 100, 50), Rect(1, 400, 0, 100, 50), Rect(2, 200, 0, 100, 50)};

    // Wire clear of every non-endpoint rect: nothing reports.
    {
        const FWireQualityReport R =
            AnalyzeWires({Wire(0, 1, 100, 25, 400, 25)}, ABC_Clear, 4.f);
        TestEqual(TEXT("clear through"), R.ThroughNodes.Num(), 0);
        TestEqual(TEXT("clear crossings"), R.NumCrossings, 0);
    }

    // Chord through a mid rect: exact clipped length (rect deflated 4/side:
    // x in [204,296] of a 300px horizontal chord => 92).
    {
        const FWireQualityReport R =
            AnalyzeWires({Wire(0, 1, 100, 25, 400, 25)}, ABC_Blocking, 4.f);
        TestEqual(TEXT("through count"), R.ThroughNodes.Num(), 1);
        if (R.ThroughNodes.Num() == 1)
        {
            TestEqual(TEXT("through wire"), R.ThroughNodes[0].WireIndex, 0);
            TestEqual(TEXT("through node"), R.ThroughNodes[0].NodeId, 2);
            TestTrue(TEXT("through cut"),
                     FMath::IsNearlyEqual(R.ThroughNodes[0].CutLength, 92.f, 0.1f));
        }
    }

    // A wire's own endpoint nodes never report, even when the chord runs
    // straight through them.
    {
        const FWireQualityReport R =
            AnalyzeWires({Wire(0, 2, 100, 25, 250, 25)}, ABC_Blocking, 4.f);
        TestEqual(TEXT("endpoint exempt"), R.ThroughNodes.Num(), 0);
    }

    // Slack forgives a graze along the deflated border.
    {
        const TArray<FNodeRect> Grazed = {
            Rect(0, 0, 0, 100, 50), Rect(1, 400, 0, 100, 50), Rect(2, 200, 10, 100, 50)};
        const FWireQualityReport Miss =
            AnalyzeWires({Wire(0, 1, 100, 12, 400, 12)}, Grazed, 4.f);
        TestEqual(TEXT("graze forgiven"), Miss.ThroughNodes.Num(), 0);
        const FWireQualityReport Hit =
            AnalyzeWires({Wire(0, 1, 100, 25, 400, 25)}, Grazed, 4.f);
        TestEqual(TEXT("deep hit"), Hit.ThroughNodes.Num(), 1);
    }

    // Worst-first ordering: full-span cut beats a corner clip.
    {
        const FWireQualityReport R = AnalyzeWires(
            {Wire(0, 1, 100, 25, 400, 25), Wire(0, 1, 150, 60, 210, 0)},
            ABC_Blocking, 4.f);
        TestEqual(TEXT("worst count"), R.ThroughNodes.Num(), 2);
        if (R.ThroughNodes.Num() == 2)
        {
            TestEqual(TEXT("worst first"), R.ThroughNodes[0].WireIndex, 0);
            TestEqual(TEXT("worst second"), R.ThroughNodes[1].WireIndex, 1);
            TestTrue(TEXT("worst ordering"),
                     R.ThroughNodes[0].CutLength > R.ThroughNodes[1].CutLength);
        }
    }

    // X-pattern between four distinct nodes: one proper crossing.
    {
        const FWireQualityReport R = AnalyzeWires(
            {Wire(0, 1, 0, 0, 100, 100), Wire(2, 3, 0, 100, 100, 0)}, {}, 4.f);
        TestEqual(TEXT("x crossing"), R.NumCrossings, 1);
    }

    // Same X-pattern but the wires share an endpoint node: fan-out, not a crossing.
    {
        const FWireQualityReport R = AnalyzeWires(
            {Wire(0, 1, 0, 0, 100, 100), Wire(1, 3, 0, 100, 100, 0)}, {}, 4.f);
        TestEqual(TEXT("shared endpoint"), R.NumCrossings, 0);
    }

    // Collinear overlap is not a proper crossing.
    {
        const FWireQualityReport R = AnalyzeWires(
            {Wire(0, 1, 0, 0, 100, 0), Wire(2, 3, 50, 0, 150, 0)}, {}, 4.f);
        TestEqual(TEXT("collinear"), R.NumCrossings, 0);
    }

    // Determinism: identical input, identical report.
    {
        const TArray<FWireSegment> Wires = {
            Wire(0, 1, 100, 25, 400, 25), Wire(0, 1, 150, 60, 210, 0)};
        const FWireQualityReport R1 = AnalyzeWires(Wires, ABC_Blocking, 4.f);
        const FWireQualityReport R2 = AnalyzeWires(Wires, ABC_Blocking, 4.f);
        TestEqual(TEXT("det crossings"), R1.NumCrossings, R2.NumCrossings);
        TestEqual(TEXT("det through count"), R1.ThroughNodes.Num(), R2.ThroughNodes.Num());
        for (int32 I = 0; I < FMath::Min(R1.ThroughNodes.Num(), R2.ThroughNodes.Num()); ++I)
        {
            TestEqual(TEXT("det wire"), R1.ThroughNodes[I].WireIndex, R2.ThroughNodes[I].WireIndex);
            TestEqual(TEXT("det node"), R1.ThroughNodes[I].NodeId, R2.ThroughNodes[I].NodeId);
            TestEqual(TEXT("det cut"), R1.ThroughNodes[I].CutLength, R2.ThroughNodes[I].CutLength);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpGraphLayoutPinAnchor, "McpBridge.GraphLayout.PinAnchor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpGraphLayoutPinAnchor::RunTest(const FString& Parameters)
{
    using namespace McpGraphLayout;
    using namespace McpGraphLayoutTestUtil;

    const FNodeRect Node = Rect(0, 100, 200, 224, 120);
    const FPinAnchorParams P;   // TitleBand 46, RowPitch 24

    // Input row 0: left edge, title band + half a row.
    {
        const FVector2D A = EstimatePinAnchor(Node, 0, /*bOutput=*/false, P);
        TestTrue(TEXT("input row 0"), A.Equals(FVector2D(100, 258), 0.01f));
    }

    // Output row 2: right edge, two rows down.
    {
        const FVector2D A = EstimatePinAnchor(Node, 2, /*bOutput=*/true, P);
        TestTrue(TEXT("output row 2"), A.Equals(FVector2D(324, 306), 0.01f));
    }

    // A row beyond the estimated height clamps into the rect.
    {
        const FVector2D A = EstimatePinAnchor(Node, 10, /*bOutput=*/false, P);
        TestTrue(TEXT("row clamps"), A.Equals(FVector2D(100, 320), 0.01f));
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS
