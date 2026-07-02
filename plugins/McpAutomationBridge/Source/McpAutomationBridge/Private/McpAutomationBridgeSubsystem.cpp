// Ensure the subsystem type and bridge socket helpers are available before the
// generated subsystem header pulls in UE version-dependent declarations.
#if WITH_EDITOR
#include "McpAutomationBridgeHelpers.h"
#endif

#include "McpAutomationBridgeSubsystem.h"
#include "MCP/McpNativeTransport.h"
#include "MCP/McpConsolidatedActionRouting.h"
#include "MCP/McpStartupValidation.h"
#include "HAL/PlatformTLS.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h" // OnTestEndEvent unbind in Deinitialize (TDD runner)

// =============================================================================
// FMcpRequestErrorDevice - Custom log device for per-request error capture
// =============================================================================

static constexpr int32 MaxCapturedRequestMessages = 32;
static constexpr int32 MaxCapturedRequestMessageChars = 1024;

static bool IsKnownBenignMcpCompilerWarning(const FString& Message)
{
    // GAS cooldown effect that grants no tags — cosmetic compiler warning.
    if (Message.Contains(TEXT("CooldownGameplayEffectClass"), ESearchCase::IgnoreCase) &&
        (Message.Contains(TEXT("no tags"), ESearchCase::IgnoreCase) ||
         Message.Contains(TEXT("grant any tags"), ESearchCase::IgnoreCase) ||
         Message.Contains(TEXT("grants no tag"), ESearchCase::IgnoreCase) ||
         Message.Contains(TEXT("granting no tag"), ESearchCase::IgnoreCase)))
    {
        return true;
    }
    // Self-healing WidgetBlueprint variable->GUID ensure: a stale variable-GUID entry for
    // a removed widget can trip a handled ensure on the next compile, which the compiler
    // then prunes itself. The asset is fine — don't surface it as ENGINE_ERROR. (The
    // remove_widget root fix prevents it; this is defense-in-depth for other paths.)
    if (Message.Contains(TEXT("SeenVariableNames"), ESearchCase::IgnoreCase) ||
        Message.Contains(TEXT("was deleted but still has a GUID"), ESearchCase::IgnoreCase))
    {
        return true;
    }
    // Editor-scripting utilities (UEditorAssetLibrary et al.) refuse to run
    // during PIE via EditorScriptingHelpers::CheckIfInEditorAndPIE(), which
    // logs this line at Error severity and bails. Bridge call paths have
    // PIE-safe fallbacks (LoadObject/FindObject/AssetRegistry); when the
    // fallback also fails the handler surfaces its own specific error, and
    // this guard only promotes on SUCCESS — so suppressing the line never
    // hides a real failure. (Captured form: "[LogUtils] The Editor is
    // currently in a play mode." — match without prefix/period.)
    if (Message.Contains(TEXT("The Editor is currently in a play mode"), ESearchCase::IgnoreCase))
    {
        return true;
    }
    return false;
}

/**
 * Custom output device that captures errors and warnings during request processing.
 * This is temporarily attached to GLog during handler execution to detect
 * engine-level errors (like ensure failures) that don't propagate as exceptions.
 * 
 * Note: Uses the subsystem's shared capture with mutex-protected access and
 * only records messages from the request thread that enabled capture.
 */
class FMcpRequestErrorDevice : public FOutputDevice
{
public:
    FMcpRequestErrorDevice(UMcpAutomationBridgeSubsystem* InSubsystem)
        : Subsystem(InSubsystem)
    {
    }

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        if (!Subsystem)
        {
            return;
        }

        const bool bIsErrorOrWarning =
            (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Warning);

        auto& Capture = Subsystem->CurrentErrorCapture;

        // Lock-free fast path: the editor logs a high volume of non-Error/Warning lines,
        // almost always with no request capture active -- skip those without touching the
        // mutex. Error/Warning lines are rare enough to always take the slow path, and a
        // non-Error line while a capture IS active still needs the lock (to close an open
        // handled-ensure block). A stale relaxed read only risks skipping a block-close at
        // the very edges of capture start/stop, which is harmless.
        if (!bIsErrorOrWarning &&
            !Capture.bActive.load(std::memory_order_relaxed))
        {
            return;
        }

        // Thread-safe access to shared capture
        FScopeLock Lock(&Subsystem->ErrorCaptureMutex);
        if (!Capture.bActive ||
            Capture.CapturingThreadId != FPlatformTLS::GetCurrentThreadId())
        {
            return;
        }

        // A "Handled ensure" prints a multi-line dump -- a header, the condition, the
        // message, "Stack:", then ~25 "[Callstack]" frames -- and the engine logs EVERY
        // line at Error verbosity even though it caught and recovered from the ensure.
        // None of those lines should fail the tool call: a handled ensure is a soft
        // assertion, not an operation failure (a real failure surfaces as a handler
        // returning false or a non-ensure Error). So we track the dump as a block: it
        // opens on the marker line and closes as soon as a normal (non-Error/Warning)
        // line is logged, i.e. logging has moved on. Bounding it this way means a genuine
        // Error logged after the dump is still caught.
        if (!bIsErrorOrWarning)
        {
            // Normal logging resumed -> any handled-ensure dump has ended.
            Capture.bInHandledEnsureBlock = false;
            return; // we only record Error/Warning
        }

        if (Verbosity == ELogVerbosity::Error &&
            FCString::Strstr(V, TEXT("=== Handled ensure")) != nullptr)
        {
            Capture.bInHandledEnsureBlock = true;
        }

        {
            FString Message = FString::Printf(TEXT("[%s] %s"), *Category.ToString(), V);
            if (Message.Len() > MaxCapturedRequestMessageChars)
            {
                Message = Message.Left(MaxCapturedRequestMessageChars) + TEXT("[TRUNCATED]");
            }

            const bool bTreatAsWarning =
                Verbosity == ELogVerbosity::Warning ||
                Capture.bInHandledEnsureBlock ||
                IsKnownBenignMcpCompilerWarning(Message);

            if (!bTreatAsWarning)
            {
                ++Capture.ErrorCount;
                if (Capture.ErrorMessages.Num() < MaxCapturedRequestMessages)
                {
                    Capture.ErrorMessages.Add(Message);
                }
                else
                {
                    Capture.bErrorMessagesTruncated = true;
                }
                Capture.bHasErrors = true;
            }
            else
            {
                ++Capture.WarningCount;
                if (Capture.WarningMessages.Num() < MaxCapturedRequestMessages)
                {
                    Capture.WarningMessages.Add(Message);
                }
                else
                {
                    Capture.bWarningMessagesTruncated = true;
                }
                Capture.bHasWarnings = true;
            }
        }
        
        // Note: We do not explicitly forward here. When this device is attached to GLog,
        // the engine still routes messages to all other output devices; this class only
        // captures errors and warnings without suppressing normal logging.
    }

    virtual bool CanBeUsedOnAnyThread() const override
    {
        return true;
    }

    virtual bool CanBeUsedOnMultipleThreads() const override
    {
        return true;
    }

