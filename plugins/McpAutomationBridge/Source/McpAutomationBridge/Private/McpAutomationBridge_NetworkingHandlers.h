#pragma once

// manage_networking core (replication/RPC/authority/relevancy/prediction/net-driver)
// handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// The GameFramework and Sessions chains share this ::Networking namespace (one
// namespace per manage_networking family). Dispatch: MCP/Calls/McpCalls_ManageNetworking.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Networking
{
bool HandleNetworkingSetPropertyReplicated(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingSetReplicationCondition(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingConfigureNetUpdateFrequency(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingConfigureNetPriority(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingSetNetDormancy(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingConfigureReplicationGraph(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingCreateRpcFunction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingConfigureRpcValidation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingSetRpcReliability(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingSetOwner(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingSetAutonomousProxy(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingCheckHasAuthority(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingCheckIsLocallyControlled(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingConfigureNetCullDistance(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingSetAlwaysRelevant(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingSetOnlyRelevantToOwner(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingSetReplicatedUsing(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingConfigurePushModel(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingConfigureClientPrediction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingConfigureServerCorrection(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingAddNetworkPredictionData(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingConfigureMovementPrediction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingConfigureNetDriver(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingSetNetRole(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingConfigureReplicatedMovement(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleNetworkingGetInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Networking
