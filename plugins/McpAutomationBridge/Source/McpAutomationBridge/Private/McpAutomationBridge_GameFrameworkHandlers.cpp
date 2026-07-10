// =============================================================================
// McpAutomationBridge_GameFrameworkHandlers.cpp
// =============================================================================
// Game Framework System Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// ---------------------
// Section 1: Core Classes
//   - create_game_mode             : Create AGameMode Blueprint
//   - create_game_state            : Create AGameState Blueprint
//   - create_player_controller      : Create APlayerController Blueprint
//   - create_player_state          : Create APlayerState Blueprint
//   - create_game_instance         : Create UGameInstance Blueprint
//   - create_hud_class             : Create AHUD Blueprint
//
// Section 2: Game Mode Configuration
//   - set_default_pawn_class       : Set default pawn class
//   - set_player_controller_class  : Set player controller class
//   - set_game_state_class         : Set game state class
//   - set_player_state_class       : Set player state class
//   - configure_game_rules         : Set game rules
//
// Section 3: Match Flow
//   - setup_match_states           : Set current match state
//   - configure_round_system       : Setup round-based gameplay
//   - configure_team_system        : Setup team-based gameplay
//   - configure_scoring_system     : Configure scoring
//
// Section 4: Player Management
//   - configure_spawn_system       : Configure spawn system
//   - configure_player_start       : Create APlayerStart actor
//   - set_respawn_rules            : Set respawn parameters
//   - configure_spectating         : Setup spectator system
//
// Section 5: Utility
//   - get_game_framework_info      : Get game framework info
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.7: All handlers supported
// - GameMode/GameState/PlayerController APIs stable
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpResponseHelpers.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// Game Framework classes
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/Pawn.h"
#include "EdGraphSchema_K2.h"
#include "Kismet/GameplayStatics.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpGameFrameworkHandlers, Log, All);

// Helper to set blueprint variable default value (multi-version compatible)
// Uses reflection to set the default value on the CDO
static void SetBPVarDefaultValueGF(UBlueprint* Blueprint, FName VarName, const FString& DefaultValue)
{
#if WITH_EDITOR
    if (!Blueprint)
    {
        return;
    }
    
    // Compile the blueprint first to ensure GeneratedClass exists
    McpSafeCompileBlueprint(Blueprint);
    
    if (Blueprint->GeneratedClass)
    {
        if (UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject())
        {
            FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
            if (Property)
            {
                void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CDO);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                // UE 5.1+: Use ImportText_Direct
                Property->ImportText_Direct(*DefaultValue, ValuePtr, CDO, 0);
#else
                // UE 5.0: Use ImportText with different signature
                Property->ImportText(*DefaultValue, ValuePtr, PPF_None, CDO);
#endif
                Blueprint->MarkPackageDirty();
            }
        }
    }
#endif
}

// ============================================================================
// Legacy Helper Functions
// NOTE: These helpers are retained for backward compatibility.
// New code should prefer McpHandlerUtils:: functions instead.
// ============================================================================
// Helper Functions
// NOTE: These helpers follow the existing pattern in other *Handlers.cpp files.
// A future refactor could consolidate these into McpAutomationBridgeHelpers.h
// for shared use across all handler files.
// ============================================================================

namespace GameFrameworkHelpers
{
    // Get string field with default
    FString GetStringField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, const FString& Default = TEXT(""))
    {
        if (Payload.IsValid() && Payload->HasField(FieldName))
        {
            return GetJsonStringField(Payload, FieldName);
        }
        return Default;
    }

    // Get number field with default
    double GetNumberField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, double Default = 0.0)
    {
        if (Payload.IsValid() && Payload->HasField(FieldName))
        {
            return GetJsonNumberField(Payload, FieldName);
        }
        return Default;
    }

    // Get bool field with default
    bool GetBoolField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, bool Default = false)
    {
        if (Payload.IsValid() && Payload->HasField(FieldName))
        {
            return GetJsonBoolField(Payload, FieldName);
        }
        return Default;
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

#if WITH_EDITOR
    // Load Blueprint from path
    UBlueprint* LoadBlueprintFromPath(const FString& BlueprintPath)
    {
        FString CleanPath = BlueprintPath;
        if (!CleanPath.EndsWith(TEXT("_C")))
        {
            UBlueprint* BP = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *CleanPath));
            if (BP) return BP;
            
            if (CleanPath.EndsWith(TEXT(".uasset")))
            {
                CleanPath = CleanPath.LeftChop(7);
                BP = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *CleanPath));
            }
            return BP;
        }
        return nullptr;
    }

    // Create a Blueprint of specified parent class
    UBlueprint* CreateGameFrameworkBlueprint(const FString& Path, const FString& Name, UClass* ParentClass, FString& OutError)
    {
        if (!ParentClass)
        {
            OutError = TEXT("Invalid parent class");
            return nullptr;
        }

        // Ensure path starts with /Game/
        FString FullPath = Path;
        if (!FullPath.StartsWith(TEXT("/Game/")))
        {
            if (FullPath.StartsWith(TEXT("/Content/")))
            {
                FullPath = FullPath.Replace(TEXT("/Content/"), TEXT("/Game/"));
            }
            else if (!FullPath.StartsWith(TEXT("/")))
            {
                FullPath = TEXT("/Game/") + FullPath;
            }
        }
        
        // Remove trailing slash if present
        if (FullPath.EndsWith(TEXT("/")))
        {
            FullPath = FullPath.LeftChop(1);
        }
        
        FString AssetPath = FullPath / Name;
        
        // CRITICAL: Check if a Blueprint with this name already exists to prevent
        // engine assertion failure in Kismet2.cpp (line 435). The engine asserts
        // that no Blueprint with the target name exists before creation.
        // This prevents crashes from name collisions between different action tests.
        UBlueprint* ExistingBP = FindObject<UBlueprint>(nullptr, *AssetPath);
        if (ExistingBP)
        {
            OutError = FString::Printf(TEXT("Blueprint already exists: %s"), *AssetPath);
            return nullptr;
        }
        
        // Also check using UEditorAssetLibrary for assets that may not be loaded yet
        // This is version-safe and works across UE 5.0-5.7
        if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
        {
            OutError = FString::Printf(TEXT("Asset already exists at path: %s"), *AssetPath);
            return nullptr;
        }
        
        UPackage* Package = CreatePackage(*AssetPath);
        if (!Package)
        {
            OutError = FString::Printf(TEXT("Failed to create package: %s"), *AssetPath);
            return nullptr;
        }

        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        Factory->ParentClass = ParentClass;

        UBlueprint* Blueprint = Cast<UBlueprint>(
            Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                      RF_Public | RF_Standalone, nullptr, GWarn));

        if (!Blueprint)
        {
            OutError = FString::Printf(TEXT("Failed to create %s blueprint"), *ParentClass->GetName());
            return nullptr;
        }

        FAssetRegistryModule::AssetCreated(Blueprint);
        Blueprint->MarkPackageDirty();
        
        // Compile the blueprint
        McpSafeCompileBlueprint(Blueprint);
        
        return Blueprint;
    }

    // Set a TSubclassOf property on a Blueprint CDO
    bool SetClassProperty(UBlueprint* Blueprint, const FName& PropertyName, UClass* ClassToSet, FString& OutError)
    {
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            OutError = TEXT("Invalid blueprint or generated class");
            return false;
        }

        UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
        if (!CDO)
        {
            OutError = TEXT("Failed to get CDO");
            return false;
        }

        // Find the property
        FProperty* Prop = Blueprint->GeneratedClass->FindPropertyByName(PropertyName);
        if (!Prop)
        {
            // Try on parent class
            Prop = Blueprint->ParentClass->FindPropertyByName(PropertyName);
        }

        if (!Prop)
        {
            OutError = FString::Printf(TEXT("Property '%s' not found"), *PropertyName.ToString());
            return false;
        }

        FClassProperty* ClassProp = CastField<FClassProperty>(Prop);
        if (ClassProp)
        {
            ClassProp->SetPropertyValue_InContainer(CDO, ClassToSet);
            CDO->MarkPackageDirty();
            return true;
        }

        // Try soft class property
        FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Prop);
        if (SoftClassProp)
        {
            FSoftObjectPtr SoftPtr(ClassToSet);
            SoftClassProp->SetPropertyValue_InContainer(CDO, SoftPtr);
            CDO->MarkPackageDirty();
            return true;
        }

        OutError = FString::Printf(TEXT("Property '%s' is not a class property"), *PropertyName.ToString());
        return false;
    }

    // Load class from path (Blueprint or native)
    UClass* LoadClassFromPath(const FString& ClassPath)
    {
        if (ClassPath.IsEmpty())
        {
            return nullptr;
        }

        // Try loading as native class first
        UClass* NativeClass = FindObject<UClass>(nullptr, *ClassPath);
        if (NativeClass)
        {
            return NativeClass;
        }

        // Try as Blueprint
        FString BPPath = ClassPath;
        if (!BPPath.EndsWith(TEXT("_C")))
        {
            BPPath += TEXT("_C");
        }
        
        UClass* BPClass = LoadClass<UObject>(nullptr, *BPPath);
        if (BPClass)
        {
            return BPClass;
        }

        // Try loading Blueprint asset and getting its generated class
        UBlueprint* BP = LoadBlueprintFromPath(ClassPath);
        if (BP && BP->GeneratedClass)
        {
            return BP->GeneratedClass;
        }

        return nullptr;
    }

    // Helper to add a Blueprint variable with proper category
    bool AddBlueprintVariable(UBlueprint* Blueprint, const FString& VarName, const FEdGraphPinType& PinType, const FString& Category = TEXT(""))
    {
        if (!Blueprint) return false;
        
        bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
        
        if (bSuccess && !Category.IsEmpty())
        {
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*VarName), nullptr, FText::FromString(Category));
        }
        
        return bSuccess;
    }

    // Helper to set Blueprint variable default value
    void SetVariableDefaultValue(UBlueprint* Blueprint, const FString& VarName, const FString& DefaultValue)
    {
        if (!Blueprint) return;
        SetBPVarDefaultValueGF(Blueprint, FName(*VarName), DefaultValue);
    }

    // Pin type factory helpers
    FEdGraphPinType MakeIntPinType()
    {
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        return PinType;
    }

    FEdGraphPinType MakeFloatPinType()
    {
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        return PinType;
    }

    FEdGraphPinType MakeBoolPinType()
    {
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        return PinType;
    }

    FEdGraphPinType MakeNamePinType()
    {
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        return PinType;
    }

    FEdGraphPinType MakeStringPinType()
    {
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
        return PinType;
    }
#endif
}

// ============================================================================
// Game Framework Member Handlers
// ============================================================================
// Dispatch lives in the manage_networking FMcpCall classes
// (Private/MCP/Calls/McpCalls_ManageNetworking.cpp); each
// HandleGameFramework* member here owns one advertised action's body,
// extracted verbatim from the retired dispatcher chain, replicating its
// editor-build stub and the prologue reads the body uses.

