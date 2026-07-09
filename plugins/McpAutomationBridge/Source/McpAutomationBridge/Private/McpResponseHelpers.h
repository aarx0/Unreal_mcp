// McpResponseHelpers.h — structured success/error envelope senders.
// Only ControlHandlers uses this envelope shape; everything else calls
// SendAutomationResponse directly. Kept out of McpAutomationBridgeHelpers.h so
// the helpers layer does not depend on the subsystem header.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

/**
 * Sends a standardized success response.
 *
 * Format:
 * {
 *   "success": true,
 *   "data": { ... },
 *   "warnings": [],
 *   "error": null
 * }
 */
static inline void SendStandardSuccessResponse(
    UMcpAutomationBridgeSubsystem *Subsystem,
    FMcpResponseHandle Socket, const FString &RequestId,
    const FString &Message, const TSharedPtr<FJsonObject> &Data,
    const TArray<FString> &Warnings = TArray<FString>()) {
  if (!Subsystem)
    return;

  TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
  Envelope->SetBoolField(TEXT("success"), true);
  Envelope->SetObjectField(TEXT("data"),
                           Data.IsValid() ? Data : MakeShared<FJsonObject>());

  TArray<TSharedPtr<FJsonValue>> WarningVals;
  for (const FString &W : Warnings) {
    WarningVals.Add(MakeShared<FJsonValueString>(W));
  }
  Envelope->SetArrayField(TEXT("warnings"), WarningVals);

  Envelope->SetField(TEXT("error"), MakeShared<FJsonValueNull>());

  Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, Envelope,
                                    FString());
}

/**
 * Sends a standardized error response with structured error details.
 *
 * Format:
 * {
 *   "success": false,
 *   "data": {},   // Empty object for schema compliance
 *   "error": {
 *     "code": "ERROR_CODE",
 *     "message": "Human readable message",
 *     "parameter": "optional_param_name",
 *     ...
 *   }
 * }
 */
static inline void SendStandardErrorResponse(
    UMcpAutomationBridgeSubsystem *Subsystem,
    FMcpResponseHandle Socket, const FString &RequestId,
    const FString &ErrorCode, const FString &ErrorMessage,
    const TSharedPtr<FJsonObject> &ErrorDetails = nullptr) {
  if (!Subsystem)
    return;

  TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
  Envelope->SetBoolField(TEXT("success"), false);

  // CRITICAL: Add empty data object for schema compliance
  // The MCP schema requires data: { type: 'object' } in all responses
  Envelope->SetObjectField(TEXT("data"), MakeShared<FJsonObject>());

  TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
  ErrorObj->SetStringField(TEXT("code"), ErrorCode);
  ErrorObj->SetStringField(TEXT("message"), ErrorMessage);

  if (ErrorDetails.IsValid()) {
    // Merge details into error object
    for (const auto &Pair : ErrorDetails->Values) {
      ErrorObj->SetField(Pair.Key, Pair.Value);
    }
  }

  Envelope->SetObjectField(TEXT("error"), ErrorObj);

  Subsystem->SendAutomationResponse(Socket, RequestId, false, ErrorMessage,
                                    Envelope, ErrorCode);
}

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
