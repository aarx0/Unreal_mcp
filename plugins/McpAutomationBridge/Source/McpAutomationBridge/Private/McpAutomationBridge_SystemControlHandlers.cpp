#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Guid.h"
#include "Misc/AutomationTest.h"     // FAutomationTestFramework — real TDD run/poll (run_tests/get_test_results)
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_SystemControlHandlers.h"
#include "McpAutomationBridge_InsightsHandlers.h"
#include "McpAutomationBridge_ConsoleCommandHandlers.h"

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
#include "Misc/OutputDevice.h"           // live_coding_compile: LogLiveCoding capture
#include "Misc/ScopeLock.h"
#include "Internationalization/Regex.h"  // get_build_status: [N/M] progress lines
#include "Interfaces/IPluginManager.h"  // generate_test_stub: resolve the plugin Source dir
#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"           // live_coding_compile: trigger a Live Coding patch
#include "Modules/ModuleManager.h"       // FModuleManager::GetModulePtr<ILiveCodingModule>
#endif
#endif

// system_control member handlers (HandleSys*). Dispatch lives in the FMcpCall
// classes (Private/MCP/Calls/McpCalls_SystemControl.cpp); each member here
// implements one advertised action.

#if WITH_EDITOR

// generate_test_stub — scaffold a correct, registerable C++ automation test.
// The author->compile->run loop for a NEW test previously meant hand-writing
// a .cpp and getting the EAutomationTestFlags incantation right (wrong flags =
// compiles but never shows under run_tests). This emits the known-good shape
// (matching the bridge self-tests) so the only remaining step is compiling it
// in (live_coding_compile picks up new files) and running it.
bool McpHandlers::SystemControl::HandleSysGenerateTestStub(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    FString TestName;
    Payload->TryGetStringField(TEXT("testName"), TestName);
    if (TestName.IsEmpty()) {
      S.SendAutomationError(RequestingSocket, RequestId,
          TEXT("generate_test_stub requires 'testName' (e.g. 'Combat.DamageApplies')."),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString TestType = TEXT("simple");
    Payload->TryGetStringField(TEXT("testType"), TestType);
    TestType = TestType.ToLower();
    if (TestType != TEXT("simple") && TestType != TEXT("complex") && TestType != TEXT("latent")) {
      S.SendAutomationError(RequestingSocket, RequestId,
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
        S.SendAutomationError(RequestingSocket, RequestId,
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
      S.SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("File already exists: %s (pass overwrite:true to replace)."), *OutputPath),
          TEXT("ALREADY_EXISTS"));
      return true;
    }

    // SaveStringToFile does not create parent dirs.
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), /*Tree=*/true);

    if (!FFileHelper::SaveStringToFile(Body, *OutputPath)) {
      S.SendAutomationError(RequestingSocket, RequestId,
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
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Wrote %s test '%s' to %s"), *TestType, *TestName, *OutputPath),
        Result);
    return true;
}

bool McpHandlers::SystemControl::HandleSysStartSession(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    return HandleInsightsAction(S, RequestId, TEXT("manage_insights"), Payload, RequestingSocket);
}

// Trigger a Live Coding compile + patch of the running editor so .cpp-body
// changes to the bridge (or any module) apply without a close/rebuild/relaunch.
// Header / Build.cs / .uplugin changes still require a full rebuild.
bool McpHandlers::SystemControl::HandleSysLiveCodingCompile(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_LIVE_CODING
    ILiveCodingModule *LiveCoding =
        FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
    if (!LiveCoding) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Live Coding module is not loaded"),
                          TEXT("LIVE_CODING_UNAVAILABLE"));
      return true;
    }
    if (!LiveCoding->IsEnabledForSession()) {
      if (!LiveCoding->CanEnableForSession()) {
        S.SendAutomationError(
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
    // applied to the running process before this returns. Live Coding runs the
    // build out-of-process and streams its output through LogLiveCoding — scrape
    // that for the duration of the call so Failure carries the actual compiler
    // errors instead of a bare enum.
    class FMcpLiveCodingCapture final : public FOutputDevice {
    public:
      virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity,
                             const FName& Category) override {
        if (Category == LiveCodingCategory) {
          FScopeLock Lock(&Mutex);
          if (Lines.Num() < 400) {
            Lines.Add(FString(V));
          }
        }
      }
      const FName LiveCodingCategory = FName(TEXT("LogLiveCoding"));
      FCriticalSection Mutex;
      TArray<FString> Lines;
    } Capture;

    GLog->AddOutputDevice(&Capture);
    ELiveCodingCompileResult CompileResult = ELiveCodingCompileResult::NotStarted;
    LiveCoding->Compile(ELiveCodingCompileFlags::WaitForCompletion, &CompileResult);
    GLog->RemoveOutputDevice(&Capture);

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

    TArray<TSharedPtr<FJsonValue>> LcLogJson, ErrorsJson;
    {
      FScopeLock Lock(&Capture.Mutex);
      for (const FString& Line : Capture.Lines) {
        if (LcLogJson.Num() < 100) {
          LcLogJson.Add(MakeShared<FJsonValueString>(Line));
        }
        if (ErrorsJson.Num() < 25 &&
            (Line.Contains(TEXT(": error")) || Line.Contains(TEXT("): error")) ||
             Line.Contains(TEXT(": fatal error")))) {
          ErrorsJson.Add(MakeShared<FJsonValueString>(Line.TrimStartAndEnd()));
        }
      }
    }

    // On Failure the compiler output stays in the LiveCoding console process —
    // the editor log only gets "please see Live console". The underlying UBT
    // run writes the real errors to its own log; scrape that as the fallback.
    FString UbtLogPath;
    if (CompileResult == ELiveCodingCompileResult::Failure && ErrorsJson.Num() == 0) {
      UbtLogPath = FPaths::Combine(FPlatformProcess::UserSettingsDir(),
                                   TEXT("UnrealBuildTool"), TEXT("Log.txt"));
      FString UbtLog;
      if (FFileHelper::LoadFileToString(UbtLog, *UbtLogPath, FFileHelper::EHashOptions::None,
                                        FILEREAD_AllowWrite)) {
        TArray<FString> UbtLines;
        UbtLog.ParseIntoArrayLines(UbtLines);
        for (const FString& Line : UbtLines) {
          if (ErrorsJson.Num() < 25 &&
              (Line.Contains(TEXT(": error")) || Line.Contains(TEXT("): error")) ||
               Line.Contains(TEXT(": fatal error")))) {
            ErrorsJson.Add(MakeShared<FJsonValueString>(Line.TrimStartAndEnd()));
          }
        }
      }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bOk);
    Result->SetStringField(TEXT("compileResult"), ResultStr);
    Result->SetArrayField(TEXT("errors"), ErrorsJson);
    Result->SetArrayField(TEXT("log"), LcLogJson);
    if (!UbtLogPath.IsEmpty()) {
      Result->SetStringField(TEXT("ubtLogPath"), UbtLogPath);
    }
    if (CompileResult == ELiveCodingCompileResult::NoChanges) {
      Result->SetStringField(TEXT("reason"),
          TEXT("The build found no outstanding compile actions — no tracked source file is newer "
               "than its module binary. External (on-disk) edits are detected by timestamp, so if "
               "you just wrote a file: confirm the write flushed, and that the file's module is "
               "loaded in this editor and enabled for Live Coding. Header/Build.cs changes need a "
               "full rebuild, not a Live Coding patch."));
    }
    // Completion states ride a SUCCESSFUL response: the transport's error shape
    // carries only the message string and drops the details object — which is
    // where the compiler diagnostics live. The body's success/compileResult/
    // errors fields carry the real outcome (same contract as get_build_status
    // reporting a failed build). A failed compile also logs LogLiveCoding
    // errors, which would trip the post-op ENGINE_ERROR guard and replace this
    // response — clear them: the failure is fully reported here, structured.
    S.ClearCapturedErrors();
    S.SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Live Coding compile: %s"), *ResultStr), Result);
    return true;
#else
    S.SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Live Coding is not compiled into this build (WITH_LIVE_CODING=0)"),
        TEXT("LIVE_CODING_UNAVAILABLE"));
    return true;
#endif
}

