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
bool HandlePerfGenerateMemoryReport(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfGetStats(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfStartProfiling(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfStopProfiling(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfShowFps(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfSetScalability(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfSetResolutionScale(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfSetVsync(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfSetFrameRateLimit(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfEnableGpuTiming(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfConfigureNanite(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfConfigureLod(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfConfigureTextureStreaming(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfApplyBaselineSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfOptimizeDrawCalls(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfConfigureOcclusionCulling(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfOptimizeShaders(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfConfigureWorldPartition(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfMergeActors(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandlePerfRunBenchmark(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::SystemControl
