// =============================================================================
// McpAutomationBridge_CommonUIHandlers.cpp
// =============================================================================
// Common UI (Epic's CommonUI plugin) widget-authoring handlers.
//
// This translation unit is intentionally self-contained and deletable: it owns
// every `#include "Common*.h"` behind the MCP_HAS_COMMON_UI compile guard. Each
// manage_common_ui sub-action is a per-action HandleCommonUi* member called
// directly by its classed action (MCP/Calls/McpCalls_ManageBlueprint.cpp).
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
#include "McpAutomationBridge_CommonUIHandlers.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"               // GetJsonStringField
#include "McpAutomationBridge_WidgetAuthoringHelpers.h" // LoadWidgetBlueprint, Register/Unregister, SafeAddWidgetToTree
#include "MCP/McpConsolidatedActionRouting.h"          // GetPayloadSubAction
#include "McpSafeOperations.h"                          // McpSafeAssetSave (persist widget edits)

#include "Dom/JsonObject.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h" // FKismetEditorUtilities::CreateBlueprint (style-asset creation)
#include "Engine/Blueprint.h"              // UBlueprint, BPTYPE_Normal
#include "Engine/BlueprintGeneratedClass.h"// UBlueprintGeneratedClass
#include "EditorAssetLibrary.h"            // UEditorAssetLibrary::DoesAssetExist
#include "UObject/Package.h"               // CreatePackage
#include "Misc/Paths.h"                    // FPaths

#if MCP_HAS_COMMON_UI
#include "CommonButtonBase.h" // UCommonButtonBase, UCommonButtonStyle
#include "CommonTextBlock.h"  // UCommonTextBlock (concrete), UCommonTextStyle
#include "CommonBorder.h"     // UCommonBorder (concrete), UCommonBorderStyle
#include "CommonUITypes.h"    // FCommonInputActionDataBase (input-action row struct)
#include "Engine/DataTable.h" // UDataTable, FDataTableRowHandle
#endif

using namespace WidgetAuthoringHelpers;
using namespace McpSafeOperations; // McpSafeAssetSave (persist Common UI widget edits)

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

// =============================================================================
// Per-action manage_common_ui members
// =============================================================================
// Hoisted verbatim from the retired HandleCommonUiAction dispatcher, one member
// per classed action (MCP/Calls/McpCalls_ManageBlueprint.cpp), each called
// directly. The disabled-build reply (COMMON_UI_DISABLED) is replicated per
// member via the same #if !MCP_HAS_COMMON_UI gate the dispatcher carried. Two
// dispatcher branches each served two sub-actions (button/text style setters;
// button/text style creators); each sub-action gets its own member holding that
// branch body intact, so the payload sub-action still steers it.
#if MCP_HAS_COMMON_UI
// The style setter/creator members re-derive the payload sub-action the
// dispatcher read at its top (the shared branch bodies switch on it).
#define MCP_COMMONUI_SUBACTION() \
	const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(Payload);
#endif