bool McpHandlers::SystemControl::HandleSysRunUbt(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    // Extract optional parameters
    FString Target;
    Payload->TryGetStringField(TEXT("target"), Target);
    
    FString Platform;
    Payload->TryGetStringField(TEXT("platform"), Platform);
    
    FString Configuration;
    Payload->TryGetStringField(TEXT("configuration"), Configuration);
    
    FString AdditionalArgs;
    Payload->TryGetStringField(TEXT("additionalArgs"), AdditionalArgs);

    Target.TrimStartAndEndInline();
    Platform.TrimStartAndEndInline();
    Configuration.TrimStartAndEndInline();
    AdditionalArgs.TrimStartAndEndInline();

    auto ValidateBuildToken = [&](const FString& Value, const TCHAR* FieldName) -> bool {
      if (!McpIsSafeUbtPositionalToken(Value)) {
        S.SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid %s for run_ubt: %s must be a positional token"), FieldName, *Value),
            TEXT("INVALID_ARGUMENT"));
        return false;
      }
      return true;
    };

    if (Target.IsEmpty()) {
      S.SendAutomationError(RequestingSocket, RequestId,
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
      S.SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Platform is not allowed for run_ubt: %s"), *Platform),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!McpIsAllowedUbtConfiguration(Configuration)) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Configuration is not allowed for run_ubt: %s"), *Configuration),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!McpIsSafeUbtArgumentList(AdditionalArgs)) {
      S.SendAutomationError(RequestingSocket, RequestId,
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
      S.SendAutomationError(RequestingSocket, RequestId,
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

    // Fire-and-poll: UBT runs detached with output redirected to a log file
    // and this call returns immediately with a buildId for get_build_status.
    // (The old implementation pumped a pipe on the game thread for up to 300s,
    // freezing the editor AND every queued bridge call for the whole build.)

    // -NoUBA unless explicitly disabled: a UBA-built binary can't be Live
    // Coding-patched afterwards (.voltbl mismatch), and LC iteration is the
    // main reason to build from the bridge. -WaitMutex so a concurrent UBT
    // queues instead of failing on the mutex.
    bool bNoUBA = true;
    Payload->TryGetBoolField(TEXT("noUBA"), bNoUBA);
    if (bNoUBA && !Arguments.Contains(TEXT("-NoUBA"))) {
      Arguments += TEXT(" -NoUBA");
    }
    if (!Arguments.Contains(TEXT("-WaitMutex"))) {
      Arguments += TEXT(" -WaitMutex");
    }

    const FString BuildId = FString::Printf(
        TEXT("build_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12));
    const FString LogDir =
        FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("McpBuilds"));
    IFileManager::Get().MakeDirectory(*LogDir, true);
    const FString BuildLogPath = LogDir / (BuildId + TEXT(".log"));

    // Redirection needs a shell; the > path is handler-built (not caller input,
    // which McpIsSafeUbtArgumentList already screened for shell metacharacters).
#if PLATFORM_WINDOWS
    const FString ShellExe = TEXT("cmd.exe");
    const FString ShellParams = FString::Printf(
        TEXT("/d /s /c \"\"%s\" %s > \"%s\" 2>&1\""), *UBTPath, *Arguments, *BuildLogPath);
#else
    const FString ShellExe = TEXT("/bin/sh");
    const FString ShellParams = FString::Printf(
        TEXT("-c '\"%s\" %s > \"%s\" 2>&1'"), *UBTPath, *Arguments, *BuildLogPath);
#endif

    uint32 ProcessId = 0;
    FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
        *ShellExe, *ShellParams,
        true,       // bLaunchDetached
        true,       // bLaunchHidden
        true,       // bLaunchReallyHidden
        &ProcessId,
        0, nullptr, nullptr);

    if (!ProcessHandle.IsValid()) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to launch UBT process"),
                          TEXT("PROCESS_LAUNCH_FAILED"));
      return true;
    }

    // Track the job; cap history so process handles don't accumulate.
    if (S.UbtBuildJobs.Num() >= 8) {
      FString OldestId;
      double OldestStart = TNumericLimits<double>::Max();
      for (auto& Pair : S.UbtBuildJobs) {
        const bool bStillRunning =
            !Pair.Value.bFinished && FPlatformProcess::IsProcRunning(Pair.Value.ProcHandle);
        if (!bStillRunning && Pair.Value.StartTime < OldestStart) {
          OldestStart = Pair.Value.StartTime;
          OldestId = Pair.Key;
        }
      }
      if (!OldestId.IsEmpty()) {
        FPlatformProcess::CloseProc(S.UbtBuildJobs[OldestId].ProcHandle);
        S.UbtBuildJobs.Remove(OldestId);
      }
    }

    UMcpAutomationBridgeSubsystem::FMcpUbtBuildJob Job;
    Job.BuildId = BuildId;
    Job.Target = Target;
    Job.Platform = Platform;
    Job.Configuration = Configuration;
    Job.LogPath = BuildLogPath;
    Job.CommandLine = FString::Printf(TEXT("\"%s\" %s"), *UBTPath, *Arguments);
    Job.ProcHandle = ProcessHandle;
    Job.ProcessId = ProcessId;
    Job.StartTime = FPlatformTime::Seconds();
    S.UbtBuildJobs.Add(BuildId, MoveTemp(Job));
    S.LastUbtBuildId = BuildId;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("buildId"), BuildId);
    Result->SetStringField(TEXT("logPath"), BuildLogPath);
    Result->SetNumberField(TEXT("processId"), ProcessId);
    Result->SetStringField(TEXT("target"), Target);
    Result->SetStringField(TEXT("platform"), Platform);
    Result->SetStringField(TEXT("configuration"), Configuration);
    Result->SetBoolField(TEXT("noUBA"), bNoUBA);
    S.SendAutomationResponse(
        RequestingSocket, RequestId, true,
        TEXT("UBT started; poll { action:\"get_build_status\" } for progress and results."),
        Result);
    return true;
}

