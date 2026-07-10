#pragma once

// system_control handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S). One header per handler TU; all declare
// into the shared McpHandlers::SystemControl namespace. Dispatch:
// MCP/Calls/McpCalls_SystemControl.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::SystemControl
{
bool HandleDebugSpawnCategory(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::SystemControl
