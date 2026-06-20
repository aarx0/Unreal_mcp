// =============================================================================
// McpAutomationBridge_LogHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Log capture + retrieval (manage_logs)
//
// UE Version Support: 5.0 - 5.7
//
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_logs (reached via system_control subscribe/unsubscribe/
//         get_log/tail_log/clear_log)
//   - get_log / tail_log : return recent captured log lines (poll-based)
//   - clear_log          : empty the capture ring (seq counter preserved)
//   - subscribe          : (re)enable capture into the ring
//   - unsubscribe        : pause capture (device stays registered, just idles)
//
// Architecture (pull-only):
//   The transport is request/response — there is NO server->client push (the
//   old WebSocket push path fed SendRawMessage, which is now a no-op). So log
//   visibility is a SERVER-SIDE RING BUFFER the agent POLLS. FMcpLogOutputDevice
//   captures every (filtered) line into UMcpAutomationBridgeSubsystem's bounded
//   circular buffer (cheap append, formatting deferred to read time); get_log
//   reads it. The device is registered always-on in Initialize so tail_log can
//   report "what just happened" with no prior subscribe.
//
// Notes:
//   - Capture runs on arbitrary threads -> CaptureLogLine is mutex-guarded.
//   - Own-category + noisy engine categories are filtered to keep the ring useful.
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"
#include "Misc/OutputDevice.h"
#include "HAL/PlatformTime.h"

// =============================================================================
// Verbosity helpers
// =============================================================================
namespace
{
    FString VerbosityToString(ELogVerbosity::Type Verbosity)
    {
        switch (Verbosity & ELogVerbosity::VerbosityMask)
        {
            case ELogVerbosity::Fatal:       return TEXT("Fatal");
            case ELogVerbosity::Error:       return TEXT("Error");
            case ELogVerbosity::Warning:     return TEXT("Warning");
            case ELogVerbosity::Display:     return TEXT("Display");
            case ELogVerbosity::Log:         return TEXT("Log");
            case ELogVerbosity::Verbose:     return TEXT("Verbose");
            case ELogVerbosity::VeryVerbose: return TEXT("VeryVerbose");
            default:                         return TEXT("Log");
        }
    }

    // Parse a minimum-severity filter. Lower ELogVerbosity value == MORE severe
    // (Fatal=1 .. VeryVerbose=7), so "include >= this severity" means
    // (entry.Verbosity & Mask) <= threshold. Default VeryVerbose => include all.
    ELogVerbosity::Type ParseMinVerbosity(const FString& In)
    {
        if (In.Equals(TEXT("Fatal"), ESearchCase::IgnoreCase))       return ELogVerbosity::Fatal;
        if (In.Equals(TEXT("Error"), ESearchCase::IgnoreCase))       return ELogVerbosity::Error;
        if (In.Equals(TEXT("Warning"), ESearchCase::IgnoreCase))     return ELogVerbosity::Warning;
        if (In.Equals(TEXT("Display"), ESearchCase::IgnoreCase))     return ELogVerbosity::Display;
        if (In.Equals(TEXT("Log"), ESearchCase::IgnoreCase))         return ELogVerbosity::Log;
        if (In.Equals(TEXT("Verbose"), ESearchCase::IgnoreCase))     return ELogVerbosity::Verbose;
        if (In.Equals(TEXT("VeryVerbose"), ESearchCase::IgnoreCase)) return ELogVerbosity::VeryVerbose;
        return ELogVerbosity::VeryVerbose;  // default: no severity filter
    }
}

// =============================================================================
// FMcpLogOutputDevice - captures filtered log lines into the subsystem ring
// =============================================================================
class FMcpLogOutputDevice : public FOutputDevice
{
public:
    explicit FMcpLogOutputDevice(UMcpAutomationBridgeSubsystem* InSubsystem)
        : Subsystem(InSubsystem)
    {
    }

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        if (!Subsystem || !Subsystem->IsValidLowLevel())
        {
            return;
        }

        // Drop our own category entirely (the bridge is chatty at Verbose and it
        // would crowd out the lines a caller actually wants to see).
        if (Category == LogMcpAutomationBridgeSubsystem.GetCategoryName())
        {
            return;
        }

        // Filter highly verbose engine categories.
        static const FName NAME_LogRHI(TEXT("LogRHI"));
        static const FName NAME_LogEOSSDK(TEXT("LogEOSSDK"));
        static const FName NAME_LogCsvProfiler(TEXT("LogCsvProfiler"));
        if (Category == NAME_LogRHI || Category == NAME_LogEOSSDK || Category == NAME_LogCsvProfiler)
        {
            return;
        }

        // Known benign noise.
        static const FName NAME_LogSlateStyle(TEXT("LogSlateStyle"));
        if (Verbosity == ELogVerbosity::Warning && Category == NAME_LogSlateStyle &&
            FString(V).Contains(TEXT("Missing Resource from 'ProfileVisualizerStyle'")))
        {
            return;
        }
        static const FName NAME_LogStats(TEXT("LogStats"));
        if (Category == NAME_LogStats && FString(V).Contains(TEXT("There is no thread with id")))
        {
            return;
        }

        // Cheap append; formatting (verbosity->string, JSON) is deferred to the
        // get_log read path. NOTE: never UE_LOG from this path (recursion).
        Subsystem->CaptureLogLine(Verbosity, Category, V);
    }

private:
    UMcpAutomationBridgeSubsystem* Subsystem;
};