bool McpHandlers::Blueprint::HandleCommonUiAddCommonButton(
	UMcpAutomationBridgeSubsystem& S,
	const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
	FMcpResponseHandle RequestingSocket)
{
#if !MCP_HAS_COMMON_UI
	(void)Payload;
	S.SendAutomationError(RequestingSocket, RequestId,
		TEXT("Common UI support was not compiled into this build (the CommonUI plugin/module was not found at build time)."),
		TEXT("COMMON_UI_DISABLED"));
	return true;
#else
		const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
		const FString ButtonClassPath = GetJsonStringField(Payload, TEXT("buttonClass"));
		if (WidgetPath.IsEmpty() || ButtonClassPath.IsEmpty())
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				TEXT("add_common_button requires 'widgetPath' and 'buttonClass' (a concrete UCommonButtonBase subclass, e.g. /Game/UI/WBP_MenuButton.WBP_MenuButton_C)."),
				TEXT("MISSING_PARAMETER"));
			return true;
		}

		// slotName is canonical; name accepted as an alias (matches add_widget).
		FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
		if (SlotName.IsEmpty()) { SlotName = GetJsonStringField(Payload, TEXT("name"), TEXT("CommonButton")); }
		const FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));

		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
			return true;
		}

		UClass* ButtonClass = ResolveClassFromString(ButtonClassPath);
		if (!ButtonClass || !ButtonClass->IsChildOf(UCommonButtonBase::StaticClass()))
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("Could not resolve buttonClass '%s' to a UCommonButtonBase subclass."), *ButtonClassPath),
				TEXT("INVALID_CLASS"));
			return true;
		}
		if (ButtonClass->HasAnyClassFlags(CLASS_Abstract))
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("buttonClass '%s' is abstract; pass a concrete Blueprint subclass (UCommonButtonBase itself cannot be instantiated)."), *ButtonClassPath),
				TEXT("ABSTRACT_CLASS"));
			return true;
		}

		// Runtime class (resolved from a Blueprint path) → explicit <UWidget> template arg;
		// WidgetT cannot be deduced from a UClass*. ConstructWidget instantiates ButtonClass.
		UCommonButtonBase* Button = Cast<UCommonButtonBase>(WidgetBP->WidgetTree->ConstructWidget<UWidget>(ButtonClass, FName(*SlotName)));
		if (!Button)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct Common UI button"), TEXT("CREATION_ERROR"));
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
			S.SendAutomationError(RequestingSocket, RequestId, AddErr, TEXT("TREE_ERROR"));
			return true;
		}
		McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("slotName"), SlotName);
		ResultJson->SetStringField(TEXT("buttonClass"), ButtonClass->GetPathName());
		if (!StyleWarning.IsEmpty())
		{
			ResultJson->SetStringField(TEXT("warning"), StyleWarning);
		}
		McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
		S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added Common UI button"), ResultJson);
		return true;
#endif // MCP_HAS_COMMON_UI
}

bool McpHandlers::Blueprint::HandleCommonUiAddCommonText(
	UMcpAutomationBridgeSubsystem& S,
	const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
	FMcpResponseHandle RequestingSocket)
{
#if !MCP_HAS_COMMON_UI
	(void)Payload;
	S.SendAutomationError(RequestingSocket, RequestId,
		TEXT("Common UI support was not compiled into this build (the CommonUI plugin/module was not found at build time)."),
		TEXT("COMMON_UI_DISABLED"));
	return true;
#else
		const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
		if (WidgetPath.IsEmpty())
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("add_common_text requires 'widgetPath'"), TEXT("MISSING_PARAMETER"));
			return true;
		}

		// slotName is canonical; name accepted as an alias (matches add_widget).
		FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
		if (SlotName.IsEmpty()) { SlotName = GetJsonStringField(Payload, TEXT("name"), TEXT("CommonText")); }
		const FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));

		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
			return true;
		}

		UCommonTextBlock* TextWidget = WidgetBP->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), FName(*SlotName));
		if (!TextWidget)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct Common UI text block"), TEXT("CREATION_ERROR"));
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
			S.SendAutomationError(RequestingSocket, RequestId, AddErr, TEXT("TREE_ERROR"));
			return true;
		}
		McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("slotName"), SlotName);
		if (!StyleWarning.IsEmpty())
		{
			ResultJson->SetStringField(TEXT("warning"), StyleWarning);
		}
		McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
		S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added Common UI text"), ResultJson);
		return true;
#endif // MCP_HAS_COMMON_UI
}

bool McpHandlers::Blueprint::HandleCommonUiAddCommonBorder(
	UMcpAutomationBridgeSubsystem& S,
	const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
	FMcpResponseHandle RequestingSocket)
{
#if !MCP_HAS_COMMON_UI
	(void)Payload;
	S.SendAutomationError(RequestingSocket, RequestId,
		TEXT("Common UI support was not compiled into this build (the CommonUI plugin/module was not found at build time)."),
		TEXT("COMMON_UI_DISABLED"));
	return true;
#else
		const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
		if (WidgetPath.IsEmpty())
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("add_common_border requires 'widgetPath'"), TEXT("MISSING_PARAMETER"));
			return true;
		}

		// slotName is canonical; name accepted as an alias (matches add_widget).
		FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
		if (SlotName.IsEmpty()) { SlotName = GetJsonStringField(Payload, TEXT("name"), TEXT("CommonBorder")); }
		const FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));

		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
			return true;
		}

		UCommonBorder* BorderWidget = WidgetBP->WidgetTree->ConstructWidget<UCommonBorder>(UCommonBorder::StaticClass(), FName(*SlotName));
		if (!BorderWidget)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct Common UI border"), TEXT("CREATION_ERROR"));
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
			S.SendAutomationError(RequestingSocket, RequestId, AddErr, TEXT("TREE_ERROR"));
			return true;
		}
		McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("slotName"), SlotName);
		if (!StyleWarning.IsEmpty())
		{
			ResultJson->SetStringField(TEXT("warning"), StyleWarning);
		}
		McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
		S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added Common UI border"), ResultJson);
		return true;
