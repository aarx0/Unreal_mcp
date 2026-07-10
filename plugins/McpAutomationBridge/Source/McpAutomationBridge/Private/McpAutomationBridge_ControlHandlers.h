#pragma once

// control_actor + control_editor handlers as namespaced free functions. De-membered off
// UMcpAutomationBridgeSubsystem (F1 module split); the subsystem is passed by
// reference as the first parameter (S), and handlers touch instance state only
// through its public response API (S.SendAutomationResponse / S.SendAutomationError).
// Dispatch: MCP/Calls/McpCalls_*.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::ControlActor
{
bool HandleControlActorSpawn(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorSpawnBlueprint(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorDelete(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorApplyForce(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorSetTransform(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorGetTransform(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorSetVisibility(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorAddComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorSetComponentProperties(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorGetComponents(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorDuplicate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorAttach(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorDetach(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorFindByTag(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorAddTag(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorFindByName(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorDeleteByTag(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorSetBlueprintVariables(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorCreateSnapshot(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorRestoreSnapshot(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorExport(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorGetBoundingBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorGetMetadata(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorRemoveTag(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorFindByClass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorRemoveComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorGetComponentProperty(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorSetCollision(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorCallFunction(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlActorList(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::ControlActor

namespace McpHandlers::ControlEditor
{
bool HandleControlEditorPlay(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorStop(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorEject(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorPossess(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorFocusActor(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSetCamera(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSetViewMode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSetCameraFov(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSetGameSpeed(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorOpenAsset(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorScreenshot(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorPause(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorResume(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorStepFrame(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorStartRecording(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorStopRecording(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorCreateBookmark(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorJumpToBookmark(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSetPreferences(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSetViewportRealtime(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSimulateInput(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorCloseAsset(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSaveAll(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorUndo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorRedo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSetEditorMode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorShowStats(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorHideStats(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSetGameView(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSetImmersiveMode(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorSetFixedDeltaTime(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleControlEditorOpenLevel(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::ControlEditor
