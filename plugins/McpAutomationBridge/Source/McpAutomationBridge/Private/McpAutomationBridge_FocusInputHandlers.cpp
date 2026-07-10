// =============================================================================
// McpAutomationBridge_FocusInputHandlers.cpp
// =============================================================================
// CommonUI runtime focus / input introspection + drive — the observe/verify
// loop that CommonUI is missing over MCP. Pairs with the authoring-side
// DesiredFocusWidget work (COMMONUI_FOCUS_NAV.md) and the design in
// COMMONUI_FOCUS_INPUT.md.
//
// Two handlers, both PIE-runtime state:
//   - inspect ui_focus      (HandleInspectUiFocus)          — OBSERVE (read-only)
//       Returns one snapshot: focused widget + path, the activatable owning
//       focus + its desired-focus target, the active activatable stack, the
//       current input type, and the active root's bound actions.
//   - control_editor simulate_nav (HandleControlEditorSimulateNav) — DRIVE
//       Faithfully sends a navigation key through Slate (which runs the CommonUI
//       input preprocessor + action router exactly like a real pad), then
//       returns the post-nav focus snapshot so the caller can diff where focus
//       landed. "Faithful" was the chosen fidelity (vs deterministic Slate
//       NavigateFromWidget) so the test exercises the real routing.
//
// Why these public APIs (verified against UE 5.7 headers — no private coupling):
//   - FSlateApplication::GetUserFocusedWidget(0) → focused SWidget; walk
//     GetParentWidget() for the path. SWidget→UMG name via FReflectionMetaData
//     (debug metadata UMG attaches in non-shipping builds).
//   - UCommonUIActionRouterBase::FindActivatable(SWidget, LocalPlayer) (public
//     static) → the activatable owning the focused widget. Its public
//     GetDesiredFocusTarget()/IsModal()/SupportsActivationFocus() describe it.
//   - UCommonUIActionRouterBase::Get(UWidget&) (public static) → the router;
//     GatherActiveBindings() (public) → the active bound actions; each
//     FUIActionBindingHandle exposes GetActionName/GetDisplayName/etc.
//   - UCommonInputSubsystem::Get(LocalPlayer)->GetCurrentInputType() → input type.
//   The earlier design note assumed DebugDumpRootList was public; it is NOT (it
//   lives on a private nested struct), so this uses FindActivatable +
//   GatherActiveBindings + a TObjectIterator stack scan instead.
//
// When MCP_HAS_COMMON_UI == 0 the CommonUI-specific fields are simply omitted
// (the Slate focus path still works); when not in PIE, simulate_nav returns
// NOT_IN_PIE and ui_focus reports inPie:false with whatever Slate focus exists.
// =============================================================================

#include "McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_FocusInputHandlers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Modules/ModuleManager.h"                  // FModuleManager::IsModuleLoaded

#if WITH_EDITOR
#include "Editor.h"                                 // GEditor, PlayWorld
#endif

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"           // UGameViewportClient::GetGameViewportWidget
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"

#include "InputCoreTypes.h"                         // FKey, EKeys
#include "Framework/Application/SlateApplication.h" // FSlateApplication, FKeyEvent
#include "Widgets/SWidget.h"                        // SWidget, GetParentWidget
#include "Widgets/SViewport.h"                       // SViewport (complete type for the TSharedPtr upcast)
#include "Types/ReflectionMetadata.h"              // FReflectionMetaData (SWidget→UMG)
#include "UObject/UObjectIterator.h"                // TObjectIterator

#if MCP_HAS_COMMON_UI
#include "Input/CommonUIActionRouterBase.h"         // UCommonUIActionRouterBase
#include "Input/UIActionBindingHandle.h"            // FUIActionBindingHandle
#include "CommonActivatableWidget.h"                // UCommonActivatableWidget
#include "CommonInputSubsystem.h"                   // UCommonInputSubsystem
#include "CommonInputTypeEnum.h"                    // ECommonInputType
#endif