private:
    UMcpAutomationBridgeSubsystem* Subsystem = nullptr;
};
#include "Dom/JsonObject.h"
#include "Async/TaskGraphInterfaces.h"
#include "Async/Async.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Editor-only includes for ExecuteEditorCommands
#if WITH_EDITOR
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

// Define the subsystem log category declared in the public header.
DEFINE_LOG_CATEGORY(LogMcpAutomationBridgeSubsystem);

/**
 * @brief Produces a log-safe copy of a string by replacing control characters
 * and truncating long input.
 *
 * Creates a sanitized version of the input string where characters with code
 * points less than 32 or equal to 127 are replaced with '?' and the result is
 * truncated to 512 characters with "[TRUNCATED]" appended if the input is
 * longer.
 *
 * @param In Input string to sanitize.
 * @return FString Sanitized string suitable for logging.
 */
static inline FString SanitizeForLog(const FString &In) {
  if (In.IsEmpty())
    return FString();
  FString Out;
  Out.Reserve(FMath::Min<int32>(In.Len(), 1024));
  for (int32 i = 0; i < In.Len(); ++i) {
    const TCHAR C = In[i];
    if (C >= 32 && C != 127)
      Out.AppendChar(C);
    else
      Out.AppendChar('?');
  }
  if (Out.Len() > 512)
    Out = Out.Left(512) + TEXT("[TRUNCATED]");
  return Out;
}

static inline bool IsAllowedUnrealMountPrefixAt(
    const FString &Value, int32 Index, const TCHAR *Mount) {
  const int32 MountLen = FCString::Strlen(Mount);
  if (Index + MountLen > Value.Len()) {
    return false;
  }

  if (!Value.Mid(Index, MountLen).Equals(Mount, ESearchCase::CaseSensitive)) {
    return false;
  }

  const int32 AfterMount = Index + MountLen;
  return AfterMount == Value.Len() || Value[AfterMount] == '/';
}

static inline bool IsAllowedUnrealMountPath(const FString &Value, int32 Index) {
  // /Script and /Temp are object-path roots, not content mounts, so they are
  // not in QueryRootContentPaths.
  if (IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Script")) ||
      IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Temp"))) {
    return true;
  }
  // Every registered content mount (/Game, /Engine, plugin mounts). Queried
  // per call, not cached: plugin mounts can register after startup, and the
  // input side accepts them (IsValidLongPackageName), so redacting them here
  // would mangle legitimate asset paths in error messages.
  TArray<FString> RootPaths;
  FPackageName::QueryRootContentPaths(RootPaths);
  for (const FString &Root : RootPaths) {
    FString Mount = Root;
    Mount.RemoveFromEnd(TEXT("/"));
    if (IsAllowedUnrealMountPrefixAt(Value, Index, *Mount)) {
      return true;
    }
  }
  return false;
}

static inline bool IsResponsePathChar(TCHAR C) {
  return FChar::IsAlnum(C) || C == '/' || C == '\\' || C == '.' ||
         C == '_' || C == '-' || C == ':' || C == '(' || C == ')' ||
         C == '+' || C == '~' || C == ' ';
}

static inline bool IsUnrealMountPathChar(TCHAR C) {
  return FChar::IsAlnum(C) || C == '/' || C == '.' || C == '_' ||
         C == '-' || C == ':';
}

static FString RedactFilesystemPathsForResponse(const FString &Input) {
  // A '/' starts a redactable Unix path only at a token boundary with a path
  // segment following it: prose like "decorators/services use add_subnode" or
  // "set_default / set_asset_property" is not a filesystem path.
  const auto IsTokenChar = [](TCHAR C) {
    return FChar::IsAlnum(C) || C == '_' || C == '.' || C == '-';
  };
  FString Output;
  Output.Reserve(Input.Len());

  for (int32 Index = 0; Index < Input.Len();) {
    const bool bAllowedUnrealPath = Input[Index] == '/' &&
                                    IsAllowedUnrealMountPath(Input, Index);
    const bool bUnixPath = Input[Index] == '/' && !bAllowedUnrealPath &&
                           (Index == 0 || !IsTokenChar(Input[Index - 1])) &&
                           Index + 1 < Input.Len() &&
                           IsTokenChar(Input[Index + 1]);
    const bool bWindowsPath = Index + 2 < Input.Len() &&
                              FChar::IsAlpha(Input[Index]) &&
                              Input[Index + 1] == ':' &&
                              (Input[Index + 2] == '/' || Input[Index + 2] == '\\');
    const bool bUncPath = Index + 1 < Input.Len() &&
                          Input[Index] == '\\' && Input[Index + 1] == '\\';

    if (bAllowedUnrealPath) {
      while (Index < Input.Len() && IsUnrealMountPathChar(Input[Index])) {
        Output.AppendChar(Input[Index]);
        ++Index;
      }
      continue;
    }

    if (bUnixPath || bWindowsPath || bUncPath) {
      Output += TEXT("[path redacted]");
      while (Index < Input.Len() && IsResponsePathChar(Input[Index])) {
        ++Index;
      }
      continue;
    }

    Output.AppendChar(Input[Index]);
    ++Index;
  }

  return Output;
}

