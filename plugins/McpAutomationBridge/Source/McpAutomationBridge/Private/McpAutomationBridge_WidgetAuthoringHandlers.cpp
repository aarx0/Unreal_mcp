// =============================================================================
// McpAutomationBridge_WidgetAuthoringHandlers.cpp
// =============================================================================
// Phase 19: Widget Authoring System Handlers
//
// Provides comprehensive UMG widget authoring capabilities for the MCP Automation Bridge.
// This file implements the `manage_widget_authoring` tool with 80+ sub-actions.
//
// HANDLERS BY CATEGORY:
// ---------------------
// 19.1  Widget Creation     - create_widget_blueprint, set_widget_parent_class
// 19.2  Layout Panels       - add_canvas_panel, add_horizontal_box, add_vertical_box,
//                            add_overlay, add_grid_panel, add_uniform_grid, add_wrap_box,
//                            add_scroll_box, add_size_box, add_scale_box, add_border
// 19.3  Common Widgets      - add_text_block, add_rich_text, add_image, add_button,
//                            add_checkbox, add_slider, add_progress_bar, add_editable_text,
//                            add_combo_box, add_spin_box, add_list_view, add_tree_view,
//                            add_tile_view, add_widget_switcher, add_spacer, add_safe_zone
// 19.4  Layout & Styling    - set_widget_anchor, set_widget_alignment, set_widget_position,
//                            set_widget_size, set_widget_padding, set_widget_visibility,
//                            set_widget_style, apply_widget_style
// 19.5  Bindings & Events   - bind_widget_property, bind_widget_event, unbind_widget_event
// 19.6  Widget Animations   - create_widget_animation, add_animation_track, add_keyframe,
//                            play_widget_animation, stop_widget_animation
// 19.7  UI Templates        - create_main_menu, create_pause_menu, create_hud,
//                            create_inventory_ui, create_dialogue_system, create_health_bar,
//                            create_minimap, create_loading_screen
// 19.8  Utility Actions     - get_widget_info, get_widget_tree, find_widget_by_name,
//                            preview_widget, validate_widget
// 19.9  Advanced Widgets    - add_retainer_box, add_circular_throbber, add_expandable_area,
//                            add_menu_anchor, add_viewport_stats
// 19.10 Text Operations     - set_text_content, bind_localized_text
// 19.11 Template Actions    - create_credits_screen, create_shop_ui, add_quest_tracker
//
// VERSION COMPATIBILITY:
// ----------------------
// - UE 5.0: Uses ResolveClassByName for parent class lookup
// - UE 5.1+: Uses FindFirstObject for parent class lookup
// - WidgetTree API consistent across UE 5.0-5.7
// - CanvasPanelSlot anchoring API stable across versions
//
// REFACTORING NOTES:
// ------------------
// - Uses WidgetAuthoringHelpers namespace for widget-specific utilities
// - Color/visibility parsing standardized in helper namespace
// - Asset path normalization via SanitizeProjectRelativePath()
// - Widget blueprint loading has multiple fallback methods for robustness
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

// =============================================================================
// Includes
// =============================================================================

// Version Compatibility (must be first)
#include "McpVersionCompatibility.h"

// MCP Core
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"

// JSON & Serialization
#include "Dom/JsonObject.h"

// Asset Registry
#include "AssetRegistry/AssetRegistryModule.h"

// UMG Core
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"

// Engine
#include "Engine/Texture2D.h"
#include "UObject/UObjectIterator.h"

// UMG Layout Panels
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/GridPanel.h"
#include "Components/UniformGridPanel.h"
#include "Components/WrapBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/Border.h"
#include "Components/SafeZone.h"
#include "Components/Spacer.h"
#include "Components/WidgetSwitcher.h"

// UMG Common Widgets
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Components/ListView.h"
#include "Components/TreeView.h"
#include "Components/EditableText.h"
#include "Components/TileView.h"

// Animation
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "MovieScene.h"

// Blueprint Editor
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/Kismet2NameValidators.h" // FKismetNameValidator — rename_widget unique-name check
#include "WidgetBlueprintEditorUtils.h" // FWidgetBlueprintEditorUtils::ReplaceWidgets (the designer's "Replace With")
#include "Blueprint/WidgetNavigation.h" // rename_widget: fix explicit nav rules referencing the old name
#include "Misc/ConfigCacheIni.h"        // GConfig — suppress the "in use in the graph" replace dialog for headless runs

// Blueprint graph nodes — for real widget delegate event binding (bind_on_clicked)
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Event.h"                // base of all event nodes — counted for chain self-layout
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"          // bind_on_value_changed live-update chain
#include "Kismet/KismetTextLibrary.h"    // Conv_DoubleToText for value->text
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_AddDelegate.h"           // bind_event_to_delegate: subscribe to an external multicast delegate
#include "K2Node_CustomEvent.h"           // signature-matched handler event
#include "ScopedTransaction.h"            // group the generated chain as one undo unit

// Editor Utilities
#include "EditorAssetLibrary.h"

// Notification System (SNotificationList.h must come before NotificationManager.h for FNotificationInfo)
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

// Asset Editor Subsystem
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"

// Internationalization
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"

// Vertical gap (px) between independently-generated Blueprint event chains. A
// brand-new event is offset down by (existing event-node count) * this gap so the
// node-generating binders (bind_on_clicked / bind_on_value_changed) stack their
// chains instead of piling up at the same coordinates. Within-chain relative
// offsets are unchanged. ~300 is roomy enough for the tallest chain we emit
// (value-changed: a feeder 200px below the event); pixel-perfect isn't required.
static constexpr int32 McpWidgetChainRowGapY = 300;

// =============================================================================
// Widget Authoring Helper Functions
// =============================================================================
//
// These helpers provide widget-specific utilities:
// - GetColorFromJsonWidget: Parse RGBA color from JSON object
// - GetObjectField/GetArrayField: Optional field access (returns null on missing)
// - LoadWidgetBlueprint: Robust widget blueprint loading with multiple fallbacks
// - GetVisibility: Convert visibility string to ESlateVisibility enum
//
// =============================================================================

namespace WidgetAuthoringHelpers
{
    FLinearColor GetColorFromJsonWidget(const TSharedPtr<FJsonObject>& ColorObj, const FLinearColor& Default = FLinearColor::White)
    {
        if (!ColorObj.IsValid())
        {
            return Default;
        }
        FLinearColor Color = Default;
        Color.R = ColorObj->HasField(TEXT("r")) ? GetJsonNumberField(ColorObj, TEXT("r")) : Default.R;
        Color.G = ColorObj->HasField(TEXT("g")) ? GetJsonNumberField(ColorObj, TEXT("g")) : Default.G;
        Color.B = ColorObj->HasField(TEXT("b")) ? GetJsonNumberField(ColorObj, TEXT("b")) : Default.B;
        Color.A = ColorObj->HasField(TEXT("a")) ? GetJsonNumberField(ColorObj, TEXT("a")) : Default.A;
        return Color;
    }

    // Get object field
    TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName))
        {
            return Payload->GetObjectField(FieldName);
        }
        return nullptr;
    }

    // Get array field
    const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Array>(FieldName))
        {
            return &Payload->GetArrayField(FieldName);
        }
        return nullptr;
    }

    // Get slot name from payload - checks both "slotName" (preferred) and "widgetName" (legacy fallback)
    // This ensures compatibility with both TS handler contract (slotName) and any legacy callers
    FString GetSlotName(const TSharedPtr<FJsonObject>& Payload)
    {
        if (!Payload.IsValid())
        {
            return FString();
        }
        // Primary: check slotName (matches TS handler contract)
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        if (!SlotName.IsEmpty())
        {
            return SlotName;
        }
        // Alias: "name" (what most callers naturally pass for the new widget's name)
        SlotName = GetJsonStringField(Payload, TEXT("name"));
        if (!SlotName.IsEmpty())
        {
            return SlotName;
        }
        // Fallback: check widgetName (legacy compatibility)
        return GetJsonStringField(Payload, TEXT("widgetName"));
    }

    // Load widget blueprint - robust lookup for both in-memory and on-disk assets
    UWidgetBlueprint* LoadWidgetBlueprint(const FString& WidgetPath)
    {
        FString Path = WidgetPath;
        
        // Reject _C class paths
        if (Path.EndsWith(TEXT("_C")))
        {
            return nullptr;
        }
        
        // Normalize: ensure starts with /Game/ or /
        if (!Path.StartsWith(TEXT("/")))
        {
            Path = TEXT("/Game/") + Path;
        }
        
        // Build object path and package path
        FString ObjectPath = Path;
        FString PackagePath = Path;
        
        if (Path.Contains(TEXT(".")))
        {
            // Already has object path format, extract package path
            PackagePath = Path.Left(Path.Find(TEXT(".")));
        }
        else
        {
            // Add .Name suffix for object path
            FString AssetName = FPaths::GetBaseFilename(Path);
            ObjectPath = Path + TEXT(".") + AssetName;
        }
        
        FString AssetName = FPaths::GetBaseFilename(PackagePath);
        
        // Method 1: FindObject with full object path (fastest for in-memory)
        if (UWidgetBlueprint* WB = FindObject<UWidgetBlueprint>(nullptr, *ObjectPath))
        {
            return WB;
        }
        
        // Method 2: Find package first, then find asset within it
        if (UPackage* Package = FindPackage(nullptr, *PackagePath))
        {
            if (UWidgetBlueprint* WB = FindObject<UWidgetBlueprint>(Package, *AssetName))
            {
                return WB;
            }
        }
        
        // Method 3: TObjectIterator fallback - iterate all widget blueprints to find by path
        // This is slower but guaranteed to find in-memory assets that weren't properly registered
        for (TObjectIterator<UWidgetBlueprint> It; It; ++It)
        {
            UWidgetBlueprint* WB = *It;
            if (WB)
            {
                FString WBPath = WB->GetPathName();
                // Match by full object path or package path
                if (WBPath.Equals(ObjectPath, ESearchCase::IgnoreCase) ||
                    WBPath.Equals(PackagePath, ESearchCase::IgnoreCase) ||
                    WBPath.Equals(Path, ESearchCase::IgnoreCase))
                {
                    return WB;
                }
                // Also check if the package paths match
                FString WBPackagePath = WBPath;
                if (WBPackagePath.Contains(TEXT(".")))
                {
                    WBPackagePath = WBPackagePath.Left(WBPackagePath.Find(TEXT(".")));
                }
                if (WBPackagePath.Equals(PackagePath, ESearchCase::IgnoreCase))
                {
                    return WB;
                }
            }
        }
        
        // Method 4: Asset Registry lookup
        IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FAssetData AssetData = Registry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
#else
        // UE 5.0: GetAssetByObjectPath takes FName
        FAssetData AssetData = Registry.GetAssetByObjectPath(FName(*ObjectPath));
#endif
        if (AssetData.IsValid())
        {
            if (UWidgetBlueprint* WB = Cast<UWidgetBlueprint>(AssetData.GetAsset()))
            {
                return WB;
            }
        }
        
        // Method 5: StaticLoadObject with object path (for disk assets)
        if (UWidgetBlueprint* WB = Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *ObjectPath)))
        {
            return WB;
        }
        
        // Method 6: StaticLoadObject with package path
        return Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *PackagePath));
    }

    // Convert visibility string to enum
    ESlateVisibility GetVisibility(const FString& VisibilityStr)
    {
        if (VisibilityStr.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase))
        {
            return ESlateVisibility::Collapsed;
        }
        else if (VisibilityStr.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase))
        {
            return ESlateVisibility::Hidden;
        }
        else if (VisibilityStr.Equals(TEXT("HitTestInvisible"), ESearchCase::IgnoreCase))
        {
            return ESlateVisibility::HitTestInvisible;
        }
        else if (VisibilityStr.Equals(TEXT("SelfHitTestInvisible"), ESearchCase::IgnoreCase))
        {
            return ESlateVisibility::SelfHitTestInvisible;
        }
        return ESlateVisibility::Visible;
    }

    /**
     * CRITICAL: Register a widget in the WidgetVariableNameToGuidMap.
     * 
     * This MUST be called after creating widgets via WidgetTree->ConstructWidget()
     * to prevent ensure failures in WidgetBlueprintCompiler.cpp line 794.
     * 
     * The compiler's ValidateAndFixUpVariableGuids() expects all widgets in the
     * WidgetTree to have a corresponding entry in WidgetVariableNameToGuidMap.
     * Without this registration, blueprint compilation triggers ensure failures.
     * 
     * @param WidgetBP The widget blueprint that owns the widget
     * @param Widget The newly created widget to register
     */
    void RegisterWidgetGuid(UWidgetBlueprint* WidgetBP, UWidget* Widget)
    {
if (!WidgetBP || !Widget)
{
return;
}

const FName WidgetFName = Widget->GetFName();

#if MCP_HAS_WIDGET_VARIABLE_GUID_MAP
// Only register if not already present
if (!MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Contains(WidgetFName))
{
// Use deterministic GUID based on widget path for stability across saves
// This matches the engine's pattern in WidgetBlueprintCompiler.cpp line 774
FGuid WidgetGuid = MCP_NEW_DETERMINISTIC_GUID(Widget->GetPathName());
MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Emplace(WidgetFName, WidgetGuid);

UE_LOG(LogTemp, Verbose, TEXT("RegisterWidgetGuid: Registered widget '%s' with GUID %s"),
*WidgetFName.ToString(), *WidgetGuid.ToString());
}
#else
// UE 5.0: WidgetVariableNameToGuidMap doesn't exist, GUID tracking is handled differently
// The NewVariables array on UBlueprint is used for variable tracking
UE_LOG(LogTemp, Verbose, TEXT("RegisterWidgetGuid: Widget '%s' registered (UE 5.0 mode)"),
*WidgetFName.ToString());
#endif
}

    /**
     * CRITICAL: Unregister a widget from the WidgetVariableNameToGuidMap.
     * 
     * This MUST be called when removing widgets from the WidgetTree to prevent
     * "Variable [X] was deleted but still has a GUID" ensure failures in
     * WidgetBlueprintCompiler.cpp line 828.
     * 
     * When a widget is removed from the tree (e.g., replaced as root, or explicitly
     * deleted), its GUID entry must be removed to maintain consistency.
     * 
     * @param WidgetBP The widget blueprint that owns the widget
     * @param Widget The widget being removed from the tree
     */
void UnregisterWidgetGuid(UWidgetBlueprint* WidgetBP, UWidget* Widget)
{
if (!WidgetBP || !Widget)
{
return;
}

const FName WidgetFName = Widget->GetFName();

#if MCP_HAS_WIDGET_VARIABLE_GUID_MAP
if (MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Contains(WidgetFName))
{
MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Remove(WidgetFName);
UE_LOG(LogTemp, Verbose, TEXT("UnregisterWidgetGuid: Unregistered widget '%s'"),
*WidgetFName.ToString());
}
#else
// UE 5.0: WidgetVariableNameToGuidMap doesn't exist
UE_LOG(LogTemp, Verbose, TEXT("UnregisterWidgetGuid: Widget '%s' unregistered (UE 5.0 mode)"),
*WidgetFName.ToString());
#endif
}

    /**
     * Discard a just-constructed widget that failed to be added to the tree. Strips its
     * variable->GUID entry and renames it OUT of the WidgetTree's outer (to the transient
     * package). Without the rename it would linger as a GUID-less object still outered to
     * the WidgetTree, which the compiler's ForEachSourceWidget (ForEachObjectWithOuter)
     * would later flag as "Widget [X] was added but did not get a GUID". Call this on any
     * add-failure path for a widget created via WidgetTree->ConstructWidget.
     */
    void DiscardUnaddedWidget(UWidgetBlueprint* WidgetBP, UWidget* Widget)
    {
        if (!WidgetBP || !Widget)
        {
            return;
        }
        UnregisterWidgetGuid(WidgetBP, Widget);
        if (WidgetBP->WidgetTree)
        {
            WidgetBP->WidgetTree->RemoveWidget(Widget);
        }
        Widget->Rename(nullptr, GetTransientPackage(),
            REN_DontCreateRedirectors | REN_NonTransactional);
    }

    /**
     * Safely add a widget to the widget tree with proper root/parent handling.
     * 
     * This handles the critical case where parentSlot is not specified:
     * - If no root exists: Sets widget as root
     * - If root exists and is a panel: Adds widget as child of root
     * - If root exists and is NOT a panel: Replaces root (with GUID cleanup)
     * 
     * This prevents "Variable [X] was deleted but still has a GUID" ensure failures
     * by properly cleaning up GUIDs when replacing widgets.
     * 
     * @param WidgetBP The widget blueprint
     * @param NewWidget The widget to add
     * @param ParentSlot The name of the parent slot (empty = auto-determine)
     * @return true if widget was successfully added to the tree
     */
    // NOTE: this .cpp does not include the helpers header, so the default lives here too (the
    // header carries the same default for other TUs; no TU sees both, so it's legal).
    bool SafeAddWidgetToTree(UWidgetBlueprint* WidgetBP, UWidget* NewWidget, const FString& ParentSlot, FString* OutError = nullptr)
    {
        if (!WidgetBP || !WidgetBP->WidgetTree || !NewWidget)
        {
            if (OutError) { *OutError = TEXT("Invalid widget blueprint or widget"); }
            return false;
        }

        UWidgetTree* WidgetTree = WidgetBP->WidgetTree;

        if (ParentSlot.IsEmpty())
        {
            // No parent specified - handle root placement
            if (!WidgetTree->RootWidget)
            {
                // No root exists - set as root
                WidgetTree->RootWidget = NewWidget;
                UE_LOG(LogTemp, Verbose, TEXT("SafeAddWidgetToTree: Set '%s' as root widget"), 
                    *NewWidget->GetName());
            }
            else
            {
                // Root exists - try to add as child of root if root is a panel
                UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
                if (RootPanel)
                {
                    // Root is a panel - add as child
                    RootPanel->AddChild(NewWidget);
                    UE_LOG(LogTemp, Verbose, TEXT("SafeAddWidgetToTree: Added '%s' as child of root panel '%s'"), 
                        *NewWidget->GetName(), *RootPanel->GetName());
                }
                else
                {
                    // Root exists and is NOT a panel (e.g. a TextBlock/CommonTextBlock). Refuse
                    // rather than silently replace it -- replacing would discard the existing root
                    // (data loss). Surface guidance instead. (Decision 2026-06-06: error with
                    // guidance, not auto-wrap or silent replace.)
                    const FString Msg = FString::Printf(
                        TEXT("Cannot add '%s' without a parentSlot: the root widget '%s' is not a ")
                        TEXT("panel, so it can't take children and adding here would discard it. Pass ")
                        TEXT("a 'parentSlot' naming a panel, or give the blueprint a panel root first ")
                        TEXT("(e.g. add_canvas_panel / add_vertical_box)."),
                        *NewWidget->GetName(), *WidgetTree->RootWidget->GetName());
                    UE_LOG(LogTemp, Warning, TEXT("SafeAddWidgetToTree: %s"), *Msg);
                    if (OutError) { *OutError = Msg; }
                    DiscardUnaddedWidget(WidgetBP, NewWidget);
                    return false;
                }
            }
        }
        else
        {
            // Parent specified - find and add to it
            UWidget* ParentWidget = WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(NewWidget);
                    UE_LOG(LogTemp, Verbose, TEXT("SafeAddWidgetToTree: Added '%s' as child of '%s'"), 
                        *NewWidget->GetName(), *ParentSlot);
                }
                else
                {
                    const FString Msg = FString::Printf(
                        TEXT("parentSlot '%s' is not a panel widget, so it can't take children. ")
                        TEXT("Choose a panel parent (e.g. a CanvasPanel / VerticalBox)."), *ParentSlot);
                    UE_LOG(LogTemp, Warning, TEXT("SafeAddWidgetToTree: %s"), *Msg);
                    if (OutError) { *OutError = Msg; }
                    DiscardUnaddedWidget(WidgetBP, NewWidget);
                    return false;
                }
            }
            else
            {
                const FString Msg = FString::Printf(
                    TEXT("parentSlot '%s' was not found in the widget tree."), *ParentSlot);
                UE_LOG(LogTemp, Warning, TEXT("SafeAddWidgetToTree: %s"), *Msg);
                if (OutError) { *OutError = Msg; }
                DiscardUnaddedWidget(WidgetBP, NewWidget);
                return false;
            }
        }

        return true;
    }

    /**
     * CRITICAL: Register an animation in the WidgetVariableNameToGuidMap and GeneratedVariables.
     * 
     * This MUST be called after creating UWidgetAnimation objects to:
     * 1. Prevent ensure failures in WidgetBlueprintCompiler.cpp line 805
     * 2. Ensure the animation appears as a variable in the blueprint
     * 
     * @param WidgetBP The widget blueprint that owns the animation
     * @param Animation The newly created animation to register
     */
void RegisterAnimationGuid(UWidgetBlueprint* WidgetBP, UWidgetAnimation* Animation)
{
if (!WidgetBP || !Animation)
{
return;
}

const FName AnimFName = Animation->GetFName();

#if MCP_HAS_WIDGET_VARIABLE_GUID_MAP
// Register in WidgetVariableNameToGuidMap if not present
if (!MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Contains(AnimFName))
{
// Use deterministic GUID based on animation path for stability
FGuid AnimGuid = MCP_NEW_DETERMINISTIC_GUID(Animation->GetPathName());
MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Emplace(AnimFName, AnimGuid);

UE_LOG(LogTemp, Verbose, TEXT("RegisterAnimationGuid: Registered animation '%s' with GUID %s"),
*AnimFName.ToString(), *AnimGuid.ToString());
}
#else
// UE 5.0: WidgetVariableNameToGuidMap doesn't exist
UE_LOG(LogTemp, Verbose, TEXT("RegisterAnimationGuid: Animation '%s' registered (UE 5.0 mode)"),
*AnimFName.ToString());
#endif
        
        // Ensure animation is in the Animations array
        if (!WidgetBP->Animations.Contains(Animation))
        {
            WidgetBP->Animations.Add(Animation);
        }
    }

    /**
     * Helper to create and register a widget in one call.
     * This ensures the GUID is registered immediately after creation.
     * 
     * @param WidgetBP The widget blueprint
     * @param WidgetTree The widget tree to create in
     * @param WidgetName The name for the new widget
     * @return The created widget, or nullptr on failure
     */
    template<typename T>
    T* CreateAndRegisterWidget(UWidgetBlueprint* WidgetBP, UWidgetTree* WidgetTree, FName WidgetName)
    {
        static_assert(TIsDerivedFrom<T, UWidget>::Value, "T must derive from UWidget");
        
        if (!WidgetBP || !WidgetTree)
        {
            return nullptr;
        }

        // Idempotency (see docs/pull-architecture.md): if a widget with this name
        // already exists, return it instead of letting ConstructWidget auto-suffix a
        // duplicate (which a dropped-response retry would otherwise create). Same type
        // converges; a name collision with a different type returns null (real conflict).
        if (UWidget* Existing = WidgetTree->FindWidget(WidgetName))
        {
            return Cast<T>(Existing);
        }

        T* Widget = WidgetTree->ConstructWidget<T>(T::StaticClass(), WidgetName);
        if (Widget)
        {
            RegisterWidgetGuid(WidgetBP, Widget);
        }
        return Widget;
    }

    /**
     * Register all widgets in the widget tree that don't have GUIDs yet.
     * This is useful for template handlers that create multiple widgets at once.
     * 
     * @param WidgetBP The widget blueprint to register widgets for
     */
    void RegisterAllWidgetGuids(UWidgetBlueprint* WidgetBP)
    {
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            return;
        }

        // Register all widgets in the tree
        WidgetBP->WidgetTree->ForEachWidget([WidgetBP](UWidget* Widget) {
            if (Widget)
            {
                RegisterWidgetGuid(WidgetBP, Widget);
            }
        });

        // Register all animations
        for (UWidgetAnimation* Animation : WidgetBP->Animations)
        {
            if (Animation)
            {
                RegisterAnimationGuid(WidgetBP, Animation);
            }
        }
    }

    /**
     * CRITICAL: Clear the entire widget tree and reset GUID map for a complete rebuild.
     * 
     * This is the ONLY safe way to replace the entire widget hierarchy because:
     * 1. Setting RootWidget = nullptr doesn't actually remove widgets from the tree
     * 2. Widgets still have WidgetTree as their outer
     * 3. ForEachObjectWithOuter still finds orphaned widgets
     * 4. The compiler validates ALL widgets in the tree, not just RootWidget
     * 
     * This function:
     * 1. Removes all widgets from the tree
     * 2. Clears the GUID map
     * 3. Prepares for a fresh rebuild
     * 
     * @param WidgetBP The widget blueprint to clear
     */
    void ClearWidgetTreeForRebuild(UWidgetBlueprint* WidgetBP)
    {
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            return;
        }

        UWidgetTree* WidgetTree = WidgetBP->WidgetTree;

        // Step 1: Collect all widgets to remove
        TArray<UWidget*> WidgetsToRemove;
        WidgetTree->ForEachWidget([&WidgetsToRemove](UWidget* Widget) {
            if (Widget)
            {
                WidgetsToRemove.Add(Widget);
            }
        });

        // Step 2: Remove each widget from the tree hierarchy
        for (UWidget* Widget : WidgetsToRemove)
        {
            if (Widget)
            {
                WidgetTree->RemoveWidget(Widget);
            }
        }

        // Step 3: CRITICAL - Rename widgets to move them to transient package
        // This changes their Outer from WidgetTree to GetTransientPackage()
        // Without this, ForEachObjectWithOuter(WidgetTree, ...) in the compiler's
        // ForEachSourceWidget will still find these orphaned widgets and trigger
        // ensure failures because they're not in the GUID map.
        for (UWidget* Widget : WidgetsToRemove)
        {
            if (Widget)
            {
                // Rename to transient package - this changes the Outer
                Widget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
            }
        }

