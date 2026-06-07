// =============================================================================
// McpAutomationBridge_CommonUIHandlers.cpp
// =============================================================================
// Common UI (Epic's CommonUI plugin) widget-authoring handlers.
//
// This translation unit is intentionally self-contained and deletable: it owns
// every `#include "Common*.h"` behind the MCP_HAS_COMMON_UI compile guard, and is
// routed via HandleCommonUiAction from the manage_blueprint dispatch lambda
// (McpAutomationBridgeSubsystem.cpp) BEFORE the widget-authoring branch, so these
// actions never enter the large WidgetAuthoringHandlers translation unit.
//
// Why typed handlers (and not the generic reflection path):
//   - UCommonButtonBase is UCLASS(Abstract) and CommonUI ships no concrete button,
//     so a button must be constructed from a concrete Blueprint subclass passed by
//     the caller (e.g. /Game/UI/WBP_MenuButton.WBP_MenuButton_C) via the runtime
//     ConstructWidget(UClass*) overload — the compile-time ConstructWidget<T>()
//     template cannot express a Blueprint-generated class.
//   - Styles (UCommonButtonStyle/UCommonTextStyle/UCommonBorderStyle) are referenced
//     by CLASS (TSubclassOf) and must be assigned through the widget's typed
//     SetStyle() so the slate style rebuild fires; a raw property write would not.
//
// When MCP_HAS_COMMON_UI == 0 (CommonUI not present at build time) the handler still
// consumes the action and returns a clean COMMON_UI_DISABLED error.
// =============================================================================

#include "McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"               // GetJsonStringField
#include "McpAutomationBridge_WidgetAuthoringHelpers.h" // LoadWidgetBlueprint, Register/Unregister, SafeAddWidgetToTree
#include "MCP/McpConsolidatedActionRouting.h"          // GetPayloadSubAction

#include "Dom/JsonObject.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/BlueprintEditorUtils.h"

#if MCP_HAS_COMMON_UI
#include "CommonButtonBase.h" // UCommonButtonBase, UCommonButtonStyle
#include "CommonTextBlock.h"  // UCommonTextBlock (concrete), UCommonTextStyle
#include "CommonBorder.h"     // UCommonBorder (concrete), UCommonBorderStyle
#include "CommonUITypes.h"    // FCommonInputActionDataBase (input-action row struct)
#include "Engine/DataTable.h" // UDataTable, FDataTableRowHandle
#endif

using namespace WidgetAuthoringHelpers;

