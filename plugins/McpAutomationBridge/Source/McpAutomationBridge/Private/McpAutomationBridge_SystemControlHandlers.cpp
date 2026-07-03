#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Guid.h"
#include "Misc/AutomationTest.h"     // FAutomationTestFramework — real TDD run/poll (run_tests/get_test_results)
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/MonitoredProcess.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Exporters/Exporter.h"
#include "Misc/FileHelper.h"
#include "Interfaces/IPluginManager.h"  // generate_test_stub: resolve the plugin Source dir
#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"           // live_coding_compile: trigger a Live Coding patch
#include "Modules/ModuleManager.h"       // FModuleManager::GetModulePtr<ILiveCodingModule>
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleSystemControlAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  // The sub-action is in the payload's "action" field
  FString SubAction;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("action"), SubAction);
  }
  
  const FString Lower = SubAction.ToLower();
  
  // Check if this handler should process this sub-action
  if (!Lower.StartsWith(TEXT("run_ubt")) &&
      !Lower.StartsWith(TEXT("run_tests")) &&
      Lower != TEXT("list_tests") &&
      Lower != TEXT("get_test_results") &&
      !Lower.StartsWith(TEXT("test_progress")) &&
      !Lower.StartsWith(TEXT("test_stale")) &&
      Lower != TEXT("generate_test_stub") &&
      Lower != TEXT("export_asset") &&
      Lower != TEXT("start_session") &&
      Lower != TEXT("execute_python") &&
      Lower != TEXT("live_coding_compile")) {
    return false; // Not handled by this function
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("System control payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // ===========================================================================
  // generate_test_stub - scaffold a correct, registerable C++ automation test
  // ===========================================================================
  // The author->compile->run loop for a NEW test previously meant hand-writing
  // a .cpp and getting the EAutomationTestFlags incantation right (wrong flags =
  // compiles but never shows under run_tests). This emits the known-good shape
  // (matching the bridge self-tests) so the only remaining step is compiling it
  // in (live_coding_compile picks up new files) and running it.
  if (Lower == TEXT("generate_test_stub")) {
    FString TestName;
    Payload->TryGetStringField(TEXT("testName"), TestName);
    if (TestName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("generate_test_stub requires 'testName' (e.g. 'Combat.DamageApplies')."),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString TestType = TEXT("simple");
    Payload->TryGetStringField(TEXT("testType"), TestType);
    TestType = TestType.ToLower();
    if (TestType != TEXT("simple") && TestType != TEXT("complex") && TestType != TEXT("latent")) {
      SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("Unknown testType '%s'. Use simple, complex, or latent."), *TestType),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Derive a valid C++ class symbol from the test name if not supplied.
    FString ClassName;
    Payload->TryGetStringField(TEXT("className"), ClassName);
    if (ClassName.IsEmpty()) {
      FString Sanitized;
      for (const TCHAR C : TestName) {
        if (FChar::IsAlnum(C)) { Sanitized.AppendChar(C); }
      }
      if (Sanitized.IsEmpty() || !FChar::IsAlpha(Sanitized[0])) {
        Sanitized = FString(TEXT("Gen")) + Sanitized;
      }
      ClassName = FString(TEXT("F")) + Sanitized + TEXT("Test");
    }

    // Default flags match the self-tests so the test appears under run_tests
    // immediately; flagsExpr lets a caller substitute a different mask.
    FString FlagsExpr = TEXT("EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter");
    {
      FString Override;
      if (Payload->TryGetStringField(TEXT("flagsExpr"), Override) && !Override.IsEmpty()) {
        FlagsExpr = Override;
      }
    }

    // Build the file content for the chosen test type.
    FString Body;
    Body += TEXT("// Auto-generated automation test stub (system_control generate_test_stub).\n");
    Body += TEXT("// Compile it in with system_control live_coding_compile (or a full rebuild),\n");
    Body += FString::Printf(TEXT("// then: system_control { action:\"run_tests\", filter:\"%s\" }\n\n"), *TestName);
    Body += TEXT("#include \"Misc/AutomationTest.h\"\n\n");
    Body += TEXT("#if WITH_AUTOMATION_TESTS\n\n");

    if (TestType == TEXT("simple")) {
      Body += FString::Printf(TEXT("IMPLEMENT_SIMPLE_AUTOMATION_TEST(\n    %s, \"%s\",\n    %s)\n\n"), *ClassName, *TestName, *FlagsExpr);
      Body += FString::Printf(TEXT("bool %s::RunTest(const FString& Parameters)\n{\n"), *ClassName);
      Body += TEXT("    // TODO: replace with real assertions.\n");
      Body += TEXT("    TestTrue(TEXT(\"stub placeholder\"), true);\n");
      Body += TEXT("    return true;\n}\n\n");
    } else if (TestType == TEXT("latent")) {
      const FString CmdName = ClassName + TEXT("WaitCommand");
      Body += FString::Printf(TEXT("DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(%s, int32, FramesRemaining);\n\n"), *CmdName);
      Body += FString::Printf(TEXT("bool %s::Update()\n{\n    if (FramesRemaining > 0) { --FramesRemaining; return false; }\n    return true;\n}\n\n"), *CmdName);
      Body += FString::Printf(TEXT("IMPLEMENT_SIMPLE_AUTOMATION_TEST(\n    %s, \"%s\",\n    %s)\n\n"), *ClassName, *TestName, *FlagsExpr);
      Body += FString::Printf(TEXT("bool %s::RunTest(const FString& Parameters)\n{\n"), *ClassName);
      Body += TEXT("    // TODO: kick off async work, then wait for it via the latent command.\n");
      Body += FString::Printf(TEXT("    ADD_LATENT_AUTOMATION_COMMAND(%s(3));\n"), *CmdName);
      Body += TEXT("    return true;\n}\n\n");
    } else { // complex (parameterized)
      Body += FString::Printf(TEXT("IMPLEMENT_COMPLEX_AUTOMATION_TEST(\n    %s, \"%s\",\n    %s)\n\n"), *ClassName, *TestName, *FlagsExpr);
      Body += FString::Printf(TEXT("void %s::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const\n{\n"), *ClassName);
      Body += TEXT("    // TODO: one entry per parameterized case.\n");
      Body += TEXT("    OutBeautifiedNames.Add(TEXT(\"Case0\")); OutTestCommands.Add(TEXT(\"0\"));\n");
      Body += TEXT("    OutBeautifiedNames.Add(TEXT(\"Case1\")); OutTestCommands.Add(TEXT(\"1\"));\n}\n\n");
      Body += FString::Printf(TEXT("bool %s::RunTest(const FString& Parameters)\n{\n"), *ClassName);
      Body += TEXT("    const int32 Case = FCString::Atoi(*Parameters);\n");
      Body += TEXT("    // TODO: assert on the parameterized case.\n");
      Body += TEXT("    TestTrue(TEXT(\"stub placeholder\"), Case >= 0);\n");
      Body += TEXT("    return true;\n}\n\n");
    }

    Body += TEXT("#endif // WITH_AUTOMATION_TESTS\n");

    // Default output: the bridge plugin's GeneratedTests dir (a module that
    // already has WITH_AUTOMATION_TESTS and is live-coding-watched, so no game
    // source is touched). Callers can target the game module via outputPath.
    FString OutputPath;
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
    if (OutputPath.IsEmpty()) {
      FString PluginDir;
      TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("McpAutomationBridge"));
      if (Plugin.IsValid()) { PluginDir = Plugin->GetBaseDir(); }
      if (PluginDir.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Could not resolve the McpAutomationBridge plugin dir; pass an explicit outputPath."),
            TEXT("NO_PLUGIN_DIR"));
        return true;
      }
      OutputPath = PluginDir / TEXT("Source/McpAutomationBridge/Private/GeneratedTests") /
                   (ClassName.RightChop(1) + TEXT(".cpp"));  // strip leading 'F' for the filename
    }
    OutputPath = FPaths::ConvertRelativePathToFull(OutputPath);

    bool bOverwrite = false;
    Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);
    if (FPaths::FileExists(OutputPath) && !bOverwrite) {
      SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("File already exists: %s (pass overwrite:true to replace)."), *OutputPath),
          TEXT("ALREADY_EXISTS"));
      return true;
    }

    // SaveStringToFile does not create parent dirs.
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), /*Tree=*/true);

    if (!FFileHelper::SaveStringToFile(Body, *OutputPath)) {
      SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to write test stub to %s"), *OutputPath),
          TEXT("WRITE_FAILED"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("path"), OutputPath);
    Result->SetStringField(TEXT("className"), ClassName);
    Result->SetStringField(TEXT("testPath"), TestName);
    Result->SetStringField(TEXT("testType"), TestType);
    Result->SetStringField(TEXT("nextSteps"),
        FString::Printf(TEXT("live_coding_compile (new files are picked up), then run_tests filter=\"%s\". A brand-new file in a not-yet-rebuilt module may need one full rebuild before Live Coding sees it."), *TestName));
    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Wrote %s test '%s' to %s"), *TestType, *TestName, *OutputPath),
        Result);
    return true;
  }

  if (Lower == TEXT("start_session")) {
    return HandleInsightsAction(RequestId, TEXT("manage_insights"), Payload, RequestingSocket);
  }

  if (Lower == TEXT("live_coding_compile")) {
    // Trigger a Live Coding compile + patch of the running editor so .cpp-body
    // changes to the bridge (or any module) apply without a close/rebuild/relaunch.
    // Header / Build.cs / .uplugin changes still require a full rebuild.
#if WITH_LIVE_CODING
    ILiveCodingModule *LiveCoding =
        FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
    if (!LiveCoding) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Live Coding module is not loaded"),
                          TEXT("LIVE_CODING_UNAVAILABLE"));
      return true;
    }
    if (!LiveCoding->IsEnabledForSession()) {
      if (!LiveCoding->CanEnableForSession()) {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(
                TEXT("Live Coding cannot be enabled for this session: %s"),
                *LiveCoding->GetEnableErrorText().ToString()),
            TEXT("LIVE_CODING_DISABLED"));
        return true;
      }
      LiveCoding->EnableForSession(true);
    }

    // Blocking compile so the response reflects the real outcome; the patch is
    // applied to the running process before this returns.
    ELiveCodingCompileResult CompileResult = ELiveCodingCompileResult::NotStarted;
    LiveCoding->Compile(ELiveCodingCompileFlags::WaitForCompletion, &CompileResult);

    FString ResultStr;
    switch (CompileResult) {
    case ELiveCodingCompileResult::Success:            ResultStr = TEXT("Success"); break;
    case ELiveCodingCompileResult::NoChanges:          ResultStr = TEXT("NoChanges"); break;
    case ELiveCodingCompileResult::InProgress:         ResultStr = TEXT("InProgress"); break;
    case ELiveCodingCompileResult::CompileStillActive: ResultStr = TEXT("CompileStillActive"); break;
    case ELiveCodingCompileResult::NotStarted:         ResultStr = TEXT("NotStarted"); break;
    case ELiveCodingCompileResult::Failure:            ResultStr = TEXT("Failure"); break;
    case ELiveCodingCompileResult::Cancelled:          ResultStr = TEXT("Cancelled"); break;
    default:                                           ResultStr = TEXT("Unknown"); break;
    }
    const bool bOk = (CompileResult == ELiveCodingCompileResult::Success ||
                      CompileResult == ELiveCodingCompileResult::NoChanges);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bOk);
    Result->SetStringField(TEXT("compileResult"), ResultStr);
    SendAutomationResponse(
        RequestingSocket, RequestId, bOk,
        FString::Printf(TEXT("Live Coding compile: %s"), *ResultStr), Result);
    return true;