static void RedactFollowingValueForResponse(FString &Text, const FString &Marker) {
  const auto IsValueDelimiter = [](TCHAR C) {
    return C == ';' || C == '&' || C == ',' || C == '\r' || C == '\n';
  };
  const auto IsHeaderDelimiter = [](TCHAR C) {
    return C == ';' || C == '&' || C == '\r' || C == '\n';
  };

  int32 SearchStart = 0;
  while (SearchStart < Text.Len()) {
    const int32 MarkerIndex = Text.Find(
        Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
    if (MarkerIndex == INDEX_NONE) {
      return;
    }

    const int32 ValueStart = MarkerIndex + Marker.Len();
    const bool bAllowWhitespaceInValue = Marker.EndsWith(TEXT(":"));
    int32 ValueEnd = ValueStart;
    if (bAllowWhitespaceInValue) {
      while (ValueEnd < Text.Len() && FChar::IsWhitespace(Text[ValueEnd]) &&
             Text[ValueEnd] != '\r' && Text[ValueEnd] != '\n') {
        ++ValueEnd;
      }
      while (ValueEnd < Text.Len() && !IsHeaderDelimiter(Text[ValueEnd])) {
        ++ValueEnd;
      }
    } else {
      while (ValueEnd < Text.Len() && !IsValueDelimiter(Text[ValueEnd]) &&
             !FChar::IsWhitespace(Text[ValueEnd])) {
        ++ValueEnd;
      }
    }

    Text = Text.Left(ValueStart) + TEXT("[redacted]") + Text.Mid(ValueEnd);
    SearchStart = ValueStart + 10;
  }
}

static void RedactSecretMarkersForResponse(FString &Out) {
  RedactFollowingValueForResponse(Out, TEXT("token="));
  RedactFollowingValueForResponse(Out, TEXT("capabilitytoken="));
  RedactFollowingValueForResponse(Out, TEXT("password="));
  RedactFollowingValueForResponse(Out, TEXT("secret="));
  RedactFollowingValueForResponse(Out, TEXT("api_key="));
  RedactFollowingValueForResponse(Out, TEXT("apikey="));
  RedactFollowingValueForResponse(Out, TEXT("authorization:"));
  RedactFollowingValueForResponse(Out, TEXT("bearer "));
}

// For engine-captured log lines quoted into responses; these can carry
// absolute filesystem paths, so paths outside content mounts are redacted.
static FString SanitizeEngineErrorForResponse(const FString &In) {
  FString Out = RedactFilesystemPathsForResponse(SanitizeForLog(In));
  RedactSecretMarkersForResponse(Out);

  if (Out.Len() > 512) {
    Out = Out.Left(512) + TEXT("[TRUNCATED]");
  }
  return Out;
}

// For handler-authored error messages: no path redaction — guidance routinely
// names action/param tokens and content paths the caller needs verbatim.
static FString SanitizeHandlerMessageForResponse(const FString &In) {
  FString Out = SanitizeForLog(In);
  RedactSecretMarkersForResponse(Out);
  return Out;
}

/**
 * @brief Initialize the automation bridge subsystem, preparing networking,
 * handlers, and periodic processing.
 *
 * Creates and initializes the connection manager, registers automation action
 * handlers and a message-received callback, starts the connection manager, and
 * registers a recurring ticker to process pending automation requests.
 *
 * NOTE: This subsystem is intentionally disabled during commandlet execution
 * (cooking, packaging, etc.) to prevent the WebSocket server from interfering
 * with cook operations and blocking writes to the staged build directory.
 *
 * @param Collection Subsystem collection provided by the engine during
 * initialization.
 */
void UMcpAutomationBridgeSubsystem::Initialize(
    FSubsystemCollectionBase &Collection) {
  Super::Initialize(Collection);

  // Skip initialization during commandlet execution (cooking, packaging, etc.)
  // The WebSocket server and background threads can interfere with cook
  // operations, particularly file I/O to the staged build directory.
  if (IsRunningCommandlet()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("McpAutomationBridgeSubsystem skipping initialization - running "
                "as commandlet (cook/package mode)."));
    return;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("McpAutomationBridgeSubsystem initializing."));

  // WebSocket transport removed — pull-only / native HTTP (see docs/pull-architecture.md).
  // FMcpConnectionManager (listener, heartbeat ticker, reconnect, telemetry, push) is
  // deleted; FMcpBridgeWebSocket remains only as the inert response-handle type used in
  // handler signatures (always nullptr on HTTP).

  // Initialize the handler registry
  InitializeHandlers();

  const auto* Settings = GetDefault<UMcpAutomationBridgeSettings>();
  const bool bBridgeEnabled = Settings && Settings->bEnableNativeMCP;

  // Register the log capture ring so get_log/tail_log can report "what just
  // happened" retrospectively (no prior subscribe needed). Bounded + cheap
  // append; see McpAutomationBridge_LogHandlers.cpp. Gated on the transport:
  // with the bridge off nothing can read the ring, so don't tax every log
  // line in ordinary editor sessions.
  if (bBridgeEnabled && GLog && !LogCaptureDevice.IsValid()) {
    LogRing.SetNum(LogRingCapacity);
    LogCaptureDevice = MakeLogCaptureDevice();
    GLog->AddOutputDevice(LogCaptureDevice.Get());
  }

  // Native MCP Streamable HTTP transport (opt-in)
  {
    if (bBridgeEnabled)
    {
      // Find plugin directory for loading tool schemas
      FString PluginDir;
      TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("McpAutomationBridge"));
      if (Plugin.IsValid())
      {
        PluginDir = Plugin->GetBaseDir();
      }

      NativeTransport = MakeShared<FMcpNativeTransport>(this);
      if (!NativeTransport->Start(Settings->NativeMCPPort, PluginDir, Settings->bLoadAllToolsOnStart,
                                  Settings->NativeMCPInstructions,
                                  Settings->ListenHost, Settings->bAllowNonLoopback))
      {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
               TEXT("Failed to start Native MCP server on port %d"), Settings->NativeMCPPort);
        NativeTransport.Reset();
      }
    }
  }

  // Register Ticker. Automation requests are drained here, after the engine
  // world tick has completed, so map transitions do not run from arbitrary
  // GameThread task-graph points inside active tick groups.
  TickHandle = FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateUObject(this,
                                     &UMcpAutomationBridgeSubsystem::Tick),
      0.0f
  );

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("McpAutomationBridgeSubsystem Initialized."));
}

/**
 * @brief Shuts down the MCP Automation Bridge subsystem and releases its
 * resources.
 *
 * Removes the registered ticker, stops and clears the connection manager,
 * detaches and clears the log capture device, and calls the superclass
 * deinitialization.
 *
 * NOTE: During commandlet execution (cooking, packaging), the subsystem
 * may not have fully initialized, so cleanup checks are defensive.
 */
void UMcpAutomationBridgeSubsystem::Deinitialize() {
  // Remove ticker if it was registered (won't be valid if we skipped init
  // during commandlet)
  if (TickHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    TickHandle.Reset();
  }

  // Unbind the automation test-end capture delegate (lazy-bound on first run_tests).
  if (AutomationTestEndHandle.IsValid()) {
    FAutomationTestFramework::Get().OnTestEndEvent.Remove(AutomationTestEndHandle);
    AutomationTestEndHandle.Reset();
  }

  // Skip verbose logging during commandlet mode since we didn't fully
  // initialize
  if (!IsRunningCommandlet()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("McpAutomationBridgeSubsystem deinitializing."));
  }

  if (NativeTransport)
  {
    NativeTransport->Shutdown();
    NativeTransport.Reset();
  }

  // (WebSocket ConnectionManager removed — nothing to stop)

  if (LogCaptureDevice.IsValid()) {
    if (GLog)
      GLog->RemoveOutputDevice(LogCaptureDevice.Get());
    LogCaptureDevice.Reset();
  }

  // Clean up RequestErrorDevice to prevent dangling pointer in GLog
  if (RequestErrorDevice.IsValid()) {
    FScopeLock Lock(&ErrorCaptureMutex);
    if (GLog)
      GLog->RemoveOutputDevice(RequestErrorDevice.Get());
    RequestErrorDevice.Reset();
  }

  Super::Deinitialize();
}

/**
 * @brief Reports whether the automation bridge currently has any active
 * connections.
 *
 * @return `true` if the connection manager exists and has one or more active
 * sockets, `false` otherwise.
 */
bool UMcpAutomationBridgeSubsystem::IsBridgeActive() const {
  // WebSocket transport removed; the native HTTP transport is the live bridge.
  return NativeTransport.IsValid() && NativeTransport->IsRunning();
}

/**
 * @brief Determine the bridge's connection state from active sockets.
 *
 * Maps the connection manager's state to the subsystem's bridge state enum.
 * Returns Connected if active sockets exist, Connecting if a reconnect is
 * pending, or Disconnected otherwise.
 *
 * @return EMcpAutomationBridgeState The current connection state.
 */