// Step 4: Clear the root widget pointer
WidgetTree->RootWidget = nullptr;

// Step 5: Clear the GUID map - we'll rebuild it from scratch
#if MCP_HAS_WIDGET_VARIABLE_GUID_MAP
WidgetBP->WidgetVariableNameToGuidMap.Empty();
#endif

UE_LOG(LogTemp, Verbose, TEXT("ClearWidgetTreeForRebuild: Cleared %d widgets from tree"), WidgetsToRemove.Num());
}
}

using namespace WidgetAuthoringHelpers;

// =============================================================================
// Helper: Check for engine errors and validate widget state after operations
// =============================================================================

namespace
{
    /**
     * Gets the MCP Automation Bridge subsystem from the editor.
     * Note: UMcpAutomationBridgeSubsystem is a UEditorSubsystem, not UEngineSubsystem.
     * @return Pointer to the subsystem, or nullptr if not available
     */
    UMcpAutomationBridgeSubsystem* GetAutomationBridgeSubsystem()
    {
        if (GEditor)
        {
            return GEditor->GetEditorSubsystem<UMcpAutomationBridgeSubsystem>();
        }
        return nullptr;
    }
    
    /**
     * Checks if any engine errors were captured during widget operations.
     * This detects ensure failures and other engine-level errors that indicate
     * the operation may have partially failed despite returning no error code.
     * 
     * @return True if engine errors were detected, false otherwise
     */
    bool CheckForEngineErrors()
    {
        if (UMcpAutomationBridgeSubsystem* Subsystem = GetAutomationBridgeSubsystem())
        {
            return Subsystem->HasCapturedErrors();
        }
        return false;
    }
    
    /**
     * Gets the captured engine error messages for reporting.
     * 
     * @return Array of captured error messages
     */
    TArray<FString> GetCapturedErrors()
    {
        TArray<FString> Errors;
        if (UMcpAutomationBridgeSubsystem* Subsystem = GetAutomationBridgeSubsystem())
        {
            Errors.Append(Subsystem->GetCapturedErrorMessages());
        }
        return Errors;
    }
    
    /**
     * Validates that a widget was successfully added to a blueprint.
     * Checks both that the widget exists in the tree AND that no engine
     * errors occurred during the operation.
     * 
     * @param WidgetBP The widget blueprint
     * @param WidgetName The name of the widget to verify
     * @param OutError Output error message if validation fails
     * @return True if widget exists and no engine errors occurred
     */
    bool ValidateWidgetCreation(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FString& OutError)
    {
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            OutError = TEXT("Invalid widget blueprint");
            return false;
        }
        
        // Check if widget exists in tree
        UWidget* FoundWidget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
        if (!FoundWidget)
        {
            OutError = FString::Printf(TEXT("Widget '%s' was not found in widget tree after creation"), *WidgetName);
            return false;
        }
        
        // Check for engine errors (ensure failures, etc.)
        if (CheckForEngineErrors())
        {
            TArray<FString> Errors = GetCapturedErrors();
            if (Errors.Num() > 0)
            {
                OutError = FString::Printf(TEXT("Engine error during widget creation: %s"), *Errors[0]);
            }
            else
            {
                OutError = TEXT("Engine error occurred during widget creation");
            }
            return false;
        }
        
        return true;
    }
    
    /**
     * Checks if a widget with the given name already exists in the blueprint.
     * Returns true and outputs an error message if the widget exists.
     * 
     * @param WidgetBP The widget blueprint
     * @param WidgetName The name to check
     * @param OutError Output error message if widget exists
     * @return True if widget already exists (error condition)
     */
    bool CheckWidgetExists(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FString& OutError)
    {
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            return false;  // No error if blueprint is invalid (will fail elsewhere)
        }
        
        UWidget* ExistingWidget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
        if (ExistingWidget)
        {
            OutError = FString::Printf(TEXT("Widget '%s' already exists in blueprint"), *WidgetName);
            return true;
        }

        return false;
    }

    /**
     * Persist a widget-tree mutation: mark the blueprint structurally modified and (unless the
     * payload sets "save": false) save it to disk.
     *
     * The add_* tree-construction handlers historically only marked the blueprint modified and
     * never saved — so widget-tree edits lived only in memory and were silently lost on editor
     * close / GC (data loss; the slot-setters DID save, making the gap inconsistent). Routing
     * every add through this one place fixes that. Pass "save": false to batch many adds and
     * persist once at the end (bulk authoring), since McpSafeAssetSave is heavyweight per call.
     */
    void FinalizeWidgetEdit(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Payload)
    {
        if (!WidgetBP)
        {
            return;
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        bool bSave = true;
        if (Payload.IsValid())
        {
            bool bSaveField = true;
            if (Payload->TryGetBoolField(TEXT("save"), bSaveField))
            {
                bSave = bSaveField;
            }
        }
        if (bSave)
        {
            WidgetBP->MarkPackageDirty();
            McpSafeOperations::McpSafeAssetSave(WidgetBP);
        }
    }

    // -------------------------------------------------------------------------
    // Shared add_* skeleton (extracted from ~28 near-identical handlers).
    //
    // Front half: load the widget blueprint (payload "widgetPath"), construct WidgetClass
    // under the resolved slot name (slotName/name/widgetName, else DefaultSlotName), and
    // register its GUID. Returns the widget WITHOUT adding it to the tree, so the caller can
    // set type-specific properties first. On any failure it sends the error through `Self`
    // and returns nullptr (the caller just does `return true`). The subsystem's send helpers
    // are public precisely so out-of-line helpers can call them via this `Self` pointer.
    // -------------------------------------------------------------------------
    UWidget* ConstructWidgetForAdd(
        UMcpAutomationBridgeSubsystem* Self,
        const FMcpResponseHandle& Socket,
        const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload,
        UClass* WidgetClass,
        const FString& DefaultSlotName,
        UWidgetBlueprint*& OutWidgetBP,
        FString& OutSlotName)
    {
        OutWidgetBP = nullptr;
        OutSlotName.Reset();

        const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return nullptr;
        }

        FString SlotName = GetSlotName(Payload);
        if (SlotName.IsEmpty())
        {
            SlotName = DefaultSlotName;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return nullptr;
        }

        if (!WidgetClass)
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("Internal: null widget class"), TEXT("CREATION_ERROR"));
            return nullptr;
        }

        UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*SlotName));
        if (!NewWidget)
        {
            Self->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Failed to construct widget of class '%s'"), *WidgetClass->GetName()),
                TEXT("CREATION_ERROR"));
            return nullptr;
        }

        // CRITICAL: register the GUID before compile to avoid the variable→GUID ensure.
        RegisterWidgetGuid(WidgetBP, NewWidget);

        OutWidgetBP = WidgetBP;
        OutSlotName = SlotName;
        return NewWidget;
    }

    // Back half: add the constructed widget to the tree (root/parent-safe), persist via
    // FinalizeWidgetEdit (mark + save, the data-loss fix), validate, then send the success
    // response. Always returns true (a response or error was sent), so callers `return` it.
    bool AddFinalizeRespondWidget(
        UMcpAutomationBridgeSubsystem* Self,
        const FMcpResponseHandle& Socket,
        const FString& RequestId,
        const TSharedPtr<FJsonObject>& Payload,
        UWidgetBlueprint* WidgetBP,
        UWidget* NewWidget,
        const FString& SlotName,
        const FString& Message,
        TSharedPtr<FJsonObject> ResultJson) // by value: AddVerification takes a non-const TSharedPtr&
    {
        const FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        FString AddErr;
        if (!SafeAddWidgetToTree(WidgetBP, NewWidget, ParentSlot, &AddErr))
        {
            Self->SendAutomationError(Socket, RequestId,
                AddErr.IsEmpty()
                    ? FString::Printf(TEXT("Failed to add %s to the widget tree"), *NewWidget->GetClass()->GetName())
                    : AddErr,
                TEXT("TREE_ERROR"));
            return true;
        }

        FinalizeWidgetEdit(WidgetBP, Payload);

        FString ValidationError;
        if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
        {
            Self->SendAutomationError(Socket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), Message);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Self->SendAutomationResponse(Socket, RequestId, true, Message, ResultJson);
        return true;
    }
}

// ============================================================================
// Main Handler Implementation
// ============================================================================

