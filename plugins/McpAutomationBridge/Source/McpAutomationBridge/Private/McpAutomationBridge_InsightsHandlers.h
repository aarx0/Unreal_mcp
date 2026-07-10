#pragma once

// system_control insights handler as a namespaced free function. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S). Declares into the shared
// McpHandlers::SystemControl namespace (start_session delegates here). Dispatch:
// MCP/Calls/McpCalls_SystemControl.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::SystemControl
{
bool HandleInsightsAction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const FString& Action, const TSharedPtr<FJsonObject>& Payload,
                          FMcpResponseHandle RequestingSocket);

} // namespace McpHandlers::SystemControl
