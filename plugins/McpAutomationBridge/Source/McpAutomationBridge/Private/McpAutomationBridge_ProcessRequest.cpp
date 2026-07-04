#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpNativeTransport.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"

void UMcpAutomationBridgeSubsystem::ProcessAutomationRequest(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket,
    ERequestOrigin Origin) {
  // This large implementation was extracted from the original subsystem
  // translation unit to keep the core file smaller and focused. It
  // contains the main dispatcher that delegates to specialized handler
  // functions (property/blueprint/sequence/asset handlers) and retains
  // the queuing/scope-exit safety logic expected by callers.

  // Ensure automation processing happens on the game thread
  // This trace is intentionally verbose — routine requests can be high
  // frequency and will otherwise flood the logs. Developers can enable
  // Verbose logging to see these messages when required.
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT(">>> ProcessAutomationRequest ENTRY: RequestId=%s action='%s' "
              "(thread=%s)"),
         *RequestId, *Action,
         IsInGameThread() ? TEXT("GameThread") : TEXT("SocketThread"));
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("ProcessAutomationRequest invoked (thread=%s) RequestId=%s "
              "action=%s activeSockets=%d"),
         IsInGameThread() ? TEXT("GameThread") : TEXT("SocketThread"),
         *RequestId, *Action, 0);
  if (!IsInGameThread()) {
    QueueAutomationRequest(RequestId, Action, Payload, RequestingSocket, Origin);
    return;
  }

  // Guard against unsafe engine states (Saving, GC, Async Loading)
  // Calling StaticFindObject (via ResolveClassByName) during these states can
  // cause crashes.
  if (GIsSavingPackage || IsGarbageCollecting() || IsAsyncLoading()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Deferring ProcessAutomationRequest due to active "
                "Serialization/GC/Loading: RequestId=%s Action=%s"),
           *RequestId, *Action);

    QueueAutomationRequest(RequestId, Action, Payload, RequestingSocket, Origin);
    return;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("Starting ProcessAutomationRequest on GameThread: RequestId=%s "
              "action=%s bProcessingAutomationRequest=%s"),
         *RequestId, *Action,
         bProcessingAutomationRequest ? TEXT("true") : TEXT("false"));

  // (WebSocket request telemetry removed — pull-only / HTTP)

  // Reentrancy guard / enqueue
  if (bProcessingAutomationRequest) {
    QueueAutomationRequest(RequestId, Action, Payload, RequestingSocket, Origin);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Enqueued automation request %s for action %s (processing in "
                "progress)."),
           *RequestId, *Action);
    return;
  }

  bProcessingAutomationRequest = true;
  CurrentRequestOrigin = Origin;
  bool bDispatchHandled = false;
  bool bErrorCaptureStarted = false;
  FString ConsumedHandlerLabel = TEXT("unknown-handler");
  const double DispatchStartSeconds = FPlatformTime::Seconds();

  auto HandleAndLog = [&](const TCHAR *HandlerLabel, auto &&Callable) -> bool {
    const bool bResult = Callable();
    if (bResult) {
      bDispatchHandled = true;
      ConsumedHandlerLabel = HandlerLabel;
    }
    return bResult;
  };

  {
    ON_SCOPE_EXIT {
      // =====================================================================
      // End Error Capture and check for captured errors
      // =====================================================================
      TArray<FString> CapturedErrors;
      bool bHadEngineErrors = false;
      if (bErrorCaptureStarted)
      {
        CapturedErrors = EndErrorCapture();
        bHadEngineErrors = HasCapturedErrors();
      }
      
      if (bHadEngineErrors && bDispatchHandled)
      {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("ProcessAutomationRequest: Handler reported success but "
                    "engine errors were detected for RequestId=%s action='%s'. "
                    "Errors: %s"),
               *RequestId, *Action,
               CapturedErrors.Num() > 0 ? *FString::Join(CapturedErrors, TEXT("; ")) : TEXT("unknown"));
        
        // The handler response path converts successful responses to
        // ENGINE_ERROR failures when captured errors exist. Keep this warning as
        // a secondary audit trail for handlers that returned after logging an
        // engine error.
      }
      
      bProcessingAutomationRequest = false;
      CurrentRequestOrigin = ERequestOrigin::Unspecified;
      const double DispatchEndSeconds = FPlatformTime::Seconds();
      const double DurationMs =
          (DispatchEndSeconds - DispatchStartSeconds) * 1000.0;
      if (bDispatchHandled) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("ProcessAutomationRequest: Completed handler='%s' "
                    "RequestId=%s action='%s' (%.3f ms) engineErrors=%s"),
               *ConsumedHandlerLabel, *RequestId, *Action, DurationMs,
               bHadEngineErrors ? TEXT("true") : TEXT("false"));
        // A handler holding the game thread past a second stalls the whole
        // editor; surface it without requiring Verbose logging.
        if (DurationMs > 1000.0) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("Slow automation request: action='%s' took %.0f ms on "
                      "the game thread (RequestId=%s)"),
                 *Action, DurationMs, *RequestId);
        }
      } else {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("ProcessAutomationRequest: No handler consumed "
                    "RequestId=%s action='%s' (%.3f ms)"),
               *RequestId, *Action, DurationMs);
      }
    };

    try {
      // =========================================================================
      // Begin Error Capture for this request (inside try block)
      // =========================================================================
      // This captures engine-level errors (like ensure failures) that occur
      // during handler execution. SendAutomationResponse checks the capture and
      // turns otherwise successful responses into ENGINE_ERROR failures so tool
      // responses stay aligned with the Unreal log.
      // Note: BeginErrorCapture is placed inside the try block to avoid
      // capturing our own catch-block error logging.
      BeginErrorCapture();
      bErrorCaptureStarted = true;

      // Map this requestId to the requesting socket so responses can be
      // delivered reliably
      // (WebSocket socket-registration removed — pull-only / HTTP)

      // A consuming handler must have responded or declared the deferral
      // via MarkRequestDeferred: handlers execute synchronously (the
      // AsyncTask re-queue completions were inlined), so an undeclared
      // still-parked request here is a handler bug. Fail it now instead
      // of leaving the client to the 300s reaper.
      auto FailIfSilentlyConsumed = [&]() {
        const bool bDeclaredDeferred = DeferredRequestIds.Remove(RequestId) > 0;
        if (!bDeclaredDeferred && Origin == ERequestOrigin::NativeHTTP &&
            NativeTransport && NativeTransport->HasPendingRequest(RequestId)) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
                 TEXT("Handler for action '%s' consumed RequestId=%s without "
                      "sending a response."),
                 *Action, *RequestId);
          SendAutomationError(
              RequestingSocket, RequestId,
              FString::Printf(TEXT("Handler for '%s' consumed the request "
                                   "without sending a response."),
                              *Action),
              TEXT("HANDLER_NO_RESPONSE"));
        }
      };

      // ---------------------------------------------------------
      // Classed actions (FMcpCall) win over the legacy family handlers —
      // the strangler-fig path (docs/action-declarations.md). The registry
      // key is (tool, payload action); lookups are case-insensitive like
      // every legacy dispatch comparison.
      // ---------------------------------------------------------
      FString PayloadAction;
      if (Payload.IsValid()) {
        Payload->TryGetStringField(TEXT("action"), PayloadAction);
      }
      if (FMcpCall *Call = FMcpCallRegistry::Get().FindCall(Action, PayloadAction)) {
        const FString CallLabel = Action + TEXT(".") + PayloadAction;
        if (HandleAndLog(*CallLabel, [&]() {
              return Call->Execute(*this, RequestId, Payload, RequestingSocket);
            })) {
          FailIfSilentlyConsumed();
          return;
        }
      }

      // ---------------------------------------------------------
      // Single dispatch: the handler registry (O(1)). Every action is
      // registered in InitializeHandlers(); a miss — or a registered handler
      // that declines by returning false — falls through to the UNKNOWN_ACTION
      // error below. There is no secondary linear scan.
      // ---------------------------------------------------------
      if (const FAutomationHandler *Handler = AutomationHandlers.Find(Action)) {
        if (HandleAndLog(*Action, [&]() {
              return (*Handler)(RequestId, Action, Payload, RequestingSocket);
            })) {
          FailIfSilentlyConsumed();
          return;
        }
      }

      // Unhandled action. For consolidated tools the dispatch Action is the
      // tool name; the sub-action the caller actually sent lives in the payload.
      bDispatchHandled = true;
      ConsumedHandlerLabel = TEXT("SendAutomationError (unknown action)");
      FString UnknownSubAction;
      if (Payload.IsValid()) {
        Payload->TryGetStringField(TEXT("action"), UnknownSubAction);
      }
      SendAutomationError(
          RequestingSocket, RequestId,
          (!UnknownSubAction.IsEmpty() && UnknownSubAction != Action)
              ? FString::Printf(TEXT("Unknown action '%s' for tool '%s'"),
                                *UnknownSubAction, *Action)
              : FString::Printf(TEXT("Unknown automation action: %s"), *Action),
          TEXT("UNKNOWN_ACTION"));
    } catch (const std::exception &E) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
             TEXT("Unhandled exception processing automation request %s: %s"),
             *RequestId, ANSI_TO_TCHAR(E.what()));
      bDispatchHandled = true;
      ConsumedHandlerLabel = TEXT("Exception handler");
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Internal error: %s"), ANSI_TO_TCHAR(E.what())),
          TEXT("INTERNAL_ERROR"));
    } catch (...) {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Error,
          TEXT("Unhandled unknown exception processing automation request %s"),
          *RequestId);
      bDispatchHandled = true;
      ConsumedHandlerLabel = TEXT("Exception handler (unknown)");
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Internal error (unknown)."),
                          TEXT("INTERNAL_ERROR"));
    }
  }
}

// ProcessPendingAutomationRequests() intentionally implemented in the
// primary subsystem translation unit (McpAutomationBridgeSubsystem.cpp)
// to ensure the linker emits the symbol into the module's object file.