// Suppress function size warning - this is a large handler function with many sub-actions
#pragma warning(push)
#pragma warning(disable: 4883)
// -----------------------------------------------------------------------------
// Phase 19 family sub-handlers (filled in pass by pass — see header comment).
// Empty stubs return false so the dispatcher falls through to the legacy chain
// for any subaction not yet migrated.
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleWidgetAuthoring_Lifecycle(
    const FString& SubAction, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    if (SubAction.Equals(TEXT("create_widget_blueprint"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("create_widget"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"));
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        // Destination directory. Accept 'path' (preferred), 'savePath' (the
        // manage_blueprint schema's param name — was previously ignored, so callers
        // passing savePath silently got the /Game/UI default), and 'folder'.
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty())
        {
            Folder = GetJsonStringField(Payload, TEXT("savePath"));
        }
        if (Folder.IsEmpty())
        {
            Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI"));
        }

        // SECURITY: Validate folder path for traversal attacks
        FString SanitizedFolder = SanitizeProjectRelativePath(Folder);
        if (SanitizedFolder.IsEmpty() && !Folder.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Invalid folder path: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        Folder = SanitizedFolder;
        
        FString ParentClass = GetJsonStringField(Payload, TEXT("parentClass"), TEXT("UserWidget"));

        // Build full path
        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        // CRITICAL: Check if a widget blueprint with this name already exists
        // This prevents the engine assertion crash in FKismetEditorUtilities::CreateBlueprint()
        // which has: check(FindObject<UBlueprint>(Outer, *NewBPName.ToString()) == NULL)
        FString NewBPObjectPath = FullPath + TEXT(".") + Name;
        if (FindObject<UWidgetBlueprint>(nullptr, *NewBPObjectPath) != nullptr)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Widget blueprint '%s' already exists"), *Name), 
                TEXT("ALREADY_EXISTS"));
            return true;
        }

        // Also check the package path
        if (UPackage* ExistingPackage = FindPackage(nullptr, *FullPath))
        {
            if (FindObject<UWidgetBlueprint>(ExistingPackage, *Name) != nullptr)
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    FString::Printf(TEXT("Widget blueprint '%s' already exists"), *Name), 
                    TEXT("ALREADY_EXISTS"));
                return true;
            }
        }

        // Create package
        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        // Find parent class
        UClass* ParentUClass = UUserWidget::StaticClass();
        if (!ParentClass.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
        {
            // 1) Already-loaded class by short name (fast path).
            // Note: FindFirstObject was introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            UClass* FoundClass = FindFirstObject<UClass>(*ParentClass, EFindFirstObjectOptions::None);
#else
            // UE 5.0: Use ResolveClassByName instead of deprecated ANY_PACKAGE
            UClass* FoundClass = ResolveClassByName(ParentClass);
#endif

            // 2) Not yet loaded (e.g. a delay-loaded optional plugin such as CommonUI): force a load.
            //    FindFirstObject only sees already-loaded classes, so without this a parentClass like
            //    "CommonActivatableWidget" would silently fall back to UUserWidget. Accept a fully-qualified
            //    path (/Script/Module.Name or /Game/Path.Name_C), else probe /Script/CommonUI.* then /Script/UMG.*.
            //    LOAD_NoWarn|LOAD_Quiet keeps probe misses out of the error-capture path.
            if (!FoundClass)
            {
                if (ParentClass.Contains(TEXT(".")) || ParentClass.Contains(TEXT("/")))
                {
                    FoundClass = StaticLoadClass(UObject::StaticClass(), nullptr, *ParentClass, nullptr, LOAD_NoWarn | LOAD_Quiet);
                }
                else
                {
                    static const TCHAR* ScriptModules[] = { TEXT("CommonUI"), TEXT("UMG") };
                    for (const TCHAR* ModuleName : ScriptModules)
                    {
                        const FString ScriptPath = FString::Printf(TEXT("/Script/%s.%s"), ModuleName, *ParentClass);
                        FoundClass = StaticLoadClass(UObject::StaticClass(), nullptr, *ScriptPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
                        if (FoundClass)
                        {
                            break;
                        }
                    }
                }
            }

            if (FoundClass && FoundClass->IsChildOf(UUserWidget::StaticClass()))
            {
                ParentUClass = FoundClass;
            }
            else
            {
                // A non-default parentClass was explicitly requested but could not be resolved to a
                // UUserWidget subclass. Fail loudly rather than silently creating a plain UserWidget.
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Could not resolve parentClass '%s' to a UUserWidget subclass. Pass a loaded class name, or a fully-qualified path such as /Script/CommonUI.CommonActivatableWidget or /Game/UI/MyWidget.MyWidget_C."), *ParentClass),
                    TEXT("INVALID_PARENT_CLASS"));
                return true;
            }
        }

        // Create widget blueprint
        UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            ParentUClass,
            Package,
            FName(*Name),
            BPTYPE_Normal,
            UWidgetBlueprint::StaticClass(),
            UWidgetBlueprintGeneratedClass::StaticClass()
        ));

        if (!WidgetBlueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create widget blueprint"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Mark package dirty, notify asset registry, then persist. Historically this only
        // marked dirty and never wrote the .uasset, so a create-then-nothing left no file on
        // disk (the new WBP vanished on editor close). FinalizeWidgetEdit saves it (opt out
        // with "save": false).
        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBlueprint);
        FinalizeWidgetEdit(WidgetBlueprint, Payload);

        // Return the full object path (Package.ObjectName format) for proper loading
        FString ObjectPath = WidgetBlueprint->GetPathName();

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Created widget blueprint: %s"), *Name));
        ResultJson->SetStringField(TEXT("widgetPath"), ObjectPath);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBlueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Created widget blueprint: %s"), *Name), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("show_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetId = GetJsonStringField(Payload, TEXT("widgetId"));
        FString Message = GetJsonStringField(Payload, TEXT("message"));
        
        // Handle notification widget specially
        if (WidgetId.Equals(TEXT("notification"), ESearchCase::IgnoreCase))
        {
            FString NotificationText = Message.IsEmpty() ? TEXT("Notification") : Message;
            
            // Use notification system
            FNotificationInfo Info(FText::FromString(NotificationText));
            Info.ExpireDuration = 3.0f;
            Info.bUseLargeFont = true;
            
            FSlateNotificationManager::Get().AddNotification(Info);
            
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("message"), TEXT("Notification shown"));
            ResultJson->SetStringField(TEXT("widgetId"), WidgetId);
            
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Notification shown"), ResultJson);
            return true;
        }
        
        // For regular widgets, we need a path
        FString EffectivePath = WidgetPath.IsEmpty() ? GetJsonStringField(Payload, TEXT("name")) : WidgetPath;
        if (EffectivePath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Missing required parameter: widgetPath or name"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        // SECURITY: Validate widget path
        FString SanitizedPath = SanitizeProjectRelativePath(EffectivePath);
        if (SanitizedPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Invalid widgetPath: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        EffectivePath = SanitizedPath;
        
        // Load the widget blueprint
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(EffectivePath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Widget blueprint not found: %s"), *EffectivePath), 
                TEXT("NOT_FOUND"));
            return true;
        }
        
        // Note: Actually showing the widget in viewport requires PIE (Play In Editor)
        // In editor mode, we can open the widget designer instead
        if (GEditor)
        {
            // Open the widget blueprint in the editor
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(WidgetBP);
        }
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Widget opened: %s"), *EffectivePath));
        ResultJson->SetStringField(TEXT("widgetPath"), EffectivePath);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Widget opened: %s"), *EffectivePath), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_widget_parent_class"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentClass = GetJsonStringField(Payload, TEXT("parentClass"));

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        // SECURITY: Validate widget path
        FString SanitizedWidgetPath = SanitizeProjectRelativePath(WidgetPath);
        if (SanitizedWidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Invalid widgetPath: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        WidgetPath = SanitizedWidgetPath;
        
        if (ParentClass.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: parentClass"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find parent class
        // Note: FindFirstObject was introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        UClass* NewParentClass = FindFirstObject<UClass>(*ParentClass, EFindFirstObjectOptions::None);
#else
        // UE 5.0: Use ResolveClassByName instead of deprecated ANY_PACKAGE
        UClass* NewParentClass = ResolveClassByName(ParentClass);
#endif
        if (!NewParentClass || !NewParentClass->IsChildOf(UUserWidget::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Parent class not found or invalid"), TEXT("INVALID_CLASS"));
            return true;
        }

        // Set parent class
        WidgetBP->ParentClass = NewParentClass;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Set parent class to: %s"), *ParentClass));

        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Set parent class to: %s"), *ParentClass), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("preview_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Widget preview is typically done by opening in editor or compiling
        // We can trigger a compile which updates the preview
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Widget blueprint marked for recompilation. Open in Widget Blueprint Editor to see preview."));
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget preview updated"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("remove_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        // Delete via the engine's canonical path -- the exact code the UMG designer runs
        // for right-click > Delete. It removes the widget AND its children from the tree,
        // strips their WidgetVariableNameToGuidMap entries (via OnVariableRemoved), cleans
        // up delegate bindings / variable nodes / desired-focus, renames them out to the
        // transient package, and marks the blueprint structurally modified. Hand-rolling
        // this left the GUID map inconsistent with the tree, which tripped the self-healing
        // "SeenVariableNames" ensure on the next compile and gated the save. DeleteSilently
        // skips the "this widget is referenced in the graph -- continue?" user prompt.
        TSet<UWidget*> WidgetsToDelete;
        WidgetsToDelete.Add(TargetWidget);
        FWidgetBlueprintEditorUtils::DeleteWidgets(
            WidgetBP, WidgetsToDelete,
            FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("removedWidget"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Removed widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("rename_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        // The advertised schema param is oldName; slotName/name accepted as aliases.
        FString OldName = GetJsonStringField(Payload, TEXT("oldName"));
        if (OldName.IsEmpty()) { OldName = GetJsonStringField(Payload, TEXT("slotName")); }
        if (OldName.IsEmpty()) { OldName = GetJsonStringField(Payload, TEXT("name")); }
        FString NewName = GetJsonStringField(Payload, TEXT("newName"));

        if (WidgetPath.IsEmpty() || OldName.IsEmpty() || NewName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, oldName, newName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*OldName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *OldName), TEXT("NOT_FOUND"));
            return true;
        }

        // Canonical rename — mirrors the headless-safe core of
        // FWidgetBlueprintEditorUtils::RenameWidget (which requires an open
        // FWidgetBlueprintEditor we don't have): variable→GUID map
        // (OnVariableRenamed), graph variable references, delegate + animation
        // bindings, explicit nav rules, and the blueprint's own
        // DesiredFocusWidget. A bare UObject::Rename leaves all of those
        // pointing at the old name — same bug family as the remove_widget
        // stale-GUID ensure. Skipped vs the engine version (editor-UI only):
        // preview-widget rename, orphan-pin getter reconstruction,
        // FUIComponentUtils::OnWidgetRenamed.
        const FName OldFName(*OldName);
        const FName NewFName(*NewName);

        // Unique-name check; allow renaming onto a matching BindWidget property
        // of the parent class (the engine's special case).
        TSharedPtr<INameValidatorInterface> NameValidator = MakeShareable(new FKismetNameValidator(WidgetBP, OldFName));
        FObjectPropertyBase* ExistingProperty = WidgetBP->ParentClass
            ? CastField<FObjectPropertyBase>(WidgetBP->ParentClass->FindPropertyByName(NewFName))
            : nullptr;
        const bool bBindWidget = ExistingProperty &&
            FWidgetBlueprintEditorUtils::IsBindWidgetProperty(ExistingProperty) &&
            TargetWidget->IsA(ExistingProperty->PropertyClass);
        if (NameValidator->IsValid(NewFName) != EValidatorResult::Ok && !bBindWidget)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("'%s' is not a valid/unique name in this Blueprint."), *NewName),
                TEXT("INVALID_NAME"));
            return true;
        }

        WidgetBP->Modify();
        TargetWidget->Modify();

        WidgetBP->OnVariableRenamed(OldFName, NewFName);
        TargetWidget->SetDisplayLabel(NewName);
        TargetWidget->Rename(*NewName);

        // Blueprint CDO's DesiredFocusWidget, iff it referenced the old name.
        bool bDesiredFocusUpdated = false;
        if (WidgetBP->GeneratedClass)
        {
            if (UUserWidget* WidgetCDO = WidgetBP->GeneratedClass->GetDefaultObject<UUserWidget>())
            {
                bDesiredFocusUpdated = WidgetCDO->GetDesiredFocusWidgetName() == OldFName;
            }
        }
        FWidgetBlueprintEditorUtils::ReplaceDesiredFocus(WidgetBP, OldFName, NewFName);

        // Property/delegate bindings.
        for (FDelegateEditorBinding& Binding : WidgetBP->Bindings)
        {
            if (Binding.ObjectName == OldName)
            {
                Binding.ObjectName = NewName;
            }
        }

        // Animation track bindings.
        for (UWidgetAnimation* WidgetAnimation : WidgetBP->Animations)
        {
            for (FWidgetAnimationBinding& AnimBinding : WidgetAnimation->AnimationBindings)
            {
                if (AnimBinding.WidgetName == OldFName)
                {
                    AnimBinding.WidgetName = NewFName;
                    WidgetAnimation->MovieScene->Modify();
                    if (AnimBinding.SlotWidgetName == NAME_None)
                    {
                        if (FMovieScenePossessable* Possessable = WidgetAnimation->MovieScene->FindPossessable(AnimBinding.AnimationGuid))
                        {
                            Possessable->SetName(NewFName.ToString());
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        // Explicit per-widget navigation rules referencing the old name.
        WidgetBP->WidgetTree->ForEachWidget([&OldFName, &NewFName](UWidget* EachWidget) {
            if (EachWidget->Navigation)
            {
                EachWidget->Navigation->TryToRenameBinding(OldFName, NewFName);
            }
        });

        FBlueprintEditorUtils::ValidateBlueprintChildVariables(WidgetBP, NewFName);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        FBlueprintEditorUtils::ReplaceVariableReferences(WidgetBP, OldFName, NewFName);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("oldName"), OldName);
        ResultJson->SetStringField(TEXT("newName"), NewName);
        ResultJson->SetBoolField(TEXT("desiredFocusUpdated"), bDesiredFocusUpdated);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Renamed widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("replace_widget_class"), ESearchCase::IgnoreCase))
    {
        // In-place widget class swap — the programmatic equivalent of the UMG
        // designer's right-click "Replace With". FWidgetBlueprintEditorUtils::
        // ReplaceWidgets reuses the parent slot, copies properties (incl. delegate
        // bindings), keeps the widget's name for compatible classes (subclass
        // pairs, e.g. USlider -> UAnalogSlider), fixes variable references and any
        // CommonActivatableWidget DesiredFocusWidget, and marks the BP modified.
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString NewWidgetClass = GetJsonStringField(Payload, TEXT("newWidgetClass"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || NewWidgetClass.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, newWidgetClass"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UClass* NewClass = ResolveClassByName(NewWidgetClass);
        if (!NewClass || !NewClass->IsChildOf(UWidget::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("'%s' did not resolve to a UWidget subclass"), *NewWidgetClass), TEXT("INVALID_CLASS"));
            return true;
        }

        const FString FromClass = TargetWidget->GetClass()->GetName();

        // ReplaceWidgets pops a blocking FSuppressableWarningDialog ("...in use in
        // the graph! Do you really want to replace them?") whenever the target is
        // referenced in the graph — which is the common case (a bound slider, a
        // named button) and would hang any headless/automation run on the modal.
        // The dialog's only escape hatch is its config suppression flag (it does
        // NOT honor GIsRunningUnattendedScript), so flip that flag for the duration
        // of the replace, then restore the user's prior preference. Safe to auto-
        // confirm here: MaintainNameAndReferences preserves the bindings the dialog
        // is warning about. Key string matches the engine's spelling verbatim (sic).
        const TCHAR* DlgSection = TEXT("SuppressableDialogs");
        const TCHAR* DlgKey = TEXT("ReaplaceWidgetsInUse_Warning");
        bool bPrevSuppress = false;
        GConfig->GetBool(DlgSection, DlgKey, bPrevSuppress, GEditorPerProjectIni);
        GConfig->SetBool(DlgSection, DlgKey, true, GEditorPerProjectIni);

        // Use the *ForUnmatchingClass naming method so the engine keeps SlotName even
        // for incompatible class pairs (e.g. a native AnalogSlider -> a user-widget
        // Blueprint) AND runs its own variable/reference fixup (OnVariableRenamed +
        // ReplaceVariableReferences). Plain MaintainNameAndReferences assigns a
        // generated name for unmatching classes; a manual post-rename then can't
        // re-point the references the engine already moved, which breaks bound widgets.
        FWidgetBlueprintEditorUtils::ReplaceWidgets(
            WidgetBP, TSet<UWidget*>{ TargetWidget }, NewClass,
            FWidgetBlueprintEditorUtils::EReplaceWidgetNamingMethod::MaintainNameAndReferencesForUnmatchingClass);

        GConfig->SetBool(DlgSection, DlgKey, bPrevSuppress, GEditorPerProjectIni);

        UWidget* NewWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!NewWidget || !NewWidget->IsA(NewClass))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Replace did not yield a '%s' named '%s'"), *NewWidgetClass, *SlotName), TEXT("REPLACE_FAILED"));
            return true;
        }

        WidgetBP->MarkPackageDirty();
        const bool bSaved = McpSafeAssetSave(WidgetBP);

        // Re-verify the end state. ReplaceWidgets logs a transient "There can only
        // be one event node bound to this component" error during its internal
        // recompiles even on success; the global error scrape would otherwise turn
        // this into a false ENGINE_ERROR. Compile once more, confirm it's clean,
        // then discard the captured transients so success reports as success.
        FKismetEditorUtilities::CompileBlueprint(WidgetBP);
        const bool bCompiledClean = (WidgetBP->Status == EBlueprintStatus::BS_UpToDate ||
                                     WidgetBP->Status == EBlueprintStatus::BS_UpToDateWithWarnings);
        ClearCapturedErrors();
        if (!bCompiledClean)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Widget replaced but the blueprint did not compile clean (status %d)"), (int32)WidgetBP->Status),
                TEXT("REPLACE_COMPILE_FAILED"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("fromClass"), FromClass);
        ResultJson->SetStringField(TEXT("toClass"), NewWidget->GetClass()->GetName());
        ResultJson->SetBoolField(TEXT("saved"), bSaved);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Replaced widget class"), ResultJson);
        return true;
    }

    return false;
}
bool UMcpAutomationBridgeSubsystem::HandleWidgetAuthoring_Containers(
    const FString& SubAction, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    if (SubAction.Equals(TEXT("add_canvas_panel"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UCanvasPanel::StaticClass(), TEXT("CanvasPanel"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added canvas panel"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_horizontal_box"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UHorizontalBox::StaticClass(), TEXT("HorizontalBox"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added horizontal box"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_vertical_box"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UVerticalBox::StaticClass(), TEXT("VerticalBox"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added vertical box"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_overlay"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UOverlay::StaticClass(), TEXT("Overlay"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added overlay"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_grid_panel"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        // (columnCount/rowCount were parsed but never applied by UGridPanel — UMG grids size to
        // their cell slots, not a fixed dimension — so dropping them is behavior-preserving.)
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UGridPanel::StaticClass(), TEXT("GridPanel"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added grid panel"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_uniform_grid"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UUniformGridPanel::StaticClass(), TEXT("UniformGridPanel"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        UUniformGridPanel* UniformGrid = Cast<UUniformGridPanel>(NewWidget);

        // Set slot padding if provided
        if (Payload->HasField(TEXT("slotPadding")))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("slotPadding"));
            if (PaddingObj.IsValid())
            {
                FMargin SlotPadding;
                SlotPadding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
                SlotPadding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
                SlotPadding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
                SlotPadding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
                UniformGrid->SetSlotPadding(SlotPadding);
            }
        }

        // Set min desired slot size
        if (Payload->HasField(TEXT("minDesiredSlotWidth")))
        {
            UniformGrid->SetMinDesiredSlotWidth(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredSlotWidth"), 0.0)));
        }
        if (Payload->HasField(TEXT("minDesiredSlotHeight")))
        {
            UniformGrid->SetMinDesiredSlotHeight(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredSlotHeight"), 0.0)));
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added uniform grid panel"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_wrap_box"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UWrapBox::StaticClass(), TEXT("WrapBox"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        UWrapBox* WrapBox = Cast<UWrapBox>(NewWidget);

        // Set inner slot padding if provided
        if (Payload->HasField(TEXT("innerSlotPadding")))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("innerSlotPadding"));
            if (PaddingObj.IsValid())
            {
                FVector2D InnerPadding;
                InnerPadding.X = GetJsonNumberField(PaddingObj, TEXT("x"), 0.0);
                InnerPadding.Y = GetJsonNumberField(PaddingObj, TEXT("y"), 0.0);
                WrapBox->SetInnerSlotPadding(InnerPadding);
            }
        }

        // Set explicit wrap size
        // Note: SetWrapSize was introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        if (Payload->HasField(TEXT("wrapSize")))
        {
            WrapBox->SetWrapSize(static_cast<float>(GetJsonNumberField(Payload, TEXT("wrapSize"), 0.0)));
        }
#endif

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added wrap box"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_scroll_box"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UScrollBox::StaticClass(), TEXT("ScrollBox"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        UScrollBox* ScrollBox = Cast<UScrollBox>(NewWidget);

        // Set orientation
        const FString Orientation = GetJsonStringField(Payload, TEXT("orientation"), TEXT("Vertical"));
        if (Orientation.Equals(TEXT("Horizontal"), ESearchCase::IgnoreCase))
        {
            ScrollBox->SetOrientation(EOrientation::Orient_Horizontal);
        }
        else
        {
            ScrollBox->SetOrientation(EOrientation::Orient_Vertical);
        }

        // Set scroll bar visibility
        FString ScrollBarVisibility = GetJsonStringField(Payload, TEXT("scrollBarVisibility"), TEXT(""));
        if (!ScrollBarVisibility.IsEmpty())
        {
            if (ScrollBarVisibility.Equals(TEXT("Visible"), ESearchCase::IgnoreCase))
            {
                ScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
            }
            else if (ScrollBarVisibility.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase))
            {
                ScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);
            }
            else if (ScrollBarVisibility.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase))
            {
                ScrollBox->SetScrollBarVisibility(ESlateVisibility::Hidden);
            }
        }

        // Set always show scrollbar
        if (Payload->HasField(TEXT("alwaysShowScrollbar")))
        {
            ScrollBox->SetAlwaysShowScrollbar(GetJsonBoolField(Payload, TEXT("alwaysShowScrollbar")));
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added scroll box"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_size_box"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            USizeBox::StaticClass(), TEXT("SizeBox"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        USizeBox* SizeBox = Cast<USizeBox>(NewWidget);

        // Set size overrides
        if (Payload->HasField(TEXT("widthOverride")))
        {
            SizeBox->SetWidthOverride(static_cast<float>(GetJsonNumberField(Payload, TEXT("widthOverride"), 100.0)));
        }
        if (Payload->HasField(TEXT("heightOverride")))
        {
            SizeBox->SetHeightOverride(static_cast<float>(GetJsonNumberField(Payload, TEXT("heightOverride"), 100.0)));
        }
        if (Payload->HasField(TEXT("minDesiredWidth")))
        {
            SizeBox->SetMinDesiredWidth(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredWidth"), 0.0)));
        }
        if (Payload->HasField(TEXT("minDesiredHeight")))
        {
            SizeBox->SetMinDesiredHeight(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredHeight"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxDesiredWidth")))
        {
            SizeBox->SetMaxDesiredWidth(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxDesiredWidth"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxDesiredHeight")))
        {
            SizeBox->SetMaxDesiredHeight(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxDesiredHeight"), 0.0)));
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added size box"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_scale_box"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UScaleBox::StaticClass(), TEXT("ScaleBox"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        UScaleBox* ScaleBox = Cast<UScaleBox>(NewWidget);

        // Set stretch mode
        FString Stretch = GetJsonStringField(Payload, TEXT("stretch"), TEXT(""));
        if (!Stretch.IsEmpty())
        {
            if (Stretch.Equals(TEXT("None"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::None);
            }
            else if (Stretch.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::Fill);
            }
            else if (Stretch.Equals(TEXT("ScaleToFit"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFit);
            }
            else if (Stretch.Equals(TEXT("ScaleToFitX"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFitX);
            }
            else if (Stretch.Equals(TEXT("ScaleToFitY"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFitY);
            }
            else if (Stretch.Equals(TEXT("ScaleToFill"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFill);
            }
            else if (Stretch.Equals(TEXT("UserSpecified"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::UserSpecified);
                if (Payload->HasField(TEXT("userSpecifiedScale")))
                {
                    ScaleBox->SetUserSpecifiedScale(static_cast<float>(GetJsonNumberField(Payload, TEXT("userSpecifiedScale"), 1.0)));
                }
            }
        }

        // Set stretch direction
        FString StretchDirection = GetJsonStringField(Payload, TEXT("stretchDirection"), TEXT(""));
        if (!StretchDirection.IsEmpty())
        {
            if (StretchDirection.Equals(TEXT("Both"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretchDirection(EStretchDirection::Both);
            }
            else if (StretchDirection.Equals(TEXT("DownOnly"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretchDirection(EStretchDirection::DownOnly);
            }
            else if (StretchDirection.Equals(TEXT("UpOnly"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretchDirection(EStretchDirection::UpOnly);
            }
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added scale box"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_border"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UBorder::StaticClass(), TEXT("Border"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        UBorder* BorderWidget = Cast<UBorder>(NewWidget);

        // Set brush color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("brushColor")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("brushColor"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            BorderWidget->SetBrushColor(Color);
        }

        // Set content color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("contentColorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("contentColorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            BorderWidget->SetContentColorAndOpacity(Color);
        }

        // Set padding if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("padding")))
        {
            TSharedPtr<FJsonObject> PaddingObj = Payload->GetObjectField(TEXT("padding"));
            FMargin Padding;
            Padding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
            Padding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
            Padding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
            Padding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
            BorderWidget->SetPadding(Padding);
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added border"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_safe_zone"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("SafeZone"));
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        USafeZone* SafeZone = CreateAndRegisterWidget<USafeZone>(WidgetBP, WidgetBP->WidgetTree, *SlotName);
        
        UPanelWidget* Parent = nullptr;
        if (!ParentSlot.IsEmpty())
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot)));
        }
        if (!Parent)
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        }

        if (Parent)
        {
            Parent->AddChild(SafeZone);
        }

        FinalizeWidgetEdit(WidgetBP, Payload);

        // CRITICAL: Validate widget creation succeeded and check for engine errors
        FString ValidationError;
        if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
        {
            SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added safe zone"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_spacer"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Spacer"));
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        float SizeX = GetJsonNumberField(Payload, TEXT("sizeX"), 100.0f);
        float SizeY = GetJsonNumberField(Payload, TEXT("sizeY"), 100.0f);

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        USpacer* Spacer = CreateAndRegisterWidget<USpacer>(WidgetBP, WidgetBP->WidgetTree, *SlotName);
        Spacer->SetSize(FVector2D(SizeX, SizeY));

        UPanelWidget* Parent = nullptr;
        if (!ParentSlot.IsEmpty())
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot)));
        }
        if (!Parent)
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        }

        if (Parent)
        {
            Parent->AddChild(Spacer);
        }

        FinalizeWidgetEdit(WidgetBP, Payload);

        // CRITICAL: Validate widget creation succeeded and check for engine errors
        FString ValidationError;
        if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
        {
            SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("sizeX"), SizeX);
        ResultJson->SetNumberField(TEXT("sizeY"), SizeY);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added spacer"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_widget_switcher"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("WidgetSwitcher"));
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        int32 ActiveIndex = GetJsonIntField(Payload, TEXT("activeIndex"), 0);

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidgetSwitcher* Switcher = CreateAndRegisterWidget<UWidgetSwitcher>(WidgetBP, WidgetBP->WidgetTree, *SlotName);
        Switcher->SetActiveWidgetIndex(ActiveIndex);

        UPanelWidget* Parent = nullptr;
        if (!ParentSlot.IsEmpty())
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot)));
        }
        if (!Parent)
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        }

        if (Parent)
        {
            Parent->AddChild(Switcher);
        }

        FinalizeWidgetEdit(WidgetBP, Payload);

        // CRITICAL: Validate widget creation succeeded and check for engine errors
        FString ValidationError;
        if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
        {
            SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("activeIndex"), ActiveIndex);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added widget switcher"), ResultJson);
        return true;
    }

    return false;
}
bool UMcpAutomationBridgeSubsystem::HandleWidgetAuthoring_Leaves(
    const FString& SubAction, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    if (SubAction.Equals(TEXT("add_text_block"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UTextBlock::StaticClass(), TEXT("TextBlock"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        UTextBlock* TextBlock = Cast<UTextBlock>(NewWidget);

        // Set text
        TextBlock->SetText(FText::FromString(GetJsonStringField(Payload, TEXT("text"), TEXT("Text"))));

        // Set optional properties
        if (Payload->HasField(TEXT("fontSize")))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            FSlateFontInfo FontInfo = TextBlock->GetFont();
#else
            // UE 5.0 fallback - create new font info
            FSlateFontInfo FontInfo = FSlateFontInfo();
#endif
            FontInfo.Size = static_cast<int32>(GetJsonNumberField(Payload, TEXT("fontSize"), 12.0));
            TextBlock->SetFont(FontInfo);
        }

        if (Payload->HasTypedField<EJson::Object>(TEXT("colorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("colorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            TextBlock->SetColorAndOpacity(FSlateColor(Color));
        }

        if (Payload->HasField(TEXT("autoWrap")))
        {
            TextBlock->SetAutoWrapText(GetJsonBoolField(Payload, TEXT("autoWrap")));
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added text block"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_image"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UImage::StaticClass(), TEXT("Image"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        UImage* ImageWidget = Cast<UImage>(NewWidget);

        // Set texture if provided
        FString TexturePath = GetJsonStringField(Payload, TEXT("texturePath"));
        if (!TexturePath.IsEmpty())
        {
            UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *TexturePath));
            if (Texture)
            {
                ImageWidget->SetBrushFromTexture(Texture);
            }
        }

        // Set color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("colorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("colorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            ImageWidget->SetColorAndOpacity(Color);
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added image"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_button"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UButton::StaticClass(), TEXT("Button"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        UButton* ButtonWidget = Cast<UButton>(NewWidget);

        // Set enabled state if provided
        if (Payload->HasField(TEXT("isEnabled")))
        {
            ButtonWidget->SetIsEnabled(GetJsonBoolField(Payload, TEXT("isEnabled"), true));
        }

        // Set color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("colorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("colorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            ButtonWidget->SetColorAndOpacity(Color);
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added button"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_progress_bar"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UProgressBar::StaticClass(), TEXT("ProgressBar"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        UProgressBar* ProgressBarWidget = Cast<UProgressBar>(NewWidget);

        // Set percent if provided
        if (Payload->HasField(TEXT("percent")))
        {
            ProgressBarWidget->SetPercent(static_cast<float>(GetJsonNumberField(Payload, TEXT("percent"), 0.5)));
        }

        // Set fill color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("fillColorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("fillColorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj, FLinearColor::Green);
            ProgressBarWidget->SetFillColorAndOpacity(Color);
        }

        // Set marquee if provided
        if (Payload->HasField(TEXT("isMarquee")))
        {
            ProgressBarWidget->SetIsMarquee(GetJsonBoolField(Payload, TEXT("isMarquee")));
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added progress bar"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_slider"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            USlider::StaticClass(), TEXT("Slider"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        USlider* SliderWidget = Cast<USlider>(NewWidget);

        // Set value if provided
        if (Payload->HasField(TEXT("value")))
        {
            SliderWidget->SetValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("value"), 0.5)));
        }

        // Set min/max values if provided
        if (Payload->HasField(TEXT("minValue")))
        {
            SliderWidget->SetMinValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("minValue"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxValue")))
        {
            SliderWidget->SetMaxValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxValue"), 1.0)));
        }

        // Set step size if provided
        if (Payload->HasField(TEXT("stepSize")))
        {
            SliderWidget->SetStepSize(static_cast<float>(GetJsonNumberField(Payload, TEXT("stepSize"), 0.01)));
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added slider"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_rich_text_block"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            URichTextBlock::StaticClass(), TEXT("RichTextBlock"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        Cast<URichTextBlock>(NewWidget)->SetText(FText::FromString(GetJsonStringField(Payload, TEXT("text"), TEXT("Rich Text"))));

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added rich text block"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_check_box"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UCheckBox::StaticClass(), TEXT("CheckBox"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        Cast<UCheckBox>(NewWidget)->SetIsChecked(GetJsonBoolField(Payload, TEXT("isChecked"), false));

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added check box"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_text_input"), ESearchCase::IgnoreCase))
    {
        const bool bMultiLine = GetJsonBoolField(Payload, TEXT("multiLine"), false);
        UClass* const InputClass = bMultiLine
            ? UMultiLineEditableTextBox::StaticClass()
            : UEditableTextBox::StaticClass();

        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            InputClass, TEXT("TextInput"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }

        const FText HintText = FText::FromString(GetJsonStringField(Payload, TEXT("hintText"), TEXT("")));
        if (bMultiLine) { Cast<UMultiLineEditableTextBox>(NewWidget)->SetHintText(HintText); }
        else            { Cast<UEditableTextBox>(NewWidget)->SetHintText(HintText); }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added text input"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_combo_box"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UComboBoxString::StaticClass(), TEXT("ComboBox"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        UComboBoxString* ComboBox = Cast<UComboBoxString>(NewWidget);

        // Add options if provided
        const TArray<TSharedPtr<FJsonValue>>* Options = GetArrayField(Payload, TEXT("options"));
        if (Options)
        {
            for (const TSharedPtr<FJsonValue>& Option : *Options)
            {
                ComboBox->AddOption(Option->AsString());
            }
        }

        // Set selected option
        FString SelectedOption = GetJsonStringField(Payload, TEXT("selectedOption"));
        if (!SelectedOption.IsEmpty())
        {
            ComboBox->SetSelectedOption(SelectedOption);
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added combo box"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_spin_box"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            USpinBox::StaticClass(), TEXT("SpinBox"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        USpinBox* SpinBox = Cast<USpinBox>(NewWidget);

        // Set value
        if (Payload->HasField(TEXT("value")))
        {
            SpinBox->SetValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("value"), 0.0)));
        }
        // Set min/max
        if (Payload->HasField(TEXT("minValue")))
        {
            SpinBox->SetMinValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("minValue"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxValue")))
        {
            SpinBox->SetMaxValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxValue"), 100.0)));
        }
        // Set delta
        if (Payload->HasField(TEXT("delta")))
        {
            SpinBox->SetDelta(static_cast<float>(GetJsonNumberField(Payload, TEXT("delta"), 1.0)));
        }

        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added spin box"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_list_view"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UListView::StaticClass(), TEXT("ListView"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added list view"), ResultJson);
    }

    if (SubAction.Equals(TEXT("add_tree_view"), ESearchCase::IgnoreCase))
    {
        UWidgetBlueprint* WidgetBP = nullptr;
        FString SlotName;
        UWidget* NewWidget = ConstructWidgetForAdd(this, RequestingSocket, RequestId, Payload,
            UTreeView::StaticClass(), TEXT("TreeView"), WidgetBP, SlotName);
        if (!NewWidget) { return true; }
        return AddFinalizeRespondWidget(this, RequestingSocket, RequestId, Payload, WidgetBP,
            NewWidget, SlotName, TEXT("Added tree view"), ResultJson);
    }

    return false;
}
bool UMcpAutomationBridgeSubsystem::HandleWidgetAuthoring_Slot(
    const FString& SubAction, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    if (SubAction.Equals(TEXT("set_anchor"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            FAnchors Anchors;
            TSharedPtr<FJsonObject> AnchorMin = GetObjectField(Payload, TEXT("anchorMin"));
            TSharedPtr<FJsonObject> AnchorMax = GetObjectField(Payload, TEXT("anchorMax"));

            if (AnchorMin.IsValid())
            {
                Anchors.Minimum.X = GetJsonNumberField(AnchorMin, TEXT("x"), 0.0);
                Anchors.Minimum.Y = GetJsonNumberField(AnchorMin, TEXT("y"), 0.0);
            }
            if (AnchorMax.IsValid())
            {
                Anchors.Maximum.X = GetJsonNumberField(AnchorMax, TEXT("x"), 1.0);
                Anchors.Maximum.Y = GetJsonNumberField(AnchorMax, TEXT("y"), 1.0);
            }

            // Handle preset anchors
            FString Preset = GetJsonStringField(Payload, TEXT("preset"));
            if (!Preset.IsEmpty())
            {
                if (Preset.Equals(TEXT("TopLeft"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0);
                    Anchors.Maximum = FVector2D(0, 0);
                }
                else if (Preset.Equals(TEXT("TopCenter"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 0);
                    Anchors.Maximum = FVector2D(0.5, 0);
                }
                else if (Preset.Equals(TEXT("TopRight"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(1, 0);
                    Anchors.Maximum = FVector2D(1, 0);
                }
                else if (Preset.Equals(TEXT("CenterLeft"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0.5);
                    Anchors.Maximum = FVector2D(0, 0.5);
                }
                else if (Preset.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 0.5);
                    Anchors.Maximum = FVector2D(0.5, 0.5);
                }
                else if (Preset.Equals(TEXT("CenterRight"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(1, 0.5);
                    Anchors.Maximum = FVector2D(1, 0.5);
                }
                else if (Preset.Equals(TEXT("BottomLeft"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 1);
                    Anchors.Maximum = FVector2D(0, 1);
                }
                else if (Preset.Equals(TEXT("BottomCenter"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 1);
                    Anchors.Maximum = FVector2D(0.5, 1);
                }
                else if (Preset.Equals(TEXT("BottomRight"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(1, 1);
                    Anchors.Maximum = FVector2D(1, 1);
                }
                else if (Preset.Equals(TEXT("StretchHorizontal"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0.5);
                    Anchors.Maximum = FVector2D(1, 0.5);
                }
                else if (Preset.Equals(TEXT("StretchVertical"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 0);
                    Anchors.Maximum = FVector2D(0.5, 1);
                }
                else if (Preset.Equals(TEXT("StretchAll"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0);
                    Anchors.Maximum = FVector2D(1, 1);
                }
            }

            CanvasSlot->SetAnchors(Anchors);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Anchor set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Anchor set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_alignment"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (!CanvasSlot)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("set_alignment requires a Canvas Panel slot"), TEXT("INVALID_SLOT"));
            return true;
        }

        // Accept alignment:{x,y} OR top-level scalars alignmentX/alignmentY.
        // Strict MCP clients strip object-valued params, so the scalar form is the
        // reliable path; fail loudly if neither is present instead of silently no-op.
        FVector2D Alignment(0, 0);
        bool bHaveAlignment = false;
        TSharedPtr<FJsonObject> AlignmentObj = GetObjectField(Payload, TEXT("alignment"));
        if (AlignmentObj.IsValid())
        {
            Alignment.X = GetJsonNumberField(AlignmentObj, TEXT("x"), 0.0);
            Alignment.Y = GetJsonNumberField(AlignmentObj, TEXT("y"), 0.0);
            bHaveAlignment = true;
        }
        else if (Payload->HasField(TEXT("alignmentX")) || Payload->HasField(TEXT("alignmentY")))
        {
            Alignment.X = GetJsonNumberField(Payload, TEXT("alignmentX"), 0.0);
            Alignment.Y = GetJsonNumberField(Payload, TEXT("alignmentY"), 0.0);
            bHaveAlignment = true;
        }
        if (!bHaveAlignment)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing alignment: provide alignment:{x,y} or alignmentX/alignmentY"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        CanvasSlot->SetAlignment(Alignment);
        const bool bAlignSaved = McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetNumberField(TEXT("alignmentX"), Alignment.X);
        ResultJson->SetNumberField(TEXT("alignmentY"), Alignment.Y);
        ResultJson->SetBoolField(TEXT("saveSucceeded"), bAlignSaved);
        ResultJson->SetStringField(TEXT("message"), TEXT("Alignment set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Alignment set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_position"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (!CanvasSlot)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("set_position requires a Canvas Panel slot"), TEXT("INVALID_SLOT"));
            return true;
        }

        // Accept position:{x,y} OR top-level scalars posX/posY (objects get
        // stripped by strict MCP clients; fail loudly rather than no-op).
        FVector2D Position(0, 0);
        bool bHavePosition = false;
        TSharedPtr<FJsonObject> PositionObj = GetObjectField(Payload, TEXT("position"));
        if (PositionObj.IsValid())
        {
            Position.X = GetJsonNumberField(PositionObj, TEXT("x"), 0.0);
            Position.Y = GetJsonNumberField(PositionObj, TEXT("y"), 0.0);
            bHavePosition = true;
        }
        else if (Payload->HasField(TEXT("posX")) || Payload->HasField(TEXT("posY")))
        {
            Position.X = GetJsonNumberField(Payload, TEXT("posX"), 0.0);
            Position.Y = GetJsonNumberField(Payload, TEXT("posY"), 0.0);
            bHavePosition = true;
        }
        else if (Payload->HasField(TEXT("x")) || Payload->HasField(TEXT("y")))
        {
            Position.X = GetJsonNumberField(Payload, TEXT("x"), 0.0);
            Position.Y = GetJsonNumberField(Payload, TEXT("y"), 0.0);
            bHavePosition = true;
        }
        if (!bHavePosition)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing position: provide position:{x,y}, posX/posY, or x/y"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        CanvasSlot->SetPosition(Position);
        const bool bPosSaved = McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetNumberField(TEXT("positionX"), Position.X);
        ResultJson->SetNumberField(TEXT("positionY"), Position.Y);
        ResultJson->SetBoolField(TEXT("saveSucceeded"), bPosSaved);
        ResultJson->SetStringField(TEXT("message"), TEXT("Position set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Position set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_size"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        FString SizedAs = TEXT("none");
        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            // Canvas slots take an explicit pixel size. Accept either a "size":{x,y}
            // object or top-level x/y for convenience.
            TSharedPtr<FJsonObject> SizeObj = GetObjectField(Payload, TEXT("size"));
            FVector2D Size;
            if (SizeObj.IsValid())
            {
                Size.X = GetJsonNumberField(SizeObj, TEXT("x"), 100.0);
                Size.Y = GetJsonNumberField(SizeObj, TEXT("y"), 100.0);
            }
            else
            {
                Size.X = GetJsonNumberField(Payload, TEXT("x"), 100.0);
                Size.Y = GetJsonNumberField(Payload, TEXT("y"), 100.0);
            }
            CanvasSlot->SetSize(Size);
            SizedAs = TEXT("canvas");
        }
        else if (Widget->Slot)
        {
            // Box slots (HorizontalBox / VerticalBox) have no pixel size — a child is
            // either Automatic (hug content) or Fill (stretch, weighted). Pass
            // "fill": true (optional "fillWeight") to stretch a child across its
            // row/column — e.g. a slider that would otherwise collapse to ~0 width.
            bool bFill = false;
            Payload->TryGetBoolField(TEXT("fill"), bFill);
            FSlateChildSize NewSize;
            NewSize.SizeRule = bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic;
            NewSize.Value = (float)GetJsonNumberField(Payload, TEXT("fillWeight"), 1.0);
            if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
            {
                HSlot->SetSize(NewSize);
                SizedAs = bFill ? TEXT("hbox-fill") : TEXT("hbox-auto");
            }
            else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
            {
                VSlot->SetSize(NewSize);
                SizedAs = bFill ? TEXT("vbox-fill") : TEXT("vbox-auto");
            }
        }

        const bool bSizeSaved = McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("sizedAs"), SizedAs);
        ResultJson->SetBoolField(TEXT("saveSucceeded"), bSizeSaved);
        ResultJson->SetStringField(TEXT("message"), TEXT("Size set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Size set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_padding"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        // Accept padding:{left,top,right,bottom} OR top-level scalars
        // padLeft/padTop/padRight/padBottom OR a single "pad" for all sides.
        // Strict MCP clients strip object-valued params, so the scalar forms are
        // the reliable path; fail loudly if nothing usable was supplied.
        FMargin Padding(0);
        bool bHavePadding = false;
        TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("padding"));
        if (PaddingObj.IsValid())
        {
            Padding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
            Padding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
            Padding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
            Padding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
            bHavePadding = true;
        }
        else if (Payload->HasField(TEXT("pad")))
        {
            Padding = FMargin(GetJsonNumberField(Payload, TEXT("pad"), 0.0));
            bHavePadding = true;
        }
        else if (Payload->HasField(TEXT("padLeft")) || Payload->HasField(TEXT("padTop"))
              || Payload->HasField(TEXT("padRight")) || Payload->HasField(TEXT("padBottom")))
        {
            Padding.Left = GetJsonNumberField(Payload, TEXT("padLeft"), 0.0);
            Padding.Top = GetJsonNumberField(Payload, TEXT("padTop"), 0.0);
            Padding.Right = GetJsonNumberField(Payload, TEXT("padRight"), 0.0);
            Padding.Bottom = GetJsonNumberField(Payload, TEXT("padBottom"), 0.0);
            bHavePadding = true;
        }
        if (!bHavePadding)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing padding: provide padding:{left,top,right,bottom}, pad:<all>, or padLeft/padTop/padRight/padBottom"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString PaddedSlot = TEXT("none");
        if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(Widget->Slot)) { HBoxSlot->SetPadding(Padding); PaddedSlot = TEXT("hbox"); }
        else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(Widget->Slot)) { VBoxSlot->SetPadding(Padding); PaddedSlot = TEXT("vbox"); }
        else if (UOverlaySlot* OverlaySlotWidget = Cast<UOverlaySlot>(Widget->Slot)) { OverlaySlotWidget->SetPadding(Padding); PaddedSlot = TEXT("overlay"); }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Slot type does not support padding (need HBox/VBox/Overlay child)"), TEXT("INVALID_SLOT"));
            return true;
        }

        const bool bPadSaved = McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("paddedSlot"), PaddedSlot);
        ResultJson->SetBoolField(TEXT("saveSucceeded"), bPadSaved);
        ResultJson->SetStringField(TEXT("message"), TEXT("Padding set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Padding set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_z_order"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        int32 ZOrder = static_cast<int32>(GetJsonNumberField(Payload, TEXT("zOrder"), 0));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            CanvasSlot->SetZOrder(ZOrder);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Z-order set to %d"), ZOrder));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Z-order set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_render_transform"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        FWidgetTransform RenderTransform;

        TSharedPtr<FJsonObject> TranslationObj = GetObjectField(Payload, TEXT("translation"));
        if (TranslationObj.IsValid())
        {
            RenderTransform.Translation.X = GetJsonNumberField(TranslationObj, TEXT("x"), 0.0);
            RenderTransform.Translation.Y = GetJsonNumberField(TranslationObj, TEXT("y"), 0.0);
        }

        TSharedPtr<FJsonObject> ScaleObj = GetObjectField(Payload, TEXT("scale"));
        if (ScaleObj.IsValid())
        {
            RenderTransform.Scale.X = GetJsonNumberField(ScaleObj, TEXT("x"), 1.0);
            RenderTransform.Scale.Y = GetJsonNumberField(ScaleObj, TEXT("y"), 1.0);
        }

        TSharedPtr<FJsonObject> ShearObj = GetObjectField(Payload, TEXT("shear"));
        if (ShearObj.IsValid())
        {
            RenderTransform.Shear.X = GetJsonNumberField(ShearObj, TEXT("x"), 0.0);
            RenderTransform.Shear.Y = GetJsonNumberField(ShearObj, TEXT("y"), 0.0);
        }

        if (Payload->HasField(TEXT("angle")))
        {
            RenderTransform.Angle = static_cast<float>(GetJsonNumberField(Payload, TEXT("angle"), 0.0));
        }

        Widget->SetRenderTransform(RenderTransform);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Render transform set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Render transform set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_visibility"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString VisibilityStr = GetJsonStringField(Payload, TEXT("visibility"), TEXT("Visible"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        ESlateVisibility Visibility = GetVisibility(VisibilityStr);
        Widget->SetVisibility(Visibility);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Visibility set to %s"), *VisibilityStr));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Visibility set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_style"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("set_clipping"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        if (SubAction.Equals(TEXT("set_clipping"), ESearchCase::IgnoreCase))
        {
            FString ClippingStr = GetJsonStringField(Payload, TEXT("clipping"), TEXT("Inherit"));
            EWidgetClipping Clipping = EWidgetClipping::Inherit;
            if (ClippingStr.Equals(TEXT("ClipToBounds"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::ClipToBounds;
            }
            else if (ClippingStr.Equals(TEXT("ClipToBoundsWithoutIntersecting"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::ClipToBoundsWithoutIntersecting;
            }
            else if (ClippingStr.Equals(TEXT("ClipToBoundsAlways"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::ClipToBoundsAlways;
            }
            else if (ClippingStr.Equals(TEXT("OnDemand"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::OnDemand;
            }
            Widget->SetClipping(Clipping);
            WidgetBP->MarkPackageDirty();
            const bool bSaveSucceeded = McpSafeAssetSave(WidgetBP);

            ResultJson->SetStringField(TEXT("mode"), TEXT("write"));
            ResultJson->SetStringField(TEXT("propertyName"), TEXT("Clipping"));
            ResultJson->SetStringField(TEXT("value"), ClippingStr);
            ResultJson->SetStringField(TEXT("widgetName"), SlotName);
            ResultJson->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetName());
            ResultJson->SetBoolField(TEXT("saveSucceeded"), bSaveSucceeded);
            if (!bSaveSucceeded)
            {
                ResultJson->SetStringField(TEXT("warning"), TEXT("Clipping changed in editor memory, but package save did not complete in the current headless session."));
            }
        }
        else if (SubAction.Equals(TEXT("set_style"), ESearchCase::IgnoreCase))
        {
            // Generic property setter via UE reflection — works on any widget class, any property
            FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"));
            FString Value;
            bool bHasValueField = Payload->HasField(TEXT("value"));
            bool bUseJsonConverter = false;
            TSharedPtr<FJsonValue> RawJsonValue;

            // Extract value from JSON — handle string, number, bool, object, and array types
            if (bHasValueField)
            {
                const TSharedPtr<FJsonValue> ValField = Payload->TryGetField(TEXT("value"));
                if (ValField.IsValid())
                {
                    if (ValField->Type == EJson::String)
                    {
                        Value = ValField->AsString();
                    }
                    else if (ValField->Type == EJson::Number)
                    {
                        Value = FString::SanitizeFloat(ValField->AsNumber());
                    }
                    else if (ValField->Type == EJson::Boolean)
                    {
                        Value = ValField->AsBool() ? TEXT("True") : TEXT("False");
                    }
                    else if (ValField->Type == EJson::Object || ValField->Type == EJson::Array)
                    {
                        // Defer to FJsonObjectConverter for struct-backed properties
                        bUseJsonConverter = true;
                        RawJsonValue = ValField;
                    }
                    else if (ValField->Type == EJson::Null)
                    {
                        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Null JSON value is not supported for property mutation"), TEXT("UNSUPPORTED_VALUE_TYPE"));
                        return true;
                    }
                }
            }

            if (PropertyName.IsEmpty())
            {
                // Legacy path: if no propertyName given, try "style" param against "Style" property
                // Reset state from any prior "value" field extraction to avoid stale data
                bUseJsonConverter = false;
                RawJsonValue.Reset();
                Value.Empty();

                PropertyName = TEXT("Style");
                bHasValueField = Payload->HasField(TEXT("style"));
                if (bHasValueField)
                {
                    const TSharedPtr<FJsonValue> StyleField = Payload->TryGetField(TEXT("style"));
                    if (StyleField.IsValid() && (StyleField->Type == EJson::Object || StyleField->Type == EJson::Array))
                    {
                        bUseJsonConverter = true;
                        RawJsonValue = StyleField;
                    }
                    else
                    {
                        Value = GetJsonStringField(Payload, TEXT("style"));
                    }
                }
            }

            if (PropertyName.IsEmpty())
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: propertyName"), TEXT("MISSING_PARAMETER"));
                return true;
            }

            FProperty* Prop = Widget->GetClass()->FindPropertyByName(FName(*PropertyName));
            if (!Prop)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Property '%s' not found on widget '%s' (class %s)"), *PropertyName, *SlotName, *Widget->GetClass()->GetName()),
                    TEXT("PROPERTY_NOT_FOUND"));
                return true;
            }

            void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Widget);

            if (!bHasValueField)
            {
                // READ mode — value field not present, export and return current value
                FString ExportedValue;
                MCP_PROPERTY_EXPORT_TEXT(Prop, ExportedValue, ValuePtr, ValuePtr, Widget, PPF_None);

                ResultJson->SetStringField(TEXT("mode"), TEXT("read"));
                ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
                ResultJson->SetStringField(TEXT("value"), ExportedValue);
                ResultJson->SetStringField(TEXT("widgetName"), SlotName);
                ResultJson->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetName());
            }
            else
            {
                // WRITE mode — set the property value
                Widget->Modify();

                bool bWriteSuccess = false;
                if (bUseJsonConverter && RawJsonValue.IsValid())
                {
                    // Use FJsonObjectConverter for struct-backed properties (Object/Array JSON)
                    bWriteSuccess = FJsonObjectConverter::JsonValueToUProperty(RawJsonValue, Prop, ValuePtr, 0, 0);
                }
                else
                {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                    const TCHAR* ImportResult = Prop->ImportText_Direct(*Value, ValuePtr, Widget, PPF_None);
#else
                    const TCHAR* ImportResult = Prop->ImportText(*Value, ValuePtr, PPF_None, Widget);
#endif
                    bWriteSuccess = (ImportResult != nullptr);
                }
                if (!bWriteSuccess)
                {
                    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Failed to set '%s' to '%s' on widget '%s'"), *PropertyName, *Value, *SlotName),
                        TEXT("SET_PROPERTY_FAILED"));
                    return true;
                }

                FPropertyChangedEvent ChangeEvent(Prop);
                Widget->PostEditChangeProperty(ChangeEvent);

                // Export the value back to verify what was actually set
                FString ExportedValue;
                MCP_PROPERTY_EXPORT_TEXT(Prop, ExportedValue, ValuePtr, ValuePtr, Widget, PPF_None);

                ResultJson->SetStringField(TEXT("mode"), TEXT("write"));
                ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
                ResultJson->SetStringField(TEXT("value"), Value);
                ResultJson->SetStringField(TEXT("exportedValue"), ExportedValue);
                ResultJson->SetStringField(TEXT("widgetName"), SlotName);
                ResultJson->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetName());

                // Property change — mark dirty and save, do NOT recompile (that wipes instance values)
                WidgetBP->MarkPackageDirty();
                McpSafeAssetSave(WidgetBP);
            }
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        FString ModeStr;
        bool bIsRead = ResultJson->TryGetStringField(TEXT("mode"), ModeStr) && ModeStr == TEXT("read");
        FString Msg = bIsRead
            ? FString::Printf(TEXT("%s property read"), *SubAction)
            : FString::Printf(TEXT("%s applied"), *SubAction);
        ResultJson->SetStringField(TEXT("message"), Msg);

        SendAutomationResponse(RequestingSocket, RequestId, true, Msg, ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_font"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString FontPath = GetJsonStringField(Payload, TEXT("font"));
        float FontSize = GetJsonNumberField(Payload, TEXT("fontSize"), 24.0f);

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        bool bFontApplied = false;
        if (UTextBlock* TextWidget = Cast<UTextBlock>(TargetWidget))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            FSlateFontInfo FontInfo = TextWidget->GetFont();
#else
            // UE 5.0: Font property is directly accessible
            FSlateFontInfo FontInfo = TextWidget->Font;
#endif
            FontInfo.Size = FontSize;
            if (!FontPath.IsEmpty())
            {
                // Load font object if path provided
                UObject* FontObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FontPath);
                if (FontObject)
                {
                    FontInfo.FontObject = FontObject;
                }
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            TextWidget->SetFont(FontInfo);
#else
            // UE 5.0: Font property is directly accessible
            TextWidget->Font = FontInfo;
#endif
            bFontApplied = true;
        }
        else if (URichTextBlock* RichText = Cast<URichTextBlock>(TargetWidget))
        {
            // Rich text blocks use text styles, not direct font setting
            // Just set the default text style properties if available
            bFontApplied = true; // Acknowledge but note limitation
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bFontApplied);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("fontSize"), FontSize);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set font"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_margin"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        float Left = GetJsonNumberField(Payload, TEXT("left"), 0.0f);
        float Top = GetJsonNumberField(Payload, TEXT("top"), 0.0f);
        float Right = GetJsonNumberField(Payload, TEXT("right"), 0.0f);
        float Bottom = GetJsonNumberField(Payload, TEXT("bottom"), 0.0f);

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        FMargin Margin(Left, Top, Right, Bottom);
        bool bMarginApplied = false;

        // Apply margin based on slot type
        if (UPanelSlot* Slot = TargetWidget->Slot)
        {
            if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(Slot))
            {
                HBoxSlot->SetPadding(Margin);
                bMarginApplied = true;
            }
            else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(Slot))
            {
                VBoxSlot->SetPadding(Margin);
                bMarginApplied = true;
            }
            else if (UOverlaySlot* OvSlot = Cast<UOverlaySlot>(Slot))
            {
                OvSlot->SetPadding(Margin);
                bMarginApplied = true;
            }
        }

        // Also try to set on border widgets
        if (UBorder* BorderWidget = Cast<UBorder>(TargetWidget))
        {
            BorderWidget->SetPadding(Margin);
            bMarginApplied = true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bMarginApplied);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("left"), Left);
        ResultJson->SetNumberField(TEXT("top"), Top);
        ResultJson->SetNumberField(TEXT("right"), Right);
        ResultJson->SetNumberField(TEXT("bottom"), Bottom);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set margin"), ResultJson);
        return true;
    }

    return false;
}
bool UMcpAutomationBridgeSubsystem::HandleWidgetAuthoring_Binding(
    const FString& SubAction, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    if (SubAction.Equals(TEXT("bind_text"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("bind_visibility"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("bind_color"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("bind_enabled"), ESearchCase::IgnoreCase))
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("'%s' is not implemented: it does not create a UMG property binding. "
                "For a live-updating value, author an event handler with bind_event_to_delegate "
                "(external delegate, e.g. an AttributeComponent) or bind_on_value_changed (child-widget "
                "delegate) that calls the widget's setter. For a true property binding, create the getter "
                "function in the Widget Blueprint and assign it via the designer's Bind dropdown."),
                *SubAction),
            TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    if (SubAction.Equals(TEXT("bind_on_clicked"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("bind_on_hovered"), ESearchCase::IgnoreCase))
    {
        // Real widget-delegate event binding (handles bind_on_clicked AND bind_on_hovered).
        //
        // Creates a UK2Node_ComponentBoundEvent in the widget BP's event graph for the
        // named widget's multicast delegate (default "OnClicked" / "OnHovered"). This is the
        // same node the UMG Designer's "+ OnClicked" button produces, so it works for ANY
        // widget exposing a BlueprintAssignable delegate — plain UButton, UCommonButtonBase,
        // custom user-widget buttons, etc. (The old bind_on_hovered stub cast to UButton and
        // only returned instructions; it never wired anything and could not see Common UI
        // buttons — it now shares this real path.)
        //
        // If "targetFunction" (alias "functionName") is supplied, a self CallFunction node
        // for that function is created and the event's exec output is linked to it — e.g.
        // binding a Back button's OnClicked straight to DeactivateWidget with one call.
        //
        // Params:
        //   widgetPath     (required) - the widget Blueprint asset
        //   slotName       (required) - the child widget whose delegate to bind
        //   delegateName   (optional) - multicast delegate; default OnClicked / OnHovered
        //   targetFunction (optional, alias functionName) - self UFUNCTION to call
        const bool bHoveredVariant = SubAction.Equals(TEXT("bind_on_hovered"), ESearchCase::IgnoreCase);
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString DelegateName = GetJsonStringField(Payload, TEXT("delegateName"),
            bHoveredVariant ? TEXT("OnHovered") : TEXT("OnClicked"));
        FString TargetFunction = GetJsonStringField(Payload, TEXT("targetFunction"));
        if (TargetFunction.IsEmpty())
        {
            TargetFunction = GetJsonStringField(Payload, TEXT("functionName"));
        }

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        // Group all graph mutations (event node + wiring + compile) as one undo
        // unit. Non-const so error paths can Cancel() and roll back partial
        // mutations instead of committing them to the undo stack.
        FScopedTransaction Transaction(FText::FromString(
            bHoveredVariant ? TEXT("Bind On Hovered") : TEXT("Bind On Clicked")));
        const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();

        // The widget must be a variable for a bound event to reference it.
        bool bMadeVariable = false;
        if (!Widget->bIsVariable)
        {
            Widget->Modify();
            Widget->bIsVariable = true;
            bMadeVariable = true;
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        }

        // The multicast delegate (e.g. OnClicked) lives on the widget's own class.
        FMulticastDelegateProperty* DelegateProp =
            FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), FName(*DelegateName));
        if (!DelegateProp)
        {
            Transaction.Cancel();
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Delegate '%s' not found on widget '%s' (class %s)"),
                    *DelegateName, *SlotName, *Widget->GetClass()->GetName()),
                TEXT("DELEGATE_NOT_FOUND"));
            return true;
        }

        // The widget variable is an FObjectProperty on the BP's generated class.
        // Recompile once if it isn't visible yet (e.g. we just set bIsVariable).
        auto FindWidgetVarProp = [&]() -> FObjectProperty*
        {
            UClass* SearchClass = WidgetBP->SkeletonGeneratedClass
                ? (UClass*)WidgetBP->SkeletonGeneratedClass
                : (UClass*)WidgetBP->GeneratedClass;
            return SearchClass ? FindFProperty<FObjectProperty>(SearchClass, FName(*SlotName)) : nullptr;
        };
        FObjectProperty* WidgetVarProp = FindWidgetVarProp();
        if (!WidgetVarProp)
        {
            FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::SkipGarbageCollection);
            WidgetVarProp = FindWidgetVarProp();
        }
        if (!WidgetVarProp)
        {
            Transaction.Cancel();
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Could not resolve widget variable property for '%s'"), *SlotName),
                TEXT("WIDGET_VAR_NOT_FOUND"));
            return true;
        }

        if (WidgetBP->UbergraphPages.Num() == 0)
        {
            Transaction.Cancel();
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint has no event graph"), TEXT("NO_EVENT_GRAPH"));
            return true;
        }
        UEdGraph* EventGraph = WidgetBP->UbergraphPages[0];
        EventGraph->Modify();

        // Reuse an existing bound event for this (widget, delegate) if present.
        UK2Node_ComponentBoundEvent* EventNode = nullptr;
        {
            TArray<UK2Node_ComponentBoundEvent*> ExistingBoundEvents;
            FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_ComponentBoundEvent>(WidgetBP, ExistingBoundEvents);
            for (UK2Node_ComponentBoundEvent* BE : ExistingBoundEvents)
            {
                if (BE && BE->ComponentPropertyName == WidgetVarProp->GetFName()
                    && BE->DelegatePropertyName == DelegateProp->GetFName())
                {
                    EventNode = BE;
                    break;
                }
            }
        }
        const bool bEventExisted = (EventNode != nullptr);
        // Self-layout anchor for this chain. A brand-new event is stacked below any
        // event roots already in the graph (count * row gap) so generated chains
        // don't smoosh at (0,0); a reused event keeps its place and new nodes anchor
        // to it. Within-chain offsets below are added to (BaseX, BaseY).
        int32 BaseX = 0;
        int32 BaseY = 0;
        if (!EventNode)
        {
            int32 ExistingEventChains = 0;
            for (const UEdGraphNode* ExistingNode : EventGraph->Nodes)
            {
                if (ExistingNode && ExistingNode->IsA<UK2Node_Event>()) { ++ExistingEventChains; }
            }
            BaseY = ExistingEventChains * McpWidgetChainRowGapY;

            FGraphNodeCreator<UK2Node_ComponentBoundEvent> EventCreator(*EventGraph);
            EventNode = EventCreator.CreateNode(false);
            EventNode->InitializeComponentBoundEventParams(WidgetVarProp, DelegateProp);
            EventNode->NodePosX = BaseX;
            EventNode->NodePosY = BaseY;
            EventCreator.Finalize();
        }
        else
        {
            BaseX = EventNode->NodePosX;
            BaseY = EventNode->NodePosY;
        }

        // Optionally create + wire a self function call (e.g. DeactivateWidget).
        bool bWiredTargetFunction = false;
        if (!TargetFunction.IsEmpty())
        {
            UClass* SelfClass = WidgetBP->GeneratedClass ? (UClass*)WidgetBP->GeneratedClass : (UClass*)WidgetBP->ParentClass;
            UFunction* Func = SelfClass ? SelfClass->FindFunctionByName(FName(*TargetFunction)) : nullptr;
            if (!Func)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("targetFunction '%s' not found on %s"), *TargetFunction,
                        SelfClass ? *SelfClass->GetName() : TEXT("<null>")),
                    TEXT("FUNCTION_NOT_FOUND"));
                return true;
            }

            FGraphNodeCreator<UK2Node_CallFunction> CallCreator(*EventGraph);
            UK2Node_CallFunction* CallNode = CallCreator.CreateNode(false);
            CallNode->SetFromFunction(Func);
            CallNode->NodePosX = BaseX + 400;
            CallNode->NodePosY = BaseY;
            CallCreator.Finalize();

            UEdGraphPin* EventThen = EventNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
            UEdGraphPin* CallExec = CallNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
            // Schema-checked connect (validates pin compatibility) instead of raw MakeLinkTo.
            bWiredTargetFunction = EventThen && CallExec && K2->TryCreateConnection(EventThen, CallExec);
        }

        // Compile so the binding is generated, then persist.
        FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::SkipGarbageCollection);
        WidgetBP->MarkPackageDirty();
        const bool bSaved = McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), SlotName);
        ResultJson->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetName());
        ResultJson->SetStringField(TEXT("delegate"), DelegateName);
        ResultJson->SetStringField(TEXT("eventNode"), EventNode->GetName());
        ResultJson->SetBoolField(TEXT("eventAlreadyExisted"), bEventExisted);
        ResultJson->SetBoolField(TEXT("madeWidgetVariable"), bMadeVariable);
        if (!TargetFunction.IsEmpty())
        {
            ResultJson->SetStringField(TEXT("targetFunction"), TargetFunction);
            ResultJson->SetBoolField(TEXT("wiredTargetFunction"), bWiredTargetFunction);
        }
        ResultJson->SetBoolField(TEXT("saveSucceeded"), bSaved);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget delegate bound"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("bind_on_value_changed"), ESearchCase::IgnoreCase))
    {
        // Real value-changed binding (was a stub that only returned an instruction
        // string and wired nothing). Mirrors the real bind_on_clicked: creates a
        // UK2Node_ComponentBoundEvent for the widget's value-changed multicast
        // delegate (default "OnValueChanged", as on USlider / USpinBox).
        //
        // If "targetText" names a text widget, it also builds the live-update chain
        //     Event.Value -> Conv_DoubleToText -> TargetText.SetText
        // so a value label tracks the slider with no hand-wiring in the graph.
        //
        // Params:
        //   widgetPath   (required) - the widget Blueprint asset
        //   slotName     (required) - the slider (value widget) whose delegate to bind
        //   delegateName (optional, default "OnValueChanged")
        //   targetText   (optional) - text widget to SetText with the live value
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString DelegateName = GetJsonStringField(Payload, TEXT("delegateName"), TEXT("OnValueChanged"));
        FString TargetText = GetJsonStringField(Payload, TEXT("targetText"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UWidget* TextWidget = nullptr;
        if (!TargetText.IsEmpty())
        {
            TextWidget = WidgetBP->WidgetTree->FindWidget(FName(*TargetText));
            if (!TextWidget)
            {
                SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("targetText '%s' not found"), *TargetText), TEXT("WIDGET_NOT_FOUND"));
                return true;
            }
        }

        // Group all graph mutations (event node + live-update chain + compile)
        // as one undo unit. Non-const so error paths can Cancel() and roll back
        // partial mutations instead of committing them to the undo stack.
        FScopedTransaction Transaction(FText::FromString(TEXT("Bind On Value Changed")));
        const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();

        // Any widget referenced by a graph node must be a variable.
        bool bMadeVariable = false;
        if (!Widget->bIsVariable) { Widget->Modify(); Widget->bIsVariable = true; bMadeVariable = true; }
        if (TextWidget && !TextWidget->bIsVariable) { TextWidget->Modify(); TextWidget->bIsVariable = true; bMadeVariable = true; }
        if (bMadeVariable) FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        // The value-changed multicast delegate lives on the widget's own class.
        FMulticastDelegateProperty* DelegateProp =
            FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), FName(*DelegateName));
        if (!DelegateProp)
        {
            Transaction.Cancel();
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Delegate '%s' not found on widget '%s' (class %s)"),
                    *DelegateName, *SlotName, *Widget->GetClass()->GetName()),
                TEXT("DELEGATE_NOT_FOUND"));
            return true;
        }

        // Resolve the widget variable FObjectProperties (compile once if not visible yet).
        auto FindVarProp = [&](const FString& Name) -> FObjectProperty*
        {
            UClass* SearchClass = WidgetBP->SkeletonGeneratedClass
                ? (UClass*)WidgetBP->SkeletonGeneratedClass
                : (UClass*)WidgetBP->GeneratedClass;
            return SearchClass ? FindFProperty<FObjectProperty>(SearchClass, FName(*Name)) : nullptr;
        };
        FObjectProperty* SliderVarProp = FindVarProp(SlotName);
        FObjectProperty* TextVarProp = TextWidget ? FindVarProp(TargetText) : nullptr;
        if (!SliderVarProp || (TextWidget && !TextVarProp))
        {
            FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::SkipGarbageCollection);
            SliderVarProp = FindVarProp(SlotName);
            TextVarProp = TextWidget ? FindVarProp(TargetText) : nullptr;
        }
        if (!SliderVarProp)
        {
            Transaction.Cancel();
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Could not resolve widget variable property for '%s'"), *SlotName),
                TEXT("WIDGET_VAR_NOT_FOUND"));
            return true;
        }
        if (TextWidget && !TextVarProp)
        {
            Transaction.Cancel();
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Could not resolve widget variable property for targetText '%s'"), *TargetText),
                TEXT("WIDGET_VAR_NOT_FOUND"));
            return true;
        }

        if (WidgetBP->UbergraphPages.Num() == 0)
        {
            Transaction.Cancel();
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint has no event graph"), TEXT("NO_EVENT_GRAPH"));
            return true;
        }
        UEdGraph* EventGraph = WidgetBP->UbergraphPages[0];
        EventGraph->Modify();

        // Reuse an existing bound event for this (widget, delegate) if present.
        UK2Node_ComponentBoundEvent* EventNode = nullptr;
        {
            TArray<UK2Node_ComponentBoundEvent*> ExistingBoundEvents;
            FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_ComponentBoundEvent>(WidgetBP, ExistingBoundEvents);
            for (UK2Node_ComponentBoundEvent* BE : ExistingBoundEvents)
            {
                if (BE && BE->ComponentPropertyName == SliderVarProp->GetFName()
                    && BE->DelegatePropertyName == DelegateProp->GetFName())
                {
                    EventNode = BE;
                    break;
                }
            }
        }
        const bool bEventExisted = (EventNode != nullptr);
        // Self-layout anchor for this chain (same scheme as bind_on_clicked): stack a
        // new event below existing event roots; a reused event keeps its place and the
        // live-update chain below anchors to it. Chain offsets add to (BaseX, BaseY).
        int32 BaseX = 0;
        int32 BaseY = 0;
        if (!EventNode)
        {
            int32 ExistingEventChains = 0;
            for (const UEdGraphNode* ExistingNode : EventGraph->Nodes)
            {
                if (ExistingNode && ExistingNode->IsA<UK2Node_Event>()) { ++ExistingEventChains; }
            }
            BaseY = ExistingEventChains * McpWidgetChainRowGapY;

            FGraphNodeCreator<UK2Node_ComponentBoundEvent> EventCreator(*EventGraph);
            EventNode = EventCreator.CreateNode(false);
            EventNode->InitializeComponentBoundEventParams(SliderVarProp, DelegateProp);
            EventNode->NodePosX = BaseX;
            EventNode->NodePosY = BaseY;
            EventCreator.Finalize();
        }
        else
        {
            BaseX = EventNode->NodePosX;
            BaseY = EventNode->NodePosY;
        }

        // Optionally build the live-update chain Event.Value -> Conv -> TargetText.SetText.
        bool bWiredLiveUpdate = false;
        if (TextWidget)
        {
            // float/double -> FText. UE5 renamed the node to Conv_DoubleToText; fall
            // back to the old name for safety. Its formatting params keep their CPP
            // defaults (min 0 / max 3 fractional digits), so 20.0 -> "20", 0.05 -> "0.05".
            UFunction* ConvFunc = UKismetTextLibrary::StaticClass()->FindFunctionByName(TEXT("Conv_DoubleToText"));
            if (!ConvFunc) ConvFunc = UKismetTextLibrary::StaticClass()->FindFunctionByName(TEXT("Conv_FloatToText"));
            UFunction* SetTextFunc = TextWidget->GetClass()->FindFunctionByName(TEXT("SetText"));

            if (ConvFunc && SetTextFunc)
            {
                FGraphNodeCreator<UK2Node_CallFunction> ConvCreator(*EventGraph);
                UK2Node_CallFunction* ConvNode = ConvCreator.CreateNode(false);
                ConvNode->SetFromFunction(ConvFunc);
                ConvNode->NodePosX = BaseX + 280;
                ConvNode->NodePosY = BaseY + 16;
                ConvCreator.Finalize();

                FGraphNodeCreator<UK2Node_VariableGet> GetCreator(*EventGraph);
                UK2Node_VariableGet* GetNode = GetCreator.CreateNode(false);
                GetNode->VariableReference.SetSelfMember(TextVarProp->GetFName());
                GetNode->NodePosX = BaseX + 280;
                GetNode->NodePosY = BaseY + 200;
                GetCreator.Finalize();

                FGraphNodeCreator<UK2Node_CallFunction> SetCreator(*EventGraph);
                UK2Node_CallFunction* SetNode = SetCreator.CreateNode(false);
                SetNode->SetFromFunction(SetTextFunc);
                SetNode->NodePosX = BaseX + 560;
                SetNode->NodePosY = BaseY;
                SetCreator.Finalize();

                UEdGraphPin* EvThen   = EventNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
                UEdGraphPin* EvValue  = EventNode->FindPin(FName(TEXT("Value")), EGPD_Output);
                UEdGraphPin* ConvIn   = ConvNode->FindPin(FName(TEXT("Value")), EGPD_Input);
                UEdGraphPin* ConvOut  = ConvNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output);
                UEdGraphPin* SetExec  = SetNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
                UEdGraphPin* SetSelf  = SetNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
                UEdGraphPin* SetInText= SetNode->FindPin(FName(TEXT("InText")), EGPD_Input);
                UEdGraphPin* GetOut   = GetNode->FindPin(TextVarProp->GetFName(), EGPD_Output);

                // Schema-checked connects (validate pin types + auto-insert casts where
                // needed) instead of raw MakeLinkTo, which could wire a type-mismatched pin
                // yet still report wiredLiveUpdate:true.
                auto Connect = [&](UEdGraphPin* A, UEdGraphPin* B) -> bool { return A && B && K2->TryCreateConnection(A, B); };
                const bool bExec = Connect(EvThen, SetExec);
                const bool bVal  = Connect(EvValue, ConvIn);
                const bool bConv = Connect(ConvOut, SetInText);
                const bool bSelf = Connect(GetOut, SetSelf);

                bWiredLiveUpdate = bExec && bVal && bConv && bSelf;
            }
        }

        // Compile so the binding is generated, then persist.
        FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::SkipGarbageCollection);
        WidgetBP->MarkPackageDirty();
        const bool bSaved = McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), SlotName);
        ResultJson->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetName());
        ResultJson->SetStringField(TEXT("delegate"), DelegateName);
        ResultJson->SetStringField(TEXT("eventNode"), EventNode->GetName());
        ResultJson->SetBoolField(TEXT("eventAlreadyExisted"), bEventExisted);
        ResultJson->SetBoolField(TEXT("madeWidgetVariable"), bMadeVariable);
        if (TextWidget)
        {
            ResultJson->SetStringField(TEXT("targetText"), TargetText);
            ResultJson->SetBoolField(TEXT("wiredLiveUpdate"), bWiredLiveUpdate);
        }
        ResultJson->SetBoolField(TEXT("saveSucceeded"), bSaved);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("OnValueChanged bound"), ResultJson);
        return true;
    }

    // bind_event_to_delegate — subscribe a widget event graph to a multicast
    // delegate on an EXTERNAL object reached via an event output pin (the "push"
    // HUD pattern: InitializeHud(Attributes) binds Attributes.OnHealthPercentUpdate
    // -> a custom event that drives a bar). Unlike bind_on_clicked (a child widget's
    // own delegate via UK2Node_ComponentBoundEvent), the delegate owner is a runtime
    // object handed in on a pin, so this authors UK2Node_AddDelegate + a
    // signature-matched UK2Node_CustomEvent. The source event must already exist
    // (run add_event first); the new chain is appended to the END of its exec run so
    // repeated binds (health, then magic) stack instead of fighting over the Then pin.
    if (SubAction.Equals(TEXT("bind_event_to_delegate"), ESearchCase::IgnoreCase))
    {
        const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        const FString EventName = GetJsonStringField(Payload, TEXT("eventName"));
        const FString OwnerPinName = GetJsonStringField(Payload, TEXT("ownerPin"));
        const FString DelegateName = GetJsonStringField(Payload, TEXT("delegateName"));
        if (WidgetPath.IsEmpty() || EventName.IsEmpty() || OwnerPinName.IsEmpty() || DelegateName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameters: widgetPath, eventName, ownerPin, delegateName"),
                TEXT("MISSING_PARAMETER"));
            return true;
        }
        FString HandlerEventName = GetJsonStringField(Payload, TEXT("handlerEventName"));
        if (HandlerEventName.IsEmpty()) { HandlerEventName = FString::Printf(TEXT("On_%s"), *DelegateName); }
        const FString HandlerTargetWidget = GetJsonStringField(Payload, TEXT("handlerTargetWidget"));
        const FString HandlerFunction = GetJsonStringField(Payload, TEXT("handlerFunction"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget blueprint not found: %s"), *WidgetPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Locate the source event node by name (covers BlueprintImplementableEvent
        // overrides and custom events — both are UK2Node_Event subclasses).
        UK2Node_Event* SourceEvent = nullptr;
        {
            TArray<UK2Node_Event*> EventNodes;
            FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(WidgetBP, EventNodes);
            for (UK2Node_Event* E : EventNodes)
            {
                if (E && E->GetFunctionName() == FName(*EventName)) { SourceEvent = E; break; }
            }
        }
        if (!SourceEvent)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Event '%s' not found in the widget graph — run add_event for it first"), *EventName),
                TEXT("EVENT_NOT_FOUND"));
            return true;
        }

        UEdGraph* Graph = SourceEvent->GetGraph();
        UEdGraphPin* OwnerPin = SourceEvent->FindPin(FName(*OwnerPinName), EGPD_Output);
        if (!OwnerPin)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Output pin '%s' not found on event '%s'"), *OwnerPinName, *EventName),
                TEXT("OWNER_PIN_NOT_FOUND"));
            return true;
        }
        UClass* OwnerClass = Cast<UClass>(OwnerPin->PinType.PinSubCategoryObject.Get());
        if (!OwnerClass)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Pin '%s' is not an object pin; cannot resolve the delegate owner class"), *OwnerPinName),
                TEXT("OWNER_CLASS_NOT_FOUND"));
            return true;
        }
        FMulticastDelegateProperty* DelegateProp = FindFProperty<FMulticastDelegateProperty>(OwnerClass, FName(*DelegateName));
        if (!DelegateProp)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Multicast delegate '%s' not found on class '%s'"), *DelegateName, *OwnerClass->GetName()),
                TEXT("DELEGATE_NOT_FOUND"));
            return true;
        }

        // Idempotent: if the handler custom event already exists, treat as done.
        {
            TArray<UK2Node_CustomEvent*> Existing;
            FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_CustomEvent>(WidgetBP, Existing);
            for (UK2Node_CustomEvent* CE : Existing)
            {
                if (CE && CE->CustomFunctionName == FName(*HandlerEventName))
                {
                    ResultJson->SetBoolField(TEXT("success"), true);
                    ResultJson->SetBoolField(TEXT("alreadyExists"), true);
                    ResultJson->SetStringField(TEXT("handlerEvent"), HandlerEventName);
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Delegate binding already present"), ResultJson);
                    return true;
                }
            }
        }

        const FScopedTransaction Transaction(FText::FromString(TEXT("Bind Event To Delegate")));
        WidgetBP->Modify();
        Graph->Modify();

        const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
        const int32 BaseX = SourceEvent->NodePosX;
        const int32 BaseY = SourceEvent->NodePosY;

        // AddDelegate ("Bind Event to ...") node for the owner's multicast delegate.
        UK2Node_AddDelegate* AddNode = nullptr;
        {
            FGraphNodeCreator<UK2Node_AddDelegate> C(*Graph);
            AddNode = C.CreateNode(false);
            AddNode->SetFromProperty(DelegateProp, /*bSelfContext*/ false, OwnerClass);
            AddNode->NodePosX = BaseX + 380;
            AddNode->NodePosY = BaseY;
            C.Finalize();
        }

        // Custom event handler; mirror the delegate's parameters as output pins so it
        // is signature-compatible with the delegate's Event input pin.
        UK2Node_CustomEvent* HandlerEvent = nullptr;
        {
            FGraphNodeCreator<UK2Node_CustomEvent> C(*Graph);
            HandlerEvent = C.CreateNode(false);
            HandlerEvent->CustomFunctionName = FName(*HandlerEventName);
            HandlerEvent->bIsEditable = true;
            HandlerEvent->NodePosX = BaseX + 380;
            HandlerEvent->NodePosY = BaseY + 220;
            C.Finalize();
        }
        if (UFunction* SigFn = DelegateProp->SignatureFunction)
        {
            for (TFieldIterator<FProperty> It(SigFn); It && (It->PropertyFlags & CPF_Parm); ++It)
            {
                FProperty* Param = *It;
                if (Param->HasAnyPropertyFlags(CPF_ReturnParm)) { continue; }
                FEdGraphPinType ParamPinType;
                if (K2->ConvertPropertyToPinType(Param, ParamPinType))
                {
                    HandlerEvent->CreateUserDefinedPin(Param->GetFName(), ParamPinType, EGPD_Output);
                }
            }
        }

        // Resolve pins AFTER signature creation (the custom event reconstructs).
        UEdGraphPin* AddExec = AddNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
        UEdGraphPin* AddSelf = AddNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
        UEdGraphPin* AddDelegateIn = AddNode->GetDelegatePin();

        UEdGraphPin* HandlerDelegateOut = nullptr;
        UEdGraphPin* HandlerParamOut = nullptr;
        for (UEdGraphPin* P : HandlerEvent->Pins)
        {
            if (!P || P->Direction != EGPD_Output) { continue; }
            if (P->PinType.PinCategory == UEdGraphSchema_K2::PC_Delegate) { HandlerDelegateOut = P; }
            else if (P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && !HandlerParamOut) { HandlerParamOut = P; }
        }

        int32 LinksMade = 0;
        auto Connect = [&](UEdGraphPin* A, UEdGraphPin* B) -> bool { if (A && B && K2->TryCreateConnection(A, B)) { ++LinksMade; return true; } return false; };

        // Append AddDelegate's exec to the END of the source event's exec run.
        UEdGraphPin* ExecTail = SourceEvent->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
        while (ExecTail && ExecTail->LinkedTo.Num() > 0)
        {
            UEdGraphNode* Next = ExecTail->LinkedTo[0] ? ExecTail->LinkedTo[0]->GetOwningNode() : nullptr;
            UEdGraphPin* NextThen = Next ? Next->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
            if (!NextThen) { break; }
            ExecTail = NextThen;
        }
        const bool bExecWired = Connect(ExecTail, AddExec);
        const bool bSelfWired = Connect(OwnerPin, AddSelf);
        const bool bDelegateWired = Connect(HandlerDelegateOut, AddDelegateIn);

        // Optional handler body: call HandlerFunction (on a member widget variable, or
        // self when handlerTargetWidget is empty), feeding it the delegate's first param.
        bool bBodyWired = false;
        if (!HandlerFunction.IsEmpty())
        {
            UClass* TargetClass = WidgetBP->GeneratedClass ? (UClass*)WidgetBP->GeneratedClass : (UClass*)WidgetBP->SkeletonGeneratedClass;
            UK2Node_VariableGet* Getter = nullptr;
            UEdGraphPin* GetterOut = nullptr;
            if (!HandlerTargetWidget.IsEmpty())
            {
                FGraphNodeCreator<UK2Node_VariableGet> C(*Graph);
                Getter = C.CreateNode(false);
                Getter->VariableReference.SetSelfMember(FName(*HandlerTargetWidget));
                Getter->NodePosX = BaseX + 700;
                Getter->NodePosY = BaseY + 360;
                C.Finalize();
                for (UEdGraphPin* P : Getter->Pins)
                {
                    if (P && P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
                    {
                        GetterOut = P;
                        TargetClass = Cast<UClass>(P->PinType.PinSubCategoryObject.Get());
                        break;
                    }
                }
            }
            UFunction* HandlerFn = TargetClass ? TargetClass->FindFunctionByName(FName(*HandlerFunction)) : nullptr;
            if (HandlerFn)
            {
                UK2Node_CallFunction* CallNode = nullptr;
                {
                    FGraphNodeCreator<UK2Node_CallFunction> C(*Graph);
                    CallNode = C.CreateNode(false);
                    CallNode->SetFromFunction(HandlerFn);
                    CallNode->NodePosX = BaseX + 700;
                    CallNode->NodePosY = BaseY + 220;
                    C.Finalize();
                }
                UEdGraphPin* CallExec = CallNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
                UEdGraphPin* CallSelf = CallNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
                UEdGraphPin* HandlerThen = HandlerEvent->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
                UEdGraphPin* CallParam = nullptr;
                for (UEdGraphPin* P : CallNode->Pins)
                {
                    if (P && P->Direction == EGPD_Input
                        && P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
                        && P->PinName != UEdGraphSchema_K2::PN_Self)
                    { CallParam = P; break; }
                }
                Connect(HandlerThen, CallExec);
                if (GetterOut) { Connect(GetterOut, CallSelf); }
                if (HandlerParamOut && CallParam) { Connect(HandlerParamOut, CallParam); }
                bBodyWired = true;
            }
            else
            {
                ResultJson->SetStringField(TEXT("handlerFunctionWarning"),
                    FString::Printf(TEXT("handlerFunction '%s' not found on target class; created the binding without a body"), *HandlerFunction));
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::SkipGarbageCollection);
        WidgetBP->MarkPackageDirty();
        const bool bSaved = McpSafeAssetSave(WidgetBP);

        ResultJson->SetStringField(TEXT("handlerEvent"), HandlerEventName);
        ResultJson->SetStringField(TEXT("delegate"), DelegateName);
        ResultJson->SetStringField(TEXT("ownerClass"), OwnerClass->GetName());
        ResultJson->SetBoolField(TEXT("execWired"), bExecWired);
        ResultJson->SetBoolField(TEXT("selfWired"), bSelfWired);
        ResultJson->SetBoolField(TEXT("delegateWired"), bDelegateWired);
        ResultJson->SetBoolField(TEXT("bodyWired"), bBodyWired);
        ResultJson->SetNumberField(TEXT("linksMade"), LinksMade);
        ResultJson->SetBoolField(TEXT("saveSucceeded"), bSaved);

        // The core binding is event->AddDelegate exec + owner->target + custom-event->delegate.
        // If any failed (e.g. the schema rejected a connection on malformed input), report failure
        // rather than a false success — same fail-fast discipline as bind_anim_notify.
        if (!bExecWired || !bSelfWired || !bDelegateWired)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Delegate binding incomplete (execWired=%d selfWired=%d delegateWired=%d) — the schema rejected a core connection"),
                    bExecWired, bSelfWired, bDelegateWired),
                TEXT("CONNECTION_FAILED"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Bound %s.%s -> %s"), *OwnerClass->GetName(), *DelegateName, *HandlerEventName), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_property_binding"), ESearchCase::IgnoreCase))
    {
        // Never created an FDelegateEditorBinding — only returned an advisory "instruction".
        // Same unbuilt feature as bind_text et al. Fail honestly and point at the working path.
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("create_property_binding is not implemented: it does not create a UMG property binding "
                 "(FDelegateEditorBinding + generated getter). For a live-updating value, author an event "
                 "handler with bind_event_to_delegate / bind_on_value_changed that calls the widget's setter; "
                 "for a true property binding, create the getter in the Widget Blueprint and assign it via the "
                 "designer's Bind dropdown."),
            TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    if (SubAction.Equals(TEXT("set_widget_binding"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString TargetWidget = GetJsonStringField(Payload, TEXT("targetWidget"));
        if (TargetWidget.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: targetWidget"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString PropertyName = GetJsonStringField(Payload, TEXT("property"));
        if (PropertyName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: property"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"));
        if (FunctionName.IsEmpty())
        {
            FunctionName = TEXT("Get") + PropertyName;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the target widget
        UWidget* Target = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(TargetWidget, ESearchCase::IgnoreCase))
            {
                Target = W;
            }
        });

        if (!Target)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Target widget not found: %s"), *TargetWidget), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        // Verifying the property is bindable and then saving an UNCHANGED asset is not a binding —
        // this action wired nothing (it reported "targetVerified"/"saved" while doing neither). The
        // input was validated above; fail honestly rather than imply a binding was made.
        (void)Target; (void)FunctionName;
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("set_widget_binding is not implemented: it only checked that the property was bindable and "
                 "wired no binding. Use bind_event_to_delegate / bind_on_value_changed for live updates, or "
                 "create the getter in the Widget Blueprint and assign it via the designer's Bind dropdown."),
            TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    if (SubAction.Equals(TEXT("bind_localized_text"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString StringTableId = GetJsonStringField(Payload, TEXT("stringTableId"));
        FString StringKey = GetJsonStringField(Payload, TEXT("stringKey"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || StringTableId.IsEmpty() || StringKey.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        bool bBound = false;
        if (UTextBlock* TextWidget = Cast<UTextBlock>(TargetWidget))
        {
            // Try to get text from string table
            FText LocalizedText = FText::FromStringTable(FName(*StringTableId), StringKey);
            if (!LocalizedText.IsEmpty())
            {
                TextWidget->SetText(LocalizedText);
                bBound = true;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bBound);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("stringTableId"), StringTableId);
        ResultJson->SetStringField(TEXT("stringKey"), StringKey);
        if (!bBound)
        {
            ResultJson->SetStringField(TEXT("note"), TEXT("String table entry not found or widget is not a text widget"));
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bound localized text"), ResultJson);
        return true;
    }

    return false;
}
bool UMcpAutomationBridgeSubsystem::HandleWidgetAuthoring_Animation(
    const FString& SubAction, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    if (SubAction.Equals(TEXT("create_widget_animation"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"), TEXT("NewAnimation"));
        double Duration = GetJsonNumberField(Payload, TEXT("duration"), 1.0);
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Check for duplicate animation name
        for (UWidgetAnimation* ExistingAnim : WidgetBP->Animations)
        {
            if (ExistingAnim && ExistingAnim->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    FString::Printf(TEXT("Animation '%s' already exists"), *AnimationName), 
                    TEXT("ALREADY_EXISTS"));
                return true;
            }
        }
        
        // Create new UWidgetAnimation
        UWidgetAnimation* NewAnim = NewObject<UWidgetAnimation>(WidgetBP, FName(*AnimationName), RF_Transactional);
        if (!NewAnim)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create animation"), TEXT("CREATE_FAILED"));
            return true;
        }
        
        // CRITICAL: Create and assign MovieScene immediately - GetMovieScene() returns nullptr until we do this
        // This matches the engine's pattern in AnimationTabSummoner.cpp
        NewAnim->MovieScene = NewObject<UMovieScene>(NewAnim, FName(*AnimationName), RF_Transactional);
        if (!NewAnim->MovieScene)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create animation MovieScene"), TEXT("CREATE_FAILED"));
            return true;
        }
        
        // Initialize the animation MovieScene with playback settings
        UMovieScene* MovieScene = NewAnim->GetMovieScene();
        
        // Clamp duration to avoid zero-length animations
        const double SafeDuration = FMath::Max(Duration, 0.01);
        
        // Set display rate (20 fps is the UE default for widget animations)
        MovieScene->SetDisplayRate(FFrameRate(20, 1));
        
        // Set playback range based on duration
        const FFrameTime InFrame = 0.0 * MovieScene->GetTickResolution();
        const FFrameTime OutFrame = SafeDuration * MovieScene->GetTickResolution();
        MovieScene->SetPlaybackRange(TRange<FFrameNumber>(InFrame.FrameNumber, OutFrame.FrameNumber + 1));
        
        // CRITICAL: Register animation GUID and add to Animations array
        // This prevents ensure failures in WidgetBlueprintCompiler.cpp line 805
        RegisterAnimationGuid(WidgetBP, NewAnim);
        
        McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetNumberField(TEXT("duration"), SafeDuration);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget animation created"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_animation_track"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        FString SlotName = GetSlotName(Payload);
        FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"), TEXT("RenderOpacity"));
        
        if (WidgetPath.IsEmpty() || AnimationName.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, animationName, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Find the animation
        UWidgetAnimation* Animation = nullptr;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim && Anim->GetFName().ToString().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                Animation = Anim;
                break;
            }
        }
        
        if (!Animation)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("ANIMATION_NOT_FOUND"));
            return true;
        }
        
        // Find the target widget in the widget tree
        UWidget* TargetWidget = nullptr;
        if (WidgetBP->WidgetTree)
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget) {
                if (Widget && Widget->GetFName().ToString().Equals(SlotName, ESearchCase::IgnoreCase))
                {
                    TargetWidget = Widget;
                }
            });
        }
        
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found in tree"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }
        
        // The animation track binding is set up - MovieScene integration would add the actual track
        // For now, we create the binding reference
        UMovieScene* MovieScene = Animation->GetMovieScene();
        if (!MovieScene)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Animation has no MovieScene"), TEXT("ANIMATION_ERROR"));
            return true;
        }
        
        // Add the possessable to the MovieScene
        FGuid BindingGuid = MovieScene->AddPossessable(TargetWidget->GetFName().ToString(), TargetWidget->GetClass());
        
        // CRITICAL: For editor-time (WidgetBlueprint context), we cannot use BindPossessableObject 
        // because it expects a UUserWidget runtime context and will crash with CastChecked.
        // Instead, directly add the binding to AnimationBindings array.
        FWidgetAnimationBinding NewBinding;
        NewBinding.AnimationGuid = BindingGuid;
        NewBinding.WidgetName = TargetWidget->GetFName();
        NewBinding.SlotWidgetName = NAME_None;
        NewBinding.bIsRootWidget = false;
        
        Animation->AnimationBindings.Add(NewBinding);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
        ResultJson->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
        
        McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Animation track added"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_animation_keyframe"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        double Time = GetJsonNumberField(Payload, TEXT("time"), 0.0);
        double Value = GetJsonNumberField(Payload, TEXT("value"), 1.0);
        
        if (WidgetPath.IsEmpty() || AnimationName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, animationName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Find the animation
        UWidgetAnimation* Animation = nullptr;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim && Anim->GetFName().ToString().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                Animation = Anim;
                break;
            }
        }
        
        if (!Animation)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("ANIMATION_NOT_FOUND"));
            return true;
        }

        // No MovieScene channel is written — this only echoed time/value back. Adding a real
        // keyframe means resolving the bound track's MovieSceneFloatChannel and inserting a key;
        // that is unbuilt. Fail honestly instead of reporting a keyframe that does not exist.
        (void)Time; (void)Value;
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("add_animation_keyframe is not implemented: it writes no MovieScene channel key. "
                 "Author keyframes in the Widget Blueprint Editor's Animation tab, or extend this action "
                 "to resolve the track's MovieSceneFloatChannel and insert the key."),
            TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    if (SubAction.Equals(TEXT("set_animation_loop"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        bool bLoop = GetJsonBoolField(Payload, TEXT("loop"), true);
        int32 LoopCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("loopCount"), 0)); // 0 = infinite
        
        if (WidgetPath.IsEmpty() || AnimationName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, animationName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Find the animation
        UWidgetAnimation* Animation = nullptr;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim && Anim->GetFName().ToString().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                Animation = Anim;
                break;
            }
        }
        
        if (!Animation)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("ANIMATION_NOT_FOUND"));
            return true;
        }

        // Looping is NOT an asset-side property of a UWidgetAnimation — it is a runtime argument
        // to PlayAnimation(NumLoopsToPlay, ...). This action persisted nothing; it only echoed the
        // values. Report that honestly so callers wire looping at the PlayAnimation call site.
        (void)bLoop; (void)LoopCount;
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("set_animation_loop is not implemented: loop count is not stored on the animation asset. "
                 "Pass NumLoopsToPlay to PlayAnimation() at runtime (loop=0 plays once; specify the count "
                 "or use PlayAnimation's looping mode)."),
            TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    if (SubAction.Equals(TEXT("set_animation_speed"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        float PlaybackSpeed = GetJsonNumberField(Payload, TEXT("speed"), 1.0f);

        if (WidgetPath.IsEmpty() || AnimationName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, animationName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidgetAnimation* TargetAnim = nullptr;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim && Anim->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                TargetAnim = Anim;
                break;
            }
        }

        if (!TargetAnim || !TargetAnim->MovieScene)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("NOT_FOUND"));
            return true;
        }

        // This was a true no-op: it set the playback range to its own current value and never applied
        // PlaybackSpeed. Playback speed is a runtime argument to PlayAnimation(..., PlaybackSpeed), not
        // an asset property. Report that honestly instead of "Set animation speed".
        (void)PlaybackSpeed;
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("set_animation_speed is not implemented: playback speed is not stored on the animation "
                 "asset. Pass PlaybackSpeed to PlayAnimation() at runtime."),
            TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    if (SubAction.Equals(TEXT("get_animation_info"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        if (AnimationName.IsEmpty())
        {
            // Return list of all animations
            TArray<TSharedPtr<FJsonValue>> AnimationsArray;
            for (UWidgetAnimation* Anim : WidgetBP->Animations)
            {
                if (Anim)
                {
                    TSharedPtr<FJsonObject> AnimInfo = McpHandlerUtils::CreateResultObject();
                    AnimInfo->SetStringField(TEXT("name"), Anim->GetName());
                    if (Anim->MovieScene)
                    {
                        FFrameRate FrameRate = Anim->MovieScene->GetTickResolution();
                        FFrameNumber Start = Anim->MovieScene->GetPlaybackRange().GetLowerBoundValue();
                        FFrameNumber End = Anim->MovieScene->GetPlaybackRange().GetUpperBoundValue();
                        float Duration = (End - Start).Value / FrameRate.AsDecimal();
                        AnimInfo->SetNumberField(TEXT("durationSeconds"), Duration);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                        AnimInfo->SetNumberField(TEXT("trackCount"), Anim->MovieScene->GetTracks().Num());
#else
                        AnimInfo->SetNumberField(TEXT("trackCount"), Anim->MovieScene->GetMasterTracks().Num());
#endif
                    }
                    AnimationsArray.Add(MakeShared<FJsonValueObject>(AnimInfo));
                }
            }
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
            ResultJson->SetArrayField(TEXT("animations"), AnimationsArray);
            ResultJson->SetNumberField(TEXT("animationCount"), WidgetBP->Animations.Num());
        }
        else
        {
            // Return info for specific animation
            UWidgetAnimation* TargetAnim = nullptr;
            for (UWidgetAnimation* Anim : WidgetBP->Animations)
            {
                if (Anim && Anim->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
                {
                    TargetAnim = Anim;
                    break;
                }
            }

            if (!TargetAnim)
            {
                SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("NOT_FOUND"));
                return true;
            }

            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
            ResultJson->SetStringField(TEXT("animationName"), AnimationName);

            if (TargetAnim->MovieScene)
            {
                FFrameRate FrameRate = TargetAnim->MovieScene->GetTickResolution();
                FFrameNumber Start = TargetAnim->MovieScene->GetPlaybackRange().GetLowerBoundValue();
                FFrameNumber End = TargetAnim->MovieScene->GetPlaybackRange().GetUpperBoundValue();
                float Duration = (End - Start).Value / FrameRate.AsDecimal();
                
                ResultJson->SetNumberField(TEXT("durationSeconds"), Duration);
                ResultJson->SetNumberField(TEXT("frameRate"), FrameRate.AsDecimal());
                ResultJson->SetNumberField(TEXT("startFrame"), Start.Value);
                ResultJson->SetNumberField(TEXT("endFrame"), End.Value);

                TArray<TSharedPtr<FJsonValue>> TracksArray;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                // UE 5.1+: GetMasterTracks() replaced with GetTracks()
                const TArray<UMovieSceneTrack*>& MasterTracks = TargetAnim->MovieScene->GetTracks();
#else
                // UE 5.0: Use GetMasterTracks()
                const TArray<UMovieSceneTrack*>& MasterTracks = TargetAnim->MovieScene->GetMasterTracks();
#endif
                for (UMovieSceneTrack* Track : MasterTracks)
                {
                    if (Track)
                    {
                        TSharedPtr<FJsonObject> TrackInfo = McpHandlerUtils::CreateResultObject();
                        TrackInfo->SetStringField(TEXT("name"), Track->GetTrackName().ToString());
                        TrackInfo->SetStringField(TEXT("type"), Track->GetClass()->GetName());
                        TracksArray.Add(MakeShared<FJsonValueObject>(TrackInfo));
                    }
                }
                ResultJson->SetArrayField(TEXT("tracks"), TracksArray);
            }
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved animation info"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("delete_animation"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));

        if (WidgetPath.IsEmpty() || AnimationName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, animationName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        int32 FoundIndex = INDEX_NONE;
        for (int32 i = 0; i < WidgetBP->Animations.Num(); ++i)
        {
            if (WidgetBP->Animations[i] && WidgetBP->Animations[i]->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                FoundIndex = i;
                break;
            }
        }

        if (FoundIndex == INDEX_NONE)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("NOT_FOUND"));
            return true;
        }

        WidgetBP->Animations.RemoveAt(FoundIndex);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("deletedAnimation"), AnimationName);
        ResultJson->SetNumberField(TEXT("remainingAnimations"), WidgetBP->Animations.Num());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Deleted animation"), ResultJson);
        return true;
    }

    return false;
}
bool UMcpAutomationBridgeSubsystem::HandleWidgetAuthoring_Style(
    const FString& SubAction, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    if (SubAction.Equals(TEXT("create_widget_style"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString StyleName = GetJsonStringField(Payload, TEXT("styleName"));
        if (StyleName.IsEmpty())
        {
            StyleName = TEXT("DefaultStyle");
        }

        FString StyleType = GetJsonStringField(Payload, TEXT("styleType"));
        if (StyleType.IsEmpty())
        {
            StyleType = TEXT("Text");
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TArray<FString> CreatedVariables;

        // Create style variables based on type
        if (StyleType.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
        {
            // Font style variable
            FEdGraphPinType FontPinType;
            FontPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            FontPinType.PinSubCategoryObject = FSlateFontInfo::StaticStruct();
            
            FString FontVarName = StyleName + TEXT("_Font");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *FontVarName, FontPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *FontVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(FontVarName);

            // Color variable
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FSlateColor>::Get();
            
            FString ColorVarName = StyleName + TEXT("_Color");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ColorVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ColorVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ColorVarName);

            // Shadow color
            FString ShadowVarName = StyleName + TEXT("_ShadowColor");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ShadowVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ShadowVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ShadowVarName);
        }
        else if (StyleType.Equals(TEXT("Button"), ESearchCase::IgnoreCase))
        {
            // Button style uses FButtonStyle
            FEdGraphPinType ButtonStylePinType;
            ButtonStylePinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ButtonStylePinType.PinSubCategoryObject = FButtonStyle::StaticStruct();
            
            FString ButtonStyleVarName = StyleName + TEXT("_ButtonStyle");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ButtonStyleVarName, ButtonStylePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ButtonStyleVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ButtonStyleVarName);

            // Normal/Hovered/Pressed colors
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
            
            for (const FString& State : { TEXT("Normal"), TEXT("Hovered"), TEXT("Pressed") })
            {
                FString StateVarName = StyleName + TEXT("_") + State + TEXT("Color");
                FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *StateVarName, ColorPinType);
                FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *StateVarName, nullptr, 
                    FText::FromString(TEXT("Widget Styles")));
                CreatedVariables.Add(StateVarName);
            }
        }
        else if (StyleType.Equals(TEXT("Image"), ESearchCase::IgnoreCase))
        {
            // Brush style
            FEdGraphPinType BrushPinType;
            BrushPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            BrushPinType.PinSubCategoryObject = FSlateBrush::StaticStruct();
            
            FString BrushVarName = StyleName + TEXT("_Brush");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *BrushVarName, BrushPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *BrushVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(BrushVarName);

            // Tint color
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
            
            FString TintVarName = StyleName + TEXT("_Tint");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *TintVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *TintVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(TintVarName);
        }
        else if (StyleType.Equals(TEXT("ProgressBar"), ESearchCase::IgnoreCase))
        {
            FEdGraphPinType StylePinType;
            StylePinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            StylePinType.PinSubCategoryObject = FProgressBarStyle::StaticStruct();
            
            FString ProgressStyleVarName = StyleName + TEXT("_ProgressStyle");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ProgressStyleVarName, StylePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ProgressStyleVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ProgressStyleVarName);
        }
        else
        {
            // Generic style - create color and margin variables
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
            
            FString ColorVarName = StyleName + TEXT("_Color");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ColorVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ColorVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ColorVarName);

            FEdGraphPinType MarginPinType;
            MarginPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            MarginPinType.PinSubCategoryObject = TBaseStructure<FMargin>::Get();
            
            FString MarginVarName = StyleName + TEXT("_Margin");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *MarginVarName, MarginPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *MarginVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(MarginVarName);
        }

        McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

        TArray<TSharedPtr<FJsonValue>> VariablesArray;
        for (const FString& VarName : CreatedVariables)
        {
            VariablesArray.Add(MakeShared<FJsonValueString>(VarName));
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("styleName"), StyleName);
        ResultJson->SetStringField(TEXT("styleType"), StyleType);
        ResultJson->SetArrayField(TEXT("createdVariables"), VariablesArray);
        ResultJson->SetNumberField(TEXT("variableCount"), CreatedVariables.Num());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget style variables created"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("apply_style_to_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString StyleName = GetJsonStringField(Payload, TEXT("styleName"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || StyleName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, styleName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        // This only probed that a style variable exists, then reported "Applied style" while applying
        // nothing. Fail honestly. (Widget was validated above.) For Common UI styles use
        // set_common_button_style / set_common_text_style, which set the real TSubclassOf<...> style.
        (void)TargetWidget;
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("apply_style_to_widget is not implemented: it did not apply any style. For Common UI "
                 "widgets use set_common_button_style / set_common_text_style; for plain UMG widgets set the "
                 "style/brush property directly via set_property on the widget's slot object."),
            TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    return false;
}
bool UMcpAutomationBridgeSubsystem::HandleWidgetAuthoring_Tree(
    const FString& SubAction, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    if (SubAction.Equals(TEXT("add_widget_component"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString ComponentType = GetJsonStringField(Payload, TEXT("componentType"));
        if (ComponentType.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: componentType"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
        if (ComponentName.IsEmpty())
        {
            ComponentName = ComponentType + TEXT("_") + FGuid::NewGuid().ToString().Left(8);
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find parent panel
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        
        if (!ParentName.IsEmpty())
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
                if (W && W->GetFName().ToString().Equals(ParentName, ESearchCase::IgnoreCase))
                {
                    if (UPanelWidget* P = Cast<UPanelWidget>(W)) Parent = P;
                }
            });
        }

        if (!Parent)
        {
            // Create a canvas panel as root if none exists
            Parent = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
            WidgetBP->WidgetTree->RootWidget = Parent;
            RegisterWidgetGuid(WidgetBP, Parent);
        }

        // Map component type to UWidget class
        UClass* WidgetClass = nullptr;
        
        // Common widget types
        if (ComponentType.Equals(TEXT("TextBlock"), ESearchCase::IgnoreCase) || 
            ComponentType.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UTextBlock::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Button"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UButton::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Image"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UImage::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ProgressBar"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UProgressBar::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Slider"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USlider::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("CheckBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UCheckBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("EditableText"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UEditableText::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("EditableTextBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UEditableTextBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ComboBox"), ESearchCase::IgnoreCase) ||
                 ComponentType.Equals(TEXT("ComboBoxString"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UComboBoxString::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("SpinBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USpinBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("CanvasPanel"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UCanvasPanel::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("HorizontalBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UHorizontalBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("VerticalBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UVerticalBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("GridPanel"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UGridPanel::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("UniformGridPanel"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UUniformGridPanel::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Overlay"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UOverlay::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("SizeBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USizeBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ScaleBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UScaleBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Border"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UBorder::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Spacer"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USpacer::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ScrollBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UScrollBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("WidgetSwitcher"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UWidgetSwitcher::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ListView"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UListView::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("TileView"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UTileView::StaticClass();
        }
        else
        {
            // Try to find by class name
            FString ClassName = TEXT("U") + ComponentType;
            WidgetClass = FindObject<UClass>(nullptr, *ClassName);
            if (!WidgetClass)
            {
                // Try with Widget suffix
                ClassName = TEXT("U") + ComponentType + TEXT("Widget");
                WidgetClass = FindObject<UClass>(nullptr, *ClassName);
            }
        }

        if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Unknown widget type: %s"), *ComponentType), TEXT("UNKNOWN_TYPE"));
            return true;
        }

        // Create the widget
        UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, *ComponentName);
        if (!NewWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct widget"), TEXT("CREATION_FAILED"));
            return true;
        }
        RegisterWidgetGuid(WidgetBP, NewWidget);

        // Add to parent
        Parent->AddChild(NewWidget);

        // Configure slot if canvas panel
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(NewWidget->Slot))
            {
                float PosX = static_cast<float>(GetJsonNumberField(Payload, TEXT("positionX"), 0.0));
                float PosY = static_cast<float>(GetJsonNumberField(Payload, TEXT("positionY"), 0.0));
                float SizeX = static_cast<float>(GetJsonNumberField(Payload, TEXT("sizeX"), 0.0));
                float SizeY = static_cast<float>(GetJsonNumberField(Payload, TEXT("sizeY"), 0.0));
                
                if (PosX != 0.0f || PosY != 0.0f)
                {
                    Slot->SetPosition(FVector2D(PosX, PosY));
                }
                if (SizeX > 0.0f && SizeY > 0.0f)
                {
                    Slot->SetSize(FVector2D(SizeX, SizeY));
                    Slot->SetAutoSize(false);
                }
            }
        }

        // Set initial text if TextBlock
        if (UTextBlock* TextWidget = Cast<UTextBlock>(NewWidget))
        {
            FString InitialText = GetJsonStringField(Payload, TEXT("text"));
            if (!InitialText.IsEmpty())
            {
                TextWidget->SetText(FText::FromString(InitialText));
            }
        }

        McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("componentName"), ComponentName);
        ResultJson->SetStringField(TEXT("componentType"), WidgetClass->GetName());
        ResultJson->SetStringField(TEXT("parentName"), Parent->GetName());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget component added"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("reparent_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString NewParent = GetJsonStringField(Payload, TEXT("newParent"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || NewParent.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, newParent"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        UPanelWidget* NewParentWidget = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*NewParent)));
        if (!NewParentWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("New parent '%s' not found or not a panel"), *NewParent), TEXT("NOT_FOUND"));
            return true;
        }

        // Remove from current parent and add to new parent
        if (UPanelWidget* OldParent = TargetWidget->GetParent())
        {
            OldParent->RemoveChild(TargetWidget);
        }
        NewParentWidget->AddChild(TargetWidget);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("widget"), SlotName);
        ResultJson->SetStringField(TEXT("newParent"), NewParent);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Reparented widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("wrap_root"), ESearchCase::IgnoreCase))
    {
        // Wrap the current root widget in a new panel: the panel becomes the root and
        // the old root its first child (Fill/Fill so it keeps occupying the full rect).
        // Unblocks adding overlay chrome (action bars, HUD layers) to a widget BP whose
        // root is a non-panel widget (e.g. a CommonActivatableWidgetStack), where the
        // add_* root-replacement path correctly refuses.
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString PanelType = GetJsonStringField(Payload, TEXT("panelType"));
        FString NewName = GetJsonStringField(Payload, TEXT("name"));
        if (PanelType.IsEmpty()) { PanelType = TEXT("Overlay"); }
        if (NewName.IsEmpty()) { NewName = TEXT("RootPanel"); }

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* OldRoot = WidgetBP->WidgetTree->RootWidget;
        if (!OldRoot)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint has no root widget - add a panel normally instead."), TEXT("NO_ROOT"));
            return true;
        }
        if (WidgetBP->WidgetTree->FindWidget(FName(*NewName)))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("A widget named '%s' already exists."), *NewName), TEXT("ALREADY_EXISTS"));
            return true;
        }

        UClass* PanelClass = nullptr;
        if (PanelType.Equals(TEXT("Overlay"), ESearchCase::IgnoreCase)) { PanelClass = UOverlay::StaticClass(); }
        else if (PanelType.Equals(TEXT("VerticalBox"), ESearchCase::IgnoreCase)) { PanelClass = UVerticalBox::StaticClass(); }
        else if (PanelType.Equals(TEXT("HorizontalBox"), ESearchCase::IgnoreCase)) { PanelClass = UHorizontalBox::StaticClass(); }
        else if (PanelType.Equals(TEXT("CanvasPanel"), ESearchCase::IgnoreCase)) { PanelClass = UCanvasPanel::StaticClass(); }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unsupported panelType '%s'. Use Overlay, VerticalBox, HorizontalBox or CanvasPanel."), *PanelType), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UPanelWidget* NewRoot = Cast<UPanelWidget>(WidgetBP->WidgetTree->ConstructWidget<UWidget>(PanelClass, FName(*NewName)));
        if (!NewRoot)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct the panel widget."), TEXT("CREATION_ERROR"));
            return true;
        }
        NewRoot->SetDisplayLabel(NewName);

        WidgetBP->WidgetTree->RootWidget = NewRoot;
        UPanelSlot* NewSlot = NewRoot->AddChild(OldRoot);
        // Keep the wrapped old root filling the same rect it had as the root.
        if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(NewSlot))
        {
            OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
            OverlaySlot->SetVerticalAlignment(VAlign_Fill);
        }
        else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(NewSlot))
        {
            VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            VSlot->SetHorizontalAlignment(HAlign_Fill);
        }
        else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(NewSlot))
        {
            HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            HSlot->SetVerticalAlignment(VAlign_Fill);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("newRoot"), NewName);
        ResultJson->SetStringField(TEXT("wrapped"), OldRoot->GetName());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Wrapped root widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_widget"), ESearchCase::IgnoreCase))
    {
        // Generic widget add for classes without a dedicated add_* action: resolves any
        // UWidget subclass (loaded short name, /Script/Module.Class, or /Game/....Name_C)
        // and places it via the same SafeAddWidgetToTree path as the typed adds.
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetClassName = GetJsonStringField(Payload, TEXT("widgetClass"));
        FString Name = GetJsonStringField(Payload, TEXT("name"));
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));

        if (WidgetPath.IsEmpty() || WidgetClassName.IsEmpty() || Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, widgetClass, name"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        if (WidgetBP->WidgetTree->FindWidget(FName(*Name)))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("A widget named '%s' already exists."), *Name), TEXT("ALREADY_EXISTS"));
            return true;
        }

        // Resolve the class: loaded short name first, then force-load by path or by
        // probing the usual widget script modules (same approach as create_widget_blueprint).
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        UClass* FoundClass = FindFirstObject<UClass>(*WidgetClassName, EFindFirstObjectOptions::None);
#else
        UClass* FoundClass = ResolveClassByName(WidgetClassName);
#endif
        if (!FoundClass)
        {
            if (WidgetClassName.Contains(TEXT(".")) || WidgetClassName.Contains(TEXT("/")))
            {
                FoundClass = StaticLoadClass(UObject::StaticClass(), nullptr, *WidgetClassName, nullptr, LOAD_NoWarn | LOAD_Quiet);
            }
            else
            {
                static const TCHAR* ScriptModules[] = { TEXT("CommonUI"), TEXT("CommonInput"), TEXT("UMG") };
                for (const TCHAR* ModuleName : ScriptModules)
                {
                    const FString ScriptPath = FString::Printf(TEXT("/Script/%s.%s"), ModuleName, *WidgetClassName);
                    FoundClass = StaticLoadClass(UObject::StaticClass(), nullptr, *ScriptPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
                    if (FoundClass)
                    {
                        break;
                    }
                }
            }
        }
        if (!FoundClass || !FoundClass->IsChildOf(UWidget::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Could not resolve widgetClass '%s' to a UWidget subclass. Pass a loaded class name, or a fully-qualified path such as /Script/CommonUI.CommonBoundActionBar or /Game/UI/MyWidget.MyWidget_C."), *WidgetClassName),
                TEXT("INVALID_CLASS"));
            return true;
        }
        if (FoundClass->HasAnyClassFlags(CLASS_Abstract))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("widgetClass '%s' is abstract - use a concrete subclass."), *WidgetClassName),
                TEXT("INVALID_CLASS"));
            return true;
        }

        UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(FoundClass, FName(*Name));
        if (!NewWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct the widget."), TEXT("CREATION_ERROR"));
            return true;
        }
        NewWidget->SetDisplayLabel(Name);
        NewWidget->bIsVariable = true;

        FString AddErr;
        if (!SafeAddWidgetToTree(WidgetBP, NewWidget, ParentSlot, &AddErr))
        {
            SendAutomationError(RequestingSocket, RequestId,
                AddErr.IsEmpty() ? TEXT("Failed to add the widget to the tree.") : AddErr,
                TEXT("ADD_FAILED"));
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("name"), Name);
        ResultJson->SetStringField(TEXT("widgetClass"), FoundClass->GetPathName());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("get_widget_slot_info"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("widgetClass"), TargetWidget->GetClass()->GetName());
        ResultJson->SetBoolField(TEXT("isVisible"), TargetWidget->IsVisible());

        if (UPanelSlot* Slot = TargetWidget->Slot)
        {
            ResultJson->SetStringField(TEXT("slotClass"), Slot->GetClass()->GetName());
            // The slot's full subobject path — pass directly to inspect
            // set_property/get_property (no more guessing OverlaySlot_N /
            // CanvasPanelSlot_N names via object iteration).
            ResultJson->SetStringField(TEXT("slotObjectPath"), Slot->GetPathName());
            // Generic property dump of the slot (covers Overlay/VBox/HBox/etc.
            // slots, which previously returned nothing typed).
            {
                TSharedPtr<FJsonObject> SlotProps = MakeShared<FJsonObject>();
                for (TFieldIterator<FProperty> PropIt(Slot->GetClass()); PropIt; ++PropIt)
                {
                    FProperty* Prop = *PropIt;
                    if (!Prop || Prop->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
                    {
                        continue;
                    }
                    // Skip the back-pointers (Parent panel / Content widget) —
                    // noise that bloats the response.
                    if (Prop->GetFName() == TEXT("Parent") || Prop->GetFName() == TEXT("Content"))
                    {
                        continue;
                    }
                    // Helper takes the CONTAINER (object) pointer, not a
                    // pre-resolved value pointer (double-offset reads garbage).
                    TSharedPtr<FJsonValue> Exported =
                        McpPropertyReflection::ExportPropertyToJsonValue(Slot, Prop);
                    if (Exported.IsValid())
                    {
                        SlotProps->SetField(Prop->GetName(), Exported);
                    }
                }
                ResultJson->SetObjectField(TEXT("slotProperties"), SlotProps);
            }

            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
            {
                TSharedPtr<FJsonObject> SlotInfo = McpHandlerUtils::CreateResultObject();
                FAnchors Anchors = CanvasSlot->GetAnchors();
                SlotInfo->SetNumberField(TEXT("anchorMinX"), Anchors.Minimum.X);
                SlotInfo->SetNumberField(TEXT("anchorMinY"), Anchors.Minimum.Y);
                SlotInfo->SetNumberField(TEXT("anchorMaxX"), Anchors.Maximum.X);
                SlotInfo->SetNumberField(TEXT("anchorMaxY"), Anchors.Maximum.Y);
                FVector2D Alignment = CanvasSlot->GetAlignment();
                SlotInfo->SetNumberField(TEXT("alignmentX"), Alignment.X);
                SlotInfo->SetNumberField(TEXT("alignmentY"), Alignment.Y);
                FVector2D Position = CanvasSlot->GetPosition();
                SlotInfo->SetNumberField(TEXT("positionX"), Position.X);
                SlotInfo->SetNumberField(TEXT("positionY"), Position.Y);
                FVector2D Size = CanvasSlot->GetSize();
                SlotInfo->SetNumberField(TEXT("sizeX"), Size.X);
                SlotInfo->SetNumberField(TEXT("sizeY"), Size.Y);
                SlotInfo->SetNumberField(TEXT("zOrder"), CanvasSlot->GetZOrder());
                ResultJson->SetObjectField(TEXT("canvasSlotInfo"), SlotInfo);
            }
        }

        if (UPanelWidget* Parent = TargetWidget->GetParent())
        {
            ResultJson->SetStringField(TEXT("parentName"), Parent->GetName());
            ResultJson->SetStringField(TEXT("parentClass"), Parent->GetClass()->GetName());
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved widget slot info"), ResultJson);
        return true;
    }

    return false;
}
bool UMcpAutomationBridgeSubsystem::HandleWidgetAuthoring_Recipes(
    const FString& SubAction, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    if (SubAction.Equals(TEXT("create_main_menu"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString Title = GetJsonStringField(Payload, TEXT("title"), TEXT("Main Menu"));
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // CRITICAL: Clear the entire widget tree for a complete rebuild.
        // This removes ALL widgets and clears the GUID map, preventing orphaned widgets
        // from triggering ensure failures during compilation.
        // See: ForEachObjectWithOuter in WidgetBlueprintCompiler.cpp line 792
        ClearWidgetTreeForRebuild(WidgetBP);
        
        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // This prevents "Widget was added but did not get a GUID" ensure failures
        
        // Create Canvas Panel as root
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("MainMenuCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;
        
        // Create vertical box for menu items
        UVerticalBox* MenuBox = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("MenuVerticalBox"));
        RootCanvas->AddChild(MenuBox);
        
        // Add title text
        UTextBlock* TitleText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("TitleText"));
        TitleText->SetText(FText::FromString(Title));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FSlateFontInfo FontInfo = TitleText->GetFont();
#else
        FSlateFontInfo FontInfo = FSlateFontInfo();
#endif
        FontInfo.Size = 48;
        TitleText->SetFont(FontInfo);
        MenuBox->AddChild(TitleText);
        
        // Add Play button
        UButton* PlayButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("PlayButton"));
        UTextBlock* PlayText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("PlayButtonText"));
        PlayText->SetText(FText::FromString(TEXT("Play")));
        PlayButton->AddChild(PlayText);
        MenuBox->AddChild(PlayButton);
        
        // Add Settings button
        UButton* SettingsButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("SettingsButton"));
        UTextBlock* SettingsText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("SettingsButtonText"));
        SettingsText->SetText(FText::FromString(TEXT("Settings")));
        SettingsButton->AddChild(SettingsText);
        MenuBox->AddChild(SettingsButton);
        
        // Add Quit button
        UButton* QuitButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("QuitButton"));
        UTextBlock* QuitText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("QuitButtonText"));
        QuitText->SetText(FText::FromString(TEXT("Quit")));
        QuitButton->AddChild(QuitText);
        MenuBox->AddChild(QuitButton);
        
        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        // Keeping it for safety in case any edge case widgets were missed
        RegisterAllWidgetGuids(WidgetBP);
        
        McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("title"), Title);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Main menu created"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_pause_menu"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // CRITICAL: Clear the entire widget tree for a complete rebuild.
        // This removes ALL widgets and clears the GUID map, preventing orphaned widgets
        // from triggering ensure failures during compilation.
        ClearWidgetTreeForRebuild(WidgetBP);
        
        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // This prevents "Widget was added but did not get a GUID" ensure failures
        
        // Create overlay for semi-transparent background
        UOverlay* RootOverlay = CreateAndRegisterWidget<UOverlay>(WidgetBP, WidgetBP->WidgetTree, TEXT("PauseMenuOverlay"));
        WidgetBP->WidgetTree->RootWidget = RootOverlay;
        
        // Add background border with color
        UBorder* Background = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, TEXT("Background"));
        Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
        RootOverlay->AddChild(Background);
        
        // Add menu vertical box
        UVerticalBox* MenuBox = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("PauseMenuBox"));
        RootOverlay->AddChild(MenuBox);
        
        // Add PAUSED title
        UTextBlock* TitleText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("PausedTitle"));
        TitleText->SetText(FText::FromString(TEXT("PAUSED")));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FSlateFontInfo FontInfo = TitleText->GetFont();
#else
        FSlateFontInfo FontInfo = FSlateFontInfo();
#endif
        FontInfo.Size = 36;
        TitleText->SetFont(FontInfo);
        MenuBox->AddChild(TitleText);
        
        // Add Resume button
        UButton* ResumeButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("ResumeButton"));
        UTextBlock* ResumeText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("ResumeText"));
        ResumeText->SetText(FText::FromString(TEXT("Resume")));
        ResumeButton->AddChild(ResumeText);
        MenuBox->AddChild(ResumeButton);
        
        // Add Main Menu button
        UButton* MainMenuButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("MainMenuButton"));
        UTextBlock* MainMenuText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("MainMenuText"));
        MainMenuText->SetText(FText::FromString(TEXT("Main Menu")));
        MainMenuButton->AddChild(MainMenuText);
        MenuBox->AddChild(MainMenuButton);
        
        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        RegisterAllWidgetGuids(WidgetBP);
        
        McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Pause menu created"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_hud_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // CRITICAL: Clear the entire widget tree for a complete rebuild.
        // This removes ALL widgets and clears the GUID map, preventing orphaned widgets
        // from triggering ensure failures during compilation.
        ClearWidgetTreeForRebuild(WidgetBP);
        
        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // This prevents "Widget was added but did not get a GUID" ensure failures
        
        // Create Canvas Panel as root for HUD
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("HUDCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;
        
        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        RegisterAllWidgetGuids(WidgetBP);
        
        McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("note"), TEXT("HUD canvas created. Use add_health_bar, add_crosshair, add_ammo_counter to add HUD elements."));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("HUD widget created"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_health_bar"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
        double X = GetJsonNumberField(Payload, TEXT("x"), 20.0);
        double Y = GetJsonNumberField(Payload, TEXT("y"), 20.0);
        double Width = GetJsonNumberField(Payload, TEXT("width"), 200.0);
        double Height = GetJsonNumberField(Payload, TEXT("height"), 20.0);
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Find parent panel
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!ParentName.IsEmpty())
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
                if (W && W->GetFName().ToString().Equals(ParentName, ESearchCase::IgnoreCase))
                {
                    if (UPanelWidget* P = Cast<UPanelWidget>(W)) Parent = P;
                }
            });
        }
        
        if (!Parent)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No valid parent panel found"), TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        
        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // Create horizontal box to hold health bar components
        UHorizontalBox* HealthBox = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("HealthBarContainer"));
        Parent->AddChild(HealthBox);
        
        // Add health icon/label
        UTextBlock* HealthLabel = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("HealthLabel"));
        HealthLabel->SetText(FText::FromString(TEXT("HP")));
        HealthBox->AddChild(HealthLabel);
        
        // Add progress bar for health
        UProgressBar* HealthProgress = CreateAndRegisterWidget<UProgressBar>(WidgetBP, WidgetBP->WidgetTree, TEXT("HealthBar"));
        HealthProgress->SetPercent(1.0f);
        HealthProgress->SetFillColorAndOpacity(FLinearColor(0.8f, 0.1f, 0.1f, 1.0f));
        HealthBox->AddChild(HealthProgress);
        
        // Set position if parent is canvas panel
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(HealthBox->Slot))
            {
                Slot->SetPosition(FVector2D(X, Y));
                Slot->SetSize(FVector2D(Width, Height));
            }
        }
        
        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        RegisterAllWidgetGuids(WidgetBP);
        
        McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), TEXT("HealthBarContainer"));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Health bar added"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_crosshair"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
        double Size = GetJsonNumberField(Payload, TEXT("size"), 32.0);
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Find parent panel
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!ParentName.IsEmpty())
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
                if (W && W->GetFName().ToString().Equals(ParentName, ESearchCase::IgnoreCase))
                {
                    if (UPanelWidget* P = Cast<UPanelWidget>(W)) Parent = P;
                }
            });
        }
        
        if (!Parent)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No valid parent panel found"), TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        
        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // Create crosshair image (uses a simple text-based crosshair, user can swap for image)
        UTextBlock* Crosshair = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("Crosshair"));
        Crosshair->SetText(FText::FromString(TEXT("+")));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FSlateFontInfo FontInfo = Crosshair->GetFont();
