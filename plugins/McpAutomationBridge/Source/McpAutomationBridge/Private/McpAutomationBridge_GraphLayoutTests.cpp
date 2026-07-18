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

#endif // WITH_AUTOMATION_TESTS
