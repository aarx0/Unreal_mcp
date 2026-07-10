#pragma once

// manage_blueprint WidgetAuthoring handlers as namespaced free functions.
// De-membered off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem
// is passed by reference as the first parameter (S). Dispatch:
// MCP/Calls/McpCalls_ManageBlueprint.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Blueprint
{
bool HandleWidgetAuthoringCreateWidgetBlueprint(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringShowWidget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetWidgetParentClass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringPreviewWidget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringRemoveWidget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringRenameWidget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringReplaceWidgetClass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddCanvasPanel(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddHorizontalBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddVerticalBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddOverlay(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddGridPanel(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddUniformGrid(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddWrapBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddScrollBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddSizeBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddScaleBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddBorder(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddSafeZone(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddSpacer(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddWidgetSwitcher(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddTextBlock(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddImage(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddButton(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddProgressBar(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddSlider(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddRichTextBlock(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddCheckBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddTextInput(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddComboBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddSpinBox(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddListView(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddTreeView(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetAnchor(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetAlignment(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetPosition(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetSize(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetPadding(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetZOrder(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetRenderTransform(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetVisibility(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetStyle(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetClipping(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetFont(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetMargin(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringBindText(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringBindVisibility(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringBindColor(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringBindEnabled(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringBindOnClicked(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringBindOnHovered(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringBindOnValueChanged(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringBindEventToDelegate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreatePropertyBinding(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetWidgetBinding(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringBindLocalizedText(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreateWidgetAnimation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddAnimationTrack(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddAnimationKeyframe(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetAnimationLoop(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetAnimationSpeed(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringGetAnimationInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringDeleteAnimation(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreateWidgetStyle(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringApplyStyleToWidget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddWidgetComponent(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringReparentWidget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringWrapRoot(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddWidget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringGetWidgetSlotInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreateMainMenu(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreatePauseMenu(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreateHudWidget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddHealthBar(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddCrosshair(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddAmmoCounter(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreateSettingsMenu(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreateLoadingScreen(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddMinimap(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddCompass(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddInteractionPrompt(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddObjectiveTracker(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddDamageIndicator(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreateInventoryUi(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreateDialogWidget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreateRadialMenu(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreateCreditsScreen(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringCreateShopUi(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringAddQuestTracker(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringGetWidgetInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);
bool HandleWidgetAuthoringSetLocalizationKey(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket);

} // namespace McpHandlers::Blueprint
