#pragma once

// manage_blueprint Core (per-action) handlers as namespaced free functions.
// De-membered off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem
// is passed by reference as the first parameter (S). Dispatch:
// MCP/Calls/McpCalls_ManageBlueprint.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Blueprint
{
bool HandleBlueprintAddScsComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintRemoveScsComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintReparentScsComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintSetScsTransform(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintSetScsProperty(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintAddConstructionScript(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintCompile(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintCreateAction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintEnsureExists(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintProbeHandle(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintAddComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintGetScs(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintModifyScs(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintSetVariableMetadata(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintAddVariable(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintSetDefault(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintRemoveVariable(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintRenameVariable(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintAddEvent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintRemoveEvent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintRemoveFunction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintAddFunction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintGetDefault(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintListFunctions(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintGet(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintAddNode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleBlueprintSetMetadata(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Blueprint