bool McpHandlers::SystemControl::HandleSysGetBuildStatus(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    FString BuildId;
    Payload->TryGetStringField(TEXT("buildId"), BuildId);
    if (BuildId.IsEmpty()) {
      BuildId = S.LastUbtBuildId;
    }

    if (BuildId.IsEmpty()) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("status"), TEXT("no_builds"));
      S.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("No run_ubt build has been started this session."), Result);
      return true;
    }
    if (!S.UbtBuildJobs.Contains(BuildId)) {
      S.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Unknown buildId '%s' (builds are tracked per editor session)."), *BuildId),
          TEXT("BUILD_NOT_FOUND"));
      return true;
    }

    UMcpAutomationBridgeSubsystem::FMcpUbtBuildJob& BuildJob = S.UbtBuildJobs[BuildId];
    if (!BuildJob.bFinished && !FPlatformProcess::IsProcRunning(BuildJob.ProcHandle)) {
      BuildJob.bFinished = true;
      BuildJob.EndTime = FPlatformTime::Seconds();
      int32 Code = -1;
      if (FPlatformProcess::GetProcReturnCode(BuildJob.ProcHandle, &Code)) {
        BuildJob.ReturnCode = Code;
      }
    }
    const bool bRunning = !BuildJob.bFinished;

    // Parse the log for errors, warnings, the latest [N/M] progress step and
    // the final result line. FILEREAD_AllowWrite: the shell still holds the
    // file open for writing while the build runs.
    FString LogText;
    const bool bLogRead = FFileHelper::LoadFileToString(
        LogText, *BuildJob.LogPath, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite);
    TArray<TSharedPtr<FJsonValue>> ErrorsJson;
    int32 WarningCount = 0;
    FString Progress, ResultLine;
    TArray<FString> Lines;
    if (bLogRead && !LogText.IsEmpty()) {
      LogText.ParseIntoArrayLines(Lines);
      const FRegexPattern ProgressPattern(TEXT("^\\[\\d+/\\d+\\]"));
      for (const FString& Line : Lines) {
        if (ErrorsJson.Num() < 25 &&
            (Line.Contains(TEXT(": error")) || Line.Contains(TEXT("): error")) ||
             Line.Contains(TEXT("ERROR:")) || Line.Contains(TEXT(": fatal error")))) {
          ErrorsJson.Add(MakeShared<FJsonValueString>(Line.TrimStartAndEnd()));
        }
        if (Line.Contains(TEXT(": warning"))) {
          ++WarningCount;
        }
        if (FRegexMatcher(ProgressPattern, Line).FindNext()) {
          Progress = Line.TrimStartAndEnd();
        }
        if (Line.StartsWith(TEXT("Result:")) || Line.Contains(TEXT("BUILD FAILED")) ||
            Line.Contains(TEXT("BUILD SUCCESSFUL"))) {
          ResultLine = Line.TrimStartAndEnd();
        }
      }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("buildId"), BuildId);
    const FString Status = bRunning ? TEXT("running")
                          : (BuildJob.ReturnCode == 0 ? TEXT("succeeded") : TEXT("failed"));
    Result->SetStringField(TEXT("status"), Status);
    Result->SetStringField(TEXT("target"), BuildJob.Target);
    Result->SetStringField(TEXT("platform"), BuildJob.Platform);
    Result->SetStringField(TEXT("configuration"), BuildJob.Configuration);
    Result->SetNumberField(TEXT("elapsedSec"),
        (bRunning ? FPlatformTime::Seconds() : BuildJob.EndTime) - BuildJob.StartTime);
    if (!bRunning) {
      Result->SetNumberField(TEXT("returnCode"), BuildJob.ReturnCode);
    }
    if (!Progress.IsEmpty()) {
      Result->SetStringField(TEXT("progress"), Progress);
    }
    if (!ResultLine.IsEmpty()) {
      Result->SetStringField(TEXT("resultLine"), ResultLine);
    }
    Result->SetNumberField(TEXT("warningCount"), WarningCount);
    Result->SetArrayField(TEXT("errors"), ErrorsJson);
    Result->SetBoolField(TEXT("logReadable"), bLogRead);
    Result->SetStringField(TEXT("logPath"), BuildJob.LogPath);
    {
      TArray<TSharedPtr<FJsonValue>> TailJson;
      for (int32 i = FMath::Max(0, Lines.Num() - 5); i < Lines.Num(); ++i) {
        TailJson.Add(MakeShared<FJsonValueString>(Lines[i].TrimStartAndEnd()));
      }
      Result->SetArrayField(TEXT("logTail"), TailJson);
    }
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Build %s: %s"), *BuildId, *Status), Result);
    return true;
}

