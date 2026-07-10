#pragma once

// manage_effect (Niagara authoring actions) handlers as namespaced free
// functions. De-membered off UMcpAutomationBridgeSubsystem (F1 module split);
// the subsystem is passed by reference as the first parameter (S), and handlers
// touch instance state only through its public response API
// (S.SendAutomationResponse / S.SendAutomationError). HandleNiagaraAddDataInterface
// serves the three spline/audio-spectrum/collision-query adders via its SubAction
// arg. Same McpHandlers::Effect family as the effect + graph handlers. Dispatch:
// MCP/Calls/McpCalls_ManageEffect.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Effect
{
bool HandleNiagaraCreateNiagaraSystem(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraCreateNiagaraEmitter(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddEmitterToSystem(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraSetEmitterProperties(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddSpawnRateModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddSpawnBurstModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddSpawnPerUnitModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddInitializeParticleModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddParticleStateModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddForceModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddVelocityModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddAccelerationModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddSizeModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddColorModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddSpriteRendererModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddMeshRendererModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddRibbonRendererModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddLightRendererModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddCollisionModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddKillParticlesModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddCameraOffsetModule(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddUserParameter(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraSetParameterValue(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraBindParameterToSource(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddSkeletalMeshDataInterface(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddStaticMeshDataInterface(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddDataInterface(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const FString& SubAction,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddEventGenerator(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddEventReceiver(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraEnableGpuSimulation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraAddSimulationStage(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraGetNiagaraInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleNiagaraValidateNiagaraSystem(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);

} // namespace McpHandlers::Effect