EMcpAutomationBridgeState
UMcpAutomationBridgeSubsystem::GetBridgeState() const {
  // WebSocket transport removed; reflect the native HTTP transport instead.
  if (NativeTransport.IsValid() && NativeTransport->IsRunning()) {
    return EMcpAutomationBridgeState::Connected;
  }
  return EMcpAutomationBridgeState::Disconnected;
}

// =============================================================================
// Per-Request Error Capture Implementation
// =============================================================================

UMcpAutomationBridgeSubsystem::FRequestErrorCapture& UMcpAutomationBridgeSubsystem::GetCurrentErrorCapture()
{
    return CurrentErrorCapture;
}

void UMcpAutomationBridgeSubsystem::BeginErrorCapture()
{
    // Create and attach the error capture device if not already
    if (!RequestErrorDevice.IsValid())
    {
        RequestErrorDevice = MakeShared<FMcpRequestErrorDevice>(this);
    }

    {
        // Clear any previous capture state (thread-safe)
        FScopeLock Lock(&ErrorCaptureMutex);
        CurrentErrorCapture.Reset();
        CurrentErrorCapture.CapturingThreadId = FPlatformTLS::GetCurrentThreadId();
        CurrentErrorCapture.bActive = true;
    }

    // Attach to GLog to capture errors
    if (GLog && RequestErrorDevice.IsValid())
    {
        GLog->AddOutputDevice(RequestErrorDevice.Get());
    }
}

TArray<FString> UMcpAutomationBridgeSubsystem::EndErrorCapture()
{
    // Detach the error capture device
    if (GLog && RequestErrorDevice.IsValid())
    {
        GLog->RemoveOutputDevice(RequestErrorDevice.Get());
    }
    
    // Get captured errors (thread-safe)
    FScopeLock Lock(&ErrorCaptureMutex);

    TArray<FString> AllMessages;
    AllMessages.Reserve(CurrentErrorCapture.ErrorMessages.Num() +
                        CurrentErrorCapture.WarningMessages.Num());
    for (const FString& ErrorMessage : CurrentErrorCapture.ErrorMessages)
    {
        AllMessages.Add(SanitizeEngineErrorForResponse(ErrorMessage));
    }
    for (const FString& WarningMessage : CurrentErrorCapture.WarningMessages)
    {
        AllMessages.Add(SanitizeEngineErrorForResponse(WarningMessage));
    }
    CurrentErrorCapture.bActive = false;
    CurrentErrorCapture.CapturingThreadId = 0;
    
    return AllMessages;
}

void UMcpAutomationBridgeSubsystem::ClearCapturedErrors()
{
    FScopeLock Lock(&ErrorCaptureMutex);
    CurrentErrorCapture.ErrorMessages.Empty();
    CurrentErrorCapture.WarningMessages.Empty();
    CurrentErrorCapture.ErrorCount = 0;
    CurrentErrorCapture.WarningCount = 0;
    CurrentErrorCapture.bErrorMessagesTruncated = false;
    CurrentErrorCapture.bWarningMessagesTruncated = false;
    CurrentErrorCapture.bHasErrors = false;
    CurrentErrorCapture.bHasWarnings = false;
    // Leaves bActive / CapturingThreadId intact so capture continues afterwards.
}

bool UMcpAutomationBridgeSubsystem::HasCapturedErrors() const
{
    FScopeLock Lock(&ErrorCaptureMutex);
    return CurrentErrorCapture.bHasErrors.load();
}

TArray<FString> UMcpAutomationBridgeSubsystem::GetCapturedErrorMessages() const
{
    FScopeLock Lock(&ErrorCaptureMutex);
    TArray<FString> SanitizedMessages;
    SanitizedMessages.Reserve(CurrentErrorCapture.ErrorMessages.Num());
    for (const FString& ErrorMessage : CurrentErrorCapture.ErrorMessages)
    {
        SanitizedMessages.Add(SanitizeEngineErrorForResponse(ErrorMessage));
    }
    return SanitizedMessages;
}

/**
 * @brief Forward a raw text message to the connection manager for transmission.
 *
 * @param Message The raw message string to send.
 * @return `true` if the connection manager accepted the message for sending,
 * `false` otherwise.
 */
bool UMcpAutomationBridgeSubsystem::SendRawMessage(const FString &Message) {
  // WebSocket transport removed.
  (void)Message;
  return false;
}

/**
 * @brief Per-frame tick that processes deferred automation requests when it is
 * safe to do so.
 *
 * Invokes processing of any pending automation requests that were previously
 * deferred due to unsafe engine states (saving, garbage collection, or async
 * loading).
 *
 * @param DeltaTime Time elapsed since the last tick, in seconds.
 * @return true to remain registered and continue receiving ticks.
 */
bool UMcpAutomationBridgeSubsystem::Tick(float DeltaTime) {
  // Drain pending requests only outside unsafe engine states.
  if (!GIsSavingPackage && !IsGarbageCollecting() && !IsAsyncLoading()) {
    ProcessPendingAutomationRequests();
    // Advance any in-progress automation test run one step per frame
    // (system_control run_tests / get_test_results). No-op when idle.
    TickAutomationTestRun();
  }
  // Cleanup stale HTTP pending requests (5 minute timeout)
  if (NativeTransport)
  {
    NativeTransport->CleanupStaleRequests();
  }
  return true;
}

void UMcpAutomationBridgeSubsystem::QueueAutomationRequest(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket,
    ERequestOrigin Origin) {
  FPendingAutomationRequest Pending;
  Pending.RequestId = RequestId;
  Pending.Action = Action;
  Pending.Payload = Payload;
  Pending.RequestingSocket = RequestingSocket;
  Pending.Origin = Origin;

  {
    FScopeLock Lock(&PendingAutomationRequestsMutex);
    PendingAutomationRequests.Add(MoveTemp(Pending));
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("Queued automation request for core ticker: RequestId=%s action=%s"),
         *RequestId, *Action);
}

// The in-file implementation of ProcessAutomationRequest was intentionally
// removed from this translation unit. The function is now implemented in
// McpAutomationBridge_ProcessRequest.cpp to avoid duplicate definitions and
// to keep this file focused. See that file for the full request dispatcher
/**
 * @brief Sends an automation response for a specific request to the given
 * socket.
 *
 * If the connection manager is not available this call is a no-op.
 *
 * @param TargetSocket WebSocket to which the response will be sent.
 * @param RequestId Identifier of the automation request being responded to.
 * @param bSuccess `true` if the request succeeded, `false` otherwise.
 * @param Message Human-readable message or description associated with the
 * response.
 * @param Result Optional JSON object containing result data; may be null.
 * @param ErrorCode Error code string to include when `bSuccess` is `false`.
 *
 * Successful handler responses are converted to ENGINE_ERROR failures when
 * Unreal logged errors during the active automation request. This keeps tool
 * results aligned with the editor log instead of reporting success for a
 * request whose handler triggered engine errors.
 */