namespace
{
#if WITH_EDITOR
// The local player whose UI we're inspecting/driving — first PIE local player.
ULocalPlayer *GetPieLocalPlayer()
{
	if (!GEditor)
	{
		return nullptr;
	}
	UWorld *PlayWorld = GEditor->PlayWorld;
	if (!PlayWorld)
	{
		return nullptr;
	}
	if (APlayerController *PC = PlayWorld->GetFirstPlayerController())
	{
		if (ULocalPlayer *LP = Cast<ULocalPlayer>(PC->Player))
		{
			return LP;
		}
	}
	if (UGameInstance *GI = PlayWorld->GetGameInstance())
	{
		return GI->GetFirstGamePlayer();
	}
	return nullptr;
}

// {slateType, name?, class?} for one Slate widget. name/class come from UMG's
// reflection metadata when present (leaf engine widgets without metadata get
// only slateType).
TSharedPtr<FJsonObject> DescribeSlateWidget(const TSharedRef<SWidget> &W)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("slateType"), W->GetType().ToString());
	if (TSharedPtr<FReflectionMetaData> Meta = W->GetMetaData<FReflectionMetaData>())
	{
		if (!Meta->Name.IsNone())
		{
			Obj->SetStringField(TEXT("name"), Meta->Name.ToString());
		}
		if (Meta->Class.IsValid())
		{
			Obj->SetStringField(TEXT("class"), Meta->Class->GetName());
		}
	}
	return Obj;
}

#if MCP_HAS_COMMON_UI
TSharedPtr<FJsonObject> DescribeUWidget(const UWidget *W)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	if (W)
	{
		Obj->SetStringField(TEXT("name"), W->GetName());
		Obj->SetStringField(TEXT("class"), W->GetClass()->GetName());
	}
	return Obj;
}

FString CommonInputTypeToString(ECommonInputType T)
{
	switch (T)
	{
	case ECommonInputType::MouseAndKeyboard:
		return TEXT("MouseAndKeyboard");
	case ECommonInputType::Gamepad:
		return TEXT("Gamepad");
	case ECommonInputType::Touch:
		return TEXT("Touch");
	default:
		return TEXT("Unknown");
	}
}
#endif // MCP_HAS_COMMON_UI