#endif // MCP_HAS_COMMON_UI
}

bool McpHandlers::Blueprint::HandleCommonUiSetCommonButtonStyle(
	UMcpAutomationBridgeSubsystem& S,
	const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
	FMcpResponseHandle RequestingSocket)
{
#if !MCP_HAS_COMMON_UI
	(void)Payload;
	S.SendAutomationError(RequestingSocket, RequestId,
		TEXT("Common UI support was not compiled into this build (the CommonUI plugin/module was not found at build time)."),
		TEXT("COMMON_UI_DISABLED"));
	return true;
#else
	MCP_COMMONUI_SUBACTION()
	const bool bSetButtonStyle = SubAction.Equals(TEXT("set_common_button_style"), ESearchCase::IgnoreCase);
	const bool bSetTextStyle = SubAction.Equals(TEXT("set_common_text_style"), ESearchCase::IgnoreCase);
		const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
		const FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
		const FString StyleClassPath = GetJsonStringField(Payload, TEXT("styleClass"));
		if (WidgetPath.IsEmpty() || WidgetName.IsEmpty() || StyleClassPath.IsEmpty())
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				TEXT("This action requires 'widgetPath', 'widgetName', and 'styleClass'."),
				TEXT("MISSING_PARAMETER"));
			return true;
		}

		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
			return true;
		}

		UWidget* Target = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
		if (!Target)
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("Widget '%s' not found in %s"), *WidgetName, *WidgetPath), TEXT("WIDGET_NOT_FOUND"));
			return true;
		}

		UClass* StyleClass = ResolveClassFromString(StyleClassPath);

		if (bSetButtonStyle)
		{
			UCommonButtonBase* Button = Cast<UCommonButtonBase>(Target);
			if (!Button)
			{
				S.SendAutomationError(RequestingSocket, RequestId,
					FString::Printf(TEXT("Widget '%s' is not a UCommonButtonBase"), *WidgetName), TEXT("WRONG_TYPE"));
				return true;
			}
			if (!StyleClass || !StyleClass->IsChildOf(UCommonButtonStyle::StaticClass()))
			{
				S.SendAutomationError(RequestingSocket, RequestId,
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
				S.SendAutomationError(RequestingSocket, RequestId,
					FString::Printf(TEXT("Widget '%s' is not a UCommonTextBlock"), *WidgetName), TEXT("WRONG_TYPE"));
				return true;
			}
			if (!StyleClass || !StyleClass->IsChildOf(UCommonTextStyle::StaticClass()))
			{
				S.SendAutomationError(RequestingSocket, RequestId,
					FString::Printf(TEXT("styleClass '%s' is not a UCommonTextStyle subclass"), *StyleClassPath), TEXT("INVALID_CLASS"));
				return true;
			}
			TextWidget->SetStyle(TSubclassOf<UCommonTextStyle>(StyleClass));
		}

		McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
		ResultJson->SetStringField(TEXT("styleClass"), StyleClass->GetPathName());
		McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
		S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set Common UI style"), ResultJson);
		return true;
#endif // MCP_HAS_COMMON_UI
}

