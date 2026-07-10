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

  // Array manipulation operations
  // Map manipulation operations
  // Set manipulation operations
  // control_actor and control_editor dispatch chains were classed away —
  // see MCP/Calls/.
  // manage_level is classed — see MCP/Calls/McpCalls_ManageLevel.cpp.
  // system_control is classed — see MCP/Calls/McpCalls_SystemControl.cpp;
  // its member handlers (HandlePerf*/HandleSys*/HandleUi*/HandleLog*/
  // HandleDebugSpawnCategory/HandleRenderLumenUpdateScene) live in the
  // transitional public block below.
  // animation_physics is classed — see
  // MCP/Calls/McpCalls_AnimationPhysics.cpp. Its per-action members are
  // public so the FMcpCall classes (Private/MCP/Calls/) can delegate, until
  // the module split de-members the implementations off the subsystem:
  // HandleAnimPhys* (AnimationHandlers.cpp), HandleAnimAuthoring*
  // (AnimationAuthoringHandlers.cpp), HandleSkeleton* (SkeletonHandlers.cpp).
public:
  bool HandleAnimPhysConfigureVehicle(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysSetupPhysicsSimulation(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle RequestingSocket);

  // build_environment is classed — see
  // MCP/Calls/McpCalls_BuildEnvironment.cpp.
  // Its subhandlers are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members the implementations off
  // the subsystem. Implementations span three TUs: HandleEnvironment* @
  // EnvironmentHandlers.cpp, HandleLighting* @ LightingHandlers.cpp,
  // HandleSpline* @ SplineHandlers.cpp.
  bool HandleEnvironmentAddFoliageInstances(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentGetFoliageInstances(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentRemoveFoliage(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentPaintFoliage(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentCreateProceduralFoliage(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentAddFoliage(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentCreateLandscape(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentPaintLandscape(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentSculpt(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentModifyHeightmap(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentSetLandscapeMaterial(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentCreateLandscapeGrassType(const FString &RequestId,
                                                 const TSharedPtr<FJsonObject> &Payload,
                                                 FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentBakeLightmap(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);

private:
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
  bool HandleBakeLightmap(const FString &RequestId, const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleCreateProceduralFoliage(const FString &RequestId, const FString &Action,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleAddFoliageType(const FString &RequestId, const FString &Action,
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
  // Foliage handlers
  bool
  HandleAddFoliageInstances(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  // Additional consolidated tool handlers
  bool
  HandleConsoleCommandAction(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);

  bool
  HandleWorldPartitionAction(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);

  // 3. Observability, Logs, Debugging & History
  bool HandleInsightsAction(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);

  // Ticker handle for managing the subsystems tick function
  FTSTicker::FDelegateHandle TickHandle;

public:
  // Sequence helper — public: the de-membered McpHandlers::Sequence free
  // functions resolve the target sequence through it.
  FString ResolveSequencePath(const TSharedPtr<FJsonObject> &Payload);

  // World-selecting forward to McpHandlerUtils::FindActorByName — public: the
  // de-membered handler free functions resolve actors through it.
  AActor *FindActorByName(const FString &Target, bool bExactMatchOnly = false);

  // system_control subhandlers — public so the system_control FMcpCall
  // classes (Private/MCP/Calls/) can delegate, until the module split
  // de-members the implementations off the subsystem. Implementations span
  // the Performance / SystemControl / Ui / Log / Debug / Render handler TUs;
  // the log family and HandleSysExecuteCommand / HandleSysSetCvar /
  // HandleDebugSpawnCategory are NOT editor-gated.
  bool HandleSysStartSession(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleSysExecuteCommand(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleSysSetCvar(const FString &RequestId,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle Socket);
  bool HandleLogSubscribe(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleLogUnsubscribe(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);

public:
  // Lightweight snapshot cache for the de-membered control_actor
  // create_snapshot/restore_snapshot handlers.
  TMap<FString, FTransform> CachedActorSnapshots;

private:

  // subscribe/unsubscribe shared implementation.
  bool HandleLogSetSubscribed(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket, bool bSubscribe);

  /** Guards against reentrant automation request processing */
  bool bProcessingAutomationRequest = false;

  void
  ProcessAutomationRequest(const FString &RequestId, const FString &Action,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket,
                           ERequestOrigin Origin = ERequestOrigin::Unspecified);

  friend class FMcpNativeTransport;
};