// Populate Out with the full focus/input snapshot. Shared by ui_focus (observe)
// and simulate_nav (post-nav read-back). Never sends a response — caller owns
// transport. Tolerant of "no focus" / "not in PIE" / CommonUI-absent.
void BuildFocusSnapshot(const TSharedPtr<FJsonObject> &Out)
{
	UWorld *PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
	Out->SetBoolField(TEXT("inPie"), PlayWorld != nullptr);

	if (!FSlateApplication::IsInitialized())
	{
		Out->SetField(TEXT("focusedWidget"), MakeShared<FJsonValueNull>());
		return;
	}

	FSlateApplication &Slate = FSlateApplication::Get();
	ULocalPlayer *LP = GetPieLocalPlayer();

	// --- Focused widget + root→leaf path (pure Slate, no CommonUI coupling) ---
	TSharedPtr<SWidget> Focused = Slate.GetUserFocusedWidget(0);
	if (Focused.IsValid())
	{
		Out->SetObjectField(TEXT("focusedWidget"), DescribeSlateWidget(Focused.ToSharedRef()));

		TArray<TSharedPtr<SWidget>> Chain;
		TSharedPtr<SWidget> Cur = Focused;
		int32 Guard = 0;
		while (Cur.IsValid() && Guard++ < 256)
		{
			Chain.Add(Cur);
			Cur = Cur->GetParentWidget();
		}
		TArray<TSharedPtr<FJsonValue>> Path;
		for (int32 i = Chain.Num() - 1; i >= 0; --i) // root → leaf
		{
			Path.Add(MakeShared<FJsonValueObject>(DescribeSlateWidget(Chain[i].ToSharedRef())));
		}
		Out->SetArrayField(TEXT("focusPath"), Path);
	}
	else
	{
		Out->SetField(TEXT("focusedWidget"), MakeShared<FJsonValueNull>());
	}

#if MCP_HAS_COMMON_UI
	// CommonUI runtime state requires the plugin loaded + a local player.
	if (!LP || !FModuleManager::Get().IsModuleLoaded(TEXT("CommonUI")))
	{
		return;
	}

	// Current input type (Gamepad / MouseAndKeyboard / Touch).
	if (UCommonInputSubsystem *Input = UCommonInputSubsystem::Get(LP))
	{
		Out->SetStringField(TEXT("inputType"), CommonInputTypeToString(Input->GetCurrentInputType()));
		const FName Pad = Input->GetCurrentGamepadName();
		if (!Pad.IsNone())
		{
			Out->SetStringField(TEXT("currentGamepad"), Pad.ToString());
		}
	}

	// The activatable owning the focused widget + its desired-focus target.
	UCommonActivatableWidget *Active = nullptr;
	if (Focused.IsValid())
	{
		Active = UCommonUIActionRouterBase::FindActivatable(Focused, LP);
	}
	if (Active)
	{
		TSharedPtr<FJsonObject> A = MakeShared<FJsonObject>();
		A->SetStringField(TEXT("name"), Active->GetName());
		A->SetStringField(TEXT("class"), Active->GetClass()->GetName());
		A->SetBoolField(TEXT("isModal"), Active->IsModal());
		A->SetBoolField(TEXT("supportsActivationFocus"), Active->SupportsActivationFocus());
		Out->SetObjectField(TEXT("activeActivatable"), A);

		if (UWidget *Desired = Active->GetDesiredFocusTarget())
		{
			Out->SetObjectField(TEXT("desiredFocusTarget"), DescribeUWidget(Desired));
		}
		else
		{
			Out->SetField(TEXT("desiredFocusTarget"), MakeShared<FJsonValueNull>());
		}
	}

	// Active activatable stack: every activated UCommonActivatableWidget in the
	// PIE world. Order is not guaranteed to be paint/stack order (the real stack
	// lives in private router state) — it's the set of active screens.
	TArray<TSharedPtr<FJsonValue>> Stack;
	UCommonActivatableWidget *RouterContext = Active;
	for (TObjectIterator<UCommonActivatableWidget> It; It; ++It)
	{
		UCommonActivatableWidget *W = *It;
		if (!W || !W->IsActivated() || W->HasAnyFlags(RF_ClassDefaultObject))
		{
			continue;
		}
		if (PlayWorld && W->GetWorld() != PlayWorld)
		{
			continue;
		}
		TSharedPtr<FJsonObject> S = MakeShared<FJsonObject>();
		S->SetStringField(TEXT("name"), W->GetName());
		S->SetStringField(TEXT("class"), W->GetClass()->GetName());
		S->SetBoolField(TEXT("isModal"), W->IsModal());
		Stack.Add(MakeShared<FJsonValueObject>(S));
		if (!RouterContext)
		{
			RouterContext = W; // any activatable works as router-acquisition context
		}
	}
	Out->SetArrayField(TEXT("activatableStack"), Stack);

	// Bound actions at the active root (e.g. Back, Accept) + display names.
	if (RouterContext)
	{
		if (UCommonUIActionRouterBase *Router = UCommonUIActionRouterBase::Get(*RouterContext))
		{
			TArray<TSharedPtr<FJsonValue>> Actions;
			for (const FUIActionBindingHandle &H : Router->GatherActiveBindings())
			{
				if (!H.IsValid())
				{
					continue;
				}
				TSharedPtr<FJsonObject> B = MakeShared<FJsonObject>();
				B->SetStringField(TEXT("actionName"), H.GetActionName().ToString());
				B->SetStringField(TEXT("displayName"), H.GetDisplayName().ToString());
				B->SetBoolField(TEXT("displayInActionBar"), H.GetDisplayInActionBar());
				if (const UWidget *BW = H.GetBoundWidget())
				{
					B->SetStringField(TEXT("boundWidget"), BW->GetName());
				}
				Actions.Add(MakeShared<FJsonValueObject>(B));
			}
			Out->SetArrayField(TEXT("boundActions"), Actions);
		}
	}
#endif // MCP_HAS_COMMON_UI
}