void UMcpAutomationBridgeSubsystem::SendAutomationResponse(
    FMcpResponseHandle TargetSocket, const FString &RequestId,
    const bool bSuccess, const FString &Message,
    const TSharedPtr<FJsonObject> &Result, const FString &ErrorCode,
    ERequestOrigin Origin) {
  bool bEffectiveSuccess = bSuccess;
  FString EffectiveMessage = Message;
  FString EffectiveErrorCode = ErrorCode;
  TSharedPtr<FJsonObject> EffectiveResult = Result;

  if (bSuccess && bProcessingAutomationRequest)
  {
    TArray<FString> CapturedErrors;
    int32 TotalCapturedErrorCount = 0;
    bool bCapturedErrorsTruncated = false;
    {
      FScopeLock Lock(&ErrorCaptureMutex);
      if (CurrentErrorCapture.bHasErrors.load())
      {
        CapturedErrors = CurrentErrorCapture.ErrorMessages;
        TotalCapturedErrorCount = CurrentErrorCapture.ErrorCount;
        bCapturedErrorsTruncated = CurrentErrorCapture.bErrorMessagesTruncated;
      }
    }

    if (CapturedErrors.Num() > 0)
    {
      bEffectiveSuccess = false;
      EffectiveErrorCode = TEXT("ENGINE_ERROR");
      const FString FirstError = SanitizeEngineErrorForResponse(CapturedErrors[0]);
      EffectiveMessage = FString::Printf(
          TEXT("Handler reported success but Unreal logged errors: %s"),
          *FirstError.Left(240));

      TSharedPtr<FJsonObject> AugmentedResult = MakeShared<FJsonObject>();
      if (Result.IsValid())
      {
        for (const auto &Pair : Result->Values)
        {
          AugmentedResult->SetField(Pair.Key, Pair.Value);
        }
      }

      TArray<TSharedPtr<FJsonValue>> ErrorValues;
      const int32 MaxErrorsInResponse = 3;
      const int32 ErrorResponseCount = FMath::Min(CapturedErrors.Num(), MaxErrorsInResponse);
      for (int32 ErrorIndex = 0; ErrorIndex < ErrorResponseCount; ++ErrorIndex)
      {
        ErrorValues.Add(MakeShared<FJsonValueString>(
            SanitizeEngineErrorForResponse(CapturedErrors[ErrorIndex])));
      }
      AugmentedResult->SetBoolField(TEXT("success"), false);
      AugmentedResult->SetNumberField(TEXT("engineErrorCount"), TotalCapturedErrorCount);
      AugmentedResult->SetArrayField(TEXT("engineErrors"), ErrorValues);
      if (bCapturedErrorsTruncated || CapturedErrors.Num() > MaxErrorsInResponse)
      {
        AugmentedResult->SetBoolField(TEXT("engineErrorsTruncated"), true);
      }

      TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
      ErrorObj->SetStringField(TEXT("code"), EffectiveErrorCode);
      ErrorObj->SetStringField(TEXT("message"), EffectiveMessage);
      AugmentedResult->SetObjectField(TEXT("error"), ErrorObj);
      EffectiveResult = AugmentedResult;
    }
  }

  if (!bEffectiveSuccess)
  {
    EffectiveMessage = SanitizeHandlerMessageForResponse(EffectiveMessage);
  }

  // When handlers omit Origin (default Unspecified), use the stored
  // CurrentRequestOrigin from the active ProcessAutomationRequest call. A live
  // native pending request is authoritative when even that is stale.
  ERequestOrigin EffectiveOrigin = (Origin == ERequestOrigin::Unspecified)
      ? CurrentRequestOrigin : Origin;
  if (Origin == ERequestOrigin::Unspecified && NativeTransport &&
      NativeTransport->HasPendingRequest(RequestId))
  {
    EffectiveOrigin = ERequestOrigin::NativeHTTP;
  }
  if (EffectiveOrigin == ERequestOrigin::NativeHTTP && NativeTransport)
  {
    if (!NativeTransport->CompletePendingRequest(RequestId, bEffectiveSuccess, EffectiveMessage, EffectiveResult, EffectiveErrorCode))
    {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("Native HTTP response for %s dropped — request already expired or unknown"),
        *RequestId);
    }
    return;
  }
  UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
    TEXT("Response for %s has no route (origin unresolved, no pending native "
         "request) and was dropped."),
    *RequestId);
}

/**
 * @brief Log a failure and send a standardized automation error response.
 *
 * Resolves an empty ErrorCode to "AUTOMATION_ERROR", logs a sanitized warning
 * with the resolved error and message, and sends a failure response for the
 * specified request.
 *
 * @param TargetSocket Optional socket to target the response; may be null to
 * broadcast or use a default.
 * @param RequestId Identifier of the automation request that failed.
 * @param Message Human-readable failure message.
 * @param ErrorCode Error code to include with the response; "AUTOMATION_ERROR"
 * is used if empty.
 */
void UMcpAutomationBridgeSubsystem::SendAutomationError(
    FMcpResponseHandle TargetSocket, const FString &RequestId,
    const FString &Message, const FString &ErrorCode) {
  const FString ResolvedError =
      ErrorCode.IsEmpty() ? TEXT("AUTOMATION_ERROR") : ErrorCode;
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("Automation request failed (%s): %s"), *ResolvedError,
         *SanitizeForLog(Message));
  SendAutomationResponse(TargetSocket, RequestId, false, Message, nullptr,
                         ResolvedError);
}

/**
 * @brief Progress hook for long-running handlers.
 *
 * No-op under the pull-only transport: there is no server->client channel to stream
 * progress on, and (per the MCP spec) progress notifications would not extend the
 * client's wall-clock timeout anyway. Kept as a stable entry point so long-running
 * handlers can report progress without guarding every call site -- if a streaming
 * channel is ever reintroduced, this is the single place to wire it up.
 */
void UMcpAutomationBridgeSubsystem::SendProgressUpdate(
    const FString &RequestId, float Percent, const FString &Message, bool bStillWorking,
    ERequestOrigin Origin) {
  (void)RequestId;
  (void)Percent;
  (void)Message;
  (void)bStillWorking;
  (void)Origin;
}

/**
 * @brief Records telemetry for an automation request with outcome details.
 *
 * Forwards the request identifier, success flag, human-readable message, and
 * error code to the connection manager for telemetry/logging.
 *
 * @param RequestId Unique identifier of the automation request.
 * @param bSuccess `true` if the request completed successfully, `false`
 * otherwise.
 * @param Message Human-readable message describing the outcome or context.
 * @param ErrorCode Short error identifier (empty if none).
 */
void UMcpAutomationBridgeSubsystem::RecordAutomationTelemetry(
    const FString &RequestId, const bool bSuccess, const FString &Message,
    const FString &ErrorCode) {
  // WebSocket transport removed; telemetry was WS-only.
  (void)RequestId; (void)bSuccess; (void)Message; (void)ErrorCode;
}

