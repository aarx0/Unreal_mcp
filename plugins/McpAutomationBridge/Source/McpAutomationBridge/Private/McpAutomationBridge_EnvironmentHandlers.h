#pragma once

// inspect global-read handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// Dispatch: MCP/Calls/McpCalls_*.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Inspect
{
bool HandleInspectFindObjects(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectGetProjectSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectGetEditorSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectGetWorldSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectGetViewportInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectGetSelectedActors(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectGetSceneStats(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectGetPerformanceStats(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectGetMemoryStats(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectRuntimeReport(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectListObjects(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectFindByClass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectFindByTag(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectClassInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleInspectObjectGeneric(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Inspect

// build_environment core handlers de-membered off UMcpAutomationBridgeSubsystem
// (F1 module split). The foliage/landscape wrappers and HandleBakeLightmap remain
// members (they forward to still-private Foliage/Landscape/EditorFunction members),
// so only the self-contained core actions are free functions here.
namespace McpHandlers::BuildEnvironment
{
bool HandleEnvironmentCreateProceduralTerrain(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEnvironmentGenerateLODs(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEnvironmentExportSnapshot(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEnvironmentImportSnapshot(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEnvironmentDelete(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEnvironmentCreateSkySphere(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEnvironmentSetTimeOfDay(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEnvironmentCreateFogVolume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCreateProceduralTerrain(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const FString& Action, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::BuildEnvironment
