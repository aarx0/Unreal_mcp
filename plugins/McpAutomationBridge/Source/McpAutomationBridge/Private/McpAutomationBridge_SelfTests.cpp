// =============================================================================
// McpAutomationBridge_SelfTests.cpp
// =============================================================================
// Seed automation tests for the MCP bridge's TDD runner.
//
// These exist so `system_control run_tests` / `get_test_results` have something
// project-local and deterministic to execute end-to-end:
//   - McpBridge.SelfTest.Pass    : a trivial synchronous pass
//   - McpBridge.SelfTest.Latent  : exercises the multi-frame latent-command
//                                  path (TickAutomationTestRun advances one
//                                  ExecuteLatentCommands() per editor frame)
//   - McpBridge.SelfTest.Math    : a parameterized (complex) test, so the
//                                  "TestName Parameter" command form is covered
//
// Run them headlessly with:
//   system_control { action: "run_tests", filter: "McpBridge.SelfTest" }
//   system_control { action: "get_test_results", runId: "<id>" }
//
// They live in the bridge plugin module (not the game module) on purpose: the
// fork must not modify the main project's source.
// =============================================================================

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

// -----------------------------------------------------------------------------
// Trivial synchronous test
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpBridgeSelfTestPass, "McpBridge.SelfTest.Pass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpBridgeSelfTestPass::RunTest(const FString &Parameters)
{
    TestEqual(TEXT("arithmetic"), 1 + 1, 2);
    TestTrue(TEXT("boolean"), true);
    TestEqual(TEXT("string"), FString(TEXT("mcp")), FString(TEXT("mcp")));
    return true;
}

// -----------------------------------------------------------------------------
// Latent test: completes after a few frames to verify the runner ticks
// ExecuteLatentCommands() across multiple frames instead of expecting an
// instant result.
// -----------------------------------------------------------------------------
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FMcpWaitFramesLatentCommand,
                                               int32, FramesRemaining);

bool FMcpWaitFramesLatentCommand::Update()
{
    if (FramesRemaining > 0)
    {
        --FramesRemaining;
        return false; // not done yet — run again next frame
    }
    return true; // done
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpBridgeSelfTestLatent, "McpBridge.SelfTest.Latent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpBridgeSelfTestLatent::RunTest(const FString &Parameters)
{
    TestTrue(TEXT("pre-latent"), true);
    ADD_LATENT_AUTOMATION_COMMAND(FMcpWaitFramesLatentCommand(3));
    return true;
}

// -----------------------------------------------------------------------------
// Complex (parameterized) test: produces multiple entries whose command name is
// "FMcpBridgeSelfTestMath <Param>", exercising StartTestByName's name/param
// split path used by the runner.
// -----------------------------------------------------------------------------
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FMcpBridgeSelfTestMath, "McpBridge.SelfTest.Math",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FMcpBridgeSelfTestMath::GetTests(TArray<FString> &OutBeautifiedNames,
                                      TArray<FString> &OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("Zero"));
    OutTestCommands.Add(TEXT("0"));
    OutBeautifiedNames.Add(TEXT("Five"));
    OutTestCommands.Add(TEXT("5"));
}

bool FMcpBridgeSelfTestMath::RunTest(const FString &Parameters)
{
    const int32 N = FCString::Atoi(*Parameters);
    TestEqual(TEXT("square is non-negative"), N * N >= 0, true);
    return true;
}

#endif // WITH_AUTOMATION_TESTS