/**
 * @brief Registers an automation action handler for the given action string.
 *
 * Null handlers and duplicate action keys are programmer errors: both are
 * logged as errors and ensure, so a bad registration is visible at editor boot
 * instead of surfacing as UNKNOWN_ACTION (or the wrong handler) at request
 * time. A duplicate key keeps the first registration.
 *
 * @param Action The action identifier string used to look up the handler.
 * @param Handler Callable invoked when the specified action is requested.
 */
void UMcpAutomationBridgeSubsystem::RegisterHandler(
    const FString &Action, FAutomationHandler Handler) {
  if (!Handler) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("RegisterHandler: null handler for action '%s' ignored."),
           *Action);
    ensureMsgf(false, TEXT("Null automation handler for '%s'"), *Action);
    return;
  }
  if (AutomationHandlers.Contains(Action)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("RegisterHandler: action '%s' registered twice; keeping the "
                "first handler."),
           *Action);
    ensureMsgf(false, TEXT("Duplicate automation handler for '%s'"), *Action);
    return;
  }
  AutomationHandlers.Add(Action, Handler);
}

/**
 * @brief Registers all automation action handlers used by the MCP Automation
 * Bridge.
 *
 * Populates the subsystem's handler registry with mappings from action name
 * strings (for example: core/property actions, array/map/set container ops,
 * asset dependency queries, console/system and editor tooling actions,
 * blueprint/world/asset management, rendering/materials, input/control,
 * audio/lighting/physics/effects, and performance actions) to the functions
 * that handle those actions so incoming automation requests can be dispatched
 * by action name.
 *
 * This also registers a few common alias actions (e.g., "create_effect",
 * "clear_debug_shapes") so those actions dispatch directly to the intended
 * handler.
 */
// MCP_REGISTER_HANDLER collapses the trivial forwarding-lambda registration boilerplate
// (an action name -> a member handler taking the same 4 args). The multi-branch routers
// (manage_blueprint, manage_asset, system_control, ...) that inspect the sub-action before
// dispatching are registered longhand below.
#define MCP_REGISTER_HANDLER(ActionName, HandlerFn) \
  RegisterHandler(TEXT(ActionName), \
                  [this](const FString &R, const FString &A, \
                         const TSharedPtr<FJsonObject> &P, \
                         FMcpResponseHandle S) { \
                    return HandlerFn(R, A, P, S); \
                  })

