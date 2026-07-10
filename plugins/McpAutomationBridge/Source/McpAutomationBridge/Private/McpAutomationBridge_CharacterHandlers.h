#pragma once

// manage_character handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError
// / S.ResolveBlueprintOrError). Dispatch: MCP/Calls/McpCalls_ManageCharacter.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Character
{
bool HandleCharacterCreateBlueprint(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterConfigureCapsuleComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterConfigureMeshComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterConfigureCameraComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterConfigureMovementSpeeds(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterConfigureJump(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterConfigureRotation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterAddCustomMovementMode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterConfigureNavMovement(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterMapSurfaceToSound(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterGetInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterSetupMovement(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterSetWalkSpeed(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterSetJumpHeight(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterSetGravityScale(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterSetGroundFriction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterSetBrakingDeceleration(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleCharacterConfigureCrouch(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Character
