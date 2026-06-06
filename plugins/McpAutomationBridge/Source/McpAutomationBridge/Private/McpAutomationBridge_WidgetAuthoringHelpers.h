// =============================================================================
// McpAutomationBridge_WidgetAuthoringHelpers.h
// =============================================================================
// Shared declarations for the WidgetAuthoringHelpers namespace.
//
// The DEFINITIONS live in McpAutomationBridge_WidgetAuthoringHandlers.cpp (they
// already have external linkage — a named namespace with no `static`). This header
// only exposes their declarations so other translation units in this module — e.g.
// McpAutomationBridge_CommonUIHandlers.cpp — can reuse the widget load / GUID /
// tree-insertion helpers instead of forking a separate widget-create path.
//
// Note: defaults (e.g. GetColorFromJsonWidget's Default param) intentionally live
// only on the definition, not here, to avoid "redefinition of default argument"
// in the TU that owns the definition. Header-only consumers pass explicit args.
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h" // ESlateVisibility (returned by value)

class UWidgetBlueprint;
class UWidget;
class UWidgetAnimation;
class FJsonObject;
class FJsonValue;

namespace WidgetAuthoringHelpers
{
	/** Parse an RGBA color from a JSON object (missing channels fall back to Default). */
	FLinearColor GetColorFromJsonWidget(const TSharedPtr<FJsonObject>& ColorObj, const FLinearColor& Default);

	/** Optional object-field access (returns null when missing or wrong type). */
	TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);

	/** Optional array-field access (returns null when missing or wrong type). */
	const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);

	/** Resolve the target slot/widget name from a payload ("slotName" preferred, "widgetName" legacy). */
	FString GetSlotName(const TSharedPtr<FJsonObject>& Payload);

	/** Robust Widget Blueprint lookup (in-memory + on-disk, multiple fallbacks). */
	UWidgetBlueprint* LoadWidgetBlueprint(const FString& WidgetPath);

	/** Convert a visibility string to ESlateVisibility (defaults to Visible). */
	ESlateVisibility GetVisibility(const FString& VisibilityStr);

	/** Register a widget in the WidgetVariableNameToGuidMap (required before compile). */
	void RegisterWidgetGuid(UWidgetBlueprint* WidgetBP, UWidget* Widget);

	/** Remove a widget's GUID-map entry (required when removing a widget from the tree). */
	void UnregisterWidgetGuid(UWidgetBlueprint* WidgetBP, UWidget* Widget);

	/** Add a widget to the tree with safe root/parent handling and GUID cleanup. */
	bool SafeAddWidgetToTree(UWidgetBlueprint* WidgetBP, UWidget* NewWidget, const FString& ParentSlot);

	/** Register an animation in the GUID map and the Animations array. */
	void RegisterAnimationGuid(UWidgetBlueprint* WidgetBP, UWidgetAnimation* Animation);

	/** Register every widget/animation in the tree that lacks a GUID. */
	void RegisterAllWidgetGuids(UWidgetBlueprint* WidgetBP);

	/** Clear the whole widget tree and reset the GUID map for a fresh rebuild. */
	void ClearWidgetTreeForRebuild(UWidgetBlueprint* WidgetBP);
}
