#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EditorSubsystem.h"
#include "HAL/CriticalSection.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Templates/SharedPointer.h"
#include "Engine/DataAsset.h"
// FMcpResponseHandle + the subsystem log category live in Globals.h so the
// helpers layer does not need this header.
#include "McpAutomationBridgeGlobals.h"
class FMcpNativeTransport;

#include "McpAutomationBridgeSubsystem.generated.h"

// Define MCP_HAS_CONTROLRIG_FACTORY based on UE version
// ControlRigBlueprintFactory is available in all UE 5.x versions
// Note: In UE 5.1-5.4 the header is in Private folder, but the class is exported with CONTROLRIGEDITOR_API
// so we use forward declaration instead of including the header
#ifndef MCP_HAS_CONTROLRIG_FACTORY
  #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    #define MCP_HAS_CONTROLRIG_FACTORY 1
  #else
    #define MCP_HAS_CONTROLRIG_FACTORY 0
  #endif
#endif

// Forward declare USkeleton to avoid including heavy animation headers
class USkeleton;

/**
 * Concrete data asset class for MCP inventory/item operations.
 * Both UDataAsset and UPrimaryDataAsset are abstract in UE5,
 * so we need a concrete wrapper that can be instantiated.
 */
UCLASS(BlueprintType)
class MCPAUTOMATIONBRIDGE_API UMcpGenericDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    /** Generic name/ID for this data asset */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP Data")
    FString ItemName;

    /** Optional description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP Data")
    FString Description;

    /** Generic key-value properties for extensibility */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP Data")
    TMap<FString, FString> Properties;
};


UENUM(BlueprintType)
enum class EMcpAutomationBridgeState : uint8 {
  Disconnected,
  Connecting,
  Connected
};

/** Minimal payload wrapper for incoming automation messages. */
USTRUCT(BlueprintType)
struct MCPAUTOMATIONBRIDGE_API FMcpAutomationMessage {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "MCP Automation")
  FString Type;

  UPROPERTY(BlueprintReadOnly, Category = "MCP Automation")
  FString PayloadJson;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMcpAutomationMessageReceived,
                                            const FMcpAutomationMessage &,
                                            Message);


enum class ERequestOrigin : uint8
{
	// Handler-facing default meaning "not specified by the caller"; response
	// routing resolves it via CurrentRequestOrigin / the pending-request map.
	Unspecified,
	NativeHTTP
};