bool McpHandlers::Blueprint::HandleCommonUiSetCommonTextStyle(
	UMcpAutomationBridgeSubsystem& S,
	const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
	FMcpResponseHandle RequestingSocket)
{
#if !MCP_HAS_COMMON_UI
	(void)Payload;
	S.SendAutomationError(RequestingSocket, RequestId,
		TEXT("Common UI support was not compiled into this build (the CommonUI plugin/module was not found at build time)."),
		TEXT("COMMON_UI_DISABLED"));
	return true;
#else
	MCP_COMMONUI_SUBACTION()
	const bool bSetButtonStyle = SubAction.Equals(TEXT("set_common_button_style"), ESearchCase::IgnoreCase);
	const bool bSetTextStyle = SubAction.Equals(TEXT("set_common_text_style"), ESearchCase::IgnoreCase);
		const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
		const FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
		const FString StyleClassPath = GetJsonStringField(Payload, TEXT("styleClass"));
		if (WidgetPath.IsEmpty() || WidgetName.IsEmpty() || StyleClassPath.IsEmpty())
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				TEXT("This action requires 'widgetPath', 'widgetName', and 'styleClass'."),
				TEXT("MISSING_PARAMETER"));
			return true;
		}

		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
			return true;
		}

		UWidget* Target = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
		if (!Target)
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("Widget '%s' not found in %s"), *WidgetName, *WidgetPath), TEXT("WIDGET_NOT_FOUND"));
			return true;
		}

		UClass* StyleClass = ResolveClassFromString(StyleClassPath);

		if (bSetButtonStyle)
		{
			UCommonButtonBase* Button = Cast<UCommonButtonBase>(Target);
			if (!Button)
			{
				S.SendAutomationError(RequestingSocket, RequestId,
					FString::Printf(TEXT("Widget '%s' is not a UCommonButtonBase"), *WidgetName), TEXT("WRONG_TYPE"));
				return true;
			}
			if (!StyleClass || !StyleClass->IsChildOf(UCommonButtonStyle::StaticClass()))
			{
				S.SendAutomationError(RequestingSocket, RequestId,
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
				S.SendAutomationError(RequestingSocket, RequestId,
					FString::Printf(TEXT("Widget '%s' is not a UCommonTextBlock"), *WidgetName), TEXT("WRONG_TYPE"));
				return true;
			}
			if (!StyleClass || !StyleClass->IsChildOf(UCommonTextStyle::StaticClass()))
			{
				S.SendAutomationError(RequestingSocket, RequestId,
					FString::Printf(TEXT("styleClass '%s' is not a UCommonTextStyle subclass"), *StyleClassPath), TEXT("INVALID_CLASS"));
				return true;
			}
			TextWidget->SetStyle(TSubclassOf<UCommonTextStyle>(StyleClass));
		}

		McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
		ResultJson->SetStringField(TEXT("styleClass"), StyleClass->GetPathName());
		McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
		S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set Common UI style"), ResultJson);
		return true;
#endif // MCP_HAS_COMMON_UI
}

bool McpHandlers::Blueprint::HandleCommonUiSetCommonButtonInputAction(
	UMcpAutomationBridgeSubsystem& S,
	const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
	FMcpResponseHandle RequestingSocket)
{
#if !MCP_HAS_COMMON_UI
	(void)Payload;
	S.SendAutomationError(RequestingSocket, RequestId,
		TEXT("Common UI support was not compiled into this build (the CommonUI plugin/module was not found at build time)."),
		TEXT("COMMON_UI_DISABLED"));
	return true;
#else
		const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
		const FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
		const FString DataTablePath = GetJsonStringField(Payload, TEXT("dataTable"));
		const FString RowName = GetJsonStringField(Payload, TEXT("rowName"));
		if (WidgetPath.IsEmpty() || WidgetName.IsEmpty() || DataTablePath.IsEmpty() || RowName.IsEmpty())
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				TEXT("This action requires 'widgetPath', 'widgetName', 'dataTable', and 'rowName'."),
				TEXT("MISSING_PARAMETER"));
			return true;
		}

		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (!WidgetBP || !WidgetBP->WidgetTree)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
			return true;
		}

		UWidget* Target = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
		if (!Target)
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("Widget '%s' not found in %s"), *WidgetName, *WidgetPath), TEXT("WIDGET_NOT_FOUND"));
			return true;
		}

		UCommonButtonBase* Button = Cast<UCommonButtonBase>(Target);
		if (!Button)
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("Widget '%s' is not a UCommonButtonBase"), *WidgetName), TEXT("WRONG_TYPE"));
			return true;
		}

		UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *DataTablePath);
		if (!DataTable)
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				FString::Printf(TEXT("DataTable not found at '%s'"), *DataTablePath), TEXT("NOT_FOUND"));
			return true;
		}
		if (!DataTable->RowStruct || !DataTable->RowStruct->IsChildOf(FCommonInputActionDataBase::StaticStruct()))
		{
			S.SendAutomationError(RequestingSocket, RequestId,
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
			S.SendAutomationError(RequestingSocket, RequestId,
				TEXT("Could not resolve UCommonButtonBase.TriggeringInputAction property"), TEXT("INTERNAL_ERROR"));
			return true;
		}

		const FName RowFName(*RowName);
		// Non-blocking: a handle may be bound before its row is imported. Report it.
		const bool bRowFound = DataTable->GetRowMap().Contains(RowFName);

		FDataTableRowHandle* HandlePtr = TrigProp->ContainerPtrToValuePtr<FDataTableRowHandle>(Button);
		HandlePtr->DataTable = DataTable;
		HandlePtr->RowName = RowFName;

		McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
		ResultJson->SetStringField(TEXT("widgetObjectPath"), Target->GetPathName());
		ResultJson->SetStringField(TEXT("dataTable"), DataTable->GetPathName());
		ResultJson->SetStringField(TEXT("rowName"), RowName);
		ResultJson->SetBoolField(TEXT("rowFound"), bRowFound);
		McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
		S.SendAutomationResponse(RequestingSocket, RequestId, true,
			TEXT("Set Common UI button input action"), ResultJson);
		return true;
#endif // MCP_HAS_COMMON_UI
}

