#pragma once

// manage_effect (core effect actions) handlers as namespaced free functions.
// De-membered off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem
// is passed by reference as the first parameter (S), and handlers touch instance
// state only through its public response API (S.SendAutomationResponse /
// S.SendAutomationError). CreateNiagaraEffect is the shared spawn helper for the
// create_* effect actions. Dispatch: MCP/Calls/McpCalls_ManageEffect.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Effect
{
bool HandleEffectListDebugShapes(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectClearDebugShapes(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectDebugShape(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectParticle(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectSetNiagaraParameter(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectActivateNiagara(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectDeactivateNiagara(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectAdvanceSimulation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectCreateDynamicLight(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectCleanup(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectSpawnNiagara(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectCreateVolumetricFog(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectCreateParticleTrail(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectCreateEnvironmentEffect(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectCreateImpactEffect(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleEffectCreateNiagaraRibbon(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

// Shared spawn helper for the create_* Niagara-effect actions.
bool CreateNiagaraEffect(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket,
                                 const FString& EffectName, const FString& DefaultSystemPath);

} // namespace McpHandlers::Effect