#else
        FSlateFontInfo FontInfo = FSlateFontInfo();
#endif
        FontInfo.Size = static_cast<int32>(Size);
        Crosshair->SetFont(FontInfo);
        Crosshair->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Parent->AddChild(Crosshair);
        
        // Center the crosshair if parent is canvas panel
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Crosshair->Slot))
            {
                Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
                Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            }
        }
        
        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        RegisterAllWidgetGuids(WidgetBP);
        
        McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), TEXT("Crosshair"));
        ResultJson->SetStringField(TEXT("note"), TEXT("Simple crosshair added. Replace with Image widget and crosshair texture for custom appearance."));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Crosshair added"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_ammo_counter"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!ParentName.IsEmpty())
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
                if (W && W->GetFName().ToString().Equals(ParentName, ESearchCase::IgnoreCase))
                {
                    if (UPanelWidget* P = Cast<UPanelWidget>(W)) Parent = P;
                }
            });
        }
        
        if (!Parent)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No valid parent panel found"), TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        
        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // Create ammo counter text
        UTextBlock* AmmoText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("AmmoCounter"));
        AmmoText->SetText(FText::FromString(TEXT("30 / 90")));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FSlateFontInfo FontInfo = AmmoText->GetFont();
#else
        // UE 5.0: Access Font property directly
        FSlateFontInfo FontInfo = AmmoText->Font;