bool McpHandlers::Blueprint::HandleCommonUiCreateCommonButtonStyle(
	UMcpAutomationBridgeSubsystem& S,
	const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
	FMcpResponseHandle RequestingSocket)
{
#if !MCP_HAS_COMMON_UI
	(void)Payload;
	S.SendAutomationError(RequestingSocket, RequestId,
		TEXT("Common UI support was not compiled into this build (the CommonUI plugin/module was not found at build time)."),
		TEXT("COMMON_UI_DISABLED"));
	return true;
#else
	MCP_COMMONUI_SUBACTION()
		const bool bButtonStyle = SubAction.Equals(TEXT("create_common_button_style"), ESearchCase::IgnoreCase);
		UClass* ParentClass = bButtonStyle
			? UCommonButtonStyle::StaticClass()
			: UCommonTextStyle::StaticClass();

		FString Name = GetJsonStringField(Payload, TEXT("name"));
		if (Name.IsEmpty())
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name'."), TEXT("MISSING_PARAMETER"));
			return true;
		}
		if (Name.Contains(TEXT("/")) || Name.Contains(TEXT(".")))
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				TEXT("'name' must be a bare asset name (no '/' or '.'); use 'path' for the folder."),
				TEXT("INVALID_NAME"));
			return true;
		}

		FString Path = GetJsonStringField(Payload, TEXT("path"));
		if (Path.IsEmpty())
		{
			Path = TEXT("/Game/UI/Styles");
		}
		Path.RemoveFromEnd(TEXT("/"));
		const FString PackagePath = Path / Name;                       // /Game/UI/Styles/Name
		const FString ClassPath = FString::Printf(TEXT("%s.%s_C"), *PackagePath, *Name);

		if (UEditorAssetLibrary::DoesAssetExist(PackagePath))
		{
			TSharedPtr<FJsonObject> ExistJson = MakeShared<FJsonObject>();
			ExistJson->SetBoolField(TEXT("success"), true);
			ExistJson->SetBoolField(TEXT("alreadyExisted"), true);
			ExistJson->SetStringField(TEXT("blueprintPath"), PackagePath);
			ExistJson->SetStringField(TEXT("styleClass"), ClassPath);
			ExistJson->SetStringField(TEXT("styleType"), bButtonStyle ? TEXT("button") : TEXT("text"));
			S.SendAutomationResponse(RequestingSocket, RequestId, true,
				FString::Printf(TEXT("Style asset already exists: %s"), *PackagePath), ExistJson);
			return true;
		}

		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package for style asset."), TEXT("PACKAGE_ERROR"));
			return true;
		}

		UBlueprint* StyleBP = FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Package, FName(*Name),
			BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (!StyleBP)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create style blueprint."), TEXT("CREATION_FAILED"));
			return true;
		}

		McpFinalizeBlueprint(StyleBP, /*bStructural=*/true, /*bSave=*/true);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetBoolField(TEXT("alreadyExisted"), false);
		ResultJson->SetStringField(TEXT("blueprintPath"), PackagePath);
		ResultJson->SetStringField(TEXT("styleClass"), ClassPath);
		ResultJson->SetStringField(TEXT("styleType"), bButtonStyle ? TEXT("button") : TEXT("text"));
		McpHandlerUtils::AddVerification(ResultJson, StyleBP);
		S.SendAutomationResponse(RequestingSocket, RequestId, true,
			FString::Printf(TEXT("Created %s style: %s"), bButtonStyle ? TEXT("button") : TEXT("text"), *PackagePath),
			ResultJson);
		return true;