void UMcpAutomationBridgeSubsystem::InitializeHandlers() {
  // Only the 22 canonical MCP tool names are registered: the transport
  // dispatches by tool name alone, so any other key is unreachable. The
  // legacy per-action registrations from the Node-server era were removed
  // 2026-07-02.
  RegisterHandler(TEXT("build_environment"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         FMcpResponseHandle S) {
                    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
                    if (McpConsolidatedActions::IsLightingAction(SubAction)) {
                      return HandleLightingAction(R, TEXT("manage_lighting"), P, S);
                    }
                    if (McpConsolidatedActions::IsSplineAction(SubAction)) {
                      return HandleManageSplinesAction(R, TEXT("manage_splines"), P, S);
                    }
                    return HandleBuildEnvironmentAction(R, A, P, S);
                  });

  // Tools & System
  MCP_REGISTER_HANDLER("inspect", HandleInspectAction);
  RegisterHandler(TEXT("system_control"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         FMcpResponseHandle S) {
                    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
                    if (McpConsolidatedActions::IsPerformanceAction(SubAction)) {
                      return HandlePerformanceAction(R, TEXT("manage_performance"), P, S);
                    }
                    // Schema/router drift repairs: these sub-actions are
                    // advertised by the system_control schema but their real
                    // implementations live behind other canonical action
                    // names — re-dispatch like the Performance branch above.
                    // (The old linear chain that used to catch fall-throughs
                    // is gone; anything not routed here dies in
                    // HandleSystemControlAction's entry gate.)
                    if (SubAction == TEXT("screenshot") ||
                        SubAction == TEXT("create_widget") ||
                        SubAction == TEXT("add_widget_child") ||
                        SubAction == TEXT("get_project_settings") ||
                        SubAction == TEXT("set_project_setting")) {
                      return HandleUiAction(R, A, P, S);
                    }
                    if (SubAction == TEXT("console_command") ||
                        SubAction == TEXT("execute_command")) {
                      // Same path control_editor uses; inherits the console
                      // security blocklist.
                      return HandleConsoleCommandAction(
                          R, TEXT("console_command"), P, S);
                    }
                    if (SubAction == TEXT("set_cvar")) {
                      // set_cvar was advertised with key/value params but had
                      // no handler anywhere. Compose a console command and
                      // reuse the audited console path.
                      FString Key, Value;
                      P->TryGetStringField(TEXT("key"), Key);
                      P->TryGetStringField(TEXT("value"), Value);
                      if (Key.IsEmpty()) {
                        SendAutomationError(
                            S, R,
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
                          R, TEXT("console_command"), CmdPayload, S);
                    }
                    if (SubAction == TEXT("subscribe") ||
                        SubAction == TEXT("unsubscribe") ||
                        SubAction == TEXT("get_log") ||
                        SubAction == TEXT("tail_log") ||
                        SubAction == TEXT("clear_log")) {
                      return HandleLogAction(R, TEXT("manage_logs"), P, S);
                    }
                    if (SubAction == TEXT("spawn_category")) {
                      return HandleDebugAction(R, TEXT("manage_debug"), P, S);
                    }
                    if (SubAction == TEXT("lumen_update_scene")) {
                      return HandleRenderAction(R, TEXT("manage_render"), P, S);
                    }
                    return HandleSystemControlAction(R, A, P, S);
                  });

  MCP_REGISTER_HANDLER("control_actor", HandleControlActorAction);
  MCP_REGISTER_HANDLER("control_editor", HandleControlEditorAction);

  MCP_REGISTER_HANDLER("manage_level", HandleLevelAction);

  MCP_REGISTER_HANDLER("manage_sequence", HandleSequenceAction);

  RegisterHandler(TEXT("manage_asset"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         FMcpResponseHandle S) {
                    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
                    if (McpConsolidatedActions::IsMaterialAuthoringAction(SubAction)) {
                      return HandleManageMaterialAuthoringAction(
                          R, TEXT("manage_material_authoring"), P, S);
                    }
                    if (McpConsolidatedActions::IsTextureAction(SubAction)) {
                      return HandleManageTextureAction(R, TEXT("manage_texture"), P, S);
                    }
                    return HandleAssetAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_blueprint"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         FMcpResponseHandle S) {
                    FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
                    // Common UI actions are checked BEFORE widget-authoring so they
                    // route to the deletable CommonUI translation unit and never
                    // enter the large HandleManageWidgetAuthoringAction function.
                    if (McpConsolidatedActions::IsCommonUiAction(SubAction)) {
                      return HandleCommonUiAction(R, TEXT("manage_common_ui"), P, S);
                    }
                    if (McpConsolidatedActions::IsWidgetAuthoringAction(SubAction)) {
                      return HandleManageWidgetAuthoringAction(
                          R, TEXT("manage_widget_authoring"), P, S);
                    }
                    if (McpConsolidatedActions::IsBlueprintGraphAction(SubAction)) {
                      return HandleBlueprintGraphAction(R, A, P, S);
                    }
                    return HandleBlueprintAction(R, A, P, S);
                  });

  MCP_REGISTER_HANDLER("manage_geometry", HandleGeometryAction);

  MCP_REGISTER_HANDLER("manage_gas", HandleManageGASAction);

  MCP_REGISTER_HANDLER("manage_character", HandleManageCharacterAction);

  MCP_REGISTER_HANDLER("manage_combat", HandleManageCombatAction);

  MCP_REGISTER_HANDLER("manage_ai", HandleManageAIAction);

  MCP_REGISTER_HANDLER("manage_inventory", HandleManageInventoryAction);

  MCP_REGISTER_HANDLER("manage_interaction", HandleManageInteractionAction);

  RegisterHandler(TEXT("manage_networking"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         FMcpResponseHandle S) {
                    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
                    if (McpConsolidatedActions::IsInputAction(SubAction)) {
                      return HandleInputAction(R, TEXT("manage_input"), P, S);
                    }
                    if (McpConsolidatedActions::IsGameFrameworkAction(SubAction)) {
                      return HandleManageGameFrameworkAction(
                          R, TEXT("manage_game_framework"), P, S);
                    }
                    if (McpConsolidatedActions::IsSessionAction(SubAction)) {
                      return HandleManageSessionsAction(R, TEXT("manage_sessions"), P, S);
                    }
                    return HandleManageNetworkingAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_audio"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         FMcpResponseHandle S) {
                    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
                    if (McpConsolidatedActions::IsAudioAuthoringAction(SubAction)) {
                      const TSharedPtr<FJsonObject> RoutedPayload =
                          McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                      return HandleManageAudioAuthoringAction(
                          R, TEXT("manage_audio_authoring"), RoutedPayload, S);
                    }
                    // HandleAudioAction's entry gate matches on the sub-action
                    // prefix, not the tool name; passing A ("manage_audio") fails
                    // the gate and the request would go unrouted.
                    return HandleAudioAction(R, SubAction, P, S);
                  });

  RegisterHandler(TEXT("animation_physics"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         FMcpResponseHandle S) {
                    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
                    if (McpConsolidatedActions::IsAnimationAuthoringAction(SubAction)) {
                      const TSharedPtr<FJsonObject> RoutedPayload =
                          McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                      return HandleManageAnimationAuthoringAction(
                          R, TEXT("manage_animation_authoring"), RoutedPayload, S);
                    }
                    if (McpConsolidatedActions::IsSkeletonAction(SubAction)) {
                      return HandleManageSkeleton(R, TEXT("manage_skeleton"), P, S);
                    }
                    return HandleAnimationPhysicsAction(R, A, P, S);
                  });

  MCP_REGISTER_HANDLER("manage_effect", HandleEffectAction);

  RegisterHandler(TEXT("manage_level_structure"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         FMcpResponseHandle S) {
                    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
                    if (McpConsolidatedActions::IsVolumeAction(SubAction)) {
                      return HandleManageVolumesAction(R, TEXT("manage_volumes"), P, S);
                    }
                    return HandleManageLevelStructureAction(R, A, P, S);
                  });

#undef MCP_REGISTER_HANDLER

  McpStartupValidation::ValidateActionRouting();
}

// Drain and process any automation requests that were enqueued while the
// subsystem was busy. This implementation lives in the primary subsystem
// translation unit to ensure the symbol is available at link time for
/**
 * @brief Processes all queued automation requests on the game thread.
 *
 * Ensures execution on the game thread (re-dispatches if called from another
 * thread), moves the shared pending-request queue into a local list under a
 * lock, clears the shared queue and the scheduled flag, then dispatches each
 * request to ProcessAutomationRequest.
 */
void UMcpAutomationBridgeSubsystem::ProcessPendingAutomationRequests() {
  if (!IsInGameThread()) {
    AsyncTask(ENamedThreads::GameThread,
              [this]() { this->ProcessPendingAutomationRequests(); });
    return;
  }

  // One request per tick: the core ticker calls this every frame, so a burst
  // of queued requests spreads across frames instead of stacking all their
  // handler runtimes into one editor stall. Requests re-queued mid-dispatch
  // (reentrancy/GC guards) land at the back and retry next tick.
  FPendingAutomationRequest Req;
  {
    FScopeLock Lock(&PendingAutomationRequestsMutex);
    if (PendingAutomationRequests.Num() == 0) {
      return;
    }
    Req = MoveTemp(PendingAutomationRequests[0]);
    PendingAutomationRequests.RemoveAt(0);
  }

  ProcessAutomationRequest(Req.RequestId, Req.Action, Req.Payload,
                           Req.RequestingSocket, Req.Origin);
}

// ============================================================================
// ExecuteEditorCommands Implementation
// ============================================================================
/**
 * @brief Executes a list of editor console commands sequentially.
 *
 * Uses GEditor->Exec() to execute each command in the provided array.
 * Stops on first failure and returns the error message.
 *
 * @param Commands Array of console command strings to execute.
 * @param OutErrorMessage Error message if any command fails.
 * @return true if all commands executed successfully, false otherwise.
 */
bool UMcpAutomationBridgeSubsystem::ExecuteEditorCommands(
    const TArray<FString> &Commands, FString &OutErrorMessage) {
#if WITH_EDITOR
  // GEditor operations must run on the game thread
  check(IsInGameThread());
  
  if (!GEditor) {
    OutErrorMessage = TEXT("Editor not available");
    return false;
  }

  UWorld *EditorWorld = GEditor->GetEditorWorldContext().World();
  if (!EditorWorld) {
    OutErrorMessage = TEXT("Editor world context not available");
    return false;
  }

  for (const FString &Command : Commands) {
    const FString TrimmedCommand = Command.TrimStartAndEnd();
    if (TrimmedCommand.IsEmpty()) {
      continue;
    }

    if (McpContainsUnsafeCommandSeparator(TrimmedCommand)) {
      OutErrorMessage = FString::Printf(
          TEXT("Rejected unsafe editor command: %s"), *TrimmedCommand);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("ExecuteEditorCommands: %s"), *OutErrorMessage);
      return false;
    }

    // Execute the command via GEditor
    // Note: GEditor->Exec returns true if the command was handled
    if (!GEditor->Exec(EditorWorld, *TrimmedCommand)) {
      OutErrorMessage =
          FString::Printf(TEXT("Failed to execute command: %s"), *TrimmedCommand);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("ExecuteEditorCommands: %s"), *OutErrorMessage);
      return false;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("ExecuteEditorCommands: Executed '%s'"), *TrimmedCommand);
  }

  return true;
#else
  OutErrorMessage = TEXT("Editor commands only available in editor builds");
  return false;
#endif
}

// ============================================================================
// CreateControlRigBlueprint Implementation
// ============================================================================
// Note: ControlRigBlueprintFactory is only available in UE 5.1+ or as private API
// The MCP_HAS_CONTROLRIG_FACTORY macro is defined in McpAutomationBridgeHelpers.h

