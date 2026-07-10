#pragma once

// Generic editor-function execution handler as a namespaced free function.
// De-membered off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem
// is passed by reference as the first parameter (S). Reaches the still-member
// console-command handler through S.HandleConsoleCommandAction. The
// build_environment bake_lightmap wrapper forwards here.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::BuildEnvironment
{
bool HandleExecuteEditorFunction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::BuildEnvironment
