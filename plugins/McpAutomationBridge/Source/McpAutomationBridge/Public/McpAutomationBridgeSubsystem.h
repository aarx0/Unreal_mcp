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
  // Map manipulation operations
  // Set manipulation operations
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
  // animation_physics is classed — see
  // MCP/Calls/McpCalls_AnimationPhysics.cpp. Its per-action members are
  // public so the FMcpCall classes (Private/MCP/Calls/) can delegate, until
  // the module split de-members the implementations off the subsystem:
  // HandleAnimPhys* (AnimationHandlers.cpp), HandleAnimAuthoring*
  // (AnimationAuthoringHandlers.cpp), HandleSkeleton* (SkeletonHandlers.cpp).
public:
  bool HandleAnimPhysCleanup(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysCreateBlendSpace(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysCreateBlendTree(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysCreateProceduralAnim(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysCreateStateMachine(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysSetupIk(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysConfigureVehicle(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysSetupPhysicsSimulation(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysCreateAnimationAsset(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysSetupRetargeting(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysPlayMontage(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysCreatePoseLibrary(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleAnimPhysActivateRagdoll(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringCreateAnimationSequence(const FString &RequestId,
                                                  const TSharedPtr<FJsonObject> &Payload,
                                                  FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetSequenceLength(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddBoneTrack(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetBoneKey(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetCurveKey(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddNotify(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddNotifyState(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddSyncMarker(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetRootMotionSettings(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetAdditiveSettings(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringCreateMontage(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddMontageSection(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddMontageSlot(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetSectionTiming(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddMontageNotify(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetBlendIn(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetBlendOut(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringLinkSections(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringCreateBlendSpace1D(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringCreateBlendSpace2D(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddBlendSample(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringForceRebuildBlendSpace(const FString &RequestId,
                                                 const TSharedPtr<FJsonObject> &Payload,
                                                 FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetAxisSettings(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetInterpolationSettings(const FString &RequestId,
                                                   const TSharedPtr<FJsonObject> &Payload,
                                                   FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringCreateAimOffset(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddAimOffsetSample(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringCreateAnimBlueprint(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddStateMachine(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddState(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddTransition(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetTransitionRules(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddBlendNode(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddCachedPose(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddSlotNode(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringAddLayeredBlendPerBone(const FString &RequestId,
                                                 const TSharedPtr<FJsonObject> &Payload,
                                                 FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetAnimGraphNodeValue(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringCreateControlRig(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringCreateIkRig(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringCreateIkRetargeter(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringSetRetargetChainMapping(const FString &RequestId,
                                                  const TSharedPtr<FJsonObject> &Payload,
                                                  FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringGetAnimationInfo(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle RequestingSocket);
  bool HandleAnimAuthoringBindAnimNotify(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonGetSkeletonInfo(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonListBones(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonListSockets(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonCreateSocket(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonConfigureSocket(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonCreateVirtualBone(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonCreatePhysicsAsset(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonListPhysicsBodies(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonAddPhysicsBody(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonConfigurePhysicsBody(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonAddPhysicsConstraint(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonConfigureConstraintLimits(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonRenameBone(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonSetBoneTransform(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonCreateMorphTarget(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonSetMorphTargetDeltas(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonImportMorphTargets(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonNormalizeWeights(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonPruneWeights(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonBindClothToSkeletalMesh(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonAssignClothAssetToMesh(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonCreateSkeleton(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonAddBone(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonRemoveBone(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonSetBoneParent(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonSetVertexWeights(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonAutoSkinWeights(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonCopyWeights(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  bool HandleSkeletonMirrorWeights(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle RequestingSocket);

private:
  // manage_effect is classed — see MCP/Calls/McpCalls_ManageEffect.cpp; its
  // member handlers (HandleEffect*) live in the transitional public block
  // below.
  // manage_blueprint is classed — see MCP/Calls/McpCalls_ManageBlueprint.cpp.
  // Core + Graph subActions are per-action members (below), called directly by
  // their classed actions, as are the CommonUi actions and the widget Lifecycle/
  // Containers/Leaves actions; the remaining widget families still delegate to
  // HandleManageWidgetAuthoringAction. The HandleBlueprintModifyScs member is also
  // called externally by EditorFunctionHandlers.cpp (BLUEPRINT_ADD_COMPONENT).
  // manage_sequence is classed — see MCP/Calls/McpCalls_ManageSequence.cpp.
  bool CreateNiagaraEffect(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket,
                           const FString &EffectName,
                           const FString &DefaultSystemPath);

  // manage_effect subhandlers — public so the manage_effect FMcpCall classes
  // (Private/MCP/Calls/) can delegate, until the module split de-members the
  // implementations off the subsystem.
public:
  bool HandleEffectListDebugShapes(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleEffectClearDebugShapes(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleEffectDebugShape(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleEffectParticle(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleEffectSetNiagaraParameter(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleEffectActivateNiagara(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleEffectDeactivateNiagara(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleEffectAdvanceSimulation(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleEffectCreateDynamicLight(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleEffectCleanup(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleEffectSpawnNiagara(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleEffectCreateVolumetricFog(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleEffectCreateParticleTrail(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleEffectCreateEnvironmentEffect(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandleEffectCreateImpactEffect(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleEffectCreateNiagaraRibbon(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);

private:
  // manage_audio is classed — see MCP/Calls/McpCalls_ManageAudio.cpp. Its
  // per-action members are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members the implementations off
  // the subsystem: HandleAudio* (AudioHandlers.cpp), HandleAudioAuthoring*
  // (AudioAuthoringHandlers.cpp).
public:
  bool HandleAudioCreateSoundCue(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  bool HandleAudioCreateSoundClass(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle RequestingSocket);
  bool HandleAudioCreateSoundMix(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  bool HandleAudioPlaySoundAtLocation(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAudioPlaySound2D(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandleAudioPlaySoundAttached(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleAudioCreateAmbientSound(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleAudioSpawnSoundAtLocation(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleAudioPushSoundMix(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleAudioPopSoundMix(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandleAudioSetSoundMixClassOverride(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle RequestingSocket);
  bool HandleAudioClearSoundMixClassOverride(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleAudioSetBaseSoundMix(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleAudioFadeSoundOut(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleAudioFadeSoundIn(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  // fade_sound_out/fade_sound_in shared implementation.
  bool HandleAudioFadeSoundInternal(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket,
                                    bool bFadeIn);
  bool HandleAudioPrimeSound(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleAudioCreateAudioComponent(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleAudioEnableAudioAnalysis(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAudioSetDopplerEffect(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle RequestingSocket);
  bool HandleAudioSetAudioOcclusion(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleAudioSetSoundAttenuation(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAudioFadeSound(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool HandleAudioCreateReverbZone(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringAddCueNode(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringConnectCueNodes(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringSetCueAttenuation(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringSetCueConcurrency(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringCreateMetasound(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringAddMetasoundNode(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringConnectMetasoundNodes(const FString &RequestId,
                                                 const TSharedPtr<FJsonObject> &Payload,
                                                 FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringAddMetasoundInput(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringAddMetasoundOutput(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringSetMetasoundDefault(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringSetClassProperties(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringSetClassParent(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringAddMixModifier(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringConfigureMixEq(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringCreateAttenuationSettings(const FString &RequestId,
                                                     const TSharedPtr<FJsonObject> &Payload,
                                                     FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringConfigureDistanceAttenuation(const FString &RequestId,
                                                        const TSharedPtr<FJsonObject> &Payload,
                                                        FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringConfigureSpatialization(const FString &RequestId,
                                                   const TSharedPtr<FJsonObject> &Payload,
                                                   FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringConfigureOcclusion(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringConfigureReverbSend(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringCreateDialogueVoice(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringCreateDialogueWave(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringSetDialogueContext(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringCreateReverbEffect(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringCreateSourceEffectChain(const FString &RequestId,
                                                   const TSharedPtr<FJsonObject> &Payload,
                                                   FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringAddSourceEffect(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringCreateSubmixEffect(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle RequestingSocket);
  bool HandleAudioAuthoringGetAudioInfo(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);

private:
  // build_environment is classed — see
  // MCP/Calls/McpCalls_BuildEnvironment.cpp.
  // Its subhandlers are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members the implementations off
  // the subsystem. Implementations span three TUs: HandleEnvironment* @
  // EnvironmentHandlers.cpp, HandleLighting* @ LightingHandlers.cpp,
  // HandleSpline* @ SplineHandlers.cpp.
public:
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
  bool HandleEnvironmentCreateProceduralTerrain(const FString &RequestId,
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
  bool HandleEnvironmentGenerateLODs(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentBakeLightmap(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentExportSnapshot(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentImportSnapshot(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentDelete(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentCreateSkySphere(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentSetTimeOfDay(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleEnvironmentCreateFogVolume(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleLightingListLightTypes(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleLightingCreateLight(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  bool HandleLightingCreateSkyLight(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleLightingBuildLighting(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle RequestingSocket);
  bool HandleLightingEnsureSingleSkyLight(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleLightingCreateLightmassVolume(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle RequestingSocket);
  bool HandleLightingSetupVolumetricFog(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleLightingSetupGlobalIllumination(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle RequestingSocket);
  bool HandleLightingConfigureShadows(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleLightingSetExposure(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  bool HandleLightingSetAmbientOcclusion(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle RequestingSocket);
  bool HandleLightingCreateLightingEnabledLevel(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle RequestingSocket);
  bool HandleSplineCreateSplineActor(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleSplineAddSplinePoint(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleSplineRemoveSplinePoint(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleSplineSetSplinePointPosition(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleSplineSetSplinePointTangents(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleSplineSetSplinePointRotation(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleSplineSetSplinePointScale(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleSplineSetSplineType(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleSplineCreateSplineMeshComponent(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleSplineSetSplineMeshAsset(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleSplineConfigureSplineMeshAxis(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandleSplineSetSplineMeshMaterial(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleSplineScatterMeshesAlongSpline(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleSplineCreateRoadSpline(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleSplineCreateRiverSpline(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleSplineCreateFenceSpline(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleSplineCreateWallSpline(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleSplineCreateCableSpline(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleSplineCreatePipeSpline(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleSplineGetSplinesInfo(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);

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
  bool HandleCreateProceduralTerrain(const FString &RequestId, const FString &Action,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleCreateProceduralFoliage(const FString &RequestId, const FString &Action,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleAddFoliageType(const FString &RequestId, const FString &Action,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
public:
  // Asset workflow / query handlers — manage_asset is classed
  // (MCP/Calls/McpCalls_ManageAsset.cpp); these members are public so the
  // FMcpCall classes can delegate. Action carries the routed action literal.
  bool HandleGenerateLODs(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleSourceControlCheckout(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleSourceControlSubmit(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleGetSourceControlState(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleAnalyzeGraph(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleFixupRedirectors(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBulkRenameAssets(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBulkDeleteAssets(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleGenerateThumbnail(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNaniteRebuildMesh(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleFindByTag(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleSearchAssets(const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
private:
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

public:
  // 1. Editor Authoring & Graph Editing
  // manage_blueprint is classed (MCP/Calls/McpCalls_ManageBlueprint.cpp); each
  // graph subAction is one member below, called directly by its classed action.
  // HandleBlueprintGraphCreateNode is also reused by HandleBlueprintAddNode's
  // cast/getter/CallFunction fast-path (BlueprintHandlers.cpp).
  bool HandleBlueprintGraphCreateNode(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphConnectPins(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphBreakPinLinks(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphDeleteNode(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphCreateRerouteNode(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphSetNodeProperty(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphGetNodeDetails(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphGetGraphDetails(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphArrangeGraph(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphGetPinDetails(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphListNodeTypes(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphSetPinDefaultValue(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphListAnimbpGraphs(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGraphGetTransitionRuleGraph(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);

  // manage_blueprint Core + SCS per-action members (BlueprintHandlers.cpp),
  // called directly by their classed actions. Hoisted verbatim from the retired
  // HandleBlueprintAction dispatcher and the retired HandleSCSAction.
  bool HandleBlueprintAddScsComponent(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintRemoveScsComponent(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintReparentScsComponent(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintSetScsTransform(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintSetScsProperty(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintAddConstructionScript(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintCompile(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintCreateAction(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintEnsureExists(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintProbeHandle(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintAddComponent(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGetScs(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGet(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintSetDefault(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintGetDefault(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintListFunctions(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintModifyScs(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintAddVariable(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintRemoveVariable(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintRenameVariable(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintAddFunction(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintRemoveFunction(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintAddEvent(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintRemoveEvent(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintSetVariableMetadata(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintSetMetadata(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleBlueprintAddNode(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);

  // Called directly by the classed manage_effect graph actions (MCP/Calls/);
  // Action carries its manage_niagara_graph gate literal.
  bool
  HandleNiagaraGraphAction(const FString &RequestId, const FString &Action,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);

private:
  bool
  HandleWorldPartitionAction(const FString &RequestId, const FString &Action,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  // manage_geometry is classed — see MCP/Calls/McpCalls_ManageGeometry.cpp.
  // Its subhandlers are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members the implementations off
  // the subsystem.
public:
  bool HandleGeometryCreateBox(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleGeometryCreateSphere(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleGeometryCreateCylinder(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleGeometryCreateCone(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleGeometryCreateCapsule(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleGeometryCreateTorus(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGeometryCreatePlane(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGeometryCreateDisc(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleGeometryCreateStairs(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleGeometryCreateSpiralStairs(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleGeometryCreateRing(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleGeometryCreateArch(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleGeometryCreatePipe(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleGeometryCreateRamp(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleGeometryRevolve(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleGeometryBooleanUnion(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleGeometryBooleanSubtract(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleGeometryBooleanIntersection(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleGeometryBooleanTrim(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGeometrySelfUnion(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleGeometryGetMeshInfo(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGeometryRecalculateNormals(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleGeometryFlipNormals(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGeometrySimplifyMesh(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleGeometrySubdivide(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleGeometryAutoUV(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleGeometryConvertToStaticMesh(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleGeometryExtrude(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleGeometryInset(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleGeometryOutset(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleGeometryBevel(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleGeometryOffsetFaces(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGeometryShell(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleGeometryChamfer(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleGeometryBend(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleGeometryTwist(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleGeometryTaper(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleGeometryNoiseDeform(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGeometrySmooth(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleGeometryRelax(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleGeometryStretch(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleGeometrySpherify(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleGeometryCylindrify(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleGeometryLatticeDeform(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleGeometryDisplaceByTexture(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleGeometryWeldVertices(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleGeometryFillHoles(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleGeometryRemoveDegenerates(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleGeometryRemeshUniform(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleGeometryMergeVertices(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleGeometryGenerateCollision(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleGeometryMirror(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleGeometryArrayLinear(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGeometryArrayRadial(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGeometryTriangulate(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGeometryPoke(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleGeometryProjectUV(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleGeometryTransformUVs(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleGeometryRecomputeTangents(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleGeometryBridge(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleGeometryLoft(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle Socket);
  bool HandleGeometrySweep(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleGeometryLoopCut(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleGeometryDuplicateAlongSpline(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleGeometryUnwrapUV(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleGeometryPackUVIslands(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleGeometryConvertToNanite(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleGeometryExtrudeAlongSpline(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleGeometryEdgeSplit(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleGeometryQuadrangulate(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleGeometryRemeshVoxel(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGeometryGenerateComplexCollision(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleGeometrySimplifyCollision(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleGeometryGenerateLODs(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleGeometrySetLODSettings(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleGeometrySetLODScreenSizes(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);

private:
  // Phase 7: Skeleton & Rigging handlers
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
  // manage_asset is classed — see MCP/Calls/McpCalls_ManageAsset.cpp. Its
  // per-action members are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members them off the subsystem:
  // MaterialAuthoring actions use HandleMaterial* (MaterialAuthoringHandlers.cpp),
  // Texture actions use HandleTexture* (TextureHandlers.cpp); the Core actions
  // keep their existing AssetWorkflow/AssetQuery members (made public elsewhere).
public:
  bool HandleSaveAsset(const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       FMcpResponseHandle RequestingSocket);
  bool HandleMaterialCreateMaterial(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialSetBlendMode(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialSetShadingModel(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialSetMaterialDomain(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddTextureSample(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddTextureCoordinate(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddScalarParameter(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddVectorParameter(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddStaticSwitchParameter(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddMathNode(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddSimpleExpression(const FString &RequestId, const FString &SubAction,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddConditional(const FString &RequestId, const FString &SubAction,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddCustomExpression(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialConnectNodes(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialDisconnectNodes(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialCreateMaterialFunction(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddFunctionIO(const FString &RequestId, const FString &SubAction,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialUseMaterialFunction(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialCreateMaterialInstance(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialSetScalarParameterValue(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialSetVectorParameterValue(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialSetTextureParameterValue(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialCreateSpecialMaterial(const FString &RequestId, const FString &SubAction,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddLandscapeLayer(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialConfigureLayerBlend(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialCompileMaterial(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialGetMaterialInfo(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialGetMaterialFunctionInfo(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialFindNode(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialGetNodeConnections(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialGetNodeProperties(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialSetNodeValue(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialSetStaticSwitchParameterValue(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialDeleteNode(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialUpdateCustomExpression(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialGetNodeChain(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialGetConnectedSubgraph(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialAddMaterialNode(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialRemoveMaterialNode(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialSetMaterialParameter(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialGetMaterialNodeDetails(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialSetTwoSided(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleMaterialArrangeGraph(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
  bool HandleTextureCreateNoiseTexture(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureCreateGradientTexture(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureCreatePatternTexture(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureCreateNormalFromHeight(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureSetCompressionSettings(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureSetTextureGroup(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureSetLodBias(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureConfigureVirtualTexture(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureSetStreamingPriority(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureGetTextureInfo(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureResizeTexture(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureInvert(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureDesaturate(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureAdjustLevels(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureBlur(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureSharpen(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureChannelPack(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureCombineTextures(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureAdjustCurves(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureChannelExtract(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureCreateRenderTarget(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleTextureCreateAoFromMesh(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
private:
  // Phase 10: Animation Authoring handlers — animation_physics is classed;
  // its HandleAnimAuthoring* members live in the transitional public block
  // above (AnimationAuthoringHandlers.cpp).
  // Phase 11: Audio Authoring handlers — manage_audio is classed; its
  // HandleAudioAuthoring* members live in the transitional public block
  // above (AudioAuthoringHandlers.cpp).
public:
  // Phase 12: Niagara Authoring handlers — one member per classed manage_effect
  // Niagara-authoring action (MCP/Calls/McpCalls_ManageEffect.cpp), implemented
  // in NiagaraAuthoringHandlers.cpp. HandleNiagaraAddDataInterface serves the
  // three spline/audio-spectrum/collision-query adders via its SubAction arg.
  bool HandleNiagaraCreateNiagaraSystem(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraCreateNiagaraEmitter(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddEmitterToSystem(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraSetEmitterProperties(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddSpawnRateModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddSpawnBurstModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddSpawnPerUnitModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddInitializeParticleModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddParticleStateModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddForceModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddVelocityModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddAccelerationModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddSizeModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddColorModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddSpriteRendererModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddMeshRendererModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddRibbonRendererModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddLightRendererModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddCollisionModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddKillParticlesModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddCameraOffsetModule(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddUserParameter(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraSetParameterValue(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraBindParameterToSource(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddSkeletalMeshDataInterface(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddStaticMeshDataInterface(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddDataInterface(const FString &RequestId, const FString &SubAction,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddEventGenerator(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddEventReceiver(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraEnableGpuSimulation(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraAddSimulationStage(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraGetNiagaraInfo(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleNiagaraValidateNiagaraSystem(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);

private:
  // manage_gas is classed — see MCP/Calls/McpCalls_ManageGas.cpp.
  // Its subhandlers are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members the implementations off
  // the subsystem.
public:
  bool HandleGasAddAbilitySystemComponent(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleGasConfigureAsc(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleGasCreateAttributeSet(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleGasAddAttribute(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleGasSetAttributeBaseValue(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleGasSetAttributeClamping(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleGasCreateGameplayAbility(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleGasSetAbilityTags(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleGasSetAbilityCosts(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleGasSetAbilityCooldown(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleGasSetAbilityTargeting(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleGasAddAbilityTask(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleGasSetActivationPolicy(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleGasSetInstancingPolicy(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleGasCreateGameplayEffect(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleGasSetEffectDuration(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleGasAddEffectModifier(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleGasSetModifierMagnitude(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleGasAddEffectExecutionCalculation(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleGasAddEffectCue(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleGasSetEffectStacking(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleGasSetEffectTags(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleGasCreateGameplayCueNotify(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleGasSetCueEffects(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleGasAddTagToAsset(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleGasGetAttribute(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleGasGetInfo(const FString &RequestId,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle Socket);
  bool HandleGasCreateAbilitySet(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle Socket);
  bool HandleGasAddAbility(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleGasCreateExecutionCalculation(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);

private:
  // manage_character is classed — see MCP/Calls/McpCalls_ManageCharacter.cpp.
  // Its subhandlers are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members the implementations off
  // the subsystem.
public:
  bool HandleCharacterCreateBlueprint(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleCharacterConfigureCapsuleComponent(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleCharacterConfigureMeshComponent(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleCharacterConfigureCameraComponent(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle Socket);
  bool HandleCharacterConfigureMovementSpeeds(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleCharacterConfigureJump(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleCharacterConfigureRotation(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleCharacterAddCustomMovementMode(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleCharacterConfigureNavMovement(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandleCharacterMapSurfaceToSound(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleCharacterGetInfo(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);
  bool HandleCharacterSetupMovement(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleCharacterSetWalkSpeed(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleCharacterSetJumpHeight(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleCharacterSetGravityScale(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleCharacterSetGroundFriction(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleCharacterSetBrakingDeceleration(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleCharacterConfigureCrouch(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);

private:
  // manage_combat is classed — see MCP/Calls/McpCalls_ManageCombat.cpp.
  // Its subhandlers are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members the implementations off
  // the subsystem.
public:
  bool HandleCombatCreateWeaponBlueprint(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleCombatConfigureWeaponMesh(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleCombatSetWeaponStats(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleCombatCreateProjectileBlueprint(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleCombatConfigureProjectileMovement(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle Socket);
  bool HandleCombatConfigureProjectileCollision(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleCombatConfigureProjectileHoming(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleCombatCreateDamageType(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleCombatSetupHitboxComponent(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleCombatSetupAttachmentSystem(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleCombatCreateMeleeTrace(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleCombatCreateHitPause(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleCombatGetInfo(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle Socket);
  bool HandleCombatConfigureHitDetection(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleCombatGetStats(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle Socket);
  bool HandleCombatCreateDamageEffect(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleCombatApplyDamage(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleCombatHeal(const FString &RequestId,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle Socket);
  bool HandleCombatCreateShield(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleCombatModifyArmor(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);

  // Phase 16: AI System handlers — manage_ai is classed
  // (MCP/Calls/McpCalls_ManageAi.cpp). Its per-action members are public so
  // the FMcpCall classes (Private/MCP/Calls/) can delegate, until the module
  // split de-members the implementations off the subsystem: HandleAi*
  // (AIHandlers.cpp), HandleBehaviorTree* (BehaviorTreeHandlers.cpp),
  // HandleNavigation* (NavigationHandlers.cpp).
public:
  bool HandleAiCreateAiController(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleAiAssignBehaviorTree(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleAiAssignBlackboard(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle RequestingSocket);
  bool HandleAiCreateBlackboardAsset(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleAiAddBlackboardKey(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle RequestingSocket);
  bool HandleAiSetKeyInstanceSynced(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleAiCreateBehaviorTree(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleAiAddCompositeNode(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle RequestingSocket);
  bool HandleAiAddTaskNode(const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           FMcpResponseHandle RequestingSocket);
  bool HandleAiAddDecorator(const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            FMcpResponseHandle RequestingSocket);
  bool HandleAiAddService(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleAiConfigureBtNode(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleAiCreateEqsQuery(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandleAiAddEqsGenerator(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleAiAddEqsContext(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle RequestingSocket);
  bool HandleAiAddEqsTest(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleAiConfigureTestScoring(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleAiAddAiPerceptionComponent(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle RequestingSocket);
  bool HandleAiConfigureSightConfig(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleAiConfigureHearingConfig(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAiConfigureDamageSenseConfig(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleAiSetPerceptionTeam(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  bool HandleAiCreateStateTree(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleAiAddStateTreeState(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  bool HandleAiAddStateTreeTransition(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAiConfigureStateTreeTask(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAiCreateSmartObjectDefinition(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle RequestingSocket);
  bool HandleAiAddSmartObjectSlot(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleAiConfigureSlotBehavior(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle RequestingSocket);
  bool HandleAiAddSmartObjectComponent(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle RequestingSocket);
  bool HandleAiCreateMassEntityConfig(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleAiConfigureMassEntity(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle RequestingSocket);
  bool HandleAiAddMassSpawner(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle RequestingSocket);
  bool HandleAiGetAiInfo(const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         FMcpResponseHandle RequestingSocket);
  bool HandleAiSetupPerception(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleAiSetFocus(const FString &RequestId,
                        const TSharedPtr<FJsonObject> &Payload,
                        FMcpResponseHandle RequestingSocket);
  bool HandleAiClearFocus(const FString &RequestId,
                          const TSharedPtr<FJsonObject> &Payload,
                          FMcpResponseHandle RequestingSocket);
  bool HandleAiSetBlackboardValue(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleAiGetBlackboardValue(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle RequestingSocket);
  bool HandleAiRunBehaviorTree(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle RequestingSocket);
  bool HandleAiStopBehaviorTree(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle RequestingSocket);
  bool HandleBehaviorTreeCreate(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle RequestingSocket);
  bool HandleBehaviorTreeAddNode(const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 FMcpResponseHandle RequestingSocket);
  bool HandleBehaviorTreeConnectNodes(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle RequestingSocket);
  bool HandleBehaviorTreeRemoveNode(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleBehaviorTreeBreakConnections(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle RequestingSocket);
  bool HandleBehaviorTreeSetNodeProperties(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle RequestingSocket);
  bool HandleBehaviorTreeAddSubnode(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle RequestingSocket);
  bool HandleNavigationConfigureNavMeshSettings(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleNavigationSetNavAgentProperties(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleNavigationRebuildNavigation(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleNavigationCreateNavModifierComponent(const FString &RequestId,
                                                  const TSharedPtr<FJsonObject> &Payload,
                                                  FMcpResponseHandle Socket);
  bool HandleNavigationSetNavAreaClass(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleNavigationConfigureNavAreaCost(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleNavigationCreateNavLinkProxy(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleNavigationConfigureNavLink(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleNavigationSetNavLinkType(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleNavigationCreateSmartLink(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleNavigationConfigureSmartLinkBehavior(const FString &RequestId,
                                                  const TSharedPtr<FJsonObject> &Payload,
                                                  FMcpResponseHandle Socket);
  bool HandleNavigationGetNavigationInfo(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  // manage_inventory is classed — see MCP/Calls/McpCalls_ManageInventory.cpp.
  // Its subhandlers are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members the implementations off
  // the subsystem.
public:
  bool HandleInventoryCreateItemDataAsset(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleInventorySetItemProperties(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleInventoryCreateItemCategory(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleInventoryAssignItemCategory(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleInventoryCreateComponent(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleInventoryConfigureSlots(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleInventoryAddFunctions(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleInventorySetReplication(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleInventoryCreatePickupActor(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleInventoryConfigurePickupInteraction(const FString &RequestId,
                                                 const TSharedPtr<FJsonObject> &Payload,
                                                 FMcpResponseHandle Socket);
  bool HandleInventoryConfigurePickupRespawn(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleInventoryConfigurePickupEffects(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleInventoryCreateEquipmentComponent(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle Socket);
  bool HandleInventoryConfigureEquipmentEffects(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleInventoryAddEquipmentFunctions(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleInventoryConfigureEquipmentVisuals(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleInventoryCreateLootTable(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleInventoryAddLootEntry(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleInventoryConfigureLootDrop(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleInventorySetLootQualityTiers(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleInventoryCreateCraftingRecipe(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandleInventoryConfigureRecipeRequirements(const FString &RequestId,
                                                  const TSharedPtr<FJsonObject> &Payload,
                                                  FMcpResponseHandle Socket);
  bool HandleInventoryCreateCraftingStation(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleInventoryAddCraftingComponent(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandleInventoryConfigureItemStacking(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleInventorySetItemIcon(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleInventoryAddRecipeIngredient(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleInventoryRemoveLootEntry(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleInventoryConfigureWeight(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleInventoryGetInfo(const FString &RequestId,
                              const TSharedPtr<FJsonObject> &Payload,
                              FMcpResponseHandle Socket);

private:
  // manage_interaction is classed — see MCP/Calls/McpCalls_ManageInteraction.cpp.
  // Its subhandlers are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members the implementations off
  // the subsystem.
public:
  bool HandleInteractionCreateComponent(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleInteractionConfigureTrace(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleInteractionConfigureWidget(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleInteractionAddEvents(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleInteractionCreateInteractableInterface(const FString &RequestId,
                                                    const TSharedPtr<FJsonObject> &Payload,
                                                    FMcpResponseHandle Socket);
  bool HandleInteractionCreateDoorActor(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleInteractionConfigureDoorProperties(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleInteractionCreateSwitchActor(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleInteractionConfigureSwitchProperties(const FString &RequestId,
                                                  const TSharedPtr<FJsonObject> &Payload,
                                                  FMcpResponseHandle Socket);
  bool HandleInteractionCreateChestActor(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleInteractionConfigureChestProperties(const FString &RequestId,
                                                 const TSharedPtr<FJsonObject> &Payload,
                                                 FMcpResponseHandle Socket);
  bool HandleInteractionCreateLeverActor(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleInteractionAddDestructionComponent(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleInteractionCreateTriggerActor(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandleInteractionGetInfo(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);

  // Phase 19: Widget Authoring System handlers
  // manage_blueprint is classed (MCP/Calls/McpCalls_ManageBlueprint.cpp) — this
  // route dispatcher is public so the classed WidgetAuthoring actions delegate.
public:
  bool HandleManageWidgetAuthoringAction(
      const FString &RequestId, const FString &Action,
      const TSharedPtr<FJsonObject> &Payload,
      FMcpResponseHandle RequestingSocket);
  // Per-action manage_widget_authoring members (Lifecycle / Containers / Leaves),
  // hoisted from the family scanners; called directly by their classed actions.
  bool HandleWidgetAuthoringCreateWidgetBlueprint(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringShowWidget(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringSetWidgetParentClass(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringPreviewWidget(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringRemoveWidget(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringRenameWidget(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringReplaceWidgetClass(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddCanvasPanel(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddHorizontalBox(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddVerticalBox(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddOverlay(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddGridPanel(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddUniformGrid(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddWrapBox(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddScrollBox(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddSizeBox(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddScaleBox(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddBorder(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddSafeZone(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddSpacer(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddWidgetSwitcher(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddTextBlock(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddImage(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddButton(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddProgressBar(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddSlider(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddRichTextBlock(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddCheckBox(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddTextInput(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddComboBox(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddSpinBox(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddListView(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleWidgetAuthoringAddTreeView(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
private:
  // Phase 19 family sub-handlers — each owns one cluster of subactions and
  // returns false if SubAction isn't one of its own (so the dispatcher falls
  // through to the next family). Split out of the former monolithic dispatcher
  // purely for navigability; branch bodies are unchanged.
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
  // Common UI (CommonUI plugin) authoring handlers — per-action members
  // (CommonUIHandlers.cpp) called directly by their classed manage_blueprint
  // actions (MCP/Calls/McpCalls_ManageBlueprint.cpp).
public:
  bool HandleCommonUiAddCommonButton(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleCommonUiAddCommonText(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleCommonUiAddCommonBorder(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleCommonUiSetCommonButtonStyle(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleCommonUiSetCommonTextStyle(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleCommonUiSetCommonButtonInputAction(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleCommonUiCreateCommonButtonStyle(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
  bool HandleCommonUiCreateCommonTextStyle(const FString &RequestId,
      const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle RequestingSocket);
private:
  // manage_networking is classed — see
  // MCP/Calls/McpCalls_ManageNetworking.cpp.
  // Its subhandlers are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members the implementations off
  // the subsystem. Implementations span four TUs: HandleNetworking* @
  // NetworkingHandlers.cpp, HandleInput* @ InputHandlers.cpp,
  // HandleGameFramework* @ GameFrameworkHandlers.cpp, HandleSessions* @
  // SessionsHandlers.cpp.
public:
  bool HandleNetworkingSetPropertyReplicated(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleNetworkingSetReplicationCondition(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle Socket);
  bool HandleNetworkingConfigureNetUpdateFrequency(const FString &RequestId,
                                                   const TSharedPtr<FJsonObject> &Payload,
                                                   FMcpResponseHandle Socket);
  bool HandleNetworkingConfigureNetPriority(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleNetworkingSetNetDormancy(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleNetworkingConfigureReplicationGraph(const FString &RequestId,
                                                 const TSharedPtr<FJsonObject> &Payload,
                                                 FMcpResponseHandle Socket);
  bool HandleNetworkingCreateRpcFunction(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleNetworkingConfigureRpcValidation(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleNetworkingSetRpcReliability(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleNetworkingSetOwner(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleNetworkingSetAutonomousProxy(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleNetworkingCheckHasAuthority(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleNetworkingCheckIsLocallyControlled(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleNetworkingConfigureNetCullDistance(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleNetworkingSetAlwaysRelevant(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleNetworkingSetOnlyRelevantToOwner(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleNetworkingSetReplicatedUsing(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleNetworkingConfigurePushModel(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleNetworkingConfigureClientPrediction(const FString &RequestId,
                                                 const TSharedPtr<FJsonObject> &Payload,
                                                 FMcpResponseHandle Socket);
  bool HandleNetworkingConfigureServerCorrection(const FString &RequestId,
                                                 const TSharedPtr<FJsonObject> &Payload,
                                                 FMcpResponseHandle Socket);
  bool HandleNetworkingAddNetworkPredictionData(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleNetworkingConfigureMovementPrediction(const FString &RequestId,
                                                   const TSharedPtr<FJsonObject> &Payload,
                                                   FMcpResponseHandle Socket);
  bool HandleNetworkingConfigureNetDriver(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleNetworkingSetNetRole(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleNetworkingConfigureReplicatedMovement(const FString &RequestId,
                                                   const TSharedPtr<FJsonObject> &Payload,
                                                   FMcpResponseHandle Socket);
  bool HandleNetworkingGetInfo(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleInputCreateInputAction(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleInputCreateInputMappingContext(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleInputAddMapping(const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload,
                             FMcpResponseHandle Socket);
  bool HandleInputRemoveMapping(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleInputSetInputTrigger(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleInputSetInputModifier(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleInputEnableInputMapping(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleInputDisableInputAction(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleInputGetInputInfo(const FString &RequestId,
                               const TSharedPtr<FJsonObject> &Payload,
                               FMcpResponseHandle Socket);
  bool HandleGameFrameworkCreateGameMode(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleGameFrameworkCreateGameState(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleGameFrameworkCreatePlayerController(const FString &RequestId,
                                                 const TSharedPtr<FJsonObject> &Payload,
                                                 FMcpResponseHandle Socket);
  bool HandleGameFrameworkCreatePlayerState(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleGameFrameworkCreateGameInstance(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleGameFrameworkCreateHudClass(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleGameFrameworkSetDefaultPawnClass(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleGameFrameworkSetPlayerControllerClass(const FString &RequestId,
                                                   const TSharedPtr<FJsonObject> &Payload,
                                                   FMcpResponseHandle Socket);
  bool HandleGameFrameworkSetGameStateClass(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleGameFrameworkSetPlayerStateClass(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleGameFrameworkConfigureGameRules(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleGameFrameworkConfigurePlayerStart(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle Socket);
  bool HandleGameFrameworkSetRespawnRules(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleGameFrameworkConfigureSpectating(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleGameFrameworkGetGameFrameworkInfo(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle Socket);
  bool HandleSessionsSetSplitScreenType(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleSessionsAddLocalPlayer(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleSessionsRemoveLocalPlayer(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleSessionsHostLanServer(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleSessionsJoinLanServer(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleSessionsEnableVoiceChat(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleSessionsSetVoiceChannel(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleSessionsMutePlayer(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleSessionsSetVoiceAttenuation(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleSessionsGetSessionsInfo(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);

private:
  // manage_level_structure is classed — see
  // MCP/Calls/McpCalls_ManageLevelStructure.cpp.
  // Its subhandlers are public so the FMcpCall classes (Private/MCP/Calls/)
  // can delegate, until the module split de-members the implementations off
  // the subsystem. Implementations span two TUs: HandleLevelStructure* @
  // LevelStructureHandlers.cpp, HandleVolume* @ VolumeHandlers.cpp.
public:
  bool HandleLevelStructureCreateLevel(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleLevelStructureCreateSublevel(const FString &RequestId,
                                          const TSharedPtr<FJsonObject> &Payload,
                                          FMcpResponseHandle Socket);
  bool HandleLevelStructureConfigureLevelStreaming(const FString &RequestId,
                                                   const TSharedPtr<FJsonObject> &Payload,
                                                   FMcpResponseHandle Socket);
  bool HandleLevelStructureSetStreamingDistance(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleLevelStructureEnableWorldPartition(const FString &RequestId,
                                                const TSharedPtr<FJsonObject> &Payload,
                                                FMcpResponseHandle Socket);
  bool HandleLevelStructureConfigureGridSize(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleLevelStructureCreateDataLayer(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandleLevelStructureAssignActorToDataLayer(const FString &RequestId,
                                                  const TSharedPtr<FJsonObject> &Payload,
                                                  FMcpResponseHandle Socket);
  bool HandleLevelStructureConfigureHlodLayer(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleLevelStructureCreateMinimapVolume(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle Socket);
  bool HandleLevelStructureOpenLevelBlueprint(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleLevelStructureAddLevelBlueprintNode(const FString &RequestId,
                                                 const TSharedPtr<FJsonObject> &Payload,
                                                 FMcpResponseHandle Socket);
  bool HandleLevelStructureConnectLevelBlueprintNodes(const FString &RequestId,
                                                      const TSharedPtr<FJsonObject> &Payload,
                                                      FMcpResponseHandle Socket);
  bool HandleLevelStructureCreateLevelInstance(const FString &RequestId,
                                               const TSharedPtr<FJsonObject> &Payload,
                                               FMcpResponseHandle Socket);
  bool HandleLevelStructureCreatePackedLevelActor(const FString &RequestId,
                                                  const TSharedPtr<FJsonObject> &Payload,
                                                  FMcpResponseHandle Socket);
  bool HandleLevelStructureGetInfo(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleVolumeCreateTriggerVolume(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleVolumeCreateTriggerBox(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleVolumeCreateTriggerSphere(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleVolumeCreateTriggerCapsule(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleVolumeCreateBlockingVolume(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);
  bool HandleVolumeCreateKillZVolume(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleVolumeCreatePainCausingVolume(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandleVolumeCreatePhysicsVolume(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleVolumeCreateAudioVolume(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleVolumeCreateReverbVolume(const FString &RequestId,
                                      const TSharedPtr<FJsonObject> &Payload,
                                      FMcpResponseHandle Socket);
  bool HandleVolumeCreatePostProcessVolume(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandleVolumeCreateCullDistanceVolume(const FString &RequestId,
                                            const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResponseHandle Socket);
  bool HandleVolumeCreatePrecomputedVisibilityVolume(const FString &RequestId,
                                                     const TSharedPtr<FJsonObject> &Payload,
                                                     FMcpResponseHandle Socket);
  bool HandleVolumeCreateLightmassImportanceVolume(const FString &RequestId,
                                                   const TSharedPtr<FJsonObject> &Payload,
                                                   FMcpResponseHandle Socket);
  bool HandleVolumeCreateNavMeshBoundsVolume(const FString &RequestId,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResponseHandle Socket);
  bool HandleVolumeCreateNavModifierVolume(const FString &RequestId,
                                           const TSharedPtr<FJsonObject> &Payload,
                                           FMcpResponseHandle Socket);
  bool HandleVolumeCreateCameraBlockingVolume(const FString &RequestId,
                                              const TSharedPtr<FJsonObject> &Payload,
                                              FMcpResponseHandle Socket);
  bool HandleVolumeSetVolumeExtent(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleVolumeSetVolumeProperties(const FString &RequestId,
                                       const TSharedPtr<FJsonObject> &Payload,
                                       FMcpResponseHandle Socket);
  bool HandleVolumeSetVolumeBounds(const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   FMcpResponseHandle Socket);
  bool HandleVolumeRemoveVolume(const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                FMcpResponseHandle Socket);
  bool HandleVolumeGetVolumesInfo(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleVolumeAddTriggerVolume(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleVolumeAddBlockingVolume(const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResponseHandle Socket);
  bool HandleVolumeAddKillZVolume(const FString &RequestId,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  FMcpResponseHandle Socket);
  bool HandleVolumeAddPhysicsVolume(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    FMcpResponseHandle Socket);
  bool HandleVolumeAddCullDistanceVolume(const FString &RequestId,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         FMcpResponseHandle Socket);
  bool HandleVolumeAddPostProcessVolume(const FString &RequestId,
                                        const TSharedPtr<FJsonObject> &Payload,
                                        FMcpResponseHandle Socket);

private:
  // 2. Execution & Build / Test Pipeline

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

public:
  // Asset handlers
  bool HandleImportAsset(const FString &RequestId,
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
private:

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