#endif // MCP_HAS_COMMON_UI
}

bool McpHandlers::Blueprint::HandleCommonUiCreateCommonTextStyle(
	UMcpAutomationBridgeSubsystem& S,
	const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
	FMcpResponseHandle RequestingSocket)
{
#if !MCP_HAS_COMMON_UI
	(void)Payload;
	S.SendAutomationError(RequestingSocket, RequestId,
		TEXT("Common UI support was not compiled into this build (the CommonUI plugin/module was not found at build time)."),
		TEXT("COMMON_UI_DISABLED"));
	return true;
#else
	MCP_COMMONUI_SUBACTION()
		const bool bButtonStyle = SubAction.Equals(TEXT("create_common_button_style"), ESearchCase::IgnoreCase);
		UClass* ParentClass = bButtonStyle
			? UCommonButtonStyle::StaticClass()
			: UCommonTextStyle::StaticClass();

		FString Name = GetJsonStringField(Payload, TEXT("name"));
		if (Name.IsEmpty())
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name'."), TEXT("MISSING_PARAMETER"));
			return true;
		}
		if (Name.Contains(TEXT("/")) || Name.Contains(TEXT(".")))
		{
			S.SendAutomationError(RequestingSocket, RequestId,
				TEXT("'name' must be a bare asset name (no '/' or '.'); use 'path' for the folder."),
				TEXT("INVALID_NAME"));
			return true;
		}

		FString Path = GetJsonStringField(Payload, TEXT("path"));
		if (Path.IsEmpty())
		{
			Path = TEXT("/Game/UI/Styles");
		}
		Path.RemoveFromEnd(TEXT("/"));
		const FString PackagePath = Path / Name;                       // /Game/UI/Styles/Name
		const FString ClassPath = FString::Printf(TEXT("%s.%s_C"), *PackagePath, *Name);

		if (UEditorAssetLibrary::DoesAssetExist(PackagePath))
		{
			TSharedPtr<FJsonObject> ExistJson = MakeShared<FJsonObject>();
			ExistJson->SetBoolField(TEXT("success"), true);
			ExistJson->SetBoolField(TEXT("alreadyExisted"), true);
			ExistJson->SetStringField(TEXT("blueprintPath"), PackagePath);
			ExistJson->SetStringField(TEXT("styleClass"), ClassPath);
			ExistJson->SetStringField(TEXT("styleType"), bButtonStyle ? TEXT("button") : TEXT("text"));
			S.SendAutomationResponse(RequestingSocket, RequestId, true,
				FString::Printf(TEXT("Style asset already exists: %s"), *PackagePath), ExistJson);
			return true;
		}

		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package for style asset."), TEXT("PACKAGE_ERROR"));
			return true;
		}

		UBlueprint* StyleBP = FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Package, FName(*Name),
			BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (!StyleBP)
		{
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create style blueprint."), TEXT("CREATION_FAILED"));
			return true;
		}

		McpFinalizeBlueprint(StyleBP, /*bStructural=*/true, /*bSave=*/true);

		TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetBoolField(TEXT("alreadyExisted"), false);
		ResultJson->SetStringField(TEXT("blueprintPath"), PackagePath);
		ResultJson->SetStringField(TEXT("styleClass"), ClassPath);
		ResultJson->SetStringField(TEXT("styleType"), bButtonStyle ? TEXT("button") : TEXT("text"));
		McpHandlerUtils::AddVerification(ResultJson, StyleBP);
		S.SendAutomationResponse(RequestingSocket, RequestId, true,
			FString::Printf(TEXT("Created %s style: %s"), bButtonStyle ? TEXT("button") : TEXT("text"), *PackagePath),
			ResultJson);
		return true;
#endif // MCP_HAS_COMMON_UI
}
