#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UMcpAutomationBridgeSubsystem;
#include "McpAutomationBridgeGlobals.h"

/**
 * Handle a request to create or instantiate a Blueprint asset or Blueprint-derived object described in the payload.
 * @param Self Pointer to the MCP automation bridge subsystem processing the request.
 * @param RequestId Correlation identifier for the incoming request/response.
 * @param LocalPayload JSON object containing the blueprint creation parameters and metadata.
 * @param RequestingSocket WebSocket that originated the request (may be null).
 * @returns `true` if the request was parsed and processing was initiated successfully, `false` otherwise.
 */
class FBlueprintCreationHandlers
{
public:
    static bool HandleBlueprintCreate(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& LocalPayload, FMcpResponseHandle RequestingSocket);
};