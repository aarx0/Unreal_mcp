// =============================================================================
// McpAutomationBridge_InputHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Enhanced Input System Handlers
// 
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
// 
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_input (Editor Only)
//   - create_input_action: Create UInputAction asset
//   - create_input_mapping_context: Create UInputMappingContext asset
//   - add_mapping: Add key mapping to context
//   - remove_mapping: Remove key mapping from context
//   - set_input_trigger: Configure trigger on input action
//   - set_input_modifier: Configure modifier on input action
//   - enable_input_mapping: Enable mapping context at runtime
//   - disable_input_action: Disable input action
//   - get_input_info: Get info about input asset
// 
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: InputAction, InputMappingContext
//   - Editor: AssetTools, EditorAssetLibrary
//   - UE 5.1+: EnhancedInputEditorSubsystem
// 
// Security:
//   - All paths sanitized via SanitizeProjectRelativePath()
//   - Asset names validated for path traversal attempts
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_InputHandlers.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "Modules/ModuleManager.h"  // Required for FModuleManager::IsModuleLoaded() runtime checks

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"

// UE 5.1+: EnhancedInputEditorSubsystem
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "EnhancedInputEditorSubsystem.h"
#endif

#include "Factories/Factory.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#endif

namespace
{
#if WITH_EDITOR
/** Converts an optional input key name into an FKey for mapping-specific operations. */
FKey McpInputKeyFromName(const FString& KeyName)
{
    return KeyName.IsEmpty() ? FKey() : FKey(FName(*KeyName));
}

/** Echoes valueType using the schema's string names so readbacks round-trip as inputs. */
FString McpInputActionValueTypeToString(EInputActionValueType ValueType)
{
    switch (ValueType)
    {
    case EInputActionValueType::Boolean: return TEXT("Boolean");
    case EInputActionValueType::Axis1D:  return TEXT("Axis1D");
    case EInputActionValueType::Axis2D:  return TEXT("Axis2D");
    case EInputActionValueType::Axis3D:  return TEXT("Axis3D");
    }
    checkNoEntry();
    return TEXT("Boolean");
}

/** Adds verified mapping readback for an action after key-specific edits. */
void AddInputMappingSummary(
    TSharedPtr<FJsonObject> Result,
    const UInputMappingContext* Context,
    const UInputAction* InAction)
{
    TArray<TSharedPtr<FJsonValue>> Mappings;
    for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
    {
        if (Mapping.Action != InAction)
        {
            continue;
        }

        TSharedPtr<FJsonObject> MappingObject = MakeShared<FJsonObject>();
        MappingObject->SetStringField(TEXT("key"), Mapping.Key.ToString());
        MappingObject->SetNumberField(TEXT("modifierCount"), Mapping.Modifiers.Num());
        MappingObject->SetNumberField(TEXT("triggerCount"), Mapping.Triggers.Num());
        Mappings.Add(MakeShared<FJsonValueObject>(MappingObject));
    }

    Result->SetNumberField(TEXT("mappingCount"), Mappings.Num());
    Result->SetArrayField(TEXT("mappings"), Mappings);
}

/** Creates the requested Enhanced Input modifier using compatibility fallbacks across UE 5.x. */
UInputModifier* CreateInputModifierForType(const FString& ModifierType, UObject* Outer)
{
    if (ModifierType == TEXT("DeadZone") || ModifierType == TEXT("InputModifierDeadZone"))
    {
        return NewObject<UInputModifierDeadZone>(Outer);
    }
    if (ModifierType == TEXT("SmoothDelta") || ModifierType == TEXT("InputModifierSmoothDelta"))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
        return NewObject<UInputModifierSmoothDelta>(Outer);
#else
        return NewObject<UInputModifierSmooth>(Outer);
#endif
    }
    if (ModifierType == TEXT("SwizzleInputAxis") || ModifierType == TEXT("InputModifierSwizzleAxis"))
    {
        return NewObject<UInputModifierSwizzleAxis>(Outer);
    }
    if (ModifierType == TEXT("Negate") || ModifierType == TEXT("InputModifierNegate"))
    {
        return NewObject<UInputModifierNegate>(Outer);
    }
    if (ModifierType == TEXT("Scalar") || ModifierType == TEXT("InputModifierScalar"))
    {
        return NewObject<UInputModifierScalar>(Outer);
    }
    if (ModifierType == TEXT("ScaleByDeltaTime") || ModifierType == TEXT("InputModifierScaleByDeltaTime"))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        return NewObject<UInputModifierScaleByDeltaTime>(Outer);
#else
        return NewObject<UInputModifierScalar>(Outer);
#endif
    }
    if (ModifierType == TEXT("ToWorldSpace") || ModifierType == TEXT("InputModifierToWorldSpace"))
    {
        return NewObject<UInputModifierToWorldSpace>(Outer);
    }

    return nullptr;
}
#endif
}