#if MCP_HAS_CONTROLRIG_FACTORY
// Animation/Skeleton types needed for ControlRig blueprint creation
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"

// ControlRig includes - these are only available when ControlRig plugin is enabled
// UE 5.7 renamed ControlRigBlueprint.h to ControlRigBlueprintLegacy.h
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
#include "ControlRigBlueprintLegacy.h"
#else
#include "ControlRigBlueprint.h"
#endif
// Include the generated class header for UControlRigBlueprintGeneratedClass
#include "ControlRigBlueprintGeneratedClass.h"
// Note: ControlRigBlueprintFactory header is Public only in UE 5.5+
// For UE 5.1-5.4 we use a fallback implementation with FKismetEditorUtilities
#if MCP_HAS_CONTROLRIG_FACTORY && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
  #include "ControlRigBlueprintFactory.h"
#endif
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"  // For DoesAssetExist, LoadAsset in crash prevention checks
#endif

#if MCP_HAS_CONTROLRIG_FACTORY
/**
 * @brief Creates a new Control Rig Blueprint asset.
 *
 * Uses UControlRigBlueprintFactory to create the asset at the specified
 * location with the given skeleton as the target.
 *
 * @param AssetName Name for the new Control Rig Blueprint.
 * @param PackagePath Package path where the asset should be created (e.g.,
 * "/Game/Rigs").
 * @param TargetSkeleton Skeleton to associate with the Control Rig.
 * @param OutError Error message if creation fails.
 * @return Pointer to the created UBlueprint, or nullptr on failure.
 */
UBlueprint *UMcpAutomationBridgeSubsystem::CreateControlRigBlueprint(
    const FString &AssetName, const FString &PackagePath,
    USkeleton *TargetSkeleton, FString &OutError) {
#if WITH_EDITOR
  if (AssetName.IsEmpty()) {
    OutError = TEXT("Asset name cannot be empty");
    return nullptr;
  }

  if (PackagePath.IsEmpty()) {
    OutError = TEXT("Package path cannot be empty");
    return nullptr;
  }

  // Normalize the package path
  FString NormalizedPath = PackagePath;
  NormalizedPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
  NormalizedPath.ReplaceInline(TEXT("\\"), TEXT("/"));

  // Ensure path starts with /Game
  if (!NormalizedPath.StartsWith(TEXT("/Game"))) {
    NormalizedPath = TEXT("/Game") / NormalizedPath;
  }

  // Remove trailing slashes
  while (NormalizedPath.EndsWith(TEXT("/"))) {
    NormalizedPath.LeftChopInline(1);
  }

  // Build full package name
  FString FullPackageName = NormalizedPath / AssetName;

  // ============================================================================
  // CRASH PREVENTION: Pre-check for existing assets
  // ============================================================================
  // UE crashes when:
  // 1. Creating a blueprint with a name that already exists (assertion)
  // 2. Creating an object where a different class exists at that path (fatal)
  // We must check BEFORE calling CreatePackage/CreateBlueprint.

  // Check 1: Does a saved asset exist at this path?
  FString FullObjectPath = FullPackageName + TEXT(".") + AssetName;
  if (UEditorAssetLibrary::DoesAssetExist(FullObjectPath)) {
    UObject *ExistingAsset = UEditorAssetLibrary::LoadAsset(FullObjectPath);
    if (ExistingAsset) {
      // Check if it's a ControlRigBlueprint - safe to reuse
      if (ExistingAsset->IsA<UControlRigBlueprint>()) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Control Rig Blueprint already exists, reusing: %s"),
               *FullObjectPath);
        // Return existing asset - operation is idempotent
        return Cast<UBlueprint>(ExistingAsset);
      } else {
        // Different type at same path - would cause fatal crash
        OutError = FString::Printf(
            TEXT("Asset exists at path but is not a ControlRigBlueprint (is %s). "
                 "Cannot create ControlRigBlueprint at this path."),
            *ExistingAsset->GetClass()->GetName());
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"),
               *OutError);
        return nullptr;
      }
    }
  }

  // Check 2: Is there an in-memory object at this path? (unsaved blueprint)
  // This catches objects that exist in memory but aren't saved to disk yet
  UPackage *ExistingPackage = FindPackage(nullptr, *FullPackageName);
  if (ExistingPackage) {
    UObject *ExistingObject = FindObject<UObject>(ExistingPackage, *AssetName);
    if (ExistingObject) {
      if (ExistingObject->IsA<UControlRigBlueprint>()) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Control Rig Blueprint already exists in memory, reusing: %s"),
               *FullObjectPath);
        return Cast<UBlueprint>(ExistingObject);
      } else {
        OutError = FString::Printf(
            TEXT("In-memory object exists at path but is not a ControlRigBlueprint (is %s). "
                 "Cannot create ControlRigBlueprint at this path."),
            *ExistingObject->GetClass()->GetName());
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"),
               *OutError);
        return nullptr;
      }
    }
  }

  // Create the package
  UPackage *Package = CreatePackage(*FullPackageName);
  if (!Package) {
    OutError =
        FString::Printf(TEXT("Failed to create package: %s"), *FullPackageName);
    return nullptr;
  }

  Package->FullyLoad();

  // Create the Control Rig Blueprint using FKismetEditorUtilities
  // This works across all UE versions without needing ControlRigBlueprintFactory
  // Note: Use UControlRigBlueprintGeneratedClass instead of URigVMBlueprintGeneratedClass
  // to avoid needing to include RigVM module headers
  UControlRigBlueprint *NewBlueprint = Cast<UControlRigBlueprint>(
      FKismetEditorUtilities::CreateBlueprint(
          UControlRig::StaticClass(),  // Parent class
          Package,                      // Outer
          *AssetName,                   // Name
          BPTYPE_Normal,                // Blueprint type
          UControlRigBlueprint::StaticClass(),  // Blueprint class
          UControlRigBlueprintGeneratedClass::StaticClass(),  // Generated class
          NAME_None));

  if (!NewBlueprint) {
    OutError = TEXT("Factory failed to create Control Rig Blueprint");
    return nullptr;
  }

  // Set the target skeleton if provided
  if (TargetSkeleton) {
    // UControlRigBlueprint uses a preview skeletal mesh, not skeleton directly
    // Try to find a skeletal mesh that uses this skeleton
    USkeletalMesh *PreviewMesh = TargetSkeleton->GetPreviewMesh();
    if (PreviewMesh) {
      NewBlueprint->SetPreviewMesh(PreviewMesh);
    }
  }

  // Notify asset registry
  FAssetRegistryModule::AssetCreated(NewBlueprint);

  // Mark package dirty for save
  NewBlueprint->MarkPackageDirty();

  // Use safe asset save (UE 5.7 compatible)
  McpSafeAssetSave(NewBlueprint);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Created Control Rig Blueprint: %s"), *FullPackageName);

  return NewBlueprint;
#else
  OutError = TEXT("Control Rig creation only available in editor builds");
  return nullptr;
#endif // WITH_EDITOR
}
#endif // MCP_HAS_CONTROLRIG_FACTORY