// ========================================================================
// 21.1 CORE CLASSES (6 actions)
// ========================================================================

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkCreateGameMode(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    FString Name = GetStringField(Payload, TEXT("name"));
    FString Path = GetStringField(Payload, TEXT("path"), TEXT("/Game"));
    bool bSave = GetBoolField(Payload, TEXT("save"), false);
    
    // SECURITY: Validate path to prevent traversal attacks
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty() && !Path.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, 
            TEXT("Invalid path: path traversal or invalid characters detected. Path must start with /Game/, /Engine/, or /Script/"), 
            TEXT("SECURITY_VIOLATION"));
        return true;
    }
    if (!SanitizedPath.IsEmpty())
    {
        Path = SanitizedPath;
    }

    if (Name.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'name' for create_game_mode."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
    UClass* ParentClass = ParentClassPath.IsEmpty() ? AGameModeBase::StaticClass() : LoadClassFromPath(ParentClassPath);
    
    if (!ParentClass)
    {
        ParentClass = AGameModeBase::StaticClass();
    }

    FString Error;
    UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
    
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    // Set initial class defaults if provided
    FString DefaultPawnClass = GetStringField(Payload, TEXT("defaultPawnClass"));
    if (!DefaultPawnClass.IsEmpty())
    {
        UClass* PawnClass = LoadClassFromPath(DefaultPawnClass);
        if (PawnClass)
        {
            SetClassProperty(BP, TEXT("DefaultPawnClass"), PawnClass, Error);
        }
    }

    FString PlayerControllerClass = GetStringField(Payload, TEXT("playerControllerClass"));
    if (!PlayerControllerClass.IsEmpty())
    {
        UClass* PCClass = LoadClassFromPath(PlayerControllerClass);
        if (PCClass)
        {
            SetClassProperty(BP, TEXT("PlayerControllerClass"), PCClass, Error);
        }
    }

    FString GameStateClass = GetStringField(Payload, TEXT("gameStateClass"));
    if (!GameStateClass.IsEmpty())
    {
        UClass* GSClass = LoadClassFromPath(GameStateClass);
        if (GSClass)
        {
            SetClassProperty(BP, TEXT("GameStateClass"), GSClass, Error);
        }
    }

    FString PlayerStateClass = GetStringField(Payload, TEXT("playerStateClass"));
    if (!PlayerStateClass.IsEmpty())
    {
        UClass* PSClass = LoadClassFromPath(PlayerStateClass);
        if (PSClass)
        {
            SetClassProperty(BP, TEXT("PlayerStateClass"), PSClass, Error);
        }
    }

    FString HUDClass = GetStringField(Payload, TEXT("hudClass"));
    if (!HUDClass.IsEmpty())
    {
        UClass* HUDClassObj = LoadClassFromPath(HUDClass);
        if (HUDClassObj)
        {
            SetClassProperty(BP, TEXT("HUDClass"), HUDClassObj, Error);
        }
    }

    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created GameMode blueprint: %s"), *Name));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    McpHandlerUtils::AddVerification(Response, BP);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkCreateGameState(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    FString Name = GetStringField(Payload, TEXT("name"));
    FString Path = GetStringField(Payload, TEXT("path"), TEXT("/Game"));
    bool bSave = GetBoolField(Payload, TEXT("save"), false);
    
    // SECURITY: Validate path to prevent traversal attacks
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty() && !Path.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, 
            TEXT("Invalid path: path traversal or invalid characters detected. Path must start with /Game/, /Engine/, or /Script/"), 
            TEXT("SECURITY_VIOLATION"));
        return true;
    }
    if (!SanitizedPath.IsEmpty())
    {
        Path = SanitizedPath;
    }

    if (Name.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'name' for create_game_state."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
    UClass* ParentClass = ParentClassPath.IsEmpty() ? AGameStateBase::StaticClass() : LoadClassFromPath(ParentClassPath);
    
    if (!ParentClass)
    {
        ParentClass = AGameStateBase::StaticClass();
    }

    FString Error;
    UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
    
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created GameState blueprint: %s"), *Name));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    McpHandlerUtils::AddVerification(Response, BP);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkCreatePlayerController(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    FString Name = GetStringField(Payload, TEXT("name"));
    FString Path = GetStringField(Payload, TEXT("path"), TEXT("/Game"));
    bool bSave = GetBoolField(Payload, TEXT("save"), false);
    
    // SECURITY: Validate path to prevent traversal attacks
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty() && !Path.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, 
            TEXT("Invalid path: path traversal or invalid characters detected. Path must start with /Game/, /Engine/, or /Script/"), 
            TEXT("SECURITY_VIOLATION"));
        return true;
    }
    if (!SanitizedPath.IsEmpty())
    {
        Path = SanitizedPath;
    }

    if (Name.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'name' for create_player_controller."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
    UClass* ParentClass = ParentClassPath.IsEmpty() ? APlayerController::StaticClass() : LoadClassFromPath(ParentClassPath);
    
    if (!ParentClass)
    {
        ParentClass = APlayerController::StaticClass();
    }

    FString Error;
    UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
    
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created PlayerController blueprint: %s"), *Name));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    McpHandlerUtils::AddVerification(Response, BP);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkCreatePlayerState(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    FString Name = GetStringField(Payload, TEXT("name"));
    FString Path = GetStringField(Payload, TEXT("path"), TEXT("/Game"));
    bool bSave = GetBoolField(Payload, TEXT("save"), false);
    
    // SECURITY: Validate path to prevent traversal attacks
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty() && !Path.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, 
            TEXT("Invalid path: path traversal or invalid characters detected. Path must start with /Game/, /Engine/, or /Script/"), 
            TEXT("SECURITY_VIOLATION"));
        return true;
    }
    if (!SanitizedPath.IsEmpty())
    {
        Path = SanitizedPath;
    }

    if (Name.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'name' for create_player_state."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
    UClass* ParentClass = ParentClassPath.IsEmpty() ? APlayerState::StaticClass() : LoadClassFromPath(ParentClassPath);
    
    if (!ParentClass)
    {
        ParentClass = APlayerState::StaticClass();
    }

    FString Error;
    UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
    
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created PlayerState blueprint: %s"), *Name));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    McpHandlerUtils::AddVerification(Response, BP);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkCreateGameInstance(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    FString Name = GetStringField(Payload, TEXT("name"));
    FString Path = GetStringField(Payload, TEXT("path"), TEXT("/Game"));
    bool bSave = GetBoolField(Payload, TEXT("save"), false);
    
    // SECURITY: Validate path to prevent traversal attacks
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty() && !Path.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, 
            TEXT("Invalid path: path traversal or invalid characters detected. Path must start with /Game/, /Engine/, or /Script/"), 
            TEXT("SECURITY_VIOLATION"));
        return true;
    }
    if (!SanitizedPath.IsEmpty())
    {
        Path = SanitizedPath;
    }

    if (Name.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'name' for create_game_instance."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
    UClass* ParentClass = ParentClassPath.IsEmpty() ? UGameInstance::StaticClass() : LoadClassFromPath(ParentClassPath);
    
    if (!ParentClass)
    {
        ParentClass = UGameInstance::StaticClass();
    }

    FString Error;
    UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
    
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created GameInstance blueprint: %s"), *Name));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    McpHandlerUtils::AddVerification(Response, BP);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkCreateHudClass(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    FString Name = GetStringField(Payload, TEXT("name"));
    FString Path = GetStringField(Payload, TEXT("path"), TEXT("/Game"));
    bool bSave = GetBoolField(Payload, TEXT("save"), false);
    
    // SECURITY: Validate path to prevent traversal attacks
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty() && !Path.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, 
            TEXT("Invalid path: path traversal or invalid characters detected. Path must start with /Game/, /Engine/, or /Script/"), 
            TEXT("SECURITY_VIOLATION"));
        return true;
    }
    if (!SanitizedPath.IsEmpty())
    {
        Path = SanitizedPath;
    }

    if (Name.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'name' for create_hud_class."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ParentClassPath = GetStringField(Payload, TEXT("parentClass"));
    UClass* ParentClass = ParentClassPath.IsEmpty() ? AHUD::StaticClass() : LoadClassFromPath(ParentClassPath);
    
    if (!ParentClass)
    {
        ParentClass = AHUD::StaticClass();
    }

    FString Error;
    UBlueprint* BP = CreateGameFrameworkBlueprint(Path, Name, ParentClass, Error);
    
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created HUD blueprint: %s"), *Name));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    McpHandlerUtils::AddVerification(Response, BP);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

// ========================================================================
// 21.2 GAME MODE CONFIGURATION (5 actions)
// ========================================================================

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkSetDefaultPawnClass(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    bool bSave = GetBoolField(Payload, TEXT("save"), false);

    // Support both gameModeBlueprint and blueprintPath as aliases
    FString GameModeBlueprint = GetStringField(Payload, TEXT("gameModeBlueprint"));
    if (GameModeBlueprint.IsEmpty())
    {
        GameModeBlueprint = GetStringField(Payload, TEXT("blueprintPath"));
    }
    
    // SECURITY: Validate blueprint paths
    if (!GameModeBlueprint.IsEmpty())
    {
        FString SanitizedBPPath = SanitizeProjectRelativePath(GameModeBlueprint);
        if (SanitizedBPPath.IsEmpty())
        {
            SendAutomationError(Socket, RequestId, 
                TEXT("Invalid gameModeBlueprint path: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        GameModeBlueprint = SanitizedBPPath;
    }

    if (GameModeBlueprint.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Support both pawnClass and defaultPawnClass as aliases
    FString PawnClassPath = GetStringField(Payload, TEXT("pawnClass"));
    if (PawnClassPath.IsEmpty())
    {
        PawnClassPath = GetStringField(Payload, TEXT("defaultPawnClass"));
    }
    if (PawnClassPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'pawnClass' or 'defaultPawnClass'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
        return true;
    }

    UClass* PawnClass = LoadClassFromPath(PawnClassPath);
    if (!PawnClass)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to load pawn class: %s"), *PawnClassPath), TEXT("NOT_FOUND"));
        return true;
    }

    FString Error;
    if (!SetClassProperty(BP, TEXT("DefaultPawnClass"), PawnClass, Error))
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("SET_PROPERTY_FAILED"));
        return true;
    }

    McpSafeCompileBlueprint(BP);
    
    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set DefaultPawnClass to %s"), *PawnClassPath));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkSetPlayerControllerClass(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    bool bSave = GetBoolField(Payload, TEXT("save"), false);

    // Support both gameModeBlueprint and blueprintPath as aliases
    FString GameModeBlueprint = GetStringField(Payload, TEXT("gameModeBlueprint"));
    if (GameModeBlueprint.IsEmpty())
    {
        GameModeBlueprint = GetStringField(Payload, TEXT("blueprintPath"));
    }
    
    // SECURITY: Validate blueprint paths
    if (!GameModeBlueprint.IsEmpty())
    {
        FString SanitizedBPPath = SanitizeProjectRelativePath(GameModeBlueprint);
        if (SanitizedBPPath.IsEmpty())
        {
            SendAutomationError(Socket, RequestId, 
                TEXT("Invalid gameModeBlueprint path: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        GameModeBlueprint = SanitizedBPPath;
    }

    if (GameModeBlueprint.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString PCClassPath = GetStringField(Payload, TEXT("playerControllerClass"));
    if (PCClassPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'playerControllerClass'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
        return true;
    }

    UClass* PCClass = LoadClassFromPath(PCClassPath);
    if (!PCClass)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to load PlayerController class: %s"), *PCClassPath), TEXT("NOT_FOUND"));
        return true;
    }

    FString Error;
    if (!SetClassProperty(BP, TEXT("PlayerControllerClass"), PCClass, Error))
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("SET_PROPERTY_FAILED"));
        return true;
    }

    McpSafeCompileBlueprint(BP);

    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set PlayerControllerClass to %s"), *PCClassPath));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkSetGameStateClass(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    bool bSave = GetBoolField(Payload, TEXT("save"), false);

    // Support both gameModeBlueprint and blueprintPath as aliases
    FString GameModeBlueprint = GetStringField(Payload, TEXT("gameModeBlueprint"));
    if (GameModeBlueprint.IsEmpty())
    {
        GameModeBlueprint = GetStringField(Payload, TEXT("blueprintPath"));
    }
    
    // SECURITY: Validate blueprint paths
    if (!GameModeBlueprint.IsEmpty())
    {
        FString SanitizedBPPath = SanitizeProjectRelativePath(GameModeBlueprint);
        if (SanitizedBPPath.IsEmpty())
        {
            SendAutomationError(Socket, RequestId, 
                TEXT("Invalid gameModeBlueprint path: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        GameModeBlueprint = SanitizedBPPath;
    }

    if (GameModeBlueprint.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString GSClassPath = GetStringField(Payload, TEXT("gameStateClass"));
    if (GSClassPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'gameStateClass'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
        return true;
    }

    UClass* GSClass = LoadClassFromPath(GSClassPath);
    if (!GSClass)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to load GameState class: %s"), *GSClassPath), TEXT("NOT_FOUND"));
        return true;
    }

    FString Error;
    if (!SetClassProperty(BP, TEXT("GameStateClass"), GSClass, Error))
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("SET_PROPERTY_FAILED"));
        return true;
    }

    McpSafeCompileBlueprint(BP);

    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set GameStateClass to %s"), *GSClassPath));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkSetPlayerStateClass(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    bool bSave = GetBoolField(Payload, TEXT("save"), false);

    // Support both gameModeBlueprint and blueprintPath as aliases
    FString GameModeBlueprint = GetStringField(Payload, TEXT("gameModeBlueprint"));
    if (GameModeBlueprint.IsEmpty())
    {
        GameModeBlueprint = GetStringField(Payload, TEXT("blueprintPath"));
    }
    
    // SECURITY: Validate blueprint paths
    if (!GameModeBlueprint.IsEmpty())
    {
        FString SanitizedBPPath = SanitizeProjectRelativePath(GameModeBlueprint);
        if (SanitizedBPPath.IsEmpty())
        {
            SendAutomationError(Socket, RequestId, 
                TEXT("Invalid gameModeBlueprint path: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        GameModeBlueprint = SanitizedBPPath;
    }

    if (GameModeBlueprint.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString PSClassPath = GetStringField(Payload, TEXT("playerStateClass"));
    if (PSClassPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'playerStateClass'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
        return true;
    }

    UClass* PSClass = LoadClassFromPath(PSClassPath);
    if (!PSClass)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to load PlayerState class: %s"), *PSClassPath), TEXT("NOT_FOUND"));
        return true;
    }

    FString Error;
    if (!SetClassProperty(BP, TEXT("PlayerStateClass"), PSClass, Error))
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("SET_PROPERTY_FAILED"));
        return true;
    }

    McpSafeCompileBlueprint(BP);

    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set PlayerStateClass to %s"), *PSClassPath));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkConfigureGameRules(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    bool bSave = GetBoolField(Payload, TEXT("save"), false);

    // Support both gameModeBlueprint and blueprintPath as aliases
    FString GameModeBlueprint = GetStringField(Payload, TEXT("gameModeBlueprint"));
    if (GameModeBlueprint.IsEmpty())
    {
        GameModeBlueprint = GetStringField(Payload, TEXT("blueprintPath"));
    }
    
    // SECURITY: Validate blueprint paths
    if (!GameModeBlueprint.IsEmpty())
    {
        FString SanitizedBPPath = SanitizeProjectRelativePath(GameModeBlueprint);
        if (SanitizedBPPath.IsEmpty())
        {
            SendAutomationError(Socket, RequestId, 
                TEXT("Invalid gameModeBlueprint path: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        GameModeBlueprint = SanitizedBPPath;
    }

    if (GameModeBlueprint.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
    if (!BP || !BP->GeneratedClass)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
        return true;
    }

    UObject* CDO = BP->GeneratedClass->GetDefaultObject();
    if (!CDO)
    {
        SendAutomationError(Socket, RequestId, TEXT("Failed to get CDO."), TEXT("INTERNAL_ERROR"));
        return true;
    }

    McpHandlerUtils::FMcpWriteReport Report;

    if (Payload->HasField(TEXT("bDelayedStart")))
    {
        FBoolProperty* Prop = CastField<FBoolProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("bDelayedStart")));
        if (Prop)
        {
            Prop->SetPropertyValue_InContainer(CDO, GetBoolField(Payload, TEXT("bDelayedStart")));
            Report.MarkApplied(TEXT("bDelayedStart"));
        }
        else
        {
            Report.MarkFailed(TEXT("bDelayedStart"), TEXT("property not found on GameMode class (exists on AGameMode, not AGameModeBase)"));
        }
    }

    if (Payload->HasField(TEXT("startPlayersNeeded")))
    {
        Report.MarkFailed(TEXT("startPlayersNeeded"), TEXT("not a native GameMode property and not implemented as a generated Blueprint variable"));
    }

    if (Report.AnyApplied())
    {
        CDO->MarkPackageDirty();
        McpSafeCompileBlueprint(BP);
        if (bSave)
        {
            McpSafeAssetSave(BP);
        }
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    return SendWriteReportResponse(this, Socket, RequestId, Report, Response,
        TEXT("Configured game rules"), nullptr);
#endif // WITH_EDITOR
}

// ========================================================================
// 21.3 MATCH FLOW (5 actions)
// ========================================================================






// ========================================================================
// 21.4 PLAYER MANAGEMENT (3 actions)
// ========================================================================

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkConfigurePlayerStart(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    bool bSave = GetBoolField(Payload, TEXT("save"), false);

    // Support both gameModeBlueprint and blueprintPath as aliases
    FString GameModeBlueprint = GetStringField(Payload, TEXT("gameModeBlueprint"));
    if (GameModeBlueprint.IsEmpty())
    {
        GameModeBlueprint = GetStringField(Payload, TEXT("blueprintPath"));
    }
    
    // SECURITY: Validate blueprint paths
    if (!GameModeBlueprint.IsEmpty())
    {
        FString SanitizedBPPath = SanitizeProjectRelativePath(GameModeBlueprint);
        if (SanitizedBPPath.IsEmpty())
        {
            SendAutomationError(Socket, RequestId, 
                TEXT("Invalid gameModeBlueprint path: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        GameModeBlueprint = SanitizedBPPath;
    }
    FString BlueprintPath = GameModeBlueprint;

    if (BlueprintPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'blueprintPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // This typically works on PlayerStart actors in a level, not blueprints
    // For now, we'll handle it as a configuration helper
    
    TSharedPtr<FJsonObject> LocationObj = GetObjectField(Payload, TEXT("location"));
    TSharedPtr<FJsonObject> RotationObj = GetObjectField(Payload, TEXT("rotation"));
    int32 TeamIndex = static_cast<int32>(GetNumberField(Payload, TEXT("teamIndex"), 0));
    bool bPlayerOnly = GetBoolField(Payload, TEXT("bPlayerOnly"), false);

    UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configure PlayerStart: path=%s, teamIndex=%d, playerOnly=%d"), 
           *BlueprintPath, TeamIndex, bPlayerOnly);

    // Get the PlayerStart actor name to configure
    FString PlayerStartName = GetStringField(Payload, TEXT("playerStartName"));
    if (PlayerStartName.IsEmpty())
    {
        PlayerStartName = GetStringField(Payload, TEXT("actorName"));
    }

    FString PlayerStartTag = GetStringField(Payload, TEXT("playerStartTag"));
    
    // Build the tag if not explicitly provided
    if (PlayerStartTag.IsEmpty() && TeamIndex > 0)
    {
        PlayerStartTag = FString::Printf(TEXT("Team%d"), TeamIndex);
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        SendAutomationError(Socket, RequestId, TEXT("No world available."), TEXT("NO_WORLD"));
        return true;
    }

    int32 ConfiguredCount = 0;
    TSet<ULevel*> ModifiedLevels;

    // Find and configure PlayerStart actors
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        APlayerStart* PlayerStart = *It;
        if (!PlayerStart) continue;

        // If a specific name is provided, only configure that one
        if (!PlayerStartName.IsEmpty())
        {
            FString ActorLabel = PlayerStart->GetActorLabel();
            FString ActorName = PlayerStart->GetName();
            if (!ActorLabel.Equals(PlayerStartName, ESearchCase::IgnoreCase) &&
                !ActorName.Equals(PlayerStartName, ESearchCase::IgnoreCase))
            {
                continue;
            }
        }

        // Set PlayerStartTag for team assignment
        if (!PlayerStartTag.IsEmpty())
        {
            PlayerStart->PlayerStartTag = FName(*PlayerStartTag);
        }

        PlayerStart->MarkPackageDirty();
        if (ULevel* OwningLevel = PlayerStart->GetLevel())
        {
            ModifiedLevels.Add(OwningLevel);
        }
        ConfiguredCount++;

        UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configured PlayerStart: %s with tag=%s"),
               *PlayerStart->GetName(), *PlayerStartTag);
    }

    if (ConfiguredCount == 0)
    {
        const FString Reason = PlayerStartName.IsEmpty()
            ? TEXT("No PlayerStart actors found in the current level.")
            : FString::Printf(TEXT("PlayerStart '%s' not found in level."), *PlayerStartName);
        SendAutomationError(Socket, RequestId, Reason, TEXT("NOT_FOUND"));
        return true;
    }

    // Persist the owning level(s) when requested; report any failure honestly.
    int32 SavedLevelCount = 0;
    if (bSave)
    {
        for (ULevel* ModifiedLevel : ModifiedLevels)
        {
            if (!ModifiedLevel) continue;
            const FString LevelPackagePath = ModifiedLevel->GetOutermost()->GetName();
            if (McpSafeLevelSave(ModifiedLevel, LevelPackagePath))
            {
                SavedLevelCount++;
            }
            else
            {
                SendAutomationError(Socket, RequestId,
                    FString::Printf(TEXT("Configured %d PlayerStart actor(s) but failed to save level: %s"),
                        ConfiguredCount, *LevelPackagePath),
                    TEXT("SAVE_FAILED"));
                return true;
            }
        }
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured %d PlayerStart actor(s)"), ConfiguredCount));
    Response->SetNumberField(TEXT("configuredCount"), ConfiguredCount);
    if (!PlayerStartTag.IsEmpty())
    {
        Response->SetStringField(TEXT("playerStartTag"), PlayerStartTag);
    }
    Response->SetNumberField(TEXT("teamIndex"), TeamIndex);
    Response->SetBoolField(TEXT("saved"), bSave && SavedLevelCount > 0);
    Response->SetNumberField(TEXT("savedLevelCount"), SavedLevelCount);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkSetRespawnRules(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    bool bSave = GetBoolField(Payload, TEXT("save"), false);

    // Support both gameModeBlueprint and blueprintPath as aliases
    FString GameModeBlueprint = GetStringField(Payload, TEXT("gameModeBlueprint"));
    if (GameModeBlueprint.IsEmpty())
    {
        GameModeBlueprint = GetStringField(Payload, TEXT("blueprintPath"));
    }
    
    // SECURITY: Validate blueprint paths
    if (!GameModeBlueprint.IsEmpty())
    {
        FString SanitizedBPPath = SanitizeProjectRelativePath(GameModeBlueprint);
        if (SanitizedBPPath.IsEmpty())
        {
            SendAutomationError(Socket, RequestId, 
                TEXT("Invalid gameModeBlueprint path: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        GameModeBlueprint = SanitizedBPPath;
    }

    if (GameModeBlueprint.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
        return true;
    }

    double RespawnDelay = GetNumberField(Payload, TEXT("respawnDelay"), 5.0);
    FString RespawnLocation = GetStringField(Payload, TEXT("respawnLocation"), TEXT("PlayerStart"));

    bool bForceRespawn = GetBoolField(Payload, TEXT("forceRespawn"), true);
    int32 RespawnLives = static_cast<int32>(GetNumberField(Payload, TEXT("respawnLives"), -1));

    UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Setting respawn rules: delay=%.1f, location=%s, force=%d, lives=%d"), 
           RespawnDelay, *RespawnLocation, bForceRespawn, RespawnLives);

    bool bModified = false;

    // Set MinRespawnDelay on the GameMode CDO
    // Note: MinRespawnDelay is in AGameMode (not AGameModeBase)
    if (BP->GeneratedClass)
    {
        // Cast to AGameMode, not AGameModeBase
        AGameMode* GameModeCDO = Cast<AGameMode>(BP->GeneratedClass->GetDefaultObject());
        if (GameModeCDO)
        {
            GameModeCDO->MinRespawnDelay = static_cast<float>(RespawnDelay);
            GameModeCDO->MarkPackageDirty();
            bModified = true;
            
            UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Set MinRespawnDelay=%.1f on CDO"), RespawnDelay);
        }
        else
        {
            UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Blueprint is not derived from AGameMode. MinRespawnDelay not set."));
        }
    }

    // Add respawn-related Blueprint variables
    int32 VarsAdded = 0;

    // RespawnLocation (Name) - where players respawn
    if (GameFrameworkHelpers::AddBlueprintVariable(BP, TEXT("RespawnLocation"), GameFrameworkHelpers::MakeNamePinType(), TEXT("Respawn Rules")))
    {
        GameFrameworkHelpers::SetVariableDefaultValue(BP, TEXT("RespawnLocation"), RespawnLocation);
        VarsAdded++;
    }

    // bForceRespawn (bool) - whether respawn is forced or optional
    if (GameFrameworkHelpers::AddBlueprintVariable(BP, TEXT("bForceRespawn"), GameFrameworkHelpers::MakeBoolPinType(), TEXT("Respawn Rules")))
    {
        GameFrameworkHelpers::SetVariableDefaultValue(BP, TEXT("bForceRespawn"), bForceRespawn ? TEXT("true") : TEXT("false"));
        VarsAdded++;
    }

    // RespawnLives (int) - number of lives (-1 for unlimited)
    if (GameFrameworkHelpers::AddBlueprintVariable(BP, TEXT("RespawnLives"), GameFrameworkHelpers::MakeIntPinType(), TEXT("Respawn Rules")))
    {
        GameFrameworkHelpers::SetVariableDefaultValue(BP, TEXT("RespawnLives"), FString::FromInt(RespawnLives));
        VarsAdded++;
    }

    McpSafeCompileBlueprint(BP);
    BP->MarkPackageDirty();

    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set respawn rules (MinRespawnDelay=%.1f, added %d variables)"), RespawnDelay, VarsAdded));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    Response->SetNumberField(TEXT("variablesAdded"), VarsAdded);
    
    TSharedPtr<FJsonObject> ConfigObj = McpHandlerUtils::CreateResultObject();
    ConfigObj->SetNumberField(TEXT("respawnDelay"), RespawnDelay);
    ConfigObj->SetStringField(TEXT("respawnLocation"), RespawnLocation);
    ConfigObj->SetBoolField(TEXT("forceRespawn"), bForceRespawn);
    ConfigObj->SetNumberField(TEXT("respawnLives"), RespawnLives);
    Response->SetObjectField(TEXT("configuration"), ConfigObj);
    
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkConfigureSpectating(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    bool bSave = GetBoolField(Payload, TEXT("save"), false);

    // Support both gameModeBlueprint and blueprintPath as aliases
    FString GameModeBlueprint = GetStringField(Payload, TEXT("gameModeBlueprint"));
    if (GameModeBlueprint.IsEmpty())
    {
        GameModeBlueprint = GetStringField(Payload, TEXT("blueprintPath"));
    }
    
    // SECURITY: Validate blueprint paths
    if (!GameModeBlueprint.IsEmpty())
    {
        FString SanitizedBPPath = SanitizeProjectRelativePath(GameModeBlueprint);
        if (SanitizedBPPath.IsEmpty())
        {
            SendAutomationError(Socket, RequestId, 
                TEXT("Invalid gameModeBlueprint path: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        GameModeBlueprint = SanitizedBPPath;
    }

    if (GameModeBlueprint.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'gameModeBlueprint'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
    if (!BP)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to load GameMode: %s"), *GameModeBlueprint), TEXT("NOT_FOUND"));
        return true;
    }

    FString SpectatorClassPath = GetStringField(Payload, TEXT("spectatorClass"));
    bool bAllowSpectating = GetBoolField(Payload, TEXT("allowSpectating"), true);
    FString ViewMode = GetStringField(Payload, TEXT("spectatorViewMode"), TEXT("FreeCam"));

    // Set spectator class if provided
    if (!SpectatorClassPath.IsEmpty())
    {
        UClass* SpectatorClass = LoadClassFromPath(SpectatorClassPath);
        if (SpectatorClass)
        {
            FString Error;
            SetClassProperty(BP, TEXT("SpectatorClass"), SpectatorClass, Error);
        }
    }

    McpSafeCompileBlueprint(BP);
    BP->MarkPackageDirty();

    if (bSave)
    {
        McpSafeAssetSave(BP);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), TEXT("Spectating configured."));
    Response->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}

// ========================================================================
// UTILITY (1 action)
// ========================================================================

bool UMcpAutomationBridgeSubsystem::HandleGameFrameworkGetGameFrameworkInfo(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    using namespace GameFrameworkHelpers;

    // Support both gameModeBlueprint and blueprintPath as aliases
    FString GameModeBlueprint = GetStringField(Payload, TEXT("gameModeBlueprint"));
    if (GameModeBlueprint.IsEmpty())
    {
        GameModeBlueprint = GetStringField(Payload, TEXT("blueprintPath"));
    }
    
    // SECURITY: Validate blueprint paths
    if (!GameModeBlueprint.IsEmpty())
    {
        FString SanitizedBPPath = SanitizeProjectRelativePath(GameModeBlueprint);
        if (SanitizedBPPath.IsEmpty())
        {
            SendAutomationError(Socket, RequestId, 
                TEXT("Invalid gameModeBlueprint path: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        GameModeBlueprint = SanitizedBPPath;
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> InfoObj = McpHandlerUtils::CreateResultObject();

    // If a specific GameMode blueprint is provided, query it
    if (!GameModeBlueprint.IsEmpty())
    {
        UBlueprint* BP = LoadBlueprintFromPath(GameModeBlueprint);
        if (BP && BP->GeneratedClass)
        {
            UObject* CDO = BP->GeneratedClass->GetDefaultObject();
            if (CDO)
            {
                // Try to get class properties
                FClassProperty* PawnProp = CastField<FClassProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("DefaultPawnClass")));
                if (PawnProp)
                {
                    UClass* PawnClass = Cast<UClass>(PawnProp->GetPropertyValue_InContainer(CDO));
                    if (PawnClass)
                    {
                        InfoObj->SetStringField(TEXT("defaultPawnClass"), PawnClass->GetPathName());
                    }
                }

                FClassProperty* PCProp = CastField<FClassProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("PlayerControllerClass")));
                if (PCProp)
                {
                    UClass* PCClass = Cast<UClass>(PCProp->GetPropertyValue_InContainer(CDO));
                    if (PCClass)
                    {
                        InfoObj->SetStringField(TEXT("playerControllerClass"), PCClass->GetPathName());
                    }
                }

                FClassProperty* GSProp = CastField<FClassProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("GameStateClass")));
                if (GSProp)
                {
                    UClass* GSClass = Cast<UClass>(GSProp->GetPropertyValue_InContainer(CDO));
                    if (GSClass)
                    {
                        InfoObj->SetStringField(TEXT("gameStateClass"), GSClass->GetPathName());
                    }
                }

                FClassProperty* PSProp = CastField<FClassProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("PlayerStateClass")));
                if (PSProp)
                {
                    UClass* PSClass = Cast<UClass>(PSProp->GetPropertyValue_InContainer(CDO));
                    if (PSClass)
                    {
                        InfoObj->SetStringField(TEXT("playerStateClass"), PSClass->GetPathName());
                    }
                }

                FClassProperty* HUDProp = CastField<FClassProperty>(BP->GeneratedClass->FindPropertyByName(TEXT("HUDClass")));
                if (HUDProp)
                {
                    UClass* HUDClass = Cast<UClass>(HUDProp->GetPropertyValue_InContainer(CDO));
                    if (HUDClass)
                    {
                        InfoObj->SetStringField(TEXT("hudClass"), HUDClass->GetPathName());
                    }
                }
            }

            InfoObj->SetStringField(TEXT("gameModeClass"), BP->GeneratedClass->GetPathName());
        }
    }
    else
    {
        // Query current world's game mode if available
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (World)
        {
            AGameModeBase* GM = World->GetAuthGameMode();
            if (GM)
            {
                InfoObj->SetStringField(TEXT("gameModeClass"), GM->GetClass()->GetPathName());
                
                if (GM->DefaultPawnClass)
                {
                    InfoObj->SetStringField(TEXT("defaultPawnClass"), GM->DefaultPawnClass->GetPathName());
                }
                if (GM->PlayerControllerClass)
                {
                    InfoObj->SetStringField(TEXT("playerControllerClass"), GM->PlayerControllerClass->GetPathName());
                }
                if (GM->GameStateClass)
                {
                    InfoObj->SetStringField(TEXT("gameStateClass"), GM->GameStateClass->GetPathName());
                }
                if (GM->PlayerStateClass)
                {
                    InfoObj->SetStringField(TEXT("playerStateClass"), GM->PlayerStateClass->GetPathName());
                }
                if (GM->HUDClass)
                {
                    InfoObj->SetStringField(TEXT("hudClass"), GM->HUDClass->GetPathName());
                }
            }
        }
    }

    Response->SetObjectField(TEXT("gameFrameworkInfo"), InfoObj);
    Response->SetStringField(TEXT("message"), TEXT("Game framework info retrieved."));
    SendAutomationResponse(Socket, RequestId, true, TEXT("Success"), Response);
    return true;
#endif // WITH_EDITOR
}