// Enumerate registered automation tests (optionally substring-filtered).
// Read-only: no test is started. Useful to discover exact test names/paths
// before run_tests, and to confirm seeded project tests are visible.
bool McpHandlers::SystemControl::HandleSysListTests(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
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
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("%d of %d automation tests match."),
                                           Matched, AllTests.Num()),
                           Result);
    return true;
}

bool McpHandlers::SystemControl::HandleSysRunTests(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    FString Suite;
    Payload->TryGetStringField(TEXT("suite"), Suite);
    if (Suite.Equals(TEXT("ui-nav"), ESearchCase::IgnoreCase)) {
      // External spec suite, run_ubt-style fire-and-poll: launch the checked-in
      // PowerShell runner detached (it drives THIS bridge over HTTP — its
      // play/nav/assert calls arrive as ordinary serialized requests, so a
      // blocking in-handler runner would deadlock against PIE ticks) and poll
      // with get_test_results. Requires pwsh on PATH and an otherwise idle
      // editor (Slate focus determinism — see tests/ui-nav/README.md).
      if (S.ActiveExternalTestRun.IsValid() && !S.ActiveExternalTestRun->bFinished &&
          FPlatformProcess::IsProcRunning(S.ActiveExternalTestRun->ProcHandle)) {
        TSharedPtr<FJsonObject> Busy = MakeShared<FJsonObject>();
        Busy->SetStringField(TEXT("runId"), S.ActiveExternalTestRun->RunId);
        S.SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("A ui-nav run is already in progress (runId=%s). "
                                 "Poll get_test_results until it completes."),
                            *S.ActiveExternalTestRun->RunId),
            Busy, TEXT("RUN_IN_PROGRESS"));
        return true;
      }
      if (S.ActiveAutomationRun.IsValid() && !S.ActiveAutomationRun->bComplete) {
        S.SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("An automation test run is in progress (runId=%s); "
                                 "ui-nav specs need the editor to themselves."),
                            *S.ActiveAutomationRun->RunId),
            TEXT("RUN_IN_PROGRESS"));
        return true;
      }

      // Runner + specs live at the fork repo root, two levels above the
      // plugin dir (<repo>/plugins/McpAutomationBridge).
      TSharedPtr<IPlugin> Plugin =
          IPluginManager::Get().FindPlugin(TEXT("McpAutomationBridge"));
      if (!Plugin.IsValid()) {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Could not resolve the McpAutomationBridge plugin dir."),
                            TEXT("NO_PLUGIN_DIR"));
        return true;
      }
      FString RepoRoot = FPaths::ConvertRelativePathToFull(
          Plugin->GetBaseDir() / TEXT("../.."));
      FPaths::CollapseRelativeDirectories(RepoRoot);
      const FString RunnerPath = RepoRoot / TEXT("scripts/ui-nav-test.ps1");
      const FString SpecsDir = RepoRoot / TEXT("tests/ui-nav");
      if (!FPaths::FileExists(RunnerPath)) {
        S.SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("ui-nav runner not found at: %s"), *RunnerPath),
            TEXT("RUNNER_NOT_FOUND"));
        return true;
      }

      TArray<FString> AvailableSpecs;
      IFileManager::Get().FindFiles(AvailableSpecs, *(SpecsDir / TEXT("*.json")), true, false);
      AvailableSpecs.Sort();

      FString SpecParam;
      Payload->TryGetStringField(TEXT("spec"), SpecParam);
      SpecParam.TrimStartAndEndInline();
      TArray<FString> SpecFiles;
      if (SpecParam.IsEmpty()) {
        SpecFiles = AvailableSpecs;
      } else {
        FString WantFile = FPaths::GetCleanFilename(SpecParam);
        if (!WantFile.EndsWith(TEXT(".json"))) {
          WantFile += TEXT(".json");
        }
        if (AvailableSpecs.Contains(WantFile)) {
          SpecFiles.Add(WantFile);
        }
      }
      if (SpecFiles.Num() == 0) {
        S.SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("No ui-nav spec matched '%s'. Available: %s"),
                            *SpecParam, *FString::Join(AvailableSpecs, TEXT(", "))),
            TEXT("SPEC_NOT_FOUND"));
        return true;
      }

      const FString RunId = FString::Printf(
          TEXT("uinav_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12));
      const FString LogDir =
          FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("McpTests"));
      IFileManager::Get().MakeDirectory(*LogDir, true);
      const FString RunLogPath = LogDir / (RunId + TEXT(".log"));

      // A generated driver script sidesteps cmd.exe→pwsh -Command quoting.
      // Each spec runs in its own nested pwsh so one spec's throw (step error)
      // can't abort the rest of the suite; exit code = failed spec count.
      FString Driver;
      Driver += TEXT("$fails = 0\n");
      for (const FString& SpecFile : SpecFiles) {
        const FString SpecFull = FPaths::ConvertRelativePathToFull(SpecsDir / SpecFile);
        Driver += FString::Printf(TEXT("Write-Output \"### SPEC: %s\"\n"), *SpecFile);
        Driver += FString::Printf(
            TEXT("& pwsh -NoProfile -ExecutionPolicy Bypass -File '%s' -Spec '%s'\n"),
            *RunnerPath, *SpecFull);
        Driver += TEXT("if ($LASTEXITCODE -ne 0) { $fails++ }\n");
      }
      Driver += TEXT("exit $fails\n");
      const FString DriverPath = LogDir / (RunId + TEXT("_driver.ps1"));
      if (!FFileHelper::SaveStringToFile(Driver, *DriverPath,
                                         FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Could not write driver script: %s"), *DriverPath),
                            TEXT("FILE_WRITE_FAILED"));
        return true;
      }

      const FString ShellParams = FString::Printf(
          TEXT("/d /s /c \"\"pwsh\" -NoProfile -ExecutionPolicy Bypass -File \"%s\" > \"%s\" 2>&1\""),
          *DriverPath, *RunLogPath);
      uint32 ProcessId = 0;
      FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
          TEXT("cmd.exe"), *ShellParams,
          true, true, true, &ProcessId, 0, nullptr, nullptr);
      if (!ProcessHandle.IsValid()) {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to launch the ui-nav runner process (is pwsh installed?)"),
                            TEXT("PROCESS_LAUNCH_FAILED"));
        return true;
      }

      TSharedPtr<UMcpAutomationBridgeSubsystem::FMcpExternalTestRun> Run = MakeShared<UMcpAutomationBridgeSubsystem::FMcpExternalTestRun>();
      Run->RunId = RunId;
      Run->Suite = TEXT("ui-nav");
      Run->SpecFiles = SpecFiles;
      Run->LogPath = RunLogPath;
      Run->ProcHandle = ProcessHandle;
      Run->ProcessId = ProcessId;
      Run->StartTime = FPlatformTime::Seconds();
      S.ActiveExternalTestRun = Run;

      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("runId"), RunId);
      Result->SetStringField(TEXT("suite"), TEXT("ui-nav"));
      Result->SetStringField(TEXT("status"), TEXT("running"));
      Result->SetNumberField(TEXT("specsTotal"), SpecFiles.Num());
      TArray<TSharedPtr<FJsonValue>> SpecsJson;
      for (const FString& SpecName : SpecFiles) {
        SpecsJson.Add(MakeShared<FJsonValueString>(SpecName));
      }
      Result->SetArrayField(TEXT("specs"), SpecsJson);
      Result->SetStringField(TEXT("logPath"), RunLogPath);
      S.SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("Started ui-nav suite (%d spec(s)). Poll get_test_results "
                               "with runId=%s. Keep the editor idle while it runs."),
                          SpecFiles.Num(), *RunId),
          Result);
      return true;
    }

    // Real TDD runner. Builds a queue of matching tests and returns immediately;
    // the subsystem Tick() drives one ExecuteLatentCommands step per frame via
    // TickAutomationTestRun() so latent/functional tests run over real frames and
    // never block this request handler. Poll results with get_test_results.
    if (S.ActiveExternalTestRun.IsValid() && !S.ActiveExternalTestRun->bFinished &&
        FPlatformProcess::IsProcRunning(S.ActiveExternalTestRun->ProcHandle)) {
      S.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("A ui-nav run is in progress (runId=%s); it drives PIE and "
                               "needs the editor to itself."),
                          *S.ActiveExternalTestRun->RunId),
          TEXT("RUN_IN_PROGRESS"));
      return true;
    }
    if (S.ActiveAutomationRun.IsValid() && !S.ActiveAutomationRun->bComplete) {
      TSharedPtr<FJsonObject> Busy = MakeShared<FJsonObject>();
      Busy->SetStringField(TEXT("runId"), S.ActiveAutomationRun->RunId);
      Busy->SetNumberField(TEXT("total"), S.ActiveAutomationRun->TestCommandNames.Num());
      Busy->SetNumberField(TEXT("completed"), S.ActiveAutomationRun->CurrentIndex);
      S.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("A test run is already in progress (runId=%s). "
                               "Poll get_test_results until it completes."),
                          *S.ActiveAutomationRun->RunId),
          Busy, TEXT("RUN_IN_PROGRESS"));
      return true;
    }

    // Capture per-test results via the framework's end-of-test delegate. The
    // editor's in-process automation worker concludes any active test on its own
    // tick, so this is how we read pass/fail even when we didn't call StopTest.
    if (!S.AutomationTestEndHandle.IsValid()) {
      S.AutomationTestEndHandle =
          FAutomationTestFramework::Get().OnTestEndEvent.AddUObject(
              &S, &UMcpAutomationBridgeSubsystem::OnAutomationTestEnded);
    }

    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);
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

    TSharedPtr<UMcpAutomationBridgeSubsystem::FMcpAutomationRun> Run = MakeShared<UMcpAutomationBridgeSubsystem::FMcpAutomationRun>();
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
      S.SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("No automation tests match filter '%s' (%d available). "
                               "Use list_tests to discover names."),
                          *Filter, AllTests.Num()),
          Result);
      return true;
    }

    // Don't start while the engine is busy / in PIE; the queue still starts and
    // TickAutomationTestRun waits for a clean frame.
    S.ActiveAutomationRun = Run;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("runId"), Run->RunId);
    Result->SetStringField(TEXT("status"), TEXT("running"));
    Result->SetStringField(TEXT("filter"), Filter);
    Result->SetNumberField(TEXT("total"), Run->TestCommandNames.Num());
    Result->SetArrayField(TEXT("tests"), QueuedNames);
    S.SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Started %d test(s). Poll get_test_results with runId=%s."),
                        Run->TestCommandNames.Num(), *Run->RunId),
        Result);
    return true;
}

