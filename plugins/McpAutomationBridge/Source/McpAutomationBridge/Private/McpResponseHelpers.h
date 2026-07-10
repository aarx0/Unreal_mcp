// McpResponseHelpers.h — SendWriteReportResponse, the fail-in-place
// write-report responder. Kept out of McpAutomationBridgeHelpers.h so the
// helpers layer does not depend on the subsystem header.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

/**
 * Finalize a fail-in-place "bag" setter from an FMcpWriteReport (the shipped
 * silent-success convention). One place owns the exact success rule:
 *   - nothing requested (no applied, no failed) -> NO_CHANGES_REQUESTED
 *   - any requested field failed -> success=false, PROPERTY_WRITE_FAILED, data
 *     carries appliedProperties[] + failed[] so the caller sees what landed
 *   - every requested field applied -> success=true, data carries
 *     appliedProperties[] + AddVerification(VerifyObject)
 * Always returns true (the request is answered). Data may be a caller-populated
 * result object (its own fields are preserved) or null.
 */
static inline bool SendWriteReportResponse(
    UMcpAutomationBridgeSubsystem *Subsystem, FMcpResponseHandle Socket,
    const FString &RequestId, const McpHandlerUtils::FMcpWriteReport &Report,
    const TSharedPtr<FJsonObject> &Data, const FString &SuccessMessage,
    UObject *VerifyObject = nullptr) {
  if (!Subsystem)
    return true;

  if (!Report.AnyApplied() && !Report.AnyFailed()) {
    Subsystem->SendAutomationError(Socket, RequestId,
                                   TEXT("no writable fields were provided"),
                                   TEXT("NO_CHANGES_REQUESTED"));
    return true;
  }

  TSharedPtr<FJsonObject> Result =
      Data.IsValid() ? Data : McpHandlerUtils::CreateResultObject();
  Report.WriteInto(Result);

  if (Report.AnyFailed()) {
    const int32 Total = Report.Applied.Num() + Report.Failed.Num();
    Subsystem->SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("%d of %d requested field(s) failed to apply"),
                        Report.Failed.Num(), Total),
        Result, TEXT("PROPERTY_WRITE_FAILED"));
    return true;
  }

  if (VerifyObject) {
    McpHandlerUtils::AddVerification(Result, VerifyObject);
  }
  Subsystem->SendAutomationResponse(Socket, RequestId, true, SuccessMessage,
                                    Result);
  return true;
}
