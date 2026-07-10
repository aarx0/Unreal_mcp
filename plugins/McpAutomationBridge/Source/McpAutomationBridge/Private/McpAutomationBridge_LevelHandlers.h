#pragma once

// manage_level handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// stream/unload share HandleLevelStreamInternal. Dispatch:
// MCP/Calls/McpCalls_ManageLevel.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Level
{
bool HandleLevelLoad(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelSave(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelSaveAs(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelCreate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelStreamInternal(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket, bool bForceUnload);
bool HandleLevelStream(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelUnload(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelBuildLighting(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelSetMetadata(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelValidate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelList(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelGetCurrent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelExport(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelImport(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelAddSublevel(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelDelete(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelRename(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelDuplicate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleLevelGetSummary(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Level