UCLASS()
class MCPAUTOMATIONBRIDGE_API UMcpAutomationBridgeSubsystem
    : public UEditorSubsystem {
  GENERATED_BODY()

public:
  virtual void Initialize(FSubsystemCollectionBase &Collection) override;
  virtual void Deinitialize() override;

  UFUNCTION(BlueprintCallable, Category = "MCP Automation")
  bool IsBridgeActive() const;

  UFUNCTION(BlueprintCallable, Category = "MCP Automation")
  EMcpAutomationBridgeState GetBridgeState() const;

  UFUNCTION(BlueprintCallable, Category = "MCP Automation")
  bool SendRawMessage(const FString &Message);

  // Append one log line to the capture ring (see the log members below and
  // McpAutomationBridge_LogHandlers.cpp). Called by FMcpLogOutputDevice from
  // arbitrary threads — thread-safe, cheap (stores raw fields; formatting is
  // deferred to read time in get_log/tail_log).
  void CaptureLogLine(ELogVerbosity::Type Verbosity, const FName &Category,
                      const TCHAR *Message);

  // Constructs the FMcpLogOutputDevice (defined in the LogHandlers TU, so the
  // concrete type stays encapsulated there). Returned as the base FOutputDevice.
  TSharedPtr<FOutputDevice> MakeLogCaptureDevice();

  UPROPERTY(BlueprintAssignable, Category = "MCP Automation")
  FMcpAutomationMessageReceived OnMessageReceived;

  // Public helpers for sending automation responses/errors. These need to be
  // callable from out-of-line helper functions and translation-unit-level
  // handlers that receive a UMcpAutomationBridgeSubsystem* (e.g. static
  // blueprint helper routines). They were previously declared private which
  // prevented those helpers from invoking them via a 'Self' pointer.
  //
  // OWNERSHIP: the Result object is handed off by this call — it is
  // serialized on a worker thread after the call returns, so callers must
  // not mutate it (or anything reachable from it) afterwards. The universal
  // `SendAutomationResponse(...); return true;` shape satisfies this.
  void SendAutomationResponse(FMcpResponseHandle TargetSocket,
                              const FString &RequestId,
                              bool bSuccess, const FString &Message,
                              const TSharedPtr<FJsonObject> &Result = nullptr,
                              const FString &ErrorCode = FString(),
                              ERequestOrigin Origin = ERequestOrigin::Unspecified);
  void SendAutomationError(FMcpResponseHandle TargetSocket,
                            const FString &RequestId, const FString &Message,
                            const FString &ErrorCode);

  // Loads the UBlueprint named by BlueprintPath, or sends the standard error and returns
  // nullptr: INVALID_ARGUMENT when BlueprintPath is empty, NOT_FOUND when it fails to load.
  // Collapses the load-or-error prologue hand-rolled across handler families; callers do
  // `UBlueprint* BP = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket); if (!BP) return true;`.
  UBlueprint *ResolveBlueprintOrError(const FString &BlueprintPath,
                                      const FString &RequestId,
                                      FMcpResponseHandle Socket,
                                      const TCHAR *FieldName = TEXT("blueprintPath"));

  /**
   * Send a progress update message during long-running operations.
   * This keeps the request alive by extending its timeout on the server side.
   * 
   * @param RequestId The request ID being tracked
   * @param Percent Optional progress percent (0-100), negative to omit
   * @param Message Optional status message
   * @param bStillWorking True if operation is still in progress (prevents stale detection)
   */
  void SendProgressUpdate(const FString &RequestId, float Percent = -1.0f, 
                          const FString &Message = TEXT(""), bool bStillWorking = true,
                          ERequestOrigin Origin = ERequestOrigin::Unspecified);

  bool ExecuteEditorCommands(const TArray<FString> &Commands,
                             FString &OutErrorMessage);
#if MCP_HAS_CONTROLRIG_FACTORY
  UBlueprint *CreateControlRigBlueprint(const FString &AssetName,
                                        const FString &PackagePath,
                                        USkeleton *TargetSkeleton,
                                        FString &OutError);
#endif

  // Automation Handler Delegate
  using FAutomationHandler = TFunction<bool(const FString &, const FString &,
                                            const TSharedPtr<FJsonObject> &,
                                            FMcpResponseHandle)>;

  /**
   * Registers a handler for a specific automation action.
   * This allows for O(1) dispatch of automation requests and runtime
   * extensibility.
   */
  void RegisterHandler(const FString &Action, FAutomationHandler Handler);

  // =========================================================================
  // Per-Request Error Capture (Public for handler access)
  // =========================================================================
  
  /**
   * Storage for capturing errors during request execution.
   * This is used to detect engine-level errors (like ensure failures)
   * that don't propagate as exceptions but indicate operation failure.
    *
    * Note: Uses thread-safe access via ErrorCaptureMutex because the capture
    * device is attached to global GLog while a request is active.
    */
  struct FRequestErrorCapture
  {
    TArray<FString> ErrorMessages;
    TArray<FString> WarningMessages;
    int32 ErrorCount = 0;
    int32 WarningCount = 0;
    bool bErrorMessagesTruncated = false;
    bool bWarningMessagesTruncated = false;
    std::atomic<bool> bHasErrors{false};
    std::atomic<bool> bHasWarnings{false};
    uint32 CapturingThreadId = 0;
    // Atomic so the log-capture device can take a lock-free fast path: the editor logs
    // a high volume of non-Error lines, and we only need the mutex when a capture is
    // actually active (or for the rare Error/Warning line). Set under ErrorCaptureMutex.
    std::atomic<bool> bActive{false};
    // True while we're inside a "Handled ensure" multi-line dump. The engine logs
    // the whole dump (header + condition + message + Stack: + callstack frames) at
    // Error verbosity even though it recovered, so we downgrade those lines to
    // warnings. Opens on the dump's marker line, closes when normal logging resumes.
    bool bInHandledEnsureBlock = false;

    // Reset is for internal use only - must be called with ErrorCaptureMutex held
    void Reset()
    {
      ErrorMessages.Empty();
      WarningMessages.Empty();
      ErrorCount = 0;
      WarningCount = 0;
      bErrorMessagesTruncated = false;
      bWarningMessagesTruncated = false;
      bHasErrors = false;
      bHasWarnings = false;
      CapturingThreadId = 0;
      bActive = false;
      bInHandledEnsureBlock = false;
    }
  };
  
  /** Get the current request's error capture */
  FRequestErrorCapture& GetCurrentErrorCapture();
  
  /** Begin capturing errors for a request */
  void BeginErrorCapture();
  
  /** End capturing errors and return any captured errors */
  TArray<FString> EndErrorCapture();
  
  /** Check if any errors were captured during the current request */
  bool HasCapturedErrors() const;

  /** Return a thread-safe snapshot of captured engine error messages. */
  TArray<FString> GetCapturedErrorMessages() const;

  /**
   * Discard errors/warnings captured so far during the current request while
   * leaving capture active. A handler that triggers transient engine logs (e.g.
   * a Blueprint recompile inside an edit) but then re-verifies the end state is
   * correct can call this to avoid a false ENGINE_ERROR response.
   */
  void ClearCapturedErrors();

  // Friend class for error capture device to access private members
  friend class FMcpRequestErrorDevice;

private:
  /** Request-scoped error capture (shared, not thread-local) */
  FRequestErrorCapture CurrentErrorCapture;
  
  /** Mutex for thread-safe access to error capture from worker threads */
  mutable FCriticalSection ErrorCaptureMutex;
  
  /** Custom log output device for per-request error capture */
  TSharedPtr<class FMcpRequestErrorDevice> RequestErrorDevice;

public:
  bool Tick(float DeltaTime);

  void QueueAutomationRequest(const FString &RequestId, const FString &Action,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket,
                              ERequestOrigin Origin = ERequestOrigin::Unspecified);

  // WebSocket ConnectionManager removed (pull-only / native HTTP).

  /** Native MCP Streamable HTTP transport */
  TSharedPtr<FMcpNativeTransport> NativeTransport;

  // Track a blueprint currently being modified by this subsystem request
  // so scope-exit handlers can reliably clear busy state without
  // attempting to capture local variables inside macros.
  FString CurrentBusyBlueprintKey;
  bool bCurrentBlueprintBusyMarked = false;
  bool bCurrentBlueprintBusyScheduled = false;

  // --- Automation test runs (TDD: run_tests / get_test_results) ---------------
  // run_tests builds a queue and returns immediately; the subsystem Tick advances
  // ONE ExecuteLatentCommands per frame so latent/functional tests run over real
  // frames and never block the request handler (a synchronous in-handler run could
  // open a map / enter PIE and hang headlessly). get_test_results polls this state.
  struct FMcpAutomationRun
  {
    FString RunId;
    TArray<FString> TestCommandNames;  // FAutomationTestInfo::GetTestName() (passed to StartTestByName)
    TArray<FString> TestDisplayNames;  // FAutomationTestInfo::GetDisplayName() (reporting)
    TArray<FString> TestFullPaths;     // FAutomationTestInfo::GetFullTestPath() (StartTestByName path arg)
    int32 CurrentIndex = 0;
    bool bCurrentStarted = false;
    double CurrentTestStartTime = 0.0; // wall-clock when the current test started (per-test timeout)
    bool bComplete = false;
    int32 Passed = 0;
    int32 Failed = 0;
    double StartTime = 0.0;
    TArray<TSharedPtr<FJsonObject>> Results;  // per-test: { test, success, errors[] }
    // Transient capture for the in-flight test, written by OnAutomationTestEnded
    // (bound to FAutomationTestFramework::OnTestEndEvent). The editor's in-process
    // automation worker concludes any active test itself, so we read the result
    // from that delegate rather than always owning StopTest.
    bool bCaptured = false;
    bool bCapturedSuccess = false;
    int32 CapturedErrorCount = 0;
    int32 CapturedWarningCount = 0;
    TArray<FString> CapturedErrors;
  };
  /** Active automation run (one at a time). Null when idle. */
  TSharedPtr<FMcpAutomationRun> ActiveAutomationRun;
  /** Advance the active automation test run by one step. Called from Tick(). */
  void TickAutomationTestRun();
  /** Bound to FAutomationTestFramework::OnTestEndEvent; captures the in-flight result. */
  void OnAutomationTestEnded(class FAutomationTestBase *Test);
  /** Handle for the OnTestEndEvent binding (lazy-bound on first run_tests). */
  FDelegateHandle AutomationTestEndHandle;

  // Pending automation request queue (thread-safe). Inbound socket threads
  // will enqueue requests here; the queue is drained sequentially on the
  // game thread to ensure deterministic processing order and avoid
  // reentrancy issues.
  struct FPendingAutomationRequest {
    FString RequestId;
    FString Action;
    TSharedPtr<FJsonObject> Payload;
    FMcpResponseHandle RequestingSocket;
    ERequestOrigin Origin = ERequestOrigin::Unspecified;
  };
  TArray<FPendingAutomationRequest> PendingAutomationRequests;
  FCriticalSection PendingAutomationRequestsMutex;
  void ProcessPendingAutomationRequests();

  // Origin of the currently-processing request — used by SendAutomationResponse
  // and SendAutomationError as fallback when handlers don't pass Origin explicitly.
  // Set at the start of ProcessAutomationRequest, cleared on exit.
  ERequestOrigin CurrentRequestOrigin = ERequestOrigin::Unspecified;

  // A handler that intentionally completes a request after returning true
  // (e.g. import's next-tick deferral) must call MarkRequestDeferred first;
  // otherwise the dispatcher treats a still-parked request as a bug and fails
  // it with HANDLER_NO_RESPONSE. Marks are consumed by the post-dispatch check.
  void MarkRequestDeferred(const FString &RequestId) { DeferredRequestIds.Add(RequestId); }
  TSet<FString> DeferredRequestIds;

  void RecordAutomationTelemetry(const FString &RequestId, bool bSuccess,
                                 const FString &Message,
                                 const FString &ErrorCode);

  // Active Log Device
  TSharedPtr<FOutputDevice> LogCaptureDevice;

  // ---- Log capture ring (pull-only log visibility) ----------------------
  // The capture device (registered always-on in Initialize) appends lines
  // here; system_control get_log/tail_log reads them. This replaces the old
  // server-push path that fed the now-dead SendRawMessage no-op. Bounded so
  // memory is fixed regardless of log volume; a circular buffer keeps append
  // O(1) (no front-eviction array shift).
  struct FMcpLogEntry
  {
    uint64 Seq = 0;                                    // monotonic id (survives clear)
    double Time = 0.0;                                 // FPlatformTime::Seconds() at capture
    ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
    FName Category;
    FString Message;
  };
  TArray<FMcpLogEntry> LogRing;        // preallocated to LogRingCapacity
  int32 LogRingHead = 0;               // index of the oldest valid entry
  int32 LogRingCount = 0;              // number of valid entries
  int32 LogRingCapacity = 5000;        // max retained lines
  uint64 LogRingNextSeq = 0;           // next seq to assign == total ever captured
  bool bLogCaptureEnabled = true;      // unsubscribe flips this off (device early-outs)
  FCriticalSection LogRingMutex;       // Serialize() runs on arbitrary threads

  // ---- UBT build jobs (run_ubt fire-and-poll) ----------------------------
  // run_ubt launches UBT detached with stdout redirected to a log file and
  // returns immediately; get_build_status polls the process and parses the
  // log. Game-thread only (both actions dispatch there) — no mutex needed.
  struct FMcpUbtBuildJob
  {
    FString BuildId;
    FString Target, Platform, Configuration;
    FString LogPath;
    FString CommandLine;
    FProcHandle ProcHandle;
    uint32 ProcessId = 0;
    double StartTime = 0.0;   // FPlatformTime::Seconds() at launch
    bool bFinished = false;
    int32 ReturnCode = -1;
    double EndTime = 0.0;
  };
  TMap<FString, FMcpUbtBuildJob> UbtBuildJobs;
  FString LastUbtBuildId;

  // ---- External spec-suite run (run_tests suite:"ui-nav") -----------------
  // The detached runner process drives THIS bridge over HTTP (play/nav/assert
  // steps arrive as ordinary requests) while get_test_results polls the
  // process + log — same fire-and-poll shape as run_ubt. Game-thread only;
  // one external run at a time.
  struct FMcpExternalTestRun
  {
    FString RunId;
    FString Suite;
    TArray<FString> SpecFiles;
    FString LogPath;
    FProcHandle ProcHandle;
    uint32 ProcessId = 0;
    double StartTime = 0.0;
    bool bFinished = false;
    int32 ReturnCode = -1;
  };
  TSharedPtr<FMcpExternalTestRun> ActiveExternalTestRun;

private:
  TMap<FString, FAutomationHandler> AutomationHandlers;
  void InitializeHandlers();

  /**
   * Handle lightweight, well-known editor function invocations sent from the
   * server. This action is intended as a native replacement for the
   * execute_editor_python fallback for common scripted templates (spawn,
   * delete, list actors, set viewport camera, asset existence checks, etc.).
   * When the plugin implements a native function we will handle it here and
   * avoid executing arbitrary Python inside the editor.
   */
  bool
  HandleExecuteEditorFunction(const FString &RequestId, const FString &Action,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
public:
  // Called directly by the classed inspect set_property/get_property
  // (MCP/Calls/); Action carries their internal set_object_property /
  // get_object_property dispatch keys.
  bool
  HandleSetObjectProperty(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool
  HandleGetObjectProperty(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);

private:
  // Array manipulation operations
  bool HandleArrayAppend(const FString &RequestId, const FString &Action,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle RequestingSocket);
  bool HandleArrayRemove(const FString &RequestId, const FString &Action,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle RequestingSocket);
  bool HandleArrayInsert(const FString &RequestId, const FString &Action,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle RequestingSocket);
  bool HandleArrayGetElement(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleArraySetElement(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleArrayClear(const FString &RequestId, const FString &Action,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle RequestingSocket);
  // Map manipulation operations
  bool HandleMapSetValue(const FString &RequestId, const FString &Action,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle RequestingSocket);
  bool HandleMapGetValue(const FString &RequestId, const FString &Action,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle RequestingSocket);
  bool HandleMapRemoveKey(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleMapHasKey(const FString &RequestId, const FString &Action,
                       const TSharedPtr<FJsonObject> &Payload,
                       FMcpResponseHandle RequestingSocket);
  bool HandleMapGetKeys(const FString &RequestId, const FString &Action,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle RequestingSocket);
  bool HandleMapClear(const FString &RequestId, const FString &Action,
                      const TSharedPtr<FJsonObject> &Payload,
                      FMcpResponseHandle RequestingSocket);
  // Set manipulation operations
  bool HandleSetAdd(const FString &RequestId, const FString &Action,
                    const TSharedPtr<FJsonObject> &Payload,
                    FMcpResponseHandle RequestingSocket);
  bool HandleSetRemove(const FString &RequestId, const FString &Action,
                       const TSharedPtr<FJsonObject> &Payload,
                       FMcpResponseHandle RequestingSocket);
  bool HandleSetContains(const FString &RequestId, const FString &Action,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle RequestingSocket);
  bool HandleSetClear(const FString &RequestId, const FString &Action,
                      const TSharedPtr<FJsonObject> &Payload,
                      FMcpResponseHandle RequestingSocket);
  // Asset-related automation actions implemented by the plugin (editor-only
  // operations)
  bool HandleAssetAction(const FString &RequestId, const FString &Action,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle RequestingSocket);
  // Asset dependency graph traversal
  bool
  HandleGetAssetReferences(const FString &RequestId, const FString &Action,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);
  bool
  HandleGetAssetDependencies(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  // control_actor and control_editor dispatch chains were classed away —
  // see MCP/Calls/.
  // manage_level is classed — see MCP/Calls/McpCalls_ManageLevel.cpp.
  // system_control is classed — see MCP/Calls/McpCalls_SystemControl.cpp;
  // its member handlers (HandlePerf*/HandleSys*/HandleUi*/HandleLog*/
  // HandleDebugSpawnCategory/HandleRenderLumenUpdateScene) live in the
  // transitional public block below.
  // Animation and physics related automation actions
  bool HandleAnimationPhysicsAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleEffectAction(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintAction(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  // manage_sequence is classed — see MCP/Calls/McpCalls_ManageSequence.cpp.
  bool HandleInputAction(const FString &RequestId, const FString &Action,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle RequestingSocket);
  bool CreateNiagaraEffect(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket,
                           const FString &EffectName,
                           const FString &DefaultSystemPath);
  // Audio related automation actions
  bool HandleAudioAction(const FString &RequestId, const FString &Action,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle RequestingSocket);
  bool
  HandleCreateDialogueVoice(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool
  HandleCreateDialogueWave(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);
  bool
  HandleSetDialogueContext(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);
  bool
  HandleCreateReverbEffect(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);
  bool HandleCreateSourceEffectChain(
      const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleAddSourceEffect(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool
  HandleCreateSubmixEffect(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);
  // Lighting related automation actions
public:
  // Called directly by the classed manage_level create_light (MCP/Calls/).
  bool HandleLightingAction(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);

private:
  // Environment building automation actions (landscape, foliage, etc.)
  bool HandleBuildEnvironmentAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleControlEnvironmentAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandlePaintFoliage(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool
  HandleGetFoliageInstances(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool HandleRemoveFoliage(const FString &RequestId, const FString &Action,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);
  bool HandleGenerateLODs(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleBakeLightmap(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleCreateProceduralTerrain(const FString &RequestId, const FString &Action,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleCreateProceduralFoliage(const FString &RequestId, const FString &Action,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleAddFoliageType(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  // Asset workflow handlers
  bool
  HandleSourceControlCheckout(const FString &RequestId, const FString &Action,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool
  HandleSourceControlSubmit(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool
  HandleGetSourceControlState(const FString &RequestId, const FString &Action,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool
  HandleSourceControlEnable(const FString &RequestId, const FString &Action,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool
  HandleAnalyzeGraph(const FString &RequestId, const FString &Action,
                     const TSharedPtr<FJsonObject> &Payload,
                     FMcpResponseHandle RequestingSocket);
  bool HandleFixupRedirectors(const FString &RequestId, const FString &Action,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandleBulkRenameAssets(const FString &RequestId, const FString &Action,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandleBulkDeleteAssets(const FString &RequestId, const FString &Action,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool
  HandleGenerateThumbnail(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool
  HandleNaniteRebuildMesh(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool
  HandleFindByTag(const FString &RequestId, const FString &Action,
                  const TSharedPtr<FJsonObject> &Payload,
                  FMcpResponseHandle RequestingSocket);
  bool
  HandleSearchAssets(const FString &RequestId, const FString &Action,
                     const TSharedPtr<FJsonObject> &Payload,
                     FMcpResponseHandle RequestingSocket);
  bool
  HandleAddMaterialNode(const FString &RequestId, const FString &Action,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle RequestingSocket);
  bool
  HandleConnectMaterialPins(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool
  HandleRemoveMaterialNode(const FString &RequestId, const FString &Action,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);
  bool
  HandleBreakMaterialConnections(const FString &RequestId,
                                 const FString &Action,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle
                                     RequestingSocket);
  bool
  HandleGetMaterialNodeDetails(const FString &RequestId, const FString &Action,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle
                                   RequestingSocket);
  bool
  HandleRebuildMaterial(const FString &RequestId, const FString &Action,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle RequestingSocket);
  // Landscape, foliage, and Niagara handlers
  bool HandleCreateLandscape(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleCreateLandscapeGrassType(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Aggregate landscape editor that dispatches to specific edit ops
  bool HandleEditLandscape(const FString &RequestId, const FString &Action,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);
  // Specific landscape edit operations
  bool HandleModifyHeightmap(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool
  HandlePaintLandscapeLayer(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool HandleSculptLandscape(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool
  HandleSetLandscapeMaterial(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleCreateNiagaraSystemNative(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool
  HandleAddSequencerKeyframe(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool
  HandleManageSequencerTrack(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  // Niagara system handlers
  bool
  HandleCreateNiagaraSystem(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool
  HandleCreateNiagaraRibbon(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool
  HandleCreateNiagaraEmitter(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool
  HandleSpawnNiagaraActor(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleModifyNiagaraParameter(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Animation blueprint handlers
  bool HandlePlayAnimMontage(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleSetupRagdoll(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleActivateRagdoll(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  // Sequencer track handlers
  bool HandleAddCameraTrack(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool
  HandleAddAnimationTrack(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool
  HandleAddTransformTrack(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  // Foliage handlers
  bool
  HandleAddFoliageInstances(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  // SCS Blueprint authoring handler
  bool HandleSCSAction(const FString &RequestId, const FString &Action,
                       const TSharedPtr<FJsonObject> &Payload,
                       FMcpResponseHandle RequestingSocket);

  // Additional consolidated tool handlers
  bool
  HandleConsoleCommandAction(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
public:
  // Called directly by the classed inspect inspect_cdo/diff_asset/ui_focus (MCP/Calls/).
  bool HandleInspectCdoAction(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  // Structural diff of two on-disk .uasset versions (e.g. a git revision vs the
  // working tree). Loads each as an isolated diff package and compares Blueprint
  // structure (parent/interfaces/components/variables/graphs) + CDO defaults.
  bool HandleDiffAssetAction(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  // CommonUI runtime focus/input introspection (PIE-only): focused widget +
  // path, the activatable owning focus + its desired-focus target, the active
  // activatable stack, current input type, and the active root's bound actions.
  bool HandleInspectUiFocus(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);

private:
  // 1. Editor Authoring & Graph Editing
  bool
  HandleBlueprintGraphAction(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool
  HandleNiagaraGraphAction(const FString &RequestId, const FString &Action,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);
  bool HandleMaterialGraphAction(const FString &RequestId,
                                 const FString &Action,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool
  HandleBehaviorTreeAction(const FString &RequestId, const FString &Action,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);
  bool
  HandleWorldPartitionAction(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleListBlueprints(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  // Phase 6: Geometry Script handlers
  bool HandleGeometryAction(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  // Phase 7: Skeleton & Rigging handlers
  bool HandleManageSkeleton(const FString &RequestId,
                            const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool HandleGetSkeletonInfo(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  // Phase 7: Sub-handlers for skeleton operations
  bool HandleListBones(const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       FMcpResponseHandle RequestingSocket);
  bool HandleListSockets(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle RequestingSocket);
  bool HandleCreateSocket(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleConfigureSocket(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleCreateVirtualBone(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleCreatePhysicsAsset(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle RequestingSocket);
  bool HandleListPhysicsBodies(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleAddPhysicsBody(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool HandleConfigurePhysicsBody(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleAddPhysicsConstraint(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleConfigureConstraintLimits(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleRenameBone(const FString &RequestId,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle RequestingSocket);
  bool HandleSetBoneTransform(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandleCreateMorphTarget(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleSetMorphTargetDeltas(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleImportMorphTargets(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle RequestingSocket);
  bool HandleNormalizeWeights(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandlePruneWeights(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleBindClothToSkeletalMesh(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleAssignClothAssetToMesh(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleSetPhysicsAsset(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleRemovePhysicsBody(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleSetMorphTargetValue(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  bool HandleListMorphTargets(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandleDeleteMorphTarget(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleDeleteSocket(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleGetBoneTransform(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandleListVirtualBones(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandleDeleteVirtualBone(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleGetPhysicsAssetInfo(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  // Phase 8: Material Authoring handlers
  bool HandleManageMaterialAuthoringAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 9: Texture handlers
  bool HandleManageTextureAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Internal texture processing helper
  TSharedPtr<FJsonObject> HandleManageTextureAction(const TSharedPtr<FJsonObject>& Params);
  // Phase 10: Animation Authoring handlers
  bool HandleManageAnimationAuthoringAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 11: Audio Authoring handlers
  bool HandleManageAudioAuthoringAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 12: Niagara Authoring handlers
  bool HandleManageNiagaraAuthoringAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 13: GAS (Gameplay Ability System) handlers
  bool HandleManageGASAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 14: Character & Movement handlers
  bool HandleManageCharacterAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 15: Combat & Weapons handlers
  bool HandleManageCombatAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 16: AI System handlers
  bool HandleManageAIAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 17: Inventory & Items System handlers
  bool HandleManageInventoryAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 18: Interaction System handlers
  bool HandleManageInteractionAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 19: Widget Authoring System handlers
  bool HandleManageWidgetAuthoringAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 19 family sub-handlers — each owns one cluster of subactions and
  // returns false if SubAction isn't one of its own (so the dispatcher falls
  // through to the next family). Split out of the former monolithic dispatcher
  // purely for navigability; branch bodies are unchanged.
  bool HandleWidgetAuthoring_Lifecycle(
      const FString &SubAction, const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoring_Containers(
      const FString &SubAction, const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoring_Leaves(
      const FString &SubAction, const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoring_Slot(
      const FString &SubAction, const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoring_Binding(
      const FString &SubAction, const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoring_Animation(
      const FString &SubAction, const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoring_Style(
      const FString &SubAction, const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoring_Tree(
      const FString &SubAction, const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoring_Recipes(
      const FString &SubAction, const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoring_Misc(
      const FString &SubAction, const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Common UI (CommonUI plugin) authoring handlers
  bool HandleCommonUiAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 20: Networking & Multiplayer handlers
  bool HandleManageNetworkingAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 21: Game Framework handlers
  bool HandleManageGameFrameworkAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 22: Sessions & Local Multiplayer handlers
  bool HandleManageSessionsAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 23: Level Structure handlers
  bool HandleManageLevelStructureAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 24: Volumes & Zones handlers
  bool HandleManageVolumesAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 25: Navigation System handlers
  bool HandleManageNavigationAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Phase 26: Spline System handlers
  bool HandleManageSplinesAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // 2. Execution & Build / Test Pipeline
  bool HandleTestAction(const FString &RequestId, const FString &Action,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle RequestingSocket);

  // 3. Observability, Logs, Debugging & History
  bool HandleAssetQueryAction(const FString &RequestId, const FString &Action,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandleInsightsAction(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);

  // 4. Input, UI, Hotkeys & Dialogs
  bool
  HandleUiAutomationAction(const FString &RequestId, const FString &Action,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);

private:
  // Ticker handle for managing the subsystems tick function
  FTSTicker::FDelegateHandle TickHandle;

  // Sequence helpers
  FString ResolveSequencePath(const TSharedPtr<FJsonObject> &Payload);

  // manage_sequence subhandlers — public so the manage_sequence FMcpCall
  // classes (Private/MCP/Calls/) can delegate, until the module split
  // de-members the implementations off the subsystem.
public:
  bool HandleSequenceCreate(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleSequenceSetDisplayRate(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleSequenceSetProperties(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleSequenceOpen(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleSequenceAddCamera(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleSequencePlay(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleSequenceAddActor(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleSequenceAddActors(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleSequenceAddSpawnable(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleSequenceRemoveActors(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleSequenceGetBindings(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleSequenceGetProperties(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleSequenceSetPlaybackSpeed(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleSequencePause(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleSequenceStop(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleSequenceList(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleSequenceDuplicate(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleSequenceRename(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleSequenceDelete(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleSequenceGetMetadata(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleSequenceAddKeyframe(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleSequenceAddTrack(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleSequenceListTracks(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleSequenceListTrackTypes(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleSequenceSetWorkRange(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);

private:
  // Control handlers
  AActor *FindActorByName(const FString &Target, bool bExactMatchOnly = false);

  // Control Actor Subhandlers — public so the control_actor FMcpCall classes
  // (Private/MCP/Calls/) can delegate, until the module split de-members the
  // implementations off the subsystem.
public:
  bool HandleControlActorSpawn(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleControlActorSpawnBlueprint(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleControlActorDelete(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleControlActorApplyForce(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleControlActorSetTransform(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleControlActorGetTransform(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleControlActorSetVisibility(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleControlActorAddComponent(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);

  // (manage_sequence subhandlers, continued — same transitional public block)
  bool HandleSequenceAddSection(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleSequenceSetTickResolution(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleSequenceSetViewRange(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleSequenceSetTrackMuted(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleSequenceSetTrackSolo(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleSequenceSetTrackLocked(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleSequenceRemoveTrack(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);

public:
  bool HandleControlActorSetComponentProperties(
      const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle Socket);
  bool HandleControlActorGetComponents(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleControlActorDuplicate(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleControlActorAttach(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleControlActorDetach(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleControlActorFindByTag(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleControlActorAddTag(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleControlActorRemoveTag(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleControlActorFindByName(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleControlActorDeleteByTag(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleControlActorSetBlueprintVariables(
      const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle Socket);
  bool HandleControlActorCreateSnapshot(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleControlActorList(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool
  HandleControlActorRestoreSnapshot(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleControlActorExport(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleControlActorGetBoundingBox(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleControlActorGetMetadata(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  // Additional actor handlers for test compatibility
  bool HandleControlActorFindByClass(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleControlActorRemoveComponent(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleControlActorGetComponentProperty(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleControlActorSetCollision(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleControlActorCallFunction(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);

  // Control Editor Subhandlers — public so the control_editor FMcpCall
  // classes (Private/MCP/Calls/) can delegate, until the module split
  // de-members the implementations off the subsystem.
  bool HandleControlEditorPlay(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleControlEditorStop(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleControlEditorEject(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleControlEditorPossess(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleControlEditorFocusActor(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleControlEditorSetCamera(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleControlEditorSetViewMode(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleControlEditorSetCameraFov(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleControlEditorSetGameSpeed(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleControlEditorOpenAsset(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleControlEditorScreenshot(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleControlEditorPause(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleControlEditorResume(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleControlEditorConsoleCommand(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleControlEditorStepFrame(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleControlEditorStartRecording(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleControlEditorStopRecording(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleControlEditorCreateBookmark(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleControlEditorJumpToBookmark(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleControlEditorSetPreferences(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleControlEditorSetViewportRealtime(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleControlEditorSimulateInput(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  // CommonUI gamepad/keyboard navigation drive (PIE-only). Faithfully routes a
  // nav key through Slate + the CommonUI input preprocessor/action router, then
  // returns the post-nav focus snapshot so callers can diff where focus landed.
  bool HandleControlEditorSimulateNav(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  // Additional editor handlers for test compatibility
  bool HandleControlEditorCloseAsset(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleControlEditorSaveAll(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleControlEditorUndo(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleControlEditorRedo(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleControlEditorSetEditorMode(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleControlEditorShowStats(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleControlEditorHideStats(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleControlEditorSetGameView(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleControlEditorSetImmersiveMode(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandleControlEditorSetFixedDeltaTime(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleControlEditorOpenLevel(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);

  // manage_level Subhandlers — public so the manage_level FMcpCall classes
  // (Private/MCP/Calls/) can delegate, until the module split de-members the
  // implementations off the subsystem.
  bool HandleLevelLoad(const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       FMcpResponseHandle Socket);
  bool HandleLevelSave(const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       FMcpResponseHandle Socket);
  bool HandleLevelSaveAs(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleLevelCreate(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleLevelStream(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleLevelUnload(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleLevelCreateLight(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleLevelBuildLighting(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleLevelSetMetadata(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleLevelValidate(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleLevelList(const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       FMcpResponseHandle Socket);
  bool HandleLevelGetCurrent(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleLevelExport(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleLevelImport(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleLevelAddSublevel(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleLevelDelete(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleLevelRename(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleLevelDuplicate(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleLevelGetSummary(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);

  // system_control subhandlers — public so the system_control FMcpCall
  // classes (Private/MCP/Calls/) can delegate, until the module split
  // de-members the implementations off the subsystem. Implementations span
  // the Performance / SystemControl / Ui / Log / Debug / Render handler TUs;
  // the log family and HandleSysExecuteCommand / HandleSysSetCvar /
  // HandleDebugSpawnCategory are NOT editor-gated.
  bool HandlePerfGenerateMemoryReport(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandlePerfGetStats(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandlePerfStartProfiling(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandlePerfStopProfiling(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandlePerfShowFps(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandlePerfShowStats(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandlePerfSetScalability(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandlePerfSetResolutionScale(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandlePerfSetVsync(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandlePerfSetFrameRateLimit(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandlePerfEnableGpuTiming(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandlePerfConfigureNanite(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandlePerfConfigureLod(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandlePerfConfigureTextureStreaming(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandlePerfApplyBaselineSettings(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandlePerfOptimizeDrawCalls(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandlePerfConfigureOcclusionCulling(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandlePerfOptimizeShaders(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandlePerfConfigureWorldPartition(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandlePerfMergeActors(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandlePerfRunBenchmark(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleSysGenerateTestStub(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleSysLiveCodingCompile(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleSysRunUbt(const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       FMcpResponseHandle Socket);
  bool HandleSysGetBuildStatus(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleSysListTests(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleSysRunTests(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleSysGetTestResults(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleSysExecutePython(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleSysStartSession(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleSysExecuteCommand(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleSysSetCvar(const FString &RequestId,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle Socket);
  bool HandleUiCreateWidget(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleUiAddWidgetChild(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleUiScreenshot(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleUiGetProjectSettings(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleUiSetProjectSetting(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleLogQuery(const FString &RequestId,
                      const TSharedPtr<FJsonObject> &Payload,
                      FMcpResponseHandle Socket);
  bool HandleLogClear(const FString &RequestId,
                      const TSharedPtr<FJsonObject> &Payload,
                      FMcpResponseHandle Socket);
  bool HandleLogSubscribe(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleLogUnsubscribe(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleDebugSpawnCategory(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleRenderLumenUpdateScene(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);

  // inspect subhandlers — public so the inspect FMcpCall classes
  // (Private/MCP/Calls/) can delegate, until the module split de-members the
  // implementations off the subsystem. All bodies live in
  // EnvironmentHandlers.cpp; HandleInspectRuntimeReport serves runtime_report
  // and pie_report, HandleInspectObjectGeneric serves inspect_object and the
  // seven get_*_details actions.
  bool HandleInspectFindObjects(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleInspectGetProjectSettings(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleInspectGetEditorSettings(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleInspectGetWorldSettings(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleInspectGetViewportInfo(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleInspectGetSelectedActors(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleInspectGetSceneStats(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleInspectGetPerformanceStats(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleInspectGetMemoryStats(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleInspectRuntimeReport(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleInspectListObjects(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleInspectFindByClass(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleInspectFindByTag(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleInspectClassInfo(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleInspectObjectGeneric(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);

private:
  // stream/unload shared implementation; unload passes bForceUnload=true,
  // which overrides shouldBeLoaded/shouldBeVisible to false.
  bool HandleLevelStreamInternal(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket, bool bForceUnload);

  // subscribe/unsubscribe shared implementation.
  bool HandleLogSetSubscribed(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket, bool bSubscribe);

  // Asset handlers
  bool HandleImportAsset(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleCreateMaterial(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleCreateMaterialInstance(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleCreateDataTable(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  // DataTable row CRUD + CSV/JSON import. SubAction is the (lowercased) routed action
  // string (add_row / edit_row / remove_row / get_rows / import_data_table, plus aliases).
  bool HandleDataTableRowOp(const FString &RequestId, const FString &SubAction,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleCreateNiagaraSystemAsset(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleDuplicateAsset(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleRenameAsset(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleMoveAsset(const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       FMcpResponseHandle Socket);
  bool HandleDeleteAssets(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleListAssets(const FString &RequestId,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle Socket);
  bool HandleGetAsset(const FString &RequestId,
                      const TSharedPtr<FJsonObject> &Payload,
                      FMcpResponseHandle Socket);
  bool HandleCreateFolder(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleGetDependencies(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleGetReferencers(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleGetAssetProperties(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleSetAssetProperty(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleGetAssetGraph(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleCreateThumbnail(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleSetTags(const FString &RequestId,
                     const TSharedPtr<FJsonObject> &Payload,
                     FMcpResponseHandle Socket);
  bool HandleSetMetadata(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleGetMetadata(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle Socket);
  bool HandleGenerateReport(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleValidateAsset(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleAddMaterialParameter(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleListMaterialInstances(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleResetInstanceParameters(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleDoesAssetExist(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleGetMaterialStats(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleRebuildMaterial(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);

  // Lightweight snapshot cache for automation requests (e.g., create_snapshot)
  TMap<FString, FTransform> CachedActorSnapshots;

  /** Guards against reentrant automation request processing */
  bool bProcessingAutomationRequest = false;

  void
  ProcessAutomationRequest(const FString &RequestId, const FString &Action,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket,
                           ERequestOrigin Origin = ERequestOrigin::Unspecified);

  friend class FMcpNativeTransport;
};