// Map a direction (+ device) to the FKey to send. Returns an invalid FKey and
// sets OutError on failure. bOutShift is set for keyboard "previous" (Shift+Tab).
FKey ResolveNavKey(const FString &Direction, const FString &Device,
                   const FString &KeyOverride, bool &bOutShift, FString &OutError)
{
	bOutShift = false;
	if (!KeyOverride.IsEmpty())
	{
		FKey K(*KeyOverride);
		if (!K.IsValid())
		{
			OutError = FString::Printf(TEXT("Invalid key '%s'."), *KeyOverride);
		}
		return K;
	}

	const FString D = Direction.ToLower();
	const bool bPad = !Device.Equals(TEXT("keyboard"), ESearchCase::IgnoreCase);

	if (D == TEXT("up"))
		return bPad ? EKeys::Gamepad_DPad_Up : EKeys::Up;
	if (D == TEXT("down"))
		return bPad ? EKeys::Gamepad_DPad_Down : EKeys::Down;
	if (D == TEXT("left"))
		return bPad ? EKeys::Gamepad_DPad_Left : EKeys::Left;
	if (D == TEXT("right"))
		return bPad ? EKeys::Gamepad_DPad_Right : EKeys::Right;
	if (D == TEXT("accept") || D == TEXT("select") || D == TEXT("confirm"))
		return bPad ? EKeys::Gamepad_FaceButton_Bottom : EKeys::Enter;
	if (D == TEXT("back") || D == TEXT("cancel"))
		return bPad ? EKeys::Gamepad_FaceButton_Right : EKeys::Escape;
	if (D == TEXT("next"))
		return bPad ? EKeys::Gamepad_RightShoulder : EKeys::Tab;
	if (D == TEXT("previous") || D == TEXT("prev"))
	{
		if (bPad)
		{
			return EKeys::Gamepad_LeftShoulder;
		}
		bOutShift = true; // Shift+Tab
		return EKeys::Tab;
	}

	OutError = FString::Printf(
	    TEXT("Unknown direction '%s'. Use Up/Down/Left/Right/Accept/Back/Next/Previous, "
	         "or pass an explicit 'key'."),
	    *Direction);
	return FKey();
}
#endif // WITH_EDITOR
} // namespace

// =============================================================================
// inspect ui_focus — OBSERVE
// =============================================================================
bool McpHandlers::Inspect::HandleInspectUiFocus(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
	TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
	BuildFocusSnapshot(Resp);
	Resp->SetBoolField(TEXT("success"), true);
	S.SendAutomationResponse(RequestingSocket, RequestId, true,
	                       TEXT("UI focus snapshot"), Resp, FString());
	return true;
#else
	S.SendAutomationError(RequestingSocket, RequestId,
	                    TEXT("ui_focus requires an editor build."),
	                    TEXT("NOT_IMPLEMENTED"));
	return true;
#endif
}