#else
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Live Coding is not compiled into this build (WITH_LIVE_CODING=0)"),
        TEXT("LIVE_CODING_UNAVAILABLE"));
    return true;
#endif
  }

  if (Lower == TEXT("run_ubt")) {
    // Extract optional parameters
    FString Target;
    Payload->TryGetStringField(TEXT("target"), Target);
    
    FString Platform;
    Payload->TryGetStringField(TEXT("platform"), Platform);
    
    FString Configuration;
    Payload->TryGetStringField(TEXT("configuration"), Configuration);
    
    FString AdditionalArgs;
    Payload->TryGetStringField(TEXT("additionalArgs"), AdditionalArgs);
    if (AdditionalArgs.IsEmpty()) {
      Payload->TryGetStringField(TEXT("arguments"), AdditionalArgs);
    }

    Target.TrimStartAndEndInline();
    Platform.TrimStartAndEndInline();
    Configuration.TrimStartAndEndInline();
    AdditionalArgs.TrimStartAndEndInline();

    auto ValidateBuildToken = [&](const FString& Value, const TCHAR* FieldName) -> bool {
      if (!McpIsSafeUbtPositionalToken(Value)) {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid %s for run_ubt: %s must be a positional token"), FieldName, *Value),
            TEXT("INVALID_ARGUMENT"));
        return false;
      }
      return true;
    };

    if (Target.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Target is required for run_ubt"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (Platform.IsEmpty()) {
#if PLATFORM_WINDOWS
      Platform = TEXT("Win64");
#elif PLATFORM_MAC
      Platform = TEXT("Mac");
#else
      Platform = TEXT("Linux");
#endif
    }

    if (Configuration.IsEmpty()) {
      Configuration = TEXT("Development");
    }

    if (!ValidateBuildToken(Target, TEXT("target")) ||
        !ValidateBuildToken(Platform, TEXT("platform")) ||
        !ValidateBuildToken(Configuration, TEXT("configuration"))) {
      return true;
    }

    if (!McpIsAllowedUbtPlatform(Platform)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Platform is not allowed for run_ubt: %s"), *Platform),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!McpIsAllowedUbtConfiguration(Configuration)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Configuration is not allowed for run_ubt: %s"), *Configuration),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!McpIsSafeUbtArgumentList(AdditionalArgs)) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("additionalArgs contains unsafe UBT argument characters"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Build UBT path
    FString EngineDir = FPaths::EngineDir();
    FString UBTPath;
    
#if PLATFORM_WINDOWS
    UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Build.bat"));
#elif PLATFORM_MAC
    UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Mac/Build.sh"));
#else
    UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Linux/Build.sh"));
#endif

    if (!FPaths::FileExists(UBTPath)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("UBT not found at: %s"), *UBTPath),
                          TEXT("UBT_NOT_FOUND"));
      return true;
    }

    // Build command line arguments
    FString Arguments;
    
    // Target (project or engine target)
    Arguments += Target + TEXT(" ");
    
    // Platform
    if (!Platform.IsEmpty()) {
      Arguments += Platform + TEXT(" ");
    } else {
#if PLATFORM_WINDOWS
      Arguments += TEXT("Win64 ");
#elif PLATFORM_MAC
      Arguments += TEXT("Mac ");
#else
      Arguments += TEXT("Linux ");
#endif
    }
    
    // Configuration
    if (!Configuration.IsEmpty()) {
      Arguments += Configuration + TEXT(" ");
    } else {
      Arguments += TEXT("Development ");
    }

    const FString ProjectPath = FPaths::GetProjectFilePath();
    if (!ProjectPath.IsEmpty()) {
      Arguments += FString::Printf(TEXT("-Project=\"%s\" "), *ProjectPath);
    }
    
    // Additional args
    if (!AdditionalArgs.IsEmpty()) {
      Arguments += AdditionalArgs;
    }

    // Use FMonitoredProcess for non-blocking execution with output capture
    // For simplicity, we'll use a synchronous approach with timeout
    int32 ReturnCode = -1;
    FString StdOut;
    FString StdErr;
    
    // Note: FPlatformProcess::ExecProcess is simpler but blocks
    // Using CreateProc with pipes for better control
    void* ReadPipe = nullptr;
    void* WritePipe = nullptr;
    FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
    
    FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
        *UBTPath,
        *Arguments,
        false,  // bLaunchDetached
        true,   // bLaunchHidden
        true,   // bLaunchReallyHidden
        nullptr, // OutProcessID
        0,      // PriorityModifier
        nullptr, // OptionalWorkingDirectory
        WritePipe // PipeWriteChild
    );

    if (!ProcessHandle.IsValid()) {
      FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to launch UBT process"),
                          TEXT("PROCESS_LAUNCH_FAILED"));
      return true;
    }

    // Read output with timeout (30 seconds max wait, but check periodically)
    const double TimeoutSeconds = 300.0; // 5 minute timeout for builds
    const double StartTime = FPlatformTime::Seconds();
    
    while (FPlatformProcess::IsProcRunning(ProcessHandle)) {
      // Read available output
      FString NewOutput = FPlatformProcess::ReadPipe(ReadPipe);
      if (!NewOutput.IsEmpty()) {
        StdOut += NewOutput;
      }
      
      // Check timeout
      if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds) {
        FPlatformProcess::TerminateProc(ProcessHandle, true);
        FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
        
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("output"), StdOut);
        Result->SetBoolField(TEXT("timedOut"), true);
        
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("UBT process timed out"), Result,
                               TEXT("TIMEOUT"));
        return true;
      }
      
      // Small sleep to avoid busy waiting
      FPlatformProcess::Sleep(0.1f);
    }

    // Read any remaining output
    FString FinalOutput = FPlatformProcess::ReadPipe(ReadPipe);
    if (!FinalOutput.IsEmpty()) {
      StdOut += FinalOutput;
    }

    // Get return code
    FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
    FPlatformProcess::CloseProc(ProcessHandle);
    FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("output"), StdOut);
    Result->SetNumberField(TEXT("returnCode"), ReturnCode);
    Result->SetStringField(TEXT("ubtPath"), UBTPath);
    Result->SetStringField(TEXT("arguments"), Arguments);

    if (ReturnCode == 0) {
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("UBT completed successfully"), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("UBT failed with code %d"), ReturnCode),
                             Result, TEXT("UBT_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("list_tests")) {
    // Enumerate registered automation tests (optionally substring-filtered).
    // Read-only: no test is started. Useful to discover exact test names/paths
    // before run_tests, and to confirm seeded project tests are visible.
    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);
    Filter.TrimStartAndEndInline();

    FAutomationTestFramework &Framework = FAutomationTestFramework::Get();
    Framework.SetRequestedTestFilter(EAutomationTestFlags_FilterMask);
    TArray<FAutomationTestInfo> AllTests;
    Framework.GetValidTestNames(AllTests);

    TArray<TSharedPtr<FJsonValue>> TestArray;
    int32 Matched = 0;
    for (const FAutomationTestInfo &Info : AllTests) {
      if (!Filter.IsEmpty() &&
          !Info.GetDisplayName().Contains(Filter) &&
          !Info.GetFullTestPath().Contains(Filter) &&
          !Info.GetTestName().Contains(Filter)) {
        continue;
      }
      ++Matched;
      TSharedPtr<FJsonObject> T = MakeShared<FJsonObject>();
      T->SetStringField(TEXT("displayName"), Info.GetDisplayName());
      T->SetStringField(TEXT("fullPath"), Info.GetFullTestPath());
      T->SetStringField(TEXT("testName"), Info.GetTestName());
      TestArray.Add(MakeShared<FJsonValueObject>(T));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("total"), AllTests.Num());
    Result->SetNumberField(TEXT("matched"), Matched);
    Result->SetStringField(TEXT("filter"), Filter);
    Result->SetArrayField(TEXT("tests"), TestArray);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("%d of %d automation tests match."),
                                           Matched, AllTests.Num()),
                           Result);
    return true;
  } else if (Lower == TEXT("run_tests")) {
    // Real TDD runner. Builds a queue of matching tests and returns immediately;
    // the subsystem Tick() drives one ExecuteLatentCommands step per frame via
    // TickAutomationTestRun() so latent/functional tests run over real frames and
    // never block this request handler. Poll results with get_test_results.
    if (ActiveAutomationRun.IsValid() && !ActiveAutomationRun->bComplete) {
      TSharedPtr<FJsonObject> Busy = MakeShared<FJsonObject>();
      Busy->SetStringField(TEXT("runId"), ActiveAutomationRun->RunId);
      Busy->SetNumberField(TEXT("total"), ActiveAutomationRun->TestCommandNames.Num());
      Busy->SetNumberField(TEXT("completed"), ActiveAutomationRun->CurrentIndex);
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("A test run is already in progress (runId=%s). "
                               "Poll get_test_results until it completes."),
                          *ActiveAutomationRun->RunId),
          Busy, TEXT("RUN_IN_PROGRESS"));
      return true;
    }

    // Capture per-test results via the framework's end-of-test delegate. The
    // editor's in-process automation worker concludes any active test on its own
    // tick, so this is how we read pass/fail even when we didn't call StopTest.
    if (!AutomationTestEndHandle.IsValid()) {
      AutomationTestEndHandle =
          FAutomationTestFramework::Get().OnTestEndEvent.AddUObject(
              this, &UMcpAutomationBridgeSubsystem::OnAutomationTestEnded);
    }

    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);
    {
      FString TestName;
      Payload->TryGetStringField(TEXT("test"), TestName);
      if (!TestName.IsEmpty() && Filter.IsEmpty()) {
        Filter = TestName;
      }
    }
    Filter.TrimStartAndEndInline();

    int32 MaxTests = 50;
    {
      double MaxTestsD = 0.0;
      if (Payload->TryGetNumberField(TEXT("maxTests"), MaxTestsD) && MaxTestsD > 0.0) {
        MaxTests = FMath::Clamp(static_cast<int32>(MaxTestsD), 1, 500);
      }
    }

    FAutomationTestFramework &Framework = FAutomationTestFramework::Get();
    Framework.SetRequestedTestFilter(EAutomationTestFlags_FilterMask);
    TArray<FAutomationTestInfo> AllTests;
    Framework.GetValidTestNames(AllTests);

    TSharedPtr<FMcpAutomationRun> Run = MakeShared<FMcpAutomationRun>();
    Run->RunId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Run->StartTime = FPlatformTime::Seconds();
    TArray<TSharedPtr<FJsonValue>> QueuedNames;
    for (const FAutomationTestInfo &Info : AllTests) {
      if (!Filter.IsEmpty() &&
          !Info.GetDisplayName().Contains(Filter) &&
          !Info.GetFullTestPath().Contains(Filter) &&
          !Info.GetTestName().Contains(Filter)) {
        continue;
      }
      Run->TestCommandNames.Add(Info.GetTestName());
      Run->TestDisplayNames.Add(Info.GetDisplayName());
      Run->TestFullPaths.Add(Info.GetFullTestPath());
      QueuedNames.Add(MakeShared<FJsonValueString>(Info.GetDisplayName()));
      if (Run->TestCommandNames.Num() >= MaxTests) {
        break;
      }
    }

    if (Run->TestCommandNames.Num() == 0) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("status"), TEXT("complete"));
      Result->SetStringField(TEXT("filter"), Filter);
      Result->SetNumberField(TEXT("total"), 0);
      Result->SetNumberField(TEXT("available"), AllTests.Num());
      SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("No automation tests match filter '%s' (%d available). "
                               "Use list_tests to discover names."),
                          *Filter, AllTests.Num()),
          Result);
      return true;
    }

    // Don't start while the engine is busy / in PIE; the queue still starts and
    // TickAutomationTestRun waits for a clean frame.
    ActiveAutomationRun = Run;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("runId"), Run->RunId);
    Result->SetStringField(TEXT("status"), TEXT("running"));
    Result->SetStringField(TEXT("filter"), Filter);
    Result->SetNumberField(TEXT("total"), Run->TestCommandNames.Num());
    Result->SetArrayField(TEXT("tests"), QueuedNames);
    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Started %d test(s). Poll get_test_results with runId=%s."),
                        Run->TestCommandNames.Num(), *Run->RunId),
        Result);
    return true;
  } else if (Lower == TEXT("get_test_results")) {
    // Poll the active/last automation run. Returns status running|complete plus
    // per-test results once finished.
    if (!ActiveAutomationRun.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("No test run has been started. Call run_tests first."),
                          TEXT("NO_ACTIVE_RUN"));
      return true;
    }

    FString WantRunId;
    Payload->TryGetStringField(TEXT("runId"), WantRunId);
    WantRunId.TrimStartAndEndInline();
    if (!WantRunId.IsEmpty() && WantRunId != ActiveAutomationRun->RunId) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("runId '%s' not found; current run is '%s'."),
                          *WantRunId, *ActiveAutomationRun->RunId),
          TEXT("RUN_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FMcpAutomationRun> Run = ActiveAutomationRun;
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("runId"), Run->RunId);
    Result->SetStringField(TEXT("status"),
                           Run->bComplete ? TEXT("complete") : TEXT("running"));
    Result->SetNumberField(TEXT("total"), Run->TestCommandNames.Num());
    Result->SetNumberField(TEXT("completed"), Run->CurrentIndex);
    Result->SetNumberField(TEXT("passed"), Run->Passed);
    Result->SetNumberField(TEXT("failed"), Run->Failed);
    Result->SetNumberField(TEXT("elapsedSeconds"),
                           FPlatformTime::Seconds() - Run->StartTime);
    if (!Run->bComplete &&
        Run->TestDisplayNames.IsValidIndex(Run->CurrentIndex)) {
      Result->SetStringField(TEXT("currentTest"),
                             Run->TestDisplayNames[Run->CurrentIndex]);
    }

    TArray<TSharedPtr<FJsonValue>> ResultsArray;
    for (const TSharedPtr<FJsonObject> &R : Run->Results) {
      ResultsArray.Add(MakeShared<FJsonValueObject>(R));
    }
    Result->SetArrayField(TEXT("results"), ResultsArray);

    const FString Msg =
        Run->bComplete
            ? FString::Printf(TEXT("Run complete: %d passed, %d failed of %d."),
                              Run->Passed, Run->Failed, Run->TestCommandNames.Num())
            : FString::Printf(TEXT("Run in progress: %d/%d complete."),
                              Run->CurrentIndex, Run->TestCommandNames.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true, Msg, Result);
    return true;
  } else if (Lower == TEXT("export_asset")) {
    // Export asset to FBX/OBJ/other format
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    
    FString ExportPath;
    Payload->TryGetStringField(TEXT("exportPath"), ExportPath);
    
    if (AssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("assetPath is required for export"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SafeAssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid asset path for export"),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    if (ExportPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("exportPath is required for export"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString SafeExportPath = SanitizeProjectFilePath(ExportPath);
    if (SafeExportPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe export path: %s"), *ExportPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    FString AbsoluteExportPath = FPaths::ProjectDir() / SafeExportPath;
    FPaths::MakeStandardFilename(AbsoluteExportPath);
    
    // CRITICAL: Convert to absolute path for proper comparison
    AbsoluteExportPath = FPaths::ConvertRelativePathToFull(AbsoluteExportPath);
    FPaths::NormalizeFilename(AbsoluteExportPath);

    FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(NormalizedProjectDir);
    if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
      NormalizedProjectDir += TEXT("/");
    }

    // SECURITY: Verify the resolved absolute path is within project bounds
    if (!AbsoluteExportPath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Export path escapes project directory: %s"), *ExportPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    
    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Asset not found: %s"), *SafeAssetPath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    
    // Ensure export directory exists
    FString ExportDir = FPaths::GetPath(AbsoluteExportPath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*ExportDir)) {
      PlatformFile.CreateDirectoryTree(*ExportDir);
    }
    
    // Load the asset
    UObject* Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
    if (!Asset) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Failed to load asset: %s"), *SafeAssetPath),
                          TEXT("LOAD_FAILED"));
      return true;
    }
    
    // Determine export format from file extension
    FString Extension = FPaths::GetExtension(AbsoluteExportPath).ToLower();
    
    // Try generic asset export via AssetTools
    bool bExportSuccess = false;
    FString ExportError;
    
    // CRITICAL FIX: Use AssetTools ExportAssets with explicit export path
    // This performs automated export without showing modal dialogs
    // The bPromptForIndividualFilenames=false suppresses file dialogs
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    IAssetTools& AssetTools = AssetToolsModule.Get();
    
    // Use ExportAssets with explicit path - this suppresses dialogs for automated export
    // The asset will be exported with its original name to the specified directory
    TArray<UObject*> AssetsToExport;
    AssetsToExport.Add(Asset);
    
    // ExportAssets exports to the specified directory with the asset's name
    // For custom filename, we need to rename temporarily or use UExporter directly
    AssetTools.ExportAssets(AssetsToExport, ExportDir);
    
    // Check if file was created
    FString ExpectedExportPath = ExportDir / FPaths::GetBaseFilename(SafeAssetPath) + TEXT(".") + Extension;
    if (FPaths::FileExists(ExpectedExportPath))
    {
      bExportSuccess = true;
    }
    else
    {
      // Try with the actual requested filename
      bExportSuccess = FPaths::FileExists(AbsoluteExportPath);
    }
    
    if (!bExportSuccess)
    {
      // Fallback: Use UExporter::ExportToFile directly with Prompt=false
      UExporter* Exporter = nullptr;
      
      // Find appropriate exporter for the asset type and extension
      for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt) {
        UClass* CurrentClass = *ClassIt;
        if (CurrentClass->IsChildOf(UExporter::StaticClass()) && !CurrentClass->HasAnyClassFlags(CLASS_Abstract)) {
          UExporter* DefaultExporter = Cast<UExporter>(CurrentClass->GetDefaultObject());
          if (DefaultExporter && DefaultExporter->SupportedClass) {
            if (Asset->GetClass()->IsChildOf(DefaultExporter->SupportedClass)) {
              if (DefaultExporter->PreferredFormatIndex < DefaultExporter->FormatExtension.Num()) {
                FString PreferredExt = DefaultExporter->FormatExtension[DefaultExporter->PreferredFormatIndex].ToLower();
                if (PreferredExt == Extension || PreferredExt.Contains(Extension)) {
                  Exporter = DefaultExporter;
                  break;
                }
              }
              if (!Exporter) {
                Exporter = DefaultExporter;
              }
            }
          }
        }
      }
      
      if (Exporter) {
        // ExportToFile signature: (Object, Exporter, Filename, InSelectedOnly, NoReplaceIdentical, Prompt)
        // The last parameter (Prompt=false) should suppress dialogs for most exporters
        int32 ExportResult = UExporter::ExportToFile(Asset, Exporter, *AbsoluteExportPath, false, false, false);
        bExportSuccess = (ExportResult != 0);
      }
      
      if (!bExportSuccess) {
        ExportError = FString::Printf(TEXT("Export failed for asset type '%s' and format '%s'"),
                                       *Asset->GetClass()->GetName(), *Extension);
      }
    }
    
    if (bExportSuccess) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      AddAssetVerification(Result, Asset);
      Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
      Result->SetStringField(TEXT("exportPath"), AbsoluteExportPath);
      Result->SetStringField(TEXT("format"), Extension);
      Result->SetBoolField(TEXT("success"), true);
      
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Asset exported to: %s"), *AbsoluteExportPath),
                             Result);
    } else {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
      Result->SetStringField(TEXT("exportPath"), AbsoluteExportPath);
      Result->SetStringField(TEXT("format"), Extension);
      Result->SetStringField(TEXT("error"), ExportError);
      
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Export failed: %s"), *ExportError),
                             Result, TEXT("EXPORT_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("execute_python")) {
    // Execute Python code with stdout/stderr capture via temp file wrapper
    FString Code;
    Payload->TryGetStringField(TEXT("code"), Code);
    FString File;
    Payload->TryGetStringField(TEXT("file"), File);

    const bool bHasCode = !Code.TrimStartAndEnd().IsEmpty();
    const bool bHasFile = !File.TrimStartAndEnd().IsEmpty();

    if (!bHasCode && !bHasFile) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("'code' or 'file' parameter is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (bHasCode && bHasFile) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Provide either 'code' or 'file', not both"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Enforce maximum code size (1 MB)
    static const int32 MaxCodeSize = 1048576;
    if (Code.Len() > MaxCodeSize) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Python code exceeds maximum size (%d bytes)"), MaxCodeSize),
                          TEXT("CODE_TOO_LARGE"));
      return true;
    }

    // Temp paths — GUID in filenames for concurrency safety
    FString TempDir = FPaths::ProjectSavedDir() / TEXT("Temp/MCP_Python");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*TempDir)) {
      PlatformFile.CreateDirectoryTree(*TempDir);
    }

    FString SafeId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FString ScriptPath = TempDir / FString::Printf(TEXT("mcp_exec_%s.py"), *SafeId);
    FString OutputPath = TempDir / FString::Printf(TEXT("output_%s.txt"), *SafeId);
    FString ErrorPath  = TempDir / FString::Printf(TEXT("error_%s.txt"), *SafeId);
    FString StatusPath = TempDir / FString::Printf(TEXT("status_%s.txt"), *SafeId);
    FString CodePath   = TempDir / FString::Printf(TEXT("code_%s.py"), *SafeId);

    // RAII-style scope guard for temp file cleanup
    struct FTempFileCleanup {
      TArray<FString> Paths;
      ~FTempFileCleanup() {
        IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
        for (const FString& P : Paths) {
          PF.DeleteFile(*P);
        }
      }
    } Cleanup;
    Cleanup.Paths.Add(ScriptPath);
    Cleanup.Paths.Add(OutputPath);
    Cleanup.Paths.Add(ErrorPath);
    Cleanup.Paths.Add(StatusPath);
    Cleanup.Paths.Add(CodePath);

    // Normalize paths for Python (forward slashes)
    auto NormalizePyPath = [](const FString& Path) -> FString {
      return Path.Replace(TEXT("\\"), TEXT("/"));
    };
    FString PyOutputPath = NormalizePyPath(OutputPath);
    FString PyErrorPath  = NormalizePyPath(ErrorPath);
    FString PyStatusPath = NormalizePyPath(StatusPath);

    // Modal guard: a Python call that opens a modal dialog permanently freezes
    // the GameThread in a headless bridge session — nobody can click it, every
    // later bridge call times out, and the editor needs a force-kill (the
    // 2026-06-10 reload_packages-on-IMC incident). Scan the user code for
    // known modal-popping APIs and fail fast BEFORE exec. Explicit opt-out:
    // payload {allowModalApis:true} for callers who know their form can't
    // prompt (e.g. a *_with_dialog API where a flag suppresses the dialog).
    bool bAllowModalApis = false;
    Payload->TryGetBoolField(TEXT("allowModalApis"), bAllowModalApis);
    FString ModalGuard;
    if (!bAllowModalApis) {
      ModalGuard += TEXT("    import re as _re\n");
      ModalGuard += TEXT("    _modal_match = _re.search(r'reload_packages|with_dialog|EditorDialog', _user_code)\n");
      ModalGuard += TEXT("    if _modal_match:\n");
      ModalGuard += TEXT("        raise RuntimeError(\"MCP modal guard: '\" + _modal_match.group(0) + \"' can open a modal dialog, \"\n");
      ModalGuard += TEXT("            \"which permanently freezes the editor GameThread in a headless bridge session \"\n");
      ModalGuard += TEXT("            \"(2026-06-10 incident: reload_packages on an IMC). Use a typed bridge action instead \"\n");
      ModalGuard += TEXT("            \"(execute_python is for read-only probes). If this exact call cannot prompt, \"\n");
      ModalGuard += TEXT("            \"retry with allowModalApis:true.\")\n");
    }

    // Build Python wrapper
    FString Wrapper;
    Wrapper += TEXT("import sys\nimport traceback\n\n");
    Wrapper += FString::Printf(TEXT("_out = open(r'%s', 'w', encoding='utf-8')\n"), *PyOutputPath);
    Wrapper += FString::Printf(TEXT("_err = open(r'%s', 'w', encoding='utf-8')\n"), *PyErrorPath);
    Wrapper += TEXT("_old_out, _old_err = sys.stdout, sys.stderr\n");
    Wrapper += TEXT("sys.stdout, sys.stderr = _out, _err\n\n");
    Wrapper += TEXT("_success = True\n");
    Wrapper += TEXT("try:\n");

    if (bHasCode) {
      if (!FFileHelper::SaveStringToFile(Code, *CodePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Failed to write temp code file: %s"), *CodePath),
                            TEXT("FILE_WRITE_FAILED"));
        return true;
      }
      FString PyCodePath = NormalizePyPath(CodePath);
      Wrapper += FString::Printf(TEXT("    with open(r'%s', 'r', encoding='utf-8') as _f:\n"), *PyCodePath);
      Wrapper += TEXT("        _user_code = _f.read()\n");
      Wrapper += ModalGuard;
      Wrapper += FString::Printf(TEXT("    exec(compile(_user_code, r'%s', 'exec'))\n"), *PyCodePath);
    } else {
      // SECURITY: Sanitize file path to prevent directory traversal
      FString SafeFilePath = SanitizeProjectFilePath(File);
      if (SafeFilePath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Invalid or unsafe file path: %s"), *File),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }

      // Resolve absolute path and verify it stays within project directory
      FString AbsoluteFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / SafeFilePath);
      FPaths::NormalizeFilename(AbsoluteFilePath);

      FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
        NormalizedProjectDir += TEXT("/");
      }

      if (!AbsoluteFilePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("File path escapes project directory: %s"), *File),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }

      // Resolve symlinks and re-validate (prevents symlink escape attacks)
      FString ResolvedPath = FPlatformFileManager::Get().GetPlatformFile().ConvertToAbsolutePathForExternalAppForRead(*AbsoluteFilePath);
      if (!ResolvedPath.IsEmpty())
      {
        FPaths::NormalizeFilename(ResolvedPath);
        if (!ResolvedPath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase))
        {
          SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Resolved file path escapes project directory (symlink detected)"),
                              TEXT("SECURITY_VIOLATION"));
          return true;
        }
        AbsoluteFilePath = ResolvedPath;
      }

      // Use absolute path in Python wrapper (forward slashes)
      FString PyFilePath = NormalizePyPath(AbsoluteFilePath);
      Wrapper += FString::Printf(TEXT("    with open(r'%s', 'r', encoding='utf-8') as _f:\n"), *PyFilePath);
      Wrapper += TEXT("        _user_code = _f.read()\n");
      Wrapper += ModalGuard;
      Wrapper += FString::Printf(TEXT("    exec(compile(_user_code, r'%s', 'exec'))\n"), *PyFilePath);
    }

    Wrapper += TEXT("except:\n");
    Wrapper += TEXT("    traceback.print_exc()\n");
    Wrapper += TEXT("    _success = False\n");
    Wrapper += TEXT("finally:\n");
    Wrapper += TEXT("    sys.stdout, sys.stderr = _old_out, _old_err\n");
    Wrapper += TEXT("    _out.close()\n");
    Wrapper += TEXT("    _err.close()\n");
    Wrapper += FString::Printf(TEXT("    with open(r'%s', 'w') as _sf:\n"), *PyStatusPath);
    Wrapper += TEXT("        _sf.write('1' if _success else '0')\n");

    // Write wrapper to disk
    if (!FFileHelper::SaveStringToFile(Wrapper, *ScriptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Failed to write temp script: %s"), *ScriptPath),
                          TEXT("FILE_WRITE_FAILED"));
      return true;
    }

    // Get world context (same pattern as console_command handler)
    UWorld* World = nullptr;
    if (GEditor) {
      World = GEditor->GetEditorWorldContext().World();
    }
    if (!World && GEngine && GEngine->GetWorldContexts().Num() > 0) {
      World = GEngine->GetWorldContexts()[0].World();
    }

    SendProgressUpdate(RequestId, 0.0f, TEXT("Executing Python script"), true, CurrentRequestOrigin);

    // Execute via py console command with execution time tracking
    static constexpr double MaxPythonExecutionSeconds = 60.0;
    FString PyCommand = FString::Printf(TEXT("py \"%s\""), *ScriptPath);
    bool bExecHandled = false;
    double ExecStartTime = FPlatformTime::Seconds();
    if (GEngine) {
      bExecHandled = GEngine->Exec(World, *PyCommand);
    }
    double ExecElapsed = FPlatformTime::Seconds() - ExecStartTime;
    SendProgressUpdate(RequestId, 90.0f, TEXT("Collecting Python output"), true, CurrentRequestOrigin);
    if (ExecElapsed > MaxPythonExecutionSeconds) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Python execution took %.1fs (exceeds %.1fs threshold). "
                  "Consider running long scripts via 'file' parameter in a separate process."),
             ExecElapsed, MaxPythonExecutionSeconds);
    }

    // Read results
    FString Output, Error, Status;
    FFileHelper::LoadFileToString(Output, *OutputPath);
    FFileHelper::LoadFileToString(Error, *ErrorPath);
    FFileHelper::LoadFileToString(Status, *StatusPath);

    // Cleanup happens automatically via FTempFileCleanup destructor

    // Check if Python is available
    if (!bExecHandled && Status.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Python command not recognized. Is the Python Editor Script Plugin enabled?"),
                          TEXT("PYTHON_NOT_AVAILABLE"));
      return true;
    }

    bool bSuccess = Status.TrimStartAndEnd().Equals(TEXT("1"));
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("output"), Output.TrimEnd());
    Result->SetStringField(TEXT("error"), Error.TrimEnd());

    // On failure, surface the captured Python traceback in the message itself.
    // It is also in Result.error, but clients typically drop the result payload
    // for error responses, so without this the caller only sees the generic
    // "Python execution failed" with no detail.
    FString PyMessage = TEXT("Python executed successfully");
    if (!bSuccess)
    {
      const FString Trace = Error.TrimEnd();
      PyMessage = Trace.IsEmpty()
          ? TEXT("Python execution failed (no traceback captured)")
          : FString::Printf(TEXT("Python execution failed: %s"), *Trace.Left(1500));
    }

    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, PyMessage,
                           Result, bSuccess ? FString() : TEXT("PYTHON_ERROR"));
    return true;
  }

  return false;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("System control actions require editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ---------------------------------------------------------------------------
// OnAutomationTestEnded
// ---------------------------------------------------------------------------
// Bound to FAutomationTestFramework::OnTestEndEvent (lazy-bound in run_tests).
// Fires from InternalStopTest just after the test's success state is set and
// before the test object is torn down — whether StopTest was called by the
// editor's in-process automation worker or by us. Captures the result for the
// test currently in flight so TickAutomationTestRun can report pass/fail.
void UMcpAutomationBridgeSubsystem::OnAutomationTestEnded(FAutomationTestBase *Test)
{
  if (!Test || !ActiveAutomationRun.IsValid() || !ActiveAutomationRun->bCurrentStarted)
  {
    return;
  }

  TSharedPtr<FMcpAutomationRun> Run = ActiveAutomationRun;
  if (!Run->TestCommandNames.IsValidIndex(Run->CurrentIndex))
  {
    return;
  }

  // Only capture the test we believe is in flight. Command names are
  // "ClassName" or "ClassName Param"; the ended test's name is the class name.
  const FString &CmdName = Run->TestCommandNames[Run->CurrentIndex];
  FString BaseName;
  FString Param;
  if (!CmdName.Split(TEXT(" "), &BaseName, &Param, ESearchCase::CaseSensitive))
  {
    BaseName = CmdName;
  }
  if (Test->GetTestName() != BaseName)
  {
    return; // a different test (e.g. one the editor ran on its own)
  }

  FAutomationTestExecutionInfo Info;
  Test->GetExecutionInfo(Info);

  Run->bCaptured = true;
  Run->bCapturedSuccess = Test->GetLastExecutionSuccessState();
  Run->CapturedErrorCount = Info.GetErrorTotal();
  Run->CapturedWarningCount = Info.GetWarningTotal();
  Run->CapturedErrors.Reset();
  for (const FAutomationExecutionEntry &Entry : Info.GetEntries())
  {
    if (Entry.Event.Type == EAutomationEventType::Error)
    {
      Run->CapturedErrors.Add(Entry.Event.Message);
    }
  }
}

// ---------------------------------------------------------------------------
// TickAutomationTestRun
// ---------------------------------------------------------------------------
// Advances the active automation run (started by system_control run_tests) one
// step per editor frame. Called from UMcpAutomationBridgeSubsystem::Tick()
// inside the safe-state guard that gates request draining.
//
// Why this is not a simple "StartTestByName then ExecuteLatentCommands loop":
// the editor loads FAutomationWorkerModule, whose own per-frame Tick() drives
// ExecuteLatentCommands() AND concludes (StopTest) ANY active test the moment
// GIsAutomationTesting is set — even one we started directly. So a second,
// unguarded ExecuteLatentCommands() from us in a later frame hits its
// check(GIsAutomationTesting) and crashes the editor.
//
// Design that is correct whether the worker is present or not:
//   * Every FAutomationTestFramework call is immediately preceded by a
//     GIsAutomationTesting check. Both this and the worker Tick run on the game
//     thread sequentially, so the flag cannot change mid-function.
//   * Results come from OnAutomationTestEnded (OnTestEndEvent), which fires for
//     whichever side calls StopTest. We never depend on owning the conclusion.
//   * We still drive ExecuteLatentCommands/StopTest as a fallback for when the
//     worker isn't ticking; guarded, so it is a harmless duplicate when it is.
void UMcpAutomationBridgeSubsystem::TickAutomationTestRun()
{
  if (!ActiveAutomationRun.IsValid() || ActiveAutomationRun->bComplete)
  {
    return;
  }

  // Hard ceiling per test so a hung latent command (e.g. a map that never
  // finishes loading) can't wedge the queue forever.
  static const double McpTestPerTestTimeoutSeconds = 120.0;

  TSharedPtr<FMcpAutomationRun> Run = ActiveAutomationRun;

  // Slow task / PIE active: StartTestByName() would refuse and log an error
  // that our "did it start?" probe would misread. Wait for a clean frame.
  if (GIsSlowTask || GIsPlayInEditorWorld)
  {
    return;
  }

  if (Run->CurrentIndex >= Run->TestCommandNames.Num())
  {
    Run->bComplete = true;
    return;
  }

  FAutomationTestFramework &Framework = FAutomationTestFramework::Get();

  const FString &CmdName = Run->TestCommandNames[Run->CurrentIndex];
  const FString DisplayName = Run->TestDisplayNames.IsValidIndex(Run->CurrentIndex)
                                  ? Run->TestDisplayNames[Run->CurrentIndex]
                                  : CmdName;
  const FString FullPath = Run->TestFullPaths.IsValidIndex(Run->CurrentIndex)
                               ? Run->TestFullPaths[Run->CurrentIndex]
                               : CmdName;

  // Record a result and move to the next test.
  auto RecordAndAdvance = [&](bool bSuccess, double DurationSeconds, int32 ErrC,
                              int32 WarnC, const TArray<FString> &Errors,
                              bool bTimedOut) {
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetStringField(TEXT("test"), DisplayName);
    R->SetStringField(TEXT("command"), CmdName);
    R->SetBoolField(TEXT("success"), bSuccess);
    R->SetNumberField(TEXT("durationSeconds"), DurationSeconds);
    R->SetNumberField(TEXT("errorCount"), ErrC);
    R->SetNumberField(TEXT("warningCount"), WarnC);
    if (bTimedOut)
    {
      R->SetBoolField(TEXT("timedOut"), true);
    }
    TArray<TSharedPtr<FJsonValue>> Errs;
    for (const FString &E : Errors)
    {
      Errs.Add(MakeShared<FJsonValueString>(E));
    }
    R->SetArrayField(TEXT("errors"), Errs);
    Run->Results.Add(R);

    if (bSuccess)
    {
      ++Run->Passed;
    }
    else
    {
      ++Run->Failed;
    }
    ++Run->CurrentIndex;
    Run->bCurrentStarted = false;
    Run->bCaptured = false;
    if (Run->CurrentIndex >= Run->TestCommandNames.Num())
    {
      Run->bComplete = true;
    }
  };

  if (!Run->bCurrentStarted)
  {
    // Wait while any automation test (ours from a prior frame, or one the editor
    // started) is still running; the worker will conclude it.
    if (GIsAutomationTesting)
    {
      return;
    }

    Run->bCaptured = false;
    Framework.SetRequestedTestFilter(EAutomationTestFlags_FilterMask);
    Framework.StartTestByName(CmdName, 0, FullPath);

    if (!GIsAutomationTesting)
    {
      // StartTestByName logged an error and did not begin a session.
      RecordAndAdvance(false, 0.0, 0, 0,
                       {TEXT("Test could not be started (not found, disabled, "
                             "or too slow).")},
                       false);
      return;
    }

    Run->bCurrentStarted = true;
    Run->CurrentTestStartTime = FPlatformTime::Seconds();
    return; // let the test run; conclusion is detected on a later frame
  }

  // Test is in flight. If the conclusion already happened (worker called StopTest,
  // or we did on a prior frame), GIsAutomationTesting is now false.
  const double Elapsed = FPlatformTime::Seconds() - Run->CurrentTestStartTime;

  if (!GIsAutomationTesting)
  {
    if (Run->bCaptured)
    {
      RecordAndAdvance(Run->bCapturedSuccess, Elapsed, Run->CapturedErrorCount,
                       Run->CapturedWarningCount, Run->CapturedErrors, false);
    }
    else
    {
      RecordAndAdvance(false, Elapsed, 0, 0,
                       {TEXT("Test concluded but no result was captured.")},
                       false);
    }
    return;
  }

  // Still running. Enforce the per-test timeout.
  if (Elapsed > McpTestPerTestTimeoutSeconds)
  {
    Framework.DequeueAllCommands();
    if (GIsAutomationTesting)
    {
      FAutomationTestExecutionInfo Tmp;
      Framework.StopTest(Tmp); // also fires OnTestEndEvent
    }
    TArray<FString> Errors = Run->bCaptured ? Run->CapturedErrors : TArray<FString>();
    Errors.Add(FString::Printf(TEXT("Test timed out after %.0f seconds."), Elapsed));
    RecordAndAdvance(false, Elapsed,
                     Run->bCaptured ? Run->CapturedErrorCount : 0,
                     Run->bCaptured ? Run->CapturedWarningCount : 0, Errors, true);
    return;
  }

  // Drive latent commands one step as a fallback for when the worker isn't
  // ticking. Guarded: GIsAutomationTesting is true right now (same thread).
  const bool bLatentDone = Framework.ExecuteLatentCommands();
  if (bLatentDone)
  {
    // Latent queue drained; conclude ourselves so we don't depend on the worker.
    if (GIsAutomationTesting)
    {
      FAutomationTestExecutionInfo ExecInfo;
      Framework.StopTest(ExecInfo); // fires OnTestEndEvent -> capture
    }
    if (Run->bCaptured)
    {
      RecordAndAdvance(Run->bCapturedSuccess, Elapsed, Run->CapturedErrorCount,
                       Run->CapturedWarningCount, Run->CapturedErrors, false);
    }
    else
    {
      RecordAndAdvance(false, Elapsed, 0, 0,
                       {TEXT("Test concluded but no result was captured.")},
                       false);
    }
  }
  // else: still running; wait for the next frame.
}
