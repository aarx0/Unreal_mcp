#pragma once

// World Partition / Data Layer handler as a namespaced free function.
// De-membered off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem
// is passed by reference as the first parameter (S), and the handler touches
// instance state only through its public response API
// (S.SendAutomationResponse / S.SendAutomationError).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::BuildEnvironment
{
bool HandleWorldPartitionAction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::BuildEnvironment