// =============================================================================
// Input Member Handlers
// =============================================================================
// Dispatch lives in the manage_networking FMcpCall classes
// (Private/MCP/Calls/McpCalls_ManageNetworking.cpp); each HandleInput*
// member here owns one advertised action's body, extracted verbatim from
// the retired dispatcher chain, replicating its whole-dispatcher editor
// gate and EnhancedInput runtime module check.

// -------------------------------------------------------------------------
// create_input_action: Create UInputAction asset
// -------------------------------------------------------------------------
bool McpHandlers::Input::HandleInputCreateInputAction(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    // Runtime check: Verify EnhancedInput module is loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("EnhancedInput")) ||
            !FModuleManager::Get().LoadModule(TEXT("EnhancedInput")))
        {
            S.SendAutomationError(Socket, RequestId,
                TEXT("EnhancedInput plugin is not enabled in this project. Enable the Enhanced Input plugin to use Input features."),
                TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);

    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);

    if (Name.IsEmpty() || Path.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            TEXT("Name and path are required."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Validate and sanitize path
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid path: '%s' contains traversal or invalid characters."), *Path),
            TEXT("INVALID_PATH"));
        return true;
    }

    // Validate asset name
    if (Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\")) || Name.Contains(TEXT("..")))
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid asset name '%s': contains path separators or traversal sequences"), *Name),
            TEXT("INVALID_NAME"));
        return true;
    }

    // Optional ValueType — UInputAction defaults to Boolean; an axis IA otherwise
    // needs a separate set_asset_property hop (undiscoverable). Parse + validate
    // BEFORE creating so a bad value fails fast (no half-made asset).
    EInputActionValueType ParsedValueType = EInputActionValueType::Boolean;
    bool bHasValueType = false;
    {
        FString ValueTypeStr;
        if (Payload->TryGetStringField(TEXT("valueType"), ValueTypeStr) &&
            !ValueTypeStr.TrimStartAndEnd().IsEmpty())
        {
            bHasValueType = true;
            const FString V = ValueTypeStr.TrimStartAndEnd();
            if (V.Equals(TEXT("Boolean"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
                ParsedValueType = EInputActionValueType::Boolean;
            else if (V.Equals(TEXT("Axis1D"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Float"), ESearchCase::IgnoreCase) || V.Equals(TEXT("1D"), ESearchCase::IgnoreCase))
                ParsedValueType = EInputActionValueType::Axis1D;
            else if (V.Equals(TEXT("Axis2D"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Vector2D"), ESearchCase::IgnoreCase) || V.Equals(TEXT("2D"), ESearchCase::IgnoreCase))
                ParsedValueType = EInputActionValueType::Axis2D;
            else if (V.Equals(TEXT("Axis3D"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) || V.Equals(TEXT("3D"), ESearchCase::IgnoreCase))
                ParsedValueType = EInputActionValueType::Axis3D;
            else
            {
                S.SendAutomationError(Socket, RequestId,
                    FString::Printf(TEXT("Invalid valueType '%s' (expected Boolean|Axis1D|Axis2D|Axis3D)"), *ValueTypeStr),
                    TEXT("INVALID_VALUE"));
                return true;
            }
        }
    }

    const FString FullPath = FString::Printf(TEXT("%s/%s"), *SanitizedPath, *Name);
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        UInputAction* ExistingAction = Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(FullPath));
        if (!ExistingAction)
        {
            S.SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Asset already exists at %s but is not an InputAction"), *FullPath),
                TEXT("ASSET_TYPE_MISMATCH"));
            return true;
        }

        // Idempotent: if valueType was given, ensure the existing IA matches it.
        if (bHasValueType && ExistingAction->ValueType != ParsedValueType)
        {
            ExistingAction->Modify();
            ExistingAction->ValueType = ParsedValueType;
            SaveLoadedAssetThrottled(ExistingAction, -1.0, true);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), ExistingAction->GetPathName());
        Result->SetStringField(TEXT("valueType"), McpInputActionValueTypeToString(ExistingAction->ValueType));
        McpHandlerUtils::AddVerification(Result, ExistingAction);

        S.SendAutomationResponse(Socket, RequestId, true,
            TEXT("Input Action already exists."), Result);
        return true;
    }

    IAssetTools& AssetTools = FModuleManager::Get()
        .LoadModuleChecked<FAssetToolsModule>("AssetTools")
        .Get();

    UClass* ActionClass = UInputAction::StaticClass();
    UObject* NewAsset = AssetTools.CreateAsset(Name, SanitizedPath, ActionClass, nullptr);

    if (NewAsset)
    {
        UInputAction* CreatedAction = Cast<UInputAction>(NewAsset);
        if (bHasValueType && CreatedAction)
        {
            CreatedAction->ValueType = ParsedValueType;
        }
        SaveLoadedAssetThrottled(NewAsset, -1.0, true);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
        if (CreatedAction)
        {
            Result->SetStringField(TEXT("valueType"), McpInputActionValueTypeToString(CreatedAction->ValueType));
        }
        McpHandlerUtils::AddVerification(Result, NewAsset);

        S.SendAutomationResponse(Socket, RequestId, true,
            TEXT("Input Action created."), Result);
    }
    else
    {
        S.SendAutomationError(Socket, RequestId,
            TEXT("Failed to create Input Action."), TEXT("CREATION_FAILED"));
    }
    return true;
#else
    // Non-editor build
    S.SendAutomationError(Socket, RequestId,
        TEXT("Input management requires Editor build."), TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

// -------------------------------------------------------------------------
// create_input_mapping_context: Create UInputMappingContext asset
// -------------------------------------------------------------------------
bool McpHandlers::Input::HandleInputCreateInputMappingContext(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    // Runtime check: Verify EnhancedInput module is loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("EnhancedInput")) ||
            !FModuleManager::Get().LoadModule(TEXT("EnhancedInput")))
        {
            S.SendAutomationError(Socket, RequestId,
                TEXT("EnhancedInput plugin is not enabled in this project. Enable the Enhanced Input plugin to use Input features."),
                TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);

    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);

    if (Name.IsEmpty() || Path.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            TEXT("Name and path are required."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Validate and sanitize path
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid path: '%s' contains traversal or invalid characters."), *Path),
            TEXT("INVALID_PATH"));
        return true;
    }

    // Validate asset name
    if (Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\")) || Name.Contains(TEXT("..")))
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid asset name '%s': contains path separators or traversal sequences"), *Name),
            TEXT("INVALID_NAME"));
        return true;
    }

    const FString FullPath = FString::Printf(TEXT("%s/%s"), *SanitizedPath, *Name);
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        UInputMappingContext* ExistingContext = Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(FullPath));
        if (!ExistingContext)
        {
            S.SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Asset already exists at %s but is not an InputMappingContext"), *FullPath),
                TEXT("ASSET_TYPE_MISMATCH"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), ExistingContext->GetPathName());
        McpHandlerUtils::AddVerification(Result, ExistingContext);

        S.SendAutomationResponse(Socket, RequestId, true,
            TEXT("Input Mapping Context already exists."), Result);
        return true;
    }

    IAssetTools& AssetTools = FModuleManager::Get()
        .LoadModuleChecked<FAssetToolsModule>("AssetTools")
        .Get();

    UClass* ContextClass = UInputMappingContext::StaticClass();
    UObject* NewAsset = AssetTools.CreateAsset(Name, SanitizedPath, ContextClass, nullptr);

    if (NewAsset)
    {
        SaveLoadedAssetThrottled(NewAsset, -1.0, true);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
        McpHandlerUtils::AddVerification(Result, NewAsset);

        S.SendAutomationResponse(Socket, RequestId, true,
            TEXT("Input Mapping Context created."), Result);
    }
    else
    {
        S.SendAutomationError(Socket, RequestId,
            TEXT("Failed to create Input Mapping Context."), TEXT("CREATION_FAILED"));
    }
    return true;
#else
    // Non-editor build
    S.SendAutomationError(Socket, RequestId,
        TEXT("Input management requires Editor build."), TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

// -------------------------------------------------------------------------
// add_mapping: Add key mapping to context
// -------------------------------------------------------------------------
bool McpHandlers::Input::HandleInputAddMapping(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    // Runtime check: Verify EnhancedInput module is loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("EnhancedInput")) ||
            !FModuleManager::Get().LoadModule(TEXT("EnhancedInput")))
        {
            S.SendAutomationError(Socket, RequestId,
                TEXT("EnhancedInput plugin is not enabled in this project. Enable the Enhanced Input plugin to use Input features."),
                TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);

    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

    FString KeyName;
    Payload->TryGetStringField(TEXT("key"), KeyName);

    // Validate and sanitize paths
    FString SanitizedContextPath = SanitizeProjectRelativePath(ContextPath);
    FString SanitizedActionPath = SanitizeProjectRelativePath(ActionPath);

    if (SanitizedContextPath.IsEmpty() || SanitizedActionPath.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            TEXT("Invalid context or action path: contains traversal or invalid characters."),
            TEXT("INVALID_PATH"));
        return true;
    }

    UInputMappingContext* Context = Cast<UInputMappingContext>(
        UEditorAssetLibrary::LoadAsset(SanitizedContextPath));
    UInputAction* InAction = Cast<UInputAction>(
        UEditorAssetLibrary::LoadAsset(SanitizedActionPath));

    if (!Context || !InAction || KeyName.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Context or action not found, or key is empty. Context: %s, Action: %s"),
                *SanitizedContextPath, *SanitizedActionPath),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FKey Key = FKey(FName(*KeyName));
    if (!Key.IsValid())
    {
        S.SendAutomationError(Socket, RequestId,
            TEXT("Invalid key name."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Modify() before mutating the mapping array so the transaction/dirty
    // tracking matches set_input_modifier (and the DefaultKeyMappings dirty
    // trap right next door) — MapKey dirties internally today, but relying
    // on that is the inconsistency this aligns.
    Context->Modify();
    Context->MapKey(InAction, Key);
    SaveLoadedAssetThrottled(Context, -1.0, true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
    Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
    Result->SetStringField(TEXT("key"), KeyName);
    AddAssetVerificationNested(Result, TEXT("contextVerification"), Context);
    AddAssetVerificationNested(Result, TEXT("actionVerification"), InAction);

    S.SendAutomationResponse(Socket, RequestId, true,
        TEXT("Mapping added."), Result);
    return true;
#else
    // Non-editor build
    S.SendAutomationError(Socket, RequestId,
        TEXT("Input management requires Editor build."), TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

// -------------------------------------------------------------------------
// remove_mapping: Remove key mapping from context
// -------------------------------------------------------------------------
bool McpHandlers::Input::HandleInputRemoveMapping(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    // Runtime check: Verify EnhancedInput module is loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("EnhancedInput")) ||
            !FModuleManager::Get().LoadModule(TEXT("EnhancedInput")))
        {
            S.SendAutomationError(Socket, RequestId,
                TEXT("EnhancedInput plugin is not enabled in this project. Enable the Enhanced Input plugin to use Input features."),
                TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);

    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

    FString KeyName;
    Payload->TryGetStringField(TEXT("key"), KeyName);

    // Validate and sanitize paths
    FString SanitizedContextPath = SanitizeProjectRelativePath(ContextPath);
    FString SanitizedActionPath = SanitizeProjectRelativePath(ActionPath);

    if (SanitizedContextPath.IsEmpty() || SanitizedActionPath.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            TEXT("Invalid context or action path: contains traversal or invalid characters."),
            TEXT("INVALID_PATH"));
        return true;
    }

    UInputMappingContext* Context = Cast<UInputMappingContext>(
        UEditorAssetLibrary::LoadAsset(SanitizedContextPath));
    UInputAction* InAction = Cast<UInputAction>(
        UEditorAssetLibrary::LoadAsset(SanitizedActionPath));

    if (!Context || !InAction)
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Context or action not found. Context: %s, Action: %s"),
                *SanitizedContextPath, *SanitizedActionPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    FKey RequestedKey = McpInputKeyFromName(KeyName);
    const bool bHasSpecificKey = !KeyName.IsEmpty();
    if (bHasSpecificKey && !RequestedKey.IsValid())
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid key name: %s"), *KeyName), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Collect matching keys before mutating the mapping array.
    TArray<FKey> KeysToRemove;
    for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
    {
        if (Mapping.Action == InAction && (!bHasSpecificKey || Mapping.Key == RequestedKey))
        {
            KeysToRemove.Add(Mapping.Key);
        }
    }

    if (bHasSpecificKey && KeysToRemove.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Mapping not found for action '%s' and key '%s'."),
                *SanitizedActionPath, *KeyName),
            TEXT("NOT_FOUND"));
        return true;
    }

    Context->Modify();
    for (const FKey& KeyToRemove : KeysToRemove)
    {
        Context->UnmapKey(InAction, KeyToRemove);
    }

    SaveLoadedAssetThrottled(Context, -1.0, true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
    Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
    if (bHasSpecificKey)
    {
        Result->SetStringField(TEXT("key"), KeyName);
    }
    Result->SetNumberField(TEXT("keysRemoved"), KeysToRemove.Num());

    TArray<TSharedPtr<FJsonValue>> RemovedKeys;
    for (const FKey& Key : KeysToRemove)
    {
        RemovedKeys.Add(MakeShared<FJsonValueString>(Key.ToString()));
    }
    Result->SetArrayField(TEXT("removedKeys"), RemovedKeys);
    AddInputMappingSummary(Result, Context, InAction);

    AddAssetVerificationNested(Result, TEXT("contextVerification"), Context);
    AddAssetVerificationNested(Result, TEXT("actionVerification"), InAction);

    const FString SuccessMessage = bHasSpecificKey
        ? FString::Printf(TEXT("Mapping removed for action key: %s"), *KeyName)
        : TEXT("Mappings removed for action.");
    S.SendAutomationResponse(Socket, RequestId, true, SuccessMessage, Result);
    return true;
#else
    // Non-editor build
    S.SendAutomationError(Socket, RequestId,
        TEXT("Input management requires Editor build."), TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

// -------------------------------------------------------------------------
// set_input_trigger: Configure trigger on input action
// -------------------------------------------------------------------------
bool McpHandlers::Input::HandleInputSetInputTrigger(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    // Runtime check: Verify EnhancedInput module is loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("EnhancedInput")) ||
            !FModuleManager::Get().LoadModule(TEXT("EnhancedInput")))
        {
            S.SendAutomationError(Socket, RequestId,
                TEXT("EnhancedInput plugin is not enabled in this project. Enable the Enhanced Input plugin to use Input features."),
                TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

    FString TriggerType;
    Payload->TryGetStringField(TEXT("triggerType"), TriggerType);

    FString SanitizedActionPath = SanitizeProjectRelativePath(ActionPath);
    if (SanitizedActionPath.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid action path: '%s' contains traversal or invalid characters."), *ActionPath),
            TEXT("INVALID_PATH"));
        return true;
    }

    UInputAction* InAction = Cast<UInputAction>(
        UEditorAssetLibrary::LoadAsset(SanitizedActionPath));

    if (!InAction)
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Action not found: %s"), *SanitizedActionPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    // Create the appropriate trigger based on type
    UInputTrigger* NewTrigger = nullptr;
    
    // Map common trigger type names to their classes
    if (TriggerType == TEXT("Pressed") || TriggerType == TEXT("InputTriggerPressed"))
    {
        NewTrigger = NewObject<UInputTriggerPressed>(InAction);
    }
    else if (TriggerType == TEXT("Released") || TriggerType == TEXT("InputTriggerReleased"))
    {
        NewTrigger = NewObject<UInputTriggerReleased>(InAction);
    }
    else if (TriggerType == TEXT("Down") || TriggerType == TEXT("InputTriggerDown"))
    {
        NewTrigger = NewObject<UInputTriggerDown>(InAction);
    }
    else if (TriggerType == TEXT("Tap") || TriggerType == TEXT("InputTriggerTap"))
    {
        NewTrigger = NewObject<UInputTriggerTap>(InAction);
    }
    else if (TriggerType == TEXT("Hold") || TriggerType == TEXT("InputTriggerHold"))
    {
        NewTrigger = NewObject<UInputTriggerHold>(InAction);
    }
    else if (TriggerType == TEXT("HoldAndRelease") || TriggerType == TEXT("InputTriggerHoldAndRelease"))
    {
        NewTrigger = NewObject<UInputTriggerHoldAndRelease>(InAction);
    }
    else if (TriggerType == TEXT("Pulse") || TriggerType == TEXT("InputTriggerPulse"))
    {
        NewTrigger = NewObject<UInputTriggerPulse>(InAction);
    }
    else if (TriggerType == TEXT("RepeatedTap") || TriggerType == TEXT("InputTriggerRepeatedTap") || TriggerType == TEXT("DoubleTap"))
    {
        // UInputTriggerRepeatedTap was added in UE 5.6
        // For earlier versions, use UInputTriggerTap (single tap) or UInputTriggerHold as fallback
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
        UInputTriggerRepeatedTap* RepeatedTapTrigger = NewObject<UInputTriggerRepeatedTap>(InAction);
        NewTrigger = RepeatedTapTrigger;
#else
        // Fallback for UE 5.0-5.5: Use UInputTriggerTap as closest equivalent
        NewTrigger = NewObject<UInputTriggerTap>(InAction);
#endif
    }
    
    if (!NewTrigger)
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown trigger type: %s. Supported: Pressed, Released, Down, Tap, Hold, HoldAndRelease, Pulse, RepeatedTap, DoubleTap"), *TriggerType),
            TEXT("INVALID_TRIGGER_TYPE"));
        return true;
    }

    // Triggers.Add is append-only — a plain re-run stacks duplicates of the same
    // class. Default to idempotent (skip if this trigger class is already present);
    // replace:true clears all triggers first for a clean full re-author.
    bool bReplace = false;
    Payload->TryGetBoolField(TEXT("replace"), bReplace);

    InAction->Modify();
    bool bAlreadyPresent = false;
    if (bReplace)
    {
        InAction->Triggers.Empty();
    }
    else
    {
        for (const UInputTrigger* Existing : InAction->Triggers)
        {
            if (Existing && Existing->GetClass() == NewTrigger->GetClass())
            {
                bAlreadyPresent = true;
                break;
            }
        }
    }

    if (!bAlreadyPresent)
    {
        InAction->Triggers.Add(NewTrigger);
        SaveLoadedAssetThrottled(InAction, -1.0, true);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
    Result->SetStringField(TEXT("triggerType"), TriggerType);
    Result->SetBoolField(TEXT("triggerSet"), !bAlreadyPresent);
    Result->SetBoolField(TEXT("alreadyPresent"), bAlreadyPresent);
    Result->SetBoolField(TEXT("replaced"), bReplace);
    Result->SetNumberField(TEXT("triggerCount"), InAction->Triggers.Num());
    McpHandlerUtils::AddVerification(Result, InAction);

    S.SendAutomationResponse(Socket, RequestId, true,
        bAlreadyPresent
            ? FString::Printf(TEXT("Trigger '%s' already present (idempotent); pass replace:true to re-author."), *TriggerType)
            : FString::Printf(TEXT("Trigger '%s' configured on action."), *TriggerType),
        Result);
    return true;
#else
    // Non-editor build
    S.SendAutomationError(Socket, RequestId,
        TEXT("Input management requires Editor build."), TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

// -------------------------------------------------------------------------
// set_input_modifier: Configure modifier on input action
// -------------------------------------------------------------------------
bool McpHandlers::Input::HandleInputSetInputModifier(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    // Runtime check: Verify EnhancedInput module is loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("EnhancedInput")) ||
            !FModuleManager::Get().LoadModule(TEXT("EnhancedInput")))
        {
            S.SendAutomationError(Socket, RequestId,
                TEXT("EnhancedInput plugin is not enabled in this project. Enable the Enhanced Input plugin to use Input features."),
                TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);

    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

    FString KeyName;
    Payload->TryGetStringField(TEXT("key"), KeyName);

    FString ModifierType;
    Payload->TryGetStringField(TEXT("modifierType"), ModifierType);

    const bool bTargetMapping = !ContextPath.IsEmpty() || !KeyName.IsEmpty();
    if (bTargetMapping && (ContextPath.IsEmpty() || KeyName.IsEmpty()))
    {
        S.SendAutomationError(Socket, RequestId,
            TEXT("contextPath and key are both required when setting a modifier on a specific mapping."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString SanitizedActionPath = SanitizeProjectRelativePath(ActionPath);
    if (SanitizedActionPath.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid action path: '%s' contains traversal or invalid characters."), *ActionPath),
            TEXT("INVALID_PATH"));
        return true;
    }

    UInputAction* InAction = Cast<UInputAction>(
        UEditorAssetLibrary::LoadAsset(SanitizedActionPath));

    if (!InAction)
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Action not found: %s"), *SanitizedActionPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    UObject* ModifierOuter = InAction;
    UInputMappingContext* Context = nullptr;
    FString SanitizedContextPath;
    FEnhancedActionKeyMapping* TargetMapping = nullptr;
    FKey RequestedKey = McpInputKeyFromName(KeyName);

    if (bTargetMapping)
    {
        if (!RequestedKey.IsValid())
        {
            S.SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Invalid key name: %s"), *KeyName), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        SanitizedContextPath = SanitizeProjectRelativePath(ContextPath);
        if (SanitizedContextPath.IsEmpty())
        {
            S.SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Invalid context path: '%s' contains traversal or invalid characters."), *ContextPath),
                TEXT("INVALID_PATH"));
            return true;
        }

        Context = Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(SanitizedContextPath));
        if (!Context)
        {
            S.SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Context not found: %s"), *SanitizedContextPath),
                TEXT("NOT_FOUND"));
            return true;
        }

        const int32 MappingCount = Context->GetMappings().Num();
        for (int32 MappingIndex = 0; MappingIndex < MappingCount; ++MappingIndex)
        {
            FEnhancedActionKeyMapping& Mapping = Context->GetMapping(MappingIndex);
            if (Mapping.Action == InAction && Mapping.Key == RequestedKey)
            {
                TargetMapping = &Mapping;
                break;
            }
        }

        if (!TargetMapping)
        {
            S.SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Mapping not found for action '%s' and key '%s'."),
                    *SanitizedActionPath, *KeyName),
                TEXT("NOT_FOUND"));
            return true;
        }

        ModifierOuter = Context;
    }

    UInputModifier* NewModifier = CreateInputModifierForType(ModifierType, ModifierOuter);
    if (!NewModifier)
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown modifier type: %s. Supported: DeadZone, SmoothDelta, SwizzleInputAxis, Negate, Scalar, ScaleByDeltaTime, ToWorldSpace"), *ModifierType),
            TEXT("INVALID_MODIFIER_TYPE"));
        return true;
    }

    if (TargetMapping)
    {
        Context->Modify();
        TargetMapping->Modifiers.Add(NewModifier);
        SaveLoadedAssetThrottled(Context, -1.0, true);
    }
    else
    {
        InAction->Modify();
        InAction->Modifiers.Add(NewModifier);
        SaveLoadedAssetThrottled(InAction, -1.0, true);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
    Result->SetStringField(TEXT("modifierType"), ModifierType);
    Result->SetBoolField(TEXT("modifierSet"), true);
    Result->SetStringField(TEXT("target"), TargetMapping ? TEXT("mapping") : TEXT("action"));
    if (TargetMapping)
    {
        Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
        Result->SetStringField(TEXT("key"), KeyName);
        Result->SetNumberField(TEXT("mappingModifierCount"), TargetMapping->Modifiers.Num());
        AddInputMappingSummary(Result, Context, InAction);
        AddAssetVerificationNested(Result, TEXT("contextVerification"), Context);
        AddAssetVerificationNested(Result, TEXT("actionVerification"), InAction);
    }
    else
    {
        McpHandlerUtils::AddVerification(Result, InAction);
    }

    S.SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Modifier '%s' configured on action."), *ModifierType), Result);
    return true;
#else
    // Non-editor build
    S.SendAutomationError(Socket, RequestId,
        TEXT("Input management requires Editor build."), TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

// -------------------------------------------------------------------------
// enable_input_mapping: Enable mapping context at runtime
// -------------------------------------------------------------------------
bool McpHandlers::Input::HandleInputEnableInputMapping(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    // Runtime check: Verify EnhancedInput module is loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("EnhancedInput")) ||
            !FModuleManager::Get().LoadModule(TEXT("EnhancedInput")))
        {
            S.SendAutomationError(Socket, RequestId,
                TEXT("EnhancedInput plugin is not enabled in this project. Enable the Enhanced Input plugin to use Input features."),
                TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);

    int32 Priority = 0;
    Payload->TryGetNumberField(TEXT("priority"), Priority);

    FString SanitizedContextPath = SanitizeProjectRelativePath(ContextPath);
    if (SanitizedContextPath.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid context path: '%s' contains traversal or invalid characters."), *ContextPath),
            TEXT("INVALID_PATH"));
        return true;
    }

    UInputMappingContext* Context = Cast<UInputMappingContext>(
        UEditorAssetLibrary::LoadAsset(SanitizedContextPath));

    if (!Context)
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Context not found: %s"), *SanitizedContextPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    // Note: Runtime enabling requires player controller and EnhancedInputSubsystem
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
    Result->SetNumberField(TEXT("priority"), Priority);
    Result->SetBoolField(TEXT("enabled"), true);
    McpHandlerUtils::AddVerification(Result, Context);

    S.SendAutomationResponse(Socket, RequestId, true,
        TEXT("Input mapping context enabled (requires PIE for runtime effect)."), Result);
    return true;
#else
    // Non-editor build
    S.SendAutomationError(Socket, RequestId,
        TEXT("Input management requires Editor build."), TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

// -------------------------------------------------------------------------
// disable_input_action: Disable input action
// -------------------------------------------------------------------------
bool McpHandlers::Input::HandleInputDisableInputAction(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    // Runtime check: Verify EnhancedInput module is loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("EnhancedInput")) ||
            !FModuleManager::Get().LoadModule(TEXT("EnhancedInput")))
        {
            S.SendAutomationError(Socket, RequestId,
                TEXT("EnhancedInput plugin is not enabled in this project. Enable the Enhanced Input plugin to use Input features."),
                TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

    FString SanitizedActionPath = SanitizeProjectRelativePath(ActionPath);
    if (SanitizedActionPath.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid action path: '%s' contains traversal or invalid characters."), *ActionPath),
            TEXT("INVALID_PATH"));
        return true;
    }

    UInputAction* InAction = Cast<UInputAction>(
        UEditorAssetLibrary::LoadAsset(SanitizedActionPath));

    if (!InAction)
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Action not found: %s"), *SanitizedActionPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
    Result->SetBoolField(TEXT("disabled"), true);
    McpHandlerUtils::AddVerification(Result, InAction);

    S.SendAutomationResponse(Socket, RequestId, true,
        TEXT("Input action disabled."), Result);
    return true;
#else
    // Non-editor build
    S.SendAutomationError(Socket, RequestId,
        TEXT("Input management requires Editor build."), TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

// -------------------------------------------------------------------------
// get_input_info: Get info about input asset
// -------------------------------------------------------------------------
bool McpHandlers::Input::HandleInputGetInputInfo(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    // Runtime check: Verify EnhancedInput module is loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("EnhancedInput")) ||
            !FModuleManager::Get().LoadModule(TEXT("EnhancedInput")))
        {
            S.SendAutomationError(Socket, RequestId,
                TEXT("EnhancedInput plugin is not enabled in this project. Enable the Enhanced Input plugin to use Input features."),
                TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    if (AssetPath.IsEmpty())
    {
        // manage_networking (which re-dispatches input actions) exposes blueprintPath,
        // not assetPath — accept it as a fallback so get_input_info is reachable that way.
        Payload->TryGetStringField(TEXT("blueprintPath"), AssetPath);
    }

    if (AssetPath.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            TEXT("assetPath (or blueprintPath) is required."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SanitizedAssetPath.IsEmpty())
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid asset path: '%s' contains traversal or invalid characters."), *AssetPath),
            TEXT("INVALID_PATH"));
        return true;
    }

    UObject* Asset = UEditorAssetLibrary::LoadAsset(SanitizedAssetPath);
    if (!Asset)
    {
        S.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Asset not found: %s"), *SanitizedAssetPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), SanitizedAssetPath);
    Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
    Result->SetStringField(TEXT("assetName"), Asset->GetName());

    // Add type-specific info
    if (UInputAction* InputAction = Cast<UInputAction>(Asset))
    {
        Result->SetStringField(TEXT("type"), TEXT("InputAction"));
        Result->SetStringField(TEXT("valueType"), McpInputActionValueTypeToString(InputAction->ValueType));
        Result->SetBoolField(TEXT("consumeInput"), InputAction->bConsumeInput);
        // The IA's OWN triggers (class names) — set_input_trigger writes these, but
        // only IMC mappings carried a triggerCount, so the trigger *class* was
        // un-confirmable via the bridge. List them here.
        TArray<TSharedPtr<FJsonValue>> TriggersArr;
        for (const UInputTrigger* Trigger : InputAction->Triggers)
        {
            if (Trigger)
            {
                TriggersArr.Add(MakeShared<FJsonValueString>(Trigger->GetClass()->GetName()));
            }
        }
        Result->SetArrayField(TEXT("triggers"), TriggersArr);
        Result->SetNumberField(TEXT("triggerCount"), InputAction->Triggers.Num());
    }
    else if (UInputMappingContext* Context = Cast<UInputMappingContext>(Asset))
    {
        Result->SetStringField(TEXT("type"), TEXT("InputMappingContext"));
        const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();
        Result->SetNumberField(TEXT("mappingCount"), Mappings.Num());
        // List each action->key binding. GetMappings() is the canonical accessor and reads
        // the live storage even when the raw 'Mappings' UPROPERTY reflects empty (UE5.7
        // keeps the data in the nested DefaultKeyMappings struct) — so no poking that struct.
        TArray<TSharedPtr<FJsonValue>> MappingsArr;
        for (const FEnhancedActionKeyMapping& M : Mappings)
        {
            TSharedPtr<FJsonObject> MObj = MakeShared<FJsonObject>();
            MObj->SetStringField(TEXT("action"), M.Action ? M.Action->GetPathName() : TEXT(""));
            MObj->SetStringField(TEXT("actionName"), M.Action ? M.Action->GetName() : TEXT(""));
            MObj->SetStringField(TEXT("key"), M.Key.GetFName().ToString());
            MObj->SetNumberField(TEXT("triggerCount"), M.Triggers.Num());
            MObj->SetNumberField(TEXT("modifierCount"), M.Modifiers.Num());
            MappingsArr.Add(MakeShared<FJsonValueObject>(MObj));
        }
        Result->SetArrayField(TEXT("mappings"), MappingsArr);
    }

    McpHandlerUtils::AddVerification(Result, Asset);

    S.SendAutomationResponse(Socket, RequestId, true,
        TEXT("Input asset info retrieved."), Result);
    return true;
#else
    // Non-editor build
    S.SendAutomationError(Socket, RequestId,
        TEXT("Input management requires Editor build."), TEXT("NOT_AVAILABLE"));
    return true;
#endif
}
