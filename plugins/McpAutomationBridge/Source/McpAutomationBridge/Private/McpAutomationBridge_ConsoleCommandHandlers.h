#pragma once

// system_control console-command handler as a namespaced free function.
// De-membered off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem
// is passed by reference as the first parameter (S). Declares into the shared
// McpHandlers::SystemControl namespace (execute_command / set_cvar delegate here,
// as does execute_editor_function's console alias). Dispatch:
// MCP/Calls/McpCalls_SystemControl.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::SystemControl
{
bool HandleConsoleCommandAction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                const FString& Action, const TSharedPtr<FJsonObject>& Payload,
                                FMcpResponseHandle RequestingSocket);

} // namespace McpHandlers::SystemControl
