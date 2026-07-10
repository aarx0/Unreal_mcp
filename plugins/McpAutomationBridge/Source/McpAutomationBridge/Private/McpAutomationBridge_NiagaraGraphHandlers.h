#pragma once

// manage_effect (Niagara graph actions) dispatcher as a namespaced free
// function. De-membered off UMcpAutomationBridgeSubsystem (F1 module split); the
// subsystem is passed by reference as the first parameter (S), and the handler
// touches instance state only through its public response API
// (S.SendAutomationResponse / S.SendAutomationError). Action carries the
// manage_niagara_graph gate literal; the handler dispatches on subAction
// (add_module/connect_pins/remove_node/set_parameter). Same McpHandlers::Effect
// family as the effect + Niagara-authoring handlers. Dispatch:
// MCP/Calls/McpCalls_ManageEffect.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Effect
{
bool HandleNiagaraGraphAction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& Action,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);

} // namespace McpHandlers::Effect