#if MCP_HAS_COMMON_UI
namespace
{
	/**
	 * Resolve a class from a short name or a fully-qualified path. Used for both
	 * widget classes and style classes, so it does NOT constrain to UWidget — the
	 * caller validates IsChildOf for the type it expects. Uses StaticLoadClass to
	 * force a load (so a not-yet-loaded delay-loaded CommonUI class resolves), with
	 * LOAD_NoWarn|LOAD_Quiet to keep probe misses out of the error-capture path.
	 */
	UClass* ResolveClassFromString(const FString& NameOrPath)
	{
		if (NameOrPath.IsEmpty())
		{
			return nullptr;
		}

		// Fully-qualified path: /Script/Module.Name or /Game/Path.Name_C
		if (NameOrPath.Contains(TEXT(".")) || NameOrPath.Contains(TEXT("/")))
		{
			if (UClass* Loaded = StaticLoadClass(UObject::StaticClass(), nullptr, *NameOrPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Loaded;
			}
		}

		// Already-loaded class by short name.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		if (UClass* Found = FindFirstObject<UClass>(*NameOrPath, EFindFirstObjectOptions::None))
		{
			return Found;
		}
#endif

		// Probe the likely script modules for a bare class name.
		static const TCHAR* ScriptModules[] = { TEXT("CommonUI"), TEXT("UMG"), TEXT("Engine") };
		for (const TCHAR* ModuleName : ScriptModules)
		{
			const FString ScriptPath = FString::Printf(TEXT("/Script/%s.%s"), ModuleName, *NameOrPath);
			if (UClass* Loaded = StaticLoadClass(UObject::StaticClass(), nullptr, *ScriptPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Loaded;
			}
		}

		return nullptr;
	}
}
#endif // MCP_HAS_COMMON_UI

bool UMcpAutomationBridgeSubsystem::HandleCommonUiAction(
	const FString& RequestId, const FString& Action,
	const TSharedPtr<FJsonObject>& Payload,
	TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if !MCP_HAS_COMMON_UI
	(void)Action;
	(void)Payload;
	SendAutomationError(RequestingSocket, RequestId,
		TEXT("Common UI support was not compiled into this build (the CommonUI plugin/module was not found at build time)."),
		TEXT("COMMON_UI_DISABLED"));
	return true;
#else
	const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(Payload);

	// -------------------------------------------------------------------------
	// add_common_button — construct a concrete UCommonButtonBase subclass
	// -------------------------------------------------------------------------
	if (SubAction.Equals(TEXT("add_common_button"), ESearchCase::IgnoreCase))
	{
		const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
		const FString ButtonClassPath = GetJsonStringField(Payload, TEXT("buttonClass"));
		if (WidgetPath.IsEmpty() || ButtonClassPath.IsEmpty())
		{
			SendAutomationError(RequestingSocket, RequestId,
				TEXT("add_common_button requires 'widgetPath' and 'buttonClass' (a concrete UCommonButtonBase subclass, e.g. /Game/UI/WBP_MenuButton.WBP_MenuButton_C)."),
				TEXT("MISSING_PARAMETER"));
			return true;
		}

		const FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("CommonButton"));
		const FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));

		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
			return true;
		}

		UClass* ButtonClass = ResolveClassFromString(ButtonClassPath);
		if (!ButtonClass || !ButtonClass->IsChildOf(UCommonButtonBase::StaticClass()))
		{
			SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("Could not resolve buttonClass '%s' to a UCommonButtonBase subclass."), *ButtonClassPath),
				TEXT("INVALID_CLASS"));
			return true;
		}
		if (ButtonClass->HasAnyClassFlags(CLASS_Abstract))
		{
			SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("buttonClass '%s' is abstract; pass a concrete Blueprint subclass (UCommonButtonBase itself cannot be instantiated)."), *ButtonClassPath),
				TEXT("ABSTRACT_CLASS"));
			return true;
		}

		// Runtime class (resolved from a Blueprint path) → explicit <UWidget> template arg;
		// WidgetT cannot be deduced from a UClass*. ConstructWidget instantiates ButtonClass.
		UCommonButtonBase* Button = Cast<UCommonButtonBase>(WidgetBP->WidgetTree->ConstructWidget<UWidget>(ButtonClass, FName(*SlotName)));
		if (!Button)
		{
			SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct Common UI button"), TEXT("CREATION_ERROR"));
			return true;
		}
		RegisterWidgetGuid(WidgetBP, Button);

		// Optional style assignment (typed SetStyle so the slate rebuild fires).
		FString StyleWarning;
		const FString StyleClassPath = GetJsonStringField(Payload, TEXT("styleClass"));
		if (!StyleClassPath.IsEmpty())
		{
			UClass* StyleClass = ResolveClassFromString(StyleClassPath);
			if (StyleClass && StyleClass->IsChildOf(UCommonButtonStyle::StaticClass()))
			{
				Button->SetStyle(TSubclassOf<UCommonButtonStyle>(StyleClass));
			}
			else
			{
				StyleWarning = FString::Printf(TEXT("styleClass '%s' did not resolve to a UCommonButtonStyle subclass; button created without style."), *StyleClassPath);
			}
		}

		FString AddErr;
		if (!SafeAddWidgetToTree(WidgetBP, Button, ParentSlot, &AddErr))
		{
			SendAutomationError(RequestingSocket, RequestId, AddErr, TEXT("TREE_ERROR"));
			return true;
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("slotName"), SlotName);
		ResultJson->SetStringField(TEXT("buttonClass"), ButtonClass->GetPathName());
		if (!StyleWarning.IsEmpty())
		{
			ResultJson->SetStringField(TEXT("warning"), StyleWarning);
		}
		McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
		SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added Common UI button"), ResultJson);
		return true;
	}

	// -------------------------------------------------------------------------
	// add_common_text — construct a (concrete) UCommonTextBlock
	// -------------------------------------------------------------------------
	if (SubAction.Equals(TEXT("add_common_text"), ESearchCase::IgnoreCase))
	{
		const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
		if (WidgetPath.IsEmpty())
		{
			SendAutomationError(RequestingSocket, RequestId, TEXT("add_common_text requires 'widgetPath'"), TEXT("MISSING_PARAMETER"));
			return true;
		}

		const FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("CommonText"));
		const FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));

		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
			return true;
		}

		UCommonTextBlock* TextWidget = WidgetBP->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), FName(*SlotName));
		if (!TextWidget)
		{
			SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct Common UI text block"), TEXT("CREATION_ERROR"));
			return true;
		}
		RegisterWidgetGuid(WidgetBP, TextWidget);

		const FString TextStr = GetJsonStringField(Payload, TEXT("text"));
		if (!TextStr.IsEmpty())
		{
			TextWidget->SetText(FText::FromString(TextStr)); // inherited from UTextBlock
		}

		FString StyleWarning;
		const FString StyleClassPath = GetJsonStringField(Payload, TEXT("styleClass"));
		if (!StyleClassPath.IsEmpty())
		{
			UClass* StyleClass = ResolveClassFromString(StyleClassPath);
			if (StyleClass && StyleClass->IsChildOf(UCommonTextStyle::StaticClass()))
			{
				TextWidget->SetStyle(TSubclassOf<UCommonTextStyle>(StyleClass));
			}
			else
			{
				StyleWarning = FString::Printf(TEXT("styleClass '%s' did not resolve to a UCommonTextStyle subclass; text created without style."), *StyleClassPath);
			}
		}

		FString AddErr;
		if (!SafeAddWidgetToTree(WidgetBP, TextWidget, ParentSlot, &AddErr))
		{
			SendAutomationError(RequestingSocket, RequestId, AddErr, TEXT("TREE_ERROR"));
			return true;
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("slotName"), SlotName);
		if (!StyleWarning.IsEmpty())
		{
			ResultJson->SetStringField(TEXT("warning"), StyleWarning);
		}
		McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
		SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added Common UI text"), ResultJson);
		return true;
	}

	// -------------------------------------------------------------------------
	// add_common_border — construct a (concrete) UCommonBorder
	// -------------------------------------------------------------------------
	if (SubAction.Equals(TEXT("add_common_border"), ESearchCase::IgnoreCase))
	{
		const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
		if (WidgetPath.IsEmpty())
		{
			SendAutomationError(RequestingSocket, RequestId, TEXT("add_common_border requires 'widgetPath'"), TEXT("MISSING_PARAMETER"));
			return true;
		}

		const FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("CommonBorder"));
		const FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));

		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
			return true;
		}

		UCommonBorder* BorderWidget = WidgetBP->WidgetTree->ConstructWidget<UCommonBorder>(UCommonBorder::StaticClass(), FName(*SlotName));
		if (!BorderWidget)
		{
			SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct Common UI border"), TEXT("CREATION_ERROR"));
			return true;
		}
		RegisterWidgetGuid(WidgetBP, BorderWidget);

		FString StyleWarning;
		const FString StyleClassPath = GetJsonStringField(Payload, TEXT("styleClass"));
		if (!StyleClassPath.IsEmpty())
		{
			UClass* StyleClass = ResolveClassFromString(StyleClassPath);
			if (StyleClass && StyleClass->IsChildOf(UCommonBorderStyle::StaticClass()))
			{
				BorderWidget->SetStyle(TSubclassOf<UCommonBorderStyle>(StyleClass));
			}
			else
			{
				StyleWarning = FString::Printf(TEXT("styleClass '%s' did not resolve to a UCommonBorderStyle subclass; border created without style."), *StyleClassPath);
			}
		}

		FString AddErr;
		if (!SafeAddWidgetToTree(WidgetBP, BorderWidget, ParentSlot, &AddErr))
		{
			SendAutomationError(RequestingSocket, RequestId, AddErr, TEXT("TREE_ERROR"));
			return true;
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("slotName"), SlotName);
		if (!StyleWarning.IsEmpty())
		{
			ResultJson->SetStringField(TEXT("warning"), StyleWarning);
		}
		McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
		SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added Common UI border"), ResultJson);
		return true;
	}

	// -------------------------------------------------------------------------
	// set_common_button_style / set_common_text_style — assign a style class to an
	// existing widget in the tree via its typed SetStyle (fires slate rebuild).
	// -------------------------------------------------------------------------
	const bool bSetButtonStyle = SubAction.Equals(TEXT("set_common_button_style"), ESearchCase::IgnoreCase);
	const bool bSetTextStyle = SubAction.Equals(TEXT("set_common_text_style"), ESearchCase::IgnoreCase);
	if (bSetButtonStyle || bSetTextStyle)
	{
		const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
		const FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
		const FString StyleClassPath = GetJsonStringField(Payload, TEXT("styleClass"));
		if (WidgetPath.IsEmpty() || WidgetName.IsEmpty() || StyleClassPath.IsEmpty())
		{
			SendAutomationError(RequestingSocket, RequestId,
				TEXT("This action requires 'widgetPath', 'widgetName', and 'styleClass'."),
				TEXT("MISSING_PARAMETER"));
			return true;
		}

		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
			return true;
		}

		UWidget* Target = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
		if (!Target)
		{
			SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("Widget '%s' not found in %s"), *WidgetName, *WidgetPath), TEXT("WIDGET_NOT_FOUND"));
			return true;
		}

		UClass* StyleClass = ResolveClassFromString(StyleClassPath);

		if (bSetButtonStyle)
		{
			UCommonButtonBase* Button = Cast<UCommonButtonBase>(Target);
			if (!Button)
			{
				SendAutomationError(RequestingSocket, RequestId,
					FString::Printf(TEXT("Widget '%s' is not a UCommonButtonBase"), *WidgetName), TEXT("WRONG_TYPE"));
				return true;
			}
			if (!StyleClass || !StyleClass->IsChildOf(UCommonButtonStyle::StaticClass()))
			{
				SendAutomationError(RequestingSocket, RequestId,
					FString::Printf(TEXT("styleClass '%s' is not a UCommonButtonStyle subclass"), *StyleClassPath), TEXT("INVALID_CLASS"));
				return true;
			}
			Button->SetStyle(TSubclassOf<UCommonButtonStyle>(StyleClass));
		}
		else // bSetTextStyle
		{
			UCommonTextBlock* TextWidget = Cast<UCommonTextBlock>(Target);
			if (!TextWidget)
			{
				SendAutomationError(RequestingSocket, RequestId,
					FString::Printf(TEXT("Widget '%s' is not a UCommonTextBlock"), *WidgetName), TEXT("WRONG_TYPE"));
				return true;
			}
			if (!StyleClass || !StyleClass->IsChildOf(UCommonTextStyle::StaticClass()))
			{
				SendAutomationError(RequestingSocket, RequestId,
					FString::Printf(TEXT("styleClass '%s' is not a UCommonTextStyle subclass"), *StyleClassPath), TEXT("INVALID_CLASS"));
				return true;
			}
			TextWidget->SetStyle(TSubclassOf<UCommonTextStyle>(StyleClass));
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
		ResultJson->SetStringField(TEXT("styleClass"), StyleClass->GetPathName());
		McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
		SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set Common UI style"), ResultJson);
		return true;
	}

	// -------------------------------------------------------------------------
	// set_common_button_input_action — bind a UCommonButtonBase's TriggeringInput-
	// Action (an FDataTableRowHandle into a CommonInputActionDataBase table).
	// -------------------------------------------------------------------------
	if (SubAction.Equals(TEXT("set_common_button_input_action"), ESearchCase::IgnoreCase))
	{
		const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
		const FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
		const FString DataTablePath = GetJsonStringField(Payload, TEXT("dataTable"));
		const FString RowName = GetJsonStringField(Payload, TEXT("rowName"));
		if (WidgetPath.IsEmpty() || WidgetName.IsEmpty() || DataTablePath.IsEmpty() || RowName.IsEmpty())
		{
			SendAutomationError(RequestingSocket, RequestId,
				TEXT("This action requires 'widgetPath', 'widgetName', 'dataTable', and 'rowName'."),
				TEXT("MISSING_PARAMETER"));
			return true;
		}

		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
			return true;
		}

		UWidget* Target = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
		if (!Target)
		{
			SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("Widget '%s' not found in %s"), *WidgetName, *WidgetPath), TEXT("WIDGET_NOT_FOUND"));
			return true;
		}

		UCommonButtonBase* Button = Cast<UCommonButtonBase>(Target);
		if (!Button)
		{
			SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("Widget '%s' is not a UCommonButtonBase"), *WidgetName), TEXT("WRONG_TYPE"));
			return true;
		}

		UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *DataTablePath);
		if (!DataTable)
		{
			SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("DataTable not found at '%s'"), *DataTablePath), TEXT("NOT_FOUND"));
			return true;
		}
		if (!DataTable->RowStruct || !DataTable->RowStruct->IsChildOf(FCommonInputActionDataBase::StaticStruct()))
		{
			SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("DataTable '%s' row struct must derive from CommonInputActionDataBase"), *DataTablePath),
				TEXT("INVALID_ROW_STRUCT"));
			return true;
		}

		// TriggeringInputAction is protected on UCommonButtonBase. Write the handle
		// directly via reflection — this mirrors what the details panel persists and
		// deliberately avoids UCommonButtonBase::SetTriggeringInputAction(), whose
		// runtime input binding (BindTriggeringInputActionToClick) fires when
		// !IsDesignTime() — true for a programmatically-loaded WidgetTree template.
		FStructProperty* TrigProp = FindFProperty<FStructProperty>(
			UCommonButtonBase::StaticClass(), TEXT("TriggeringInputAction"));
		if (!TrigProp || TrigProp->Struct != FDataTableRowHandle::StaticStruct())
		{
			SendAutomationError(RequestingSocket, RequestId,
				TEXT("Could not resolve UCommonButtonBase.TriggeringInputAction property"), TEXT("INTERNAL_ERROR"));
			return true;
		}

		const FName RowFName(*RowName);
		// Non-blocking: a handle may be bound before its row is imported. Report it.
		const bool bRowFound = DataTable->GetRowMap().Contains(RowFName);

		FDataTableRowHandle* HandlePtr = TrigProp->ContainerPtrToValuePtr<FDataTableRowHandle>(Button);
		HandlePtr->DataTable = DataTable;
		HandlePtr->RowName = RowFName;

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
		ResultJson->SetStringField(TEXT("widgetObjectPath"), Target->GetPathName());
		ResultJson->SetStringField(TEXT("dataTable"), DataTable->GetPathName());
		ResultJson->SetStringField(TEXT("rowName"), RowName);
		ResultJson->SetBoolField(TEXT("rowFound"), bRowFound);
		McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
		SendAutomationResponse(RequestingSocket, RequestId, true,
			TEXT("Set Common UI button input action"), ResultJson);
		return true;
	}

	SendAutomationError(RequestingSocket, RequestId,
		FString::Printf(TEXT("Unknown Common UI action: %s"), *SubAction), TEXT("UNKNOWN_ACTION"));
	return true;
#endif // MCP_HAS_COMMON_UI
}