bool McpHandlers::SystemControl::HandleSysGetTestResults(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    FString WantRunId;
    Payload->TryGetStringField(TEXT("runId"), WantRunId);
    WantRunId.TrimStartAndEndInline();

    // ui-nav suite poll: explicit uinav runId, or no runId with no automation
    // run to fall back to. Polls the detached runner process and parses its
    // log (### SPEC / [FAIL] / RESULT lines).
    if (S.ActiveExternalTestRun.IsValid() &&
        (WantRunId == S.ActiveExternalTestRun->RunId ||
         (WantRunId.IsEmpty() && !S.ActiveAutomationRun.IsValid()))) {
      TSharedPtr<UMcpAutomationBridgeSubsystem::FMcpExternalTestRun> Run = S.ActiveExternalTestRun;
      if (!Run->bFinished && !FPlatformProcess::IsProcRunning(Run->ProcHandle)) {
        Run->bFinished = true;
        int32 Code = -1;
        if (FPlatformProcess::GetProcReturnCode(Run->ProcHandle, &Code)) {
          Run->ReturnCode = Code;
        }
        FPlatformProcess::CloseProc(Run->ProcHandle);
      }

      // The redirecting shell still holds the log open while running.
      FString LogContent;
      FFileHelper::LoadFileToString(LogContent, *Run->LogPath,
                                    FFileHelper::EHashOptions::None,
                                    FILEREAD_AllowWrite);
      TArray<FString> Lines;
      LogContent.ParseIntoArrayLines(Lines);

      TArray<TSharedPtr<FJsonValue>> SpecsJson;
      TSharedPtr<FJsonObject> CurrentSpec;
      TArray<TSharedPtr<FJsonValue>> CurrentFailures;
      int32 SpecsCompleted = 0, SpecsFailed = 0, TotalPassed = 0, TotalFailed = 0;
      auto FlushSpec = [&]() {
        if (CurrentSpec.IsValid()) {
          CurrentSpec->SetArrayField(TEXT("failures"), CurrentFailures);
          SpecsJson.Add(MakeShared<FJsonValueObject>(CurrentSpec));
        }
        CurrentSpec.Reset();
        CurrentFailures.Reset();
      };
      for (const FString& RawLine : Lines) {
        FString Line = RawLine.TrimStartAndEnd();
        if (Line.StartsWith(TEXT("### SPEC: "))) {
          FlushSpec();
          CurrentSpec = MakeShared<FJsonObject>();
          CurrentSpec->SetStringField(TEXT("spec"), Line.RightChop(10));
          CurrentSpec->SetStringField(TEXT("status"), TEXT("running"));
        } else if (Line.StartsWith(TEXT("[FAIL]")) && CurrentSpec.IsValid()) {
          CurrentFailures.Add(MakeShared<FJsonValueString>(Line));
        } else if (Line.StartsWith(TEXT("RESULT: ")) && CurrentSpec.IsValid()) {
          // "RESULT: N passed, M failed"
          FRegexPattern ResultPattern(TEXT("RESULT: (\\d+) passed, (\\d+) failed"));
          FRegexMatcher Matcher(ResultPattern, Line);
          if (Matcher.FindNext()) {
            const int32 Passed = FCString::Atoi(*Matcher.GetCaptureGroup(1));
            const int32 Failed = FCString::Atoi(*Matcher.GetCaptureGroup(2));
            CurrentSpec->SetNumberField(TEXT("passed"), Passed);
            CurrentSpec->SetNumberField(TEXT("failed"), Failed);
            CurrentSpec->SetStringField(TEXT("status"),
                                        Failed == 0 ? TEXT("passed") : TEXT("failed"));
            TotalPassed += Passed;
            TotalFailed += Failed;
            ++SpecsCompleted;
            if (Failed > 0) {
              ++SpecsFailed;
            }
          }
        }
      }
      FlushSpec();

      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("runId"), Run->RunId);
      Result->SetStringField(TEXT("suite"), Run->Suite);
      Result->SetStringField(TEXT("status"),
                             Run->bFinished ? TEXT("complete") : TEXT("running"));
      Result->SetNumberField(TEXT("specsTotal"), Run->SpecFiles.Num());
      Result->SetNumberField(TEXT("specsCompleted"), SpecsCompleted);
      Result->SetNumberField(TEXT("assertionsPassed"), TotalPassed);
      Result->SetNumberField(TEXT("assertionsFailed"), TotalFailed);
      Result->SetArrayField(TEXT("specs"), SpecsJson);
      Result->SetNumberField(TEXT("elapsedSeconds"),
                             FPlatformTime::Seconds() - Run->StartTime);
      Result->SetStringField(TEXT("logPath"), Run->LogPath);
      if (Run->bFinished) {
        Result->SetNumberField(TEXT("returnCode"), Run->ReturnCode);
        // Exit code = failed spec count; a launch/runner failure (e.g. pwsh
        // missing → 9009, runner threw before RESULT) shows a nonzero code
        // with no parsed specs — surface the tail so the caller sees why.
        if (Run->ReturnCode != 0 || SpecsCompleted < Run->SpecFiles.Num()) {
          const int32 TailStart = FMath::Max(0, Lines.Num() - 20);
          TArray<TSharedPtr<FJsonValue>> Tail;
          for (int32 i = TailStart; i < Lines.Num(); ++i) {
            Tail.Add(MakeShared<FJsonValueString>(Lines[i]));
          }
          Result->SetArrayField(TEXT("logTail"), Tail);
        }
      }

      const FString Msg =
          Run->bFinished
              ? FString::Printf(TEXT("ui-nav run complete: %d/%d spec(s) passed "
                                     "(%d assertions passed, %d failed)."),
                                SpecsCompleted - SpecsFailed,
                                Run->SpecFiles.Num(), TotalPassed, TotalFailed)
              : FString::Printf(TEXT("ui-nav run in progress: %d/%d spec(s) complete."),
                                SpecsCompleted, Run->SpecFiles.Num());
      S.SendAutomationResponse(RequestingSocket, RequestId, true, Msg, Result);
      return true;
    }

    // Poll the active/last automation run. Returns status running|complete plus
    // per-test results once finished.
    if (!S.ActiveAutomationRun.IsValid()) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("No test run has been started. Call run_tests first."),
                          TEXT("NO_ACTIVE_RUN"));
      return true;
    }
    if (!WantRunId.IsEmpty() && WantRunId != S.ActiveAutomationRun->RunId) {
      S.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("runId '%s' not found; current run is '%s'."),
                          *WantRunId, *S.ActiveAutomationRun->RunId),
          TEXT("RUN_NOT_FOUND"));
      return true;
    }

    TSharedPtr<UMcpAutomationBridgeSubsystem::FMcpAutomationRun> Run = S.ActiveAutomationRun;
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
    S.SendAutomationResponse(RequestingSocket, RequestId, true, Msg, Result);
    return true;
}