// =============================================================================
// Subsystem: device factory + ring append
// =============================================================================

TSharedPtr<FOutputDevice> UMcpAutomationBridgeSubsystem::MakeLogCaptureDevice()
{
    return MakeShared<FMcpLogOutputDevice>(this);
}

void UMcpAutomationBridgeSubsystem::CaptureLogLine(
    ELogVerbosity::Type Verbosity, const FName& Category, const TCHAR* Message)
{
    if (!bLogCaptureEnabled || LogRingCapacity <= 0)
    {
        return;
    }

    FScopeLock Lock(&LogRingMutex);

    // Defensive: ensure the backing array matches capacity (Initialize sizes it).
    if (LogRing.Num() != LogRingCapacity)
    {
        LogRing.SetNum(LogRingCapacity);
        LogRingHead = 0;
        LogRingCount = 0;
    }

    FMcpLogEntry Entry;
    Entry.Seq = LogRingNextSeq++;
    Entry.Time = FPlatformTime::Seconds();
    Entry.Verbosity = Verbosity;
    Entry.Category = Category;
    Entry.Message = Message;

    if (LogRingCount < LogRingCapacity)
    {
        const int32 WriteIdx = (LogRingHead + LogRingCount) % LogRingCapacity;
        LogRing[WriteIdx] = MoveTemp(Entry);
        ++LogRingCount;
    }
    else
    {
        // Full: overwrite the oldest and advance head.
        LogRing[LogRingHead] = MoveTemp(Entry);
        LogRingHead = (LogRingHead + 1) % LogRingCapacity;
    }
}

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleLogAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_logs"))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = GetJsonStringField(Payload, TEXT("action"));
    }

    // -------------------------------------------------------------------------
    // get_log / tail_log : poll recent captured lines
    // -------------------------------------------------------------------------
    if (SubAction.Equals(TEXT("get_log"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("tail_log"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("get_logs"), ESearchCase::IgnoreCase))
    {
        int32 Count = GetJsonIntField(Payload, TEXT("count"), 100);
        Count = FMath::Clamp(Count, 1, 2000);

        const bool bHasSince = Payload->HasField(TEXT("sinceSeq"));
        const uint64 SinceSeq = bHasSince
            ? static_cast<uint64>(FMath::Max(0.0, GetJsonNumberField(Payload, TEXT("sinceSeq"), 0.0)))
            : 0;

        const FString VerbStr = GetJsonStringField(Payload, TEXT("verbosity"));
        const ELogVerbosity::Type MinVerbosity =
            VerbStr.IsEmpty() ? ELogVerbosity::VeryVerbose : ParseMinVerbosity(VerbStr);
        const FString CategoryFilter = GetJsonStringField(Payload, TEXT("category"));
        const FString Contains = GetJsonStringField(Payload, TEXT("contains"));

        TArray<FMcpLogEntry> Matched;     // oldest-first
        uint64 OldestSeq = 0;
        uint64 TotalCaptured = 0;
        uint64 Dropped = 0;
        const double Now = FPlatformTime::Seconds();
        bool bCaptureEnabled = false;
        int32 RingCount = 0;
        int32 RingCap = 0;

        {
            FScopeLock Lock(&LogRingMutex);
            bCaptureEnabled = bLogCaptureEnabled;
            TotalCaptured = LogRingNextSeq;
            RingCount = LogRingCount;
            RingCap = LogRingCapacity;
            OldestSeq = (LogRingCount > 0)
                ? LogRing[LogRingHead].Seq
                : LogRingNextSeq;

            // If the caller's cursor predates the oldest line we still hold,
            // that many lines were evicted unseen — report it (no silent gap).
            if (bHasSince && (SinceSeq + 1) < OldestSeq)
            {
                Dropped = OldestSeq - (SinceSeq + 1);
            }

            for (int32 i = 0; i < LogRingCount; ++i)
            {
                const FMcpLogEntry& E = LogRing[(LogRingHead + i) % LogRingCapacity];
                if (bHasSince && E.Seq <= SinceSeq)
                {
                    continue;
                }
                if ((E.Verbosity & ELogVerbosity::VerbosityMask) > (MinVerbosity & ELogVerbosity::VerbosityMask))
                {
                    continue;  // less severe than requested minimum
                }
                if (!CategoryFilter.IsEmpty() &&
                    !E.Category.ToString().Contains(CategoryFilter, ESearchCase::IgnoreCase))
                {
                    continue;
                }
                if (!Contains.IsEmpty() &&
                    !E.Message.Contains(Contains, ESearchCase::IgnoreCase))
                {
                    continue;
                }
                Matched.Add(E);
            }
        }

        // Windowing: incremental polling (sinceSeq) returns the OLDEST `Count`
        // past the cursor so repeated polls advance; a plain tail returns the
        // NEWEST `Count`. Either way present oldest-first.
        bool bHasMore = false;
        TArray<FMcpLogEntry> Window;
        if (bHasSince)
        {
            bHasMore = Matched.Num() > Count;
            const int32 N = FMath::Min(Count, Matched.Num());
            for (int32 i = 0; i < N; ++i) { Window.Add(Matched[i]); }
        }
        else
        {
            bHasMore = Matched.Num() > Count;
            const int32 Start = FMath::Max(0, Matched.Num() - Count);
            for (int32 i = Start; i < Matched.Num(); ++i) { Window.Add(Matched[i]); }
        }

        TArray<TSharedPtr<FJsonValue>> Entries;
        uint64 NextSeq = bHasSince ? SinceSeq : (TotalCaptured > 0 ? TotalCaptured - 1 : 0);
        for (const FMcpLogEntry& E : Window)
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetNumberField(TEXT("seq"), static_cast<double>(E.Seq));
            Obj->SetNumberField(TEXT("ageSeconds"), FMath::RoundToFloat(static_cast<float>(Now - E.Time) * 1000.0f) / 1000.0f);
            Obj->SetStringField(TEXT("verbosity"), VerbosityToString(E.Verbosity));
            Obj->SetStringField(TEXT("category"), E.Category.ToString());
            Obj->SetStringField(TEXT("message"), E.Message);
            Entries.Add(MakeShared<FJsonValueObject>(Obj));
            NextSeq = E.Seq;  // advance to last returned (Window is oldest-first)
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("manage_logs"));
        Result->SetStringField(TEXT("subAction"), TEXT("get_log"));
        Result->SetArrayField(TEXT("entries"), Entries);
        Result->SetNumberField(TEXT("returnedCount"), Entries.Num());
        Result->SetNumberField(TEXT("totalCaptured"), static_cast<double>(TotalCaptured));
        Result->SetNumberField(TEXT("oldestSeq"), static_cast<double>(OldestSeq));
        Result->SetNumberField(TEXT("nextSeq"), static_cast<double>(NextSeq));
        Result->SetNumberField(TEXT("dropped"), static_cast<double>(Dropped));
        Result->SetBoolField(TEXT("hasMore"), bHasMore);
        Result->SetBoolField(TEXT("captureEnabled"), bCaptureEnabled);
        Result->SetNumberField(TEXT("ringCount"), RingCount);
        Result->SetNumberField(TEXT("ringCapacity"), RingCap);

        const FString Msg = FString::Printf(
            TEXT("Returned %d log line(s)%s."),
            Entries.Num(),
            bHasMore ? TEXT(" (more available — raise count or poll with nextSeq)") : TEXT(""));
        SendAutomationResponse(RequestingSocket, RequestId, true, Msg, Result, FString());
        return true;
    }

    // -------------------------------------------------------------------------
    // clear_log : empty the ring (seq counter preserved so cursors stay unique)
    // -------------------------------------------------------------------------
    if (SubAction.Equals(TEXT("clear_log"), ESearchCase::IgnoreCase))
    {
        int32 Cleared = 0;
        {
            FScopeLock Lock(&LogRingMutex);
            Cleared = LogRingCount;
            LogRingHead = 0;
            LogRingCount = 0;
        }
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("manage_logs"));
        Result->SetStringField(TEXT("subAction"), TEXT("clear_log"));
        Result->SetNumberField(TEXT("cleared"), Cleared);
        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Cleared %d log line(s)."), Cleared), Result, FString());
        return true;
    }

    // -------------------------------------------------------------------------
    // subscribe / unsubscribe : enable/disable capture (NOT a push — read via
    // get_log/tail_log). Kept for backward compat with the old action names;
    // now honest about the pull model instead of reporting a push that never fires.
    // -------------------------------------------------------------------------
    if (SubAction.Equals(TEXT("subscribe"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("unsubscribe"), ESearchCase::IgnoreCase))
    {
        const bool bEnable = SubAction.Equals(TEXT("subscribe"), ESearchCase::IgnoreCase);
        {
            FScopeLock Lock(&LogRingMutex);
            bLogCaptureEnabled = bEnable;
        }
        // The device is registered always-on in Initialize; ensure it exists in
        // case subscribe is called on a build where init skipped (commandlet).
        if (bEnable && GLog && !LogCaptureDevice.IsValid())
        {
            LogRing.SetNum(LogRingCapacity);
            LogCaptureDevice = MakeLogCaptureDevice();
            GLog->AddOutputDevice(LogCaptureDevice.Get());
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("manage_logs"));
        Result->SetStringField(TEXT("subAction"), bEnable ? TEXT("subscribe") : TEXT("unsubscribe"));
        Result->SetBoolField(TEXT("captureEnabled"), bEnable);
        const FString Msg = bEnable
            ? TEXT("Log capture enabled. Read lines with get_log/tail_log (pull — no push).")
            : TEXT("Log capture paused. Re-enable with subscribe; existing lines stay readable until clear_log.");
        SendAutomationResponse(RequestingSocket, RequestId, true, Msg, Result, FString());
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Unknown subAction. Use get_log/tail_log, clear_log, subscribe, or unsubscribe."),
        TEXT("INVALID_SUBACTION"));
    return true;
}