// =============================================================================
// control_editor simulate_nav — DRIVE (faithful)
// =============================================================================
bool McpHandlers::ControlEditor::HandleControlEditorSimulateNav(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
	if (!GEditor || !GEditor->PlayWorld)
	{
		S.SendAutomationError(RequestingSocket, RequestId,
		                    TEXT("simulate_nav drives runtime UI focus and requires an "
		                         "active PIE session (control_editor play first)."),
		                    TEXT("NOT_IN_PIE"));
		return true;
	}
	if (!FSlateApplication::IsInitialized())
	{
		S.SendAutomationError(RequestingSocket, RequestId,
		                    TEXT("Slate application is not initialized."),
		                    TEXT("SLATE_NOT_AVAILABLE"));
		return true;
	}

	FString Direction;
	Payload->TryGetStringField(TEXT("direction"), Direction);
	FString Device;
	Payload->TryGetStringField(TEXT("device"), Device);
	if (Device.IsEmpty())
	{
		Device = TEXT("gamepad"); // faithful gamepad nav by default
	}
	FString KeyOverride;
	Payload->TryGetStringField(TEXT("key"), KeyOverride);

	if (Direction.IsEmpty() && KeyOverride.IsEmpty())
	{
		S.SendAutomationError(RequestingSocket, RequestId,
		                    TEXT("simulate_nav requires 'direction' "
		                         "(Up/Down/Left/Right/Accept/Back/Next/Previous) or 'key'."),
		                    TEXT("INVALID_ARGUMENT"));
		return true;
	}

	bool bShift = false;
	FString ResolveErr;
	const FKey NavKey = ResolveNavKey(Direction, Device, KeyOverride, bShift, ResolveErr);
	if (!NavKey.IsValid())
	{
		S.SendAutomationError(RequestingSocket, RequestId,
		                    ResolveErr.IsEmpty() ? TEXT("Could not resolve a navigation key.")
		                                         : ResolveErr,
		                    TEXT("INVALID_ARGUMENT"));
		return true;
	}

	FSlateApplication &Slate = FSlateApplication::Get();

	// Focus-stabilize — make the faithful drive deterministic. An automated run
	// often leaves Slate focus on the editor window / level viewport rather than the
	// PIE game viewport, so a synthesized nav never reaches CommonUI (the flaky case
	// that made focus/input assertions non-deterministic). Redirect focus to the game
	// viewport *only when it isn't already there*, so we never steal focus from a
	// correctly-focused in-game widget (its focus path walks up to the game viewport,
	// so we leave it). Opt out with stabilizeFocus:false.
	bool bStabilizeFocus = true;
	Payload->TryGetBoolField(TEXT("stabilizeFocus"), bStabilizeFocus);
	bool bFocusStabilized = false;
	bool Resp_Diag_GameViewportRegistered = false;
	bool Resp_Diag_FocusInViewportAfterStabilize = false;
	if (bStabilizeFocus)
	{
		TSharedPtr<SWidget> GameViewportWidget;
		if (UGameViewportClient *GVC = GEditor->PlayWorld->GetGameViewport())
		{
			GameViewportWidget = GVC->GetGameViewportWidget();
		}
		if (GameViewportWidget.IsValid())
		{
			bool bInGame = false;
			for (TSharedPtr<SWidget> Cur = Slate.GetUserFocusedWidget(0); Cur.IsValid();
			     Cur = Cur->GetParentWidget())
			{
				if (Cur == GameViewportWidget)
				{
					bInGame = true;
					break;
				}
			}
			if (!bInGame)
			{
				Slate.SetUserFocusToGameViewport(0); // default EFocusCause::SetDirectly
				bFocusStabilized = true;
			}
		}
		// Diagnostics: stabilization can silently no-op (FSlateApplication's
		// registered game viewport unset, or SetUserFocus refused) — surface
		// the preconditions so a failing run says WHY instead of just failing.
		{
			TSharedPtr<SViewport> RegisteredViewport = Slate.GetGameViewport();
			Resp_Diag_GameViewportRegistered = RegisteredViewport.IsValid();
			TSharedPtr<SWidget> FocusNow = Slate.GetUserFocusedWidget(0);
			Resp_Diag_FocusInViewportAfterStabilize = false;
			for (TSharedPtr<SWidget> Cur = FocusNow; Cur.IsValid(); Cur = Cur->GetParentWidget())
			{
				if (Cur == GameViewportWidget)
				{
					Resp_Diag_FocusInViewportAfterStabilize = true;
					break;
				}
			}
		}
	}

	// Capture focus before, so we can report whether the nav moved it.
	TSharedPtr<SWidget> Before = Slate.GetUserFocusedWidget(0);
	TSharedPtr<FJsonObject> FocusBefore;
	if (Before.IsValid())
	{
		FocusBefore = DescribeSlateWidget(Before.ToSharedRef());
	}

	// Faithful drive: deliver the key through Slate. ProcessKeyDownEvent runs the
	// registered input preprocessors (incl. CommonUI's) BEFORE widget routing and
	// then performs navigation via the active navigation config — i.e. exactly the
	// path a real pad takes. Down + Up to mimic a real press.
	const uint32 UserIndex = 0;
	const FModifierKeysState Mods(
	    /*bInIsLeftShiftDown*/ bShift, /*bInIsRightShiftDown*/ false,
	    /*bInIsLeftControlDown*/ false, /*bInIsRightControlDown*/ false,
	    /*bInIsLeftAltDown*/ false, /*bInIsRightAltDown*/ false,
	    /*bInIsLeftCommandDown*/ false, /*bInIsRightCommandDown*/ false,
	    /*bInAreCapsLocked*/ false);

	// CommonInput refuses to reclassify the input method while the application is
	// not OS-foreground: FCommonInputPreprocessor::IsRelevantInput gates on
	// FSlateApplication::IsActive() (CommonInputPreprocessor.cpp:176-178), which is
	// false for a bridge-driven editor in the background — the synthesized gamepad
	// key would never flip ECommonInputType to Gamepad, so CommonUI's input-method
	// focus path would never run. Temporarily allow device input while inactive so
	// the key is treated exactly like foreground hardware input, then restore.
	const bool bPrevHandleInactive = Slate.GetHandleDeviceInputWhenApplicationNotActive();
	Slate.SetHandleDeviceInputWhenApplicationNotActive(true);

	FKeyEvent DownEvent(NavKey, Mods, UserIndex, /*bIsRepeat*/ false,
	                    /*CharacterCode*/ 0, /*KeyCode*/ 0);
	const bool bHandled = Slate.ProcessKeyDownEvent(DownEvent);
	FKeyEvent UpEvent(NavKey, Mods, UserIndex, /*bIsRepeat*/ false,
	                  /*CharacterCode*/ 0, /*KeyCode*/ 0);
	Slate.ProcessKeyUpEvent(UpEvent);

	Slate.SetHandleDeviceInputWhenApplicationNotActive(bPrevHandleInactive);

	// Read focus back (synchronous: Slate navigation resolves within
	// ProcessKeyDownEvent) and return the full post-nav snapshot for diffing.
	TSharedPtr<SWidget> After = Slate.GetUserFocusedWidget(0);

	TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
	Resp->SetStringField(TEXT("direction"), Direction);
	Resp->SetStringField(TEXT("device"), Device);
	Resp->SetStringField(TEXT("key"), NavKey.ToString());
	Resp->SetBoolField(TEXT("handled"), bHandled);
	Resp->SetBoolField(TEXT("focusStabilized"), bFocusStabilized);
	// False whenever the editor isn't OS-foreground (the normal automated-run
	// state) — the inactive-input bypass above is what kept the nav faithful.
	Resp->SetBoolField(TEXT("slateAppActive"), Slate.IsActive());
	Resp->SetBoolField(TEXT("gameViewportRegistered"), Resp_Diag_GameViewportRegistered);
	Resp->SetBoolField(TEXT("focusInViewportAfterStabilize"), Resp_Diag_FocusInViewportAfterStabilize);
	Resp->SetBoolField(TEXT("focusChanged"), Before != After);
	if (FocusBefore.IsValid())
	{
		Resp->SetObjectField(TEXT("focusBefore"), FocusBefore);
	}
	BuildFocusSnapshot(Resp); // adds focusedWidget/focusPath/desiredFocusTarget/etc (post-nav)
	Resp->SetBoolField(TEXT("success"), true);

	S.SendAutomationResponse(RequestingSocket, RequestId, true,
	                       FString::Printf(TEXT("simulate_nav %s (%s) -> focusChanged=%s"),
	                                       *Direction, *NavKey.ToString(),
	                                       (Before != After) ? TEXT("true") : TEXT("false")),
	                       Resp, FString());
	return true;
#else
	S.SendAutomationError(RequestingSocket, RequestId,
	                    TEXT("simulate_nav requires an editor build."),
	                    TEXT("NOT_IMPLEMENTED"));
	return true;
#endif
}