// Execute Python code with stdout/stderr capture via temp file wrapper
bool McpHandlers::SystemControl::HandleSysExecutePython(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    FString Code;
    Payload->TryGetStringField(TEXT("code"), Code);
    FString File;
    Payload->TryGetStringField(TEXT("file"), File);

    const bool bHasCode = !Code.TrimStartAndEnd().IsEmpty();
    const bool bHasFile = !File.TrimStartAndEnd().IsEmpty();

    if (!bHasCode && !bHasFile) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("'code' or 'file' parameter is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (bHasCode && bHasFile) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Provide either 'code' or 'file', not both"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Enforce maximum code size (1 MB)
    static const int32 MaxCodeSize = 1048576;
    if (Code.Len() > MaxCodeSize) {
      S.SendAutomationError(RequestingSocket, RequestId,
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
        S.SendAutomationError(RequestingSocket, RequestId,
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
        S.SendAutomationError(RequestingSocket, RequestId,
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
        S.SendAutomationError(RequestingSocket, RequestId,
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
          S.SendAutomationError(RequestingSocket, RequestId,
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
      S.SendAutomationError(RequestingSocket, RequestId,
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

    S.SendProgressUpdate(RequestId, 0.0f, TEXT("Executing Python script"), true, S.CurrentRequestOrigin);

    // Execute via py console command with execution time tracking
    static constexpr double MaxPythonExecutionSeconds = 60.0;
    FString PyCommand = FString::Printf(TEXT("py \"%s\""), *ScriptPath);
    bool bExecHandled = false;
    double ExecStartTime = FPlatformTime::Seconds();
    if (GEngine) {
      bExecHandled = GEngine->Exec(World, *PyCommand);
    }
    double ExecElapsed = FPlatformTime::Seconds() - ExecStartTime;
    S.SendProgressUpdate(RequestId, 90.0f, TEXT("Collecting Python output"), true, S.CurrentRequestOrigin);
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
      S.SendAutomationError(RequestingSocket, RequestId,
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

    S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, PyMessage,
                           Result, bSuccess ? FString() : TEXT("PYTHON_ERROR"));
    return true;
}

#else

// RequiresEditor on every action above means Execute() rejects before Run()
// in non-editor builds; these exist only so the module links.
#define MCP_SYS_HANDLER_STUB(Fn)                                              \
  bool McpHandlers::SystemControl::Fn(UMcpAutomationBridgeSubsystem &, const FString &,                     \
                                         const TSharedPtr<FJsonObject> &,     \
                                         FMcpResponseHandle) {                \
    return false;                                                             \
  }

MCP_SYS_HANDLER_STUB(HandleSysGenerateTestStub)
MCP_SYS_HANDLER_STUB(HandleSysLiveCodingCompile)
MCP_SYS_HANDLER_STUB(HandleSysRunUbt)
MCP_SYS_HANDLER_STUB(HandleSysGetBuildStatus)
MCP_SYS_HANDLER_STUB(HandleSysListTests)
MCP_SYS_HANDLER_STUB(HandleSysRunTests)
MCP_SYS_HANDLER_STUB(HandleSysGetTestResults)
MCP_SYS_HANDLER_STUB(HandleSysExecutePython)
#undef MCP_SYS_HANDLER_STUB

bool McpHandlers::SystemControl::HandleSysStartSession(
    UMcpAutomationBridgeSubsystem &, const FString &, const TSharedPtr<FJsonObject> &,
    FMcpResponseHandle) { return false; }

#endif // WITH_EDITOR

// execute_command / set_cvar — console-handler delegations, available in all
// builds (HandleConsoleCommandAction owns availability and the security
// blocklist; "console_command" is its internal canonical name).
bool McpHandlers::SystemControl::HandleSysExecuteCommand(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  return HandleConsoleCommandAction(S, RequestId, TEXT("console_command"), Payload,
                                    Socket);
}

bool McpHandlers::SystemControl::HandleSysSetCvar(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // Compose a console command and reuse the audited console path.
  FString Key, Value;
  Payload->TryGetStringField(TEXT("key"), Key);
  Payload->TryGetStringField(TEXT("value"), Value);
  if (Key.IsEmpty()) {
    S.SendAutomationError(
        Socket, RequestId,
        TEXT("set_cvar requires 'key' (and usually "
             "'value')."),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  TSharedPtr<FJsonObject> CmdPayload = MakeShared<FJsonObject>();
  CmdPayload->SetStringField(
      TEXT("command"),
      Value.IsEmpty()
          ? Key
          : FString::Printf(TEXT("%s %s"), *Key, *Value));
  return HandleConsoleCommandAction(
      S, RequestId, TEXT("console_command"), CmdPayload, Socket);
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