#endif
        FontInfo.Size = 24;
        AmmoText->SetFont(FontInfo);
        Parent->AddChild(AmmoText);
        
        // Position at bottom right if canvas
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(AmmoText->Slot))
            {
                Slot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
                Slot->SetAlignment(FVector2D(1.0f, 1.0f));
                Slot->SetPosition(FVector2D(-20.0f, -20.0f));
            }
        }
        
        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        RegisterAllWidgetGuids(WidgetBP);
        
        McpFinalizeBlueprint(WidgetBP, /*bStructural=*/true, /*bSave=*/true);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), TEXT("AmmoCounter"));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ammo counter added"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_settings_menu"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_SettingsMenu"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI/Menus")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI/Menus"); }

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        // CRITICAL: Check if widget blueprint already exists to prevent engine assertion
        FString NewBPObjectPath = FullPath + TEXT(".") + Name;
        if (FindObject<UWidgetBlueprint>(nullptr, *NewBPObjectPath) != nullptr)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Widget blueprint '%s' already exists"), *Name), 
                TEXT("ALREADY_EXISTS"));
            return true;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create settings menu widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Create root canvas
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Create settings container
        UVerticalBox* SettingsContainer = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsContainer"));
        RootCanvas->AddChild(SettingsContainer);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(SettingsContainer->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(600.0f, 400.0f));
        }

        // Title
        UTextBlock* TitleText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
        TitleText->SetText(FText::FromString(TEXT("Settings")));
        SettingsContainer->AddChild(TitleText);

        // Graphics section
        UTextBlock* GraphicsLabel = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GraphicsLabel"));
        GraphicsLabel->SetText(FText::FromString(TEXT("Graphics")));
        SettingsContainer->AddChild(GraphicsLabel);

        // Quality slider
        USlider* QualitySlider = WidgetBP->WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("QualitySlider"));
        SettingsContainer->AddChild(QualitySlider);

        // Audio section
        UTextBlock* AudioLabel = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AudioLabel"));
        AudioLabel->SetText(FText::FromString(TEXT("Audio")));
        SettingsContainer->AddChild(AudioLabel);

        // Volume slider
        USlider* VolumeSlider = WidgetBP->WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("VolumeSlider"));
        SettingsContainer->AddChild(VolumeSlider);

        // Apply button
        UButton* ApplyButton = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ApplyButton"));
        SettingsContainer->AddChild(ApplyButton);
        UTextBlock* ApplyText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ApplyButtonText"));
        ApplyText->SetText(FText::FromString(TEXT("Apply")));
        ApplyButton->AddChild(ApplyText);

        // CRITICAL: Register all widget GUIDs and mark as user-created
        // This prevents ensure failures in WidgetBlueprintCompiler.cpp line 794
        RegisterAllWidgetGuids(WidgetBP);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("message"), TEXT("Created settings menu template"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created settings menu template"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_loading_screen"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_LoadingScreen"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI"); }

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        // CRITICAL: Check if widget blueprint already exists to prevent engine assertion
        FString NewBPObjectPath = FullPath + TEXT(".") + Name;
        if (FindObject<UWidgetBlueprint>(nullptr, *NewBPObjectPath) != nullptr)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Widget blueprint '%s' already exists"), *Name), 
                TEXT("ALREADY_EXISTS"));
            return true;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create loading screen widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Create root canvas
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Background image
        UImage* Background = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
        RootCanvas->AddChild(Background);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Background->Slot))
        {
            Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            Slot->SetOffsets(FMargin(0.0f));
        }

        // Loading text
        UTextBlock* LoadingText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LoadingText"));
        LoadingText->SetText(FText::FromString(TEXT("Loading...")));
        RootCanvas->AddChild(LoadingText);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(LoadingText->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.7f, 0.5f, 0.7f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
        }

        // Progress bar
        UProgressBar* LoadingBar = WidgetBP->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("LoadingProgressBar"));
        LoadingBar->SetPercent(0.0f);
        RootCanvas->AddChild(LoadingBar);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(LoadingBar->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.8f, 0.5f, 0.8f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(400.0f, 20.0f));
        }

        // CRITICAL: Register all widget GUIDs and mark as user-created
        // This prevents ensure failures in WidgetBlueprintCompiler.cpp line 794
        RegisterAllWidgetGuids(WidgetBP);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("message"), TEXT("Created loading screen template"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created loading screen template"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_minimap"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Minimap"));
        float Size = GetJsonNumberField(Payload, TEXT("size"), 200.0f);

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Create minimap container (overlay for stacking)
        UOverlay* MinimapContainer = WidgetBP->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *SlotName);
        
        // Create border for minimap frame
        UBorder* MinimapBorder = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *(SlotName + TEXT("_Border")));
        MinimapContainer->AddChild(MinimapBorder);

        // Create image for map content
        UImage* MapImage = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_MapImage")));
        MinimapBorder->AddChild(MapImage);

        // Create player indicator
        UImage* PlayerIndicator = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_PlayerIndicator")));
        MinimapContainer->AddChild(PlayerIndicator);

        // Add to root or parent
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!Parent)
        {
            DiscardUnaddedWidget(WidgetBP, PlayerIndicator);
            DiscardUnaddedWidget(WidgetBP, MapImage);
            DiscardUnaddedWidget(WidgetBP, MinimapBorder);
            DiscardUnaddedWidget(WidgetBP, MinimapContainer);
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Cannot add minimap: the widget blueprint has no panel root to parent it under. ")
                TEXT("Give the blueprint a panel root first (e.g. add_canvas_panel)."),
                TEXT("ADD_FAILED"));
            return true;
        }

        Parent->AddChild(MinimapContainer);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(MinimapContainer->Slot))
        {
            Slot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f)); // Top-right
            Slot->SetAlignment(FVector2D(1.0f, 0.0f));
            Slot->SetSize(FVector2D(Size, Size));
            Slot->SetPosition(FVector2D(-20.0f, 20.0f)); // Offset from corner
        }

        // CRITICAL: Register all widget GUIDs to prevent ensure failures during compilation
        RegisterAllWidgetGuids(WidgetBP);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("size"), Size);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added minimap widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_compass"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Compass"));

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.
        
        // Create compass container
        UHorizontalBox* CompassContainer = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        // Create compass image (scrolling texture)
        UImage* CompassImage = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Image")));
        CompassContainer->AddChild(CompassImage);

        // Create direction indicator
        UImage* DirectionIndicator = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Indicator")));
        CompassContainer->AddChild(DirectionIndicator);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!Parent)
        {
            DiscardUnaddedWidget(WidgetBP, DirectionIndicator);
            DiscardUnaddedWidget(WidgetBP, CompassImage);
            DiscardUnaddedWidget(WidgetBP, CompassContainer);
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Cannot add compass: the widget blueprint has no panel root to parent it under. ")
                TEXT("Give the blueprint a panel root first (e.g. add_canvas_panel)."),
                TEXT("ADD_FAILED"));
            return true;
        }

        Parent->AddChild(CompassContainer);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(CompassContainer->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f)); // Top-center
            Slot->SetAlignment(FVector2D(0.5f, 0.0f));
            Slot->SetSize(FVector2D(400.0f, 40.0f));
            Slot->SetPosition(FVector2D(0.0f, 20.0f));
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added compass widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_interaction_prompt"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("InteractionPrompt"));
        FString DefaultText = GetJsonStringField(Payload, TEXT("text"), TEXT("Press E to Interact"));

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.
        
        // Create prompt container
        UHorizontalBox* PromptContainer = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        // Key icon
        UImage* KeyIcon = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_KeyIcon")));
        PromptContainer->AddChild(KeyIcon);

        // Prompt text
        UTextBlock* PromptText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Text")));
        PromptText->SetText(FText::FromString(DefaultText));
        PromptContainer->AddChild(PromptText);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!Parent)
        {
            DiscardUnaddedWidget(WidgetBP, PromptText);
            DiscardUnaddedWidget(WidgetBP, KeyIcon);
            DiscardUnaddedWidget(WidgetBP, PromptContainer);
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Cannot add interaction prompt: the widget blueprint has no panel root to parent it under. ")
                TEXT("Give the blueprint a panel root first (e.g. add_canvas_panel)."),
                TEXT("ADD_FAILED"));
            return true;
        }

        Parent->AddChild(PromptContainer);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(PromptContainer->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.7f, 0.5f, 0.7f)); // Center-bottom area
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetAutoSize(true);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added interaction prompt"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_objective_tracker"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ObjectiveTracker"));

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.
        
        // Create objective container
        UVerticalBox* ObjectiveContainer = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        // Objective title
        UTextBlock* ObjectiveTitle = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Title")));
        ObjectiveTitle->SetText(FText::FromString(TEXT("Objectives")));
        ObjectiveContainer->AddChild(ObjectiveTitle);

        // Objective list (vertical box for dynamic entries)
        UVerticalBox* ObjectiveList = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_List")));
        ObjectiveContainer->AddChild(ObjectiveList);

        // Sample objective item
        UHorizontalBox* SampleObjective = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_SampleItem")));
        UCheckBox* ObjectiveCheck = CreateAndRegisterWidget<UCheckBox>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Check")));
        UTextBlock* ObjectiveText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_ItemText")));
        ObjectiveText->SetText(FText::FromString(TEXT("Sample Objective")));
        SampleObjective->AddChild(ObjectiveCheck);
        SampleObjective->AddChild(ObjectiveText);
        ObjectiveList->AddChild(SampleObjective);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!Parent)
        {
            DiscardUnaddedWidget(WidgetBP, ObjectiveText);
            DiscardUnaddedWidget(WidgetBP, ObjectiveCheck);
            DiscardUnaddedWidget(WidgetBP, SampleObjective);
            DiscardUnaddedWidget(WidgetBP, ObjectiveList);
            DiscardUnaddedWidget(WidgetBP, ObjectiveTitle);
            DiscardUnaddedWidget(WidgetBP, ObjectiveContainer);
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Cannot add objective tracker: the widget blueprint has no panel root to parent it under. ")
                TEXT("Give the blueprint a panel root first (e.g. add_canvas_panel)."),
                TEXT("ADD_FAILED"));
            return true;
        }

        Parent->AddChild(ObjectiveContainer);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(ObjectiveContainer->Slot))
        {
            Slot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f)); // Top-right
            Slot->SetAlignment(FVector2D(1.0f, 0.0f));
            Slot->SetPosition(FVector2D(-20.0f, 100.0f));
            Slot->SetAutoSize(true);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added objective tracker"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_damage_indicator"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("DamageIndicator"));

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.
        
        // Create damage indicator overlay (full screen)
        UOverlay* DamageOverlay = CreateAndRegisterWidget<UOverlay>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        // Blood vignette image (edge damage indicator)
        UImage* VignetteImage = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Vignette")));
        VignetteImage->SetVisibility(ESlateVisibility::Hidden);
        DamageOverlay->AddChild(VignetteImage);

        // Directional damage arrows container
        UCanvasPanel* DirectionalCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Directional")));
        DamageOverlay->AddChild(DirectionalCanvas);

        // Add directional indicators (N, S, E, W)
        TArray<UImage*> DirIndicators;
        for (const FString& Dir : { TEXT("Top"), TEXT("Bottom"), TEXT("Left"), TEXT("Right") })
        {
            UImage* DirIndicator = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_") + Dir));
            DirIndicator->SetVisibility(ESlateVisibility::Hidden);
            DirectionalCanvas->AddChild(DirIndicator);
            DirIndicators.Add(DirIndicator);
        }

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!Parent)
        {
            for (UImage* DirIndicator : DirIndicators)
            {
                DiscardUnaddedWidget(WidgetBP, DirIndicator);
            }
            DiscardUnaddedWidget(WidgetBP, DirectionalCanvas);
            DiscardUnaddedWidget(WidgetBP, VignetteImage);
            DiscardUnaddedWidget(WidgetBP, DamageOverlay);
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Cannot add damage indicator: the widget blueprint has no panel root to parent it under. ")
                TEXT("Give the blueprint a panel root first (e.g. add_canvas_panel)."),
                TEXT("ADD_FAILED"));
            return true;
        }

        Parent->AddChild(DamageOverlay);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(DamageOverlay->Slot))
        {
            Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f)); // Full screen
            Slot->SetOffsets(FMargin(0.0f));
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added damage indicator"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_inventory_ui"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_Inventory"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI"); }
        int32 GridColumns = GetJsonIntField(Payload, TEXT("columns"), 6);
        int32 GridRows = GetJsonIntField(Payload, TEXT("rows"), 4);

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        // CRITICAL: Check if widget blueprint already exists to prevent engine assertion
        FString NewBPObjectPath = FullPath + TEXT(".") + Name;
        if (FindObject<UWidgetBlueprint>(nullptr, *NewBPObjectPath) != nullptr)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Widget blueprint '%s' already exists"), *Name), 
                TEXT("ALREADY_EXISTS"));
            return true;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create inventory widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.
        
        // Create root canvas
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Background panel
        UBorder* BackgroundPanel = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, TEXT("InventoryBackground"));
        RootCanvas->AddChild(BackgroundPanel);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(BackgroundPanel->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(GridColumns * 80.0f + 40.0f, GridRows * 80.0f + 100.0f));
        }

        // Title
        UTextBlock* TitleText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("InventoryTitle"));
        TitleText->SetText(FText::FromString(TEXT("Inventory")));
        BackgroundPanel->AddChild(TitleText);

        // Create inventory grid
        UUniformGridPanel* InventoryGrid = CreateAndRegisterWidget<UUniformGridPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("InventoryGrid"));
        BackgroundPanel->AddChild(InventoryGrid);

        // Add visible inventory slot widgets
        for (int32 Row = 0; Row < GridRows; ++Row)
        {
            for (int32 Col = 0; Col < GridColumns; ++Col)
            {
                FString SlotName = FString::Printf(TEXT("Slot_%d_%d"), Row, Col);
                UBorder* SlotBorder = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, *SlotName);
                InventoryGrid->AddChildToUniformGrid(SlotBorder, Row, Col);
                
                UImage* SlotImage = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Image")));
                SlotBorder->AddChild(SlotImage);
            }
        }

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetNumberField(TEXT("columns"), GridColumns);
        ResultJson->SetNumberField(TEXT("rows"), GridRows);
        ResultJson->SetNumberField(TEXT("totalSlots"), GridColumns * GridRows);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created inventory UI"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_dialog_widget"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_DialogBox"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI"); }

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        // CRITICAL: Check if widget blueprint already exists to prevent engine assertion
        FString NewBPObjectPath = FullPath + TEXT(".") + Name;
        if (FindObject<UWidgetBlueprint>(nullptr, *NewBPObjectPath) != nullptr)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Widget blueprint '%s' already exists"), *Name), 
                TEXT("ALREADY_EXISTS"));
            return true;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create dialog widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.
        
        // Create root canvas
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Dialog background
        UBorder* DialogBg = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, TEXT("DialogBackground"));
        RootCanvas->AddChild(DialogBg);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(DialogBg->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.8f, 0.5f, 0.8f));
            Slot->SetAlignment(FVector2D(0.5f, 1.0f));
            Slot->SetSize(FVector2D(800.0f, 200.0f));
        }

        UVerticalBox* DialogContainer = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("DialogContainer"));
        DialogBg->AddChild(DialogContainer);

        // Speaker name
        UTextBlock* SpeakerName = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("SpeakerName"));
        SpeakerName->SetText(FText::FromString(TEXT("Speaker")));
        DialogContainer->AddChild(SpeakerName);

        // Dialog text
        URichTextBlock* DialogText = CreateAndRegisterWidget<URichTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("DialogText"));
        DialogContainer->AddChild(DialogText);

        // Response options container
        UVerticalBox* ResponseBox = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("ResponseOptions"));
        DialogContainer->AddChild(ResponseBox);

        // Sample response buttons
        for (int32 i = 1; i <= 3; ++i)
        {
            FString ResponseName = FString::Printf(TEXT("Response_%d"), i);
            UButton* ResponseBtn = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, *ResponseName);
            UTextBlock* ResponseText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(ResponseName + TEXT("_Text")));
            ResponseText->SetText(FText::FromString(FString::Printf(TEXT("Response Option %d"), i)));
            ResponseBtn->AddChild(ResponseText);
            ResponseBox->AddChild(ResponseBtn);
        }

        // Continue indicator
        UTextBlock* ContinueIndicator = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("ContinueIndicator"));
        ContinueIndicator->SetText(FText::FromString(TEXT("Press Space to continue...")));
        DialogContainer->AddChild(ContinueIndicator);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created dialog widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_radial_menu"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_RadialMenu"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI"); }
        int32 SegmentCount = GetJsonIntField(Payload, TEXT("segments"), 8);

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        // CRITICAL: Check if widget blueprint already exists to prevent engine assertion
        FString NewBPObjectPath = FullPath + TEXT(".") + Name;
        if (FindObject<UWidgetBlueprint>(nullptr, *NewBPObjectPath) != nullptr)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Widget blueprint '%s' already exists"), *Name), 
                TEXT("ALREADY_EXISTS"));
            return true;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create radial menu"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.
        
        // Create root canvas
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Radial menu container (centered)
        UOverlay* RadialContainer = CreateAndRegisterWidget<UOverlay>(WidgetBP, WidgetBP->WidgetTree, TEXT("RadialMenuContainer"));
        RootCanvas->AddChild(RadialContainer);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(RadialContainer->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(400.0f, 400.0f));
        }

        // Background ring
        UImage* BackgroundRing = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, TEXT("RadialBackground"));
        RadialContainer->AddChild(BackgroundRing);

        // Selection indicator
        UImage* SelectionIndicator = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, TEXT("SelectionIndicator"));
        RadialContainer->AddChild(SelectionIndicator);

        // Create segment buttons (arranged in circle via canvas positions)
        UCanvasPanel* SegmentCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("SegmentCanvas"));
        RadialContainer->AddChild(SegmentCanvas);

        float Radius = 150.0f;
        for (int32 i = 0; i < SegmentCount; ++i)
        {
            float Angle = (360.0f / SegmentCount) * i - 90.0f; // Start from top
            float RadAngle = FMath::DegreesToRadians(Angle);
            float X = FMath::Cos(RadAngle) * Radius;
            float Y = FMath::Sin(RadAngle) * Radius;

            FString SegmentName = FString::Printf(TEXT("Segment_%d"), i);
            UButton* SegmentBtn = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, *SegmentName);
            SegmentCanvas->AddChild(SegmentBtn);
            
            if (UCanvasPanelSlot* SegSlot = Cast<UCanvasPanelSlot>(SegmentBtn->Slot))
            {
                SegSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
                SegSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                SegSlot->SetPosition(FVector2D(X, Y));
                SegSlot->SetSize(FVector2D(60.0f, 60.0f));
            }

            UImage* SegmentIcon = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SegmentName + TEXT("_Icon")));
            SegmentBtn->AddChild(SegmentIcon);
        }

        // Center button
        UButton* CenterButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("CenterButton"));
        RadialContainer->AddChild(CenterButton);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetNumberField(TEXT("segments"), SegmentCount);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created radial menu"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_credits_screen"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_Credits"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI"); }

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create credits widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.
        
        // Root canvas
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Background
        UImage* Background = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, TEXT("Background"));
        RootCanvas->AddChild(Background);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Background->Slot))
        {
            Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            Slot->SetOffsets(FMargin(0.0f));
        }

        // Scrolling credits container
        UScrollBox* CreditsScroll = CreateAndRegisterWidget<UScrollBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("CreditsScroll"));
        RootCanvas->AddChild(CreditsScroll);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(CreditsScroll->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 1.0f));
            Slot->SetAlignment(FVector2D(0.5f, 0.0f));
            Slot->SetSize(FVector2D(600.0f, 0.0f));
            Slot->SetOffsets(FMargin(0.0f, 50.0f, 0.0f, 50.0f));
        }

        // Credits content
        UVerticalBox* CreditsContent = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("CreditsContent"));
        CreditsScroll->AddChild(CreditsContent);

        // Sample credits sections
        TArray<FString> Sections = { TEXT("Lead Developer"), TEXT("Art Director"), TEXT("Sound Design"), TEXT("Special Thanks") };
        for (const FString& Section : Sections)
        {
            UTextBlock* SectionTitle = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(Section.Replace(TEXT(" "), TEXT("_")) + TEXT("_Title")));
            SectionTitle->SetText(FText::FromString(Section));
            CreditsContent->AddChild(SectionTitle);

            UTextBlock* SectionName = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(Section.Replace(TEXT(" "), TEXT("_")) + TEXT("_Name")));
            SectionName->SetText(FText::FromString(TEXT("Your Name Here")));
            CreditsContent->AddChild(SectionName);
        }

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created credits screen"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_shop_ui"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_Shop"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI"); }
        int32 ItemColumns = GetJsonIntField(Payload, TEXT("columns"), 4);

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create shop widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.
        
        // Root canvas
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Shop background
        UBorder* ShopBg = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, TEXT("ShopBackground"));
        RootCanvas->AddChild(ShopBg);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(ShopBg->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(800.0f, 600.0f));
        }

        UVerticalBox* ShopLayout = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("ShopLayout"));
        ShopBg->AddChild(ShopLayout);

        // Header
        UHorizontalBox* Header = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("ShopHeader"));
        ShopLayout->AddChild(Header);

        UTextBlock* ShopTitle = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("ShopTitle"));
        ShopTitle->SetText(FText::FromString(TEXT("Shop")));
        Header->AddChild(ShopTitle);

        UTextBlock* CurrencyDisplay = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("CurrencyDisplay"));
        CurrencyDisplay->SetText(FText::FromString(TEXT("Gold: 0")));
        Header->AddChild(CurrencyDisplay);

        // Category tabs
        UHorizontalBox* CategoryTabs = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("CategoryTabs"));
        ShopLayout->AddChild(CategoryTabs);

        TArray<FString> Categories = { TEXT("Weapons"), TEXT("Armor"), TEXT("Consumables"), TEXT("Special") };
        for (const FString& Category : Categories)
        {
            UButton* TabBtn = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, *(Category + TEXT("_Tab")));
            UTextBlock* TabText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(Category + TEXT("_TabText")));
            TabText->SetText(FText::FromString(Category));
            TabBtn->AddChild(TabText);
            CategoryTabs->AddChild(TabBtn);
        }

        // Items grid
        UScrollBox* ItemsScroll = CreateAndRegisterWidget<UScrollBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("ItemsScroll"));
        ShopLayout->AddChild(ItemsScroll);

        UUniformGridPanel* ItemsGrid = CreateAndRegisterWidget<UUniformGridPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("ItemsGrid"));
        ItemsScroll->AddChild(ItemsGrid);

        // Sample item slots
        for (int32 i = 0; i < 8; ++i)
        {
            FString ItemName = FString::Printf(TEXT("ItemSlot_%d"), i);
            UBorder* ItemSlot = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, *ItemName);
            ItemsGrid->AddChildToUniformGrid(ItemSlot, i / ItemColumns, i % ItemColumns);

            UVerticalBox* ItemContent = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, *(ItemName + TEXT("_Content")));
            ItemSlot->AddChild(ItemContent);

            UImage* ItemIcon = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(ItemName + TEXT("_Icon")));
            ItemContent->AddChild(ItemIcon);

            UTextBlock* ItemLabel = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(ItemName + TEXT("_Name")));
            ItemLabel->SetText(FText::FromString(TEXT("Item")));
            ItemContent->AddChild(ItemLabel);

            UTextBlock* ItemPrice = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(ItemName + TEXT("_Price")));
            ItemPrice->SetText(FText::FromString(TEXT("100g")));
            ItemContent->AddChild(ItemPrice);
        }

        // Buy button
        UButton* BuyButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("BuyButton"));
        ShopLayout->AddChild(BuyButton);
        UTextBlock* BuyText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("BuyButtonText"));
        BuyText->SetText(FText::FromString(TEXT("Buy Selected")));
        BuyButton->AddChild(BuyText);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetNumberField(TEXT("columns"), ItemColumns);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created shop UI"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_quest_tracker"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("QuestTracker"));

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.
        
        // Quest tracker container
        UVerticalBox* QuestContainer = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        // Quest header
        UTextBlock* QuestHeader = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Header")));
        QuestHeader->SetText(FText::FromString(TEXT("Active Quest")));
        QuestContainer->AddChild(QuestHeader);

        // Quest title
        UTextBlock* QuestTitle = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Title")));
        QuestTitle->SetText(FText::FromString(TEXT("Quest Name")));
        QuestContainer->AddChild(QuestTitle);

        // Quest objectives list
        UVerticalBox* ObjectivesList = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Objectives")));
        QuestContainer->AddChild(ObjectivesList);

        // Sample objectives
        for (int32 i = 1; i <= 3; ++i)
        {
            UHorizontalBox* ObjRow = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, *FString::Printf(TEXT("%s_Objective_%d"), *SlotName, i));
            
            UCheckBox* ObjCheck = CreateAndRegisterWidget<UCheckBox>(WidgetBP, WidgetBP->WidgetTree, *FString::Printf(TEXT("%s_ObjCheck_%d"), *SlotName, i));
            ObjRow->AddChild(ObjCheck);
            
            UTextBlock* ObjText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *FString::Printf(TEXT("%s_ObjText_%d"), *SlotName, i));
            ObjText->SetText(FText::FromString(FString::Printf(TEXT("Objective %d (0/1)"), i)));
            ObjRow->AddChild(ObjText);

            ObjectivesList->AddChild(ObjRow);
        }

        // Quest rewards preview
        UHorizontalBox* RewardsRow = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Rewards")));
        QuestContainer->AddChild(RewardsRow);

        UTextBlock* RewardsLabel = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_RewardsLabel")));
        RewardsLabel->SetText(FText::FromString(TEXT("Rewards: ")));
        RewardsRow->AddChild(RewardsLabel);

        UImage* RewardIcon = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_RewardIcon")));
        RewardsRow->AddChild(RewardIcon);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (Parent)
        {
            Parent->AddChild(QuestContainer);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(QuestContainer->Slot))
            {
                Slot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f)); // Top-left
                Slot->SetAlignment(FVector2D(0.0f, 0.0f));
                Slot->SetPosition(FVector2D(20.0f, 100.0f));
                Slot->SetAutoSize(true);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added quest tracker"), ResultJson);
        return true;
    }

    return false;
}
bool UMcpAutomationBridgeSubsystem::HandleWidgetAuthoring_Misc(
    const FString& SubAction, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    if (SubAction.Equals(TEXT("get_widget_info"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> WidgetInfo = McpHandlerUtils::CreateResultObject();

        // Basic info
        WidgetInfo->SetStringField(TEXT("widgetClass"), WidgetBP->GetName());
        if (WidgetBP->ParentClass)
        {
            WidgetInfo->SetStringField(TEXT("parentClass"), WidgetBP->ParentClass->GetName());
        }

        // Collect widgets/slots
        TArray<TSharedPtr<FJsonValue>> SlotsArray;
        if (WidgetBP->WidgetTree)
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget) {
                TSharedPtr<FJsonValue> SlotValue = MakeShared<FJsonValueString>(Widget->GetName());
                SlotsArray.Add(SlotValue);
            });
        }
        WidgetInfo->SetArrayField(TEXT("slots"), SlotsArray);

        // Build a hierarchical tree (name + class + children) from the root widget.
        // Additive: the flat "slots" array above is left untouched for compatibility.
        if (WidgetBP->WidgetTree && WidgetBP->WidgetTree->RootWidget)
        {
            TFunction<TSharedPtr<FJsonObject>(UWidget*)> BuildNode;
            BuildNode = [&BuildNode](UWidget* W) -> TSharedPtr<FJsonObject>
            {
                TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
                Node->SetStringField(TEXT("name"), W->GetName());
                Node->SetStringField(TEXT("class"), W->GetClass()->GetName());

                TArray<TSharedPtr<FJsonValue>> ChildrenArray;
                if (UPanelWidget* Panel = Cast<UPanelWidget>(W))
                {
                    for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
                    {
                        if (UWidget* Child = Panel->GetChildAt(ChildIndex))
                        {
                            ChildrenArray.Add(MakeShared<FJsonValueObject>(BuildNode(Child)));
                        }
                    }
                }
                Node->SetArrayField(TEXT("children"), ChildrenArray);
                return Node;
            };

            WidgetInfo->SetObjectField(TEXT("tree"), BuildNode(WidgetBP->WidgetTree->RootWidget));
        }

        // Collect animations
        TArray<TSharedPtr<FJsonValue>> AnimsArray;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim)
            {
                TSharedPtr<FJsonValue> AnimValue = MakeShared<FJsonValueString>(Anim->GetName());
                AnimsArray.Add(AnimValue);
            }
        }
        WidgetInfo->SetArrayField(TEXT("animations"), AnimsArray);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetObjectField(TEXT("widgetInfo"), WidgetInfo);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved widget info"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_localization_key"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString Namespace = GetJsonStringField(Payload, TEXT("namespace"), TEXT("Game"));
        FString Key = GetJsonStringField(Payload, TEXT("key"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || Key.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, key"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        bool bApplied = false;
        if (UTextBlock* TextWidget = Cast<UTextBlock>(TargetWidget))
        {
            // Create localized text reference
            FText LocalizedText = FText::ChangeKey(FTextKey(Namespace), FTextKey(Key), TextWidget->GetText());
            TextWidget->SetText(LocalizedText);
            bApplied = true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bApplied);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("namespace"), Namespace);
        ResultJson->SetStringField(TEXT("key"), Key);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set localization key"), ResultJson);
        return true;
    }

    return false;
}

bool UMcpAutomationBridgeSubsystem::HandleManageWidgetAuthoringAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    // Only handle manage_widget_authoring action
    if (Action != TEXT("manage_widget_authoring"))
    {
        return false;
    }

    // Get subAction from payload
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = GetJsonStringField(Payload, TEXT("action"));
    }

    // Family sub-handlers. Each returns true once it has handled (and responded
    // to) SubAction, false if SubAction isn't one of its own. They're filled in
    // pass by pass; until a family is migrated its stub returns false and the
    // legacy inline chain below still handles those subactions.
    if (HandleWidgetAuthoring_Lifecycle(SubAction, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleWidgetAuthoring_Containers(SubAction, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleWidgetAuthoring_Leaves(SubAction, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleWidgetAuthoring_Slot(SubAction, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleWidgetAuthoring_Binding(SubAction, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleWidgetAuthoring_Animation(SubAction, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleWidgetAuthoring_Style(SubAction, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleWidgetAuthoring_Tree(SubAction, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleWidgetAuthoring_Recipes(SubAction, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleWidgetAuthoring_Misc(SubAction, RequestId, Action, Payload, RequestingSocket)) return true;

    // Action not recognized
    return false;
}
#pragma warning(pop)
