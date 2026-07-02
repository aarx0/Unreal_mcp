// =============================================================================
// McpAutomationBridge_CharacterHandlers.cpp
// =============================================================================
// Character & Movement System Handlers for MCP Automation Bridge
// Phase 14: Character creation, movement configuration, and advanced movement.
//
// HANDLERS IMPLEMENTED (19 sub-actions):
// --------------------------------------
// Section 1: Character Creation (14.1)
//   - create_character_blueprint      : Create ACharacter Blueprint with optional skeletal mesh
//   - configure_capsule_component     : Configure capsule radius and half-height
//   - configure_mesh_component        : Set skeletal mesh, anim BP, offset, rotation
//   - configure_camera_component      : Add/configure spring arm + camera via SCS
//
// Section 2: Movement Component (14.2)
//   - configure_movement_speeds       : Walk/run/crouch/swim/fly speed, accel, friction
//   - configure_jump                  : Jump apex height (cm), air control, gravity, hold time
//   - configure_rotation              : Orient to movement, controller rotation, rotation rate
//   - add_custom_movement_mode        : Add custom movement mode with state variables
//   - configure_nav_movement          : Nav agent radius/height, RVO avoidance
//
// Section 3: Advanced Movement (14.3)
//   - setup_mantling                  : Mantle height, reach, state variables
//   - setup_vaulting                  : Vault height, depth, state variables
//   - setup_climbing                  : Climb speed, climbable tag, surface normal
//   - setup_sliding                   : Slide speed, duration, cooldown timers
//   - setup_wall_running              : Wall run speed, duration, gravity scale
//   - setup_grappling                 : Grapple range, speed, target tag
//
// Section 4: Footstep System (14.4)
//   - setup_footstep_system           : Socket names, trace distance, tracking vars
//   - map_surface_to_sound            : Map a surface type to a footstep sound
//   - configure_footstep_fx           : Volume multiplier, particle scale
//
// Section 5: Utility
//   - get_character_info              : Retrieve character movement, capsule, camera info
//
// Section 6: Aliases & Convenience Sub-Actions
//   - setup_movement                  : Alias for configure_movement_speeds (subset)
//   - set_walk_speed                  : Direct walk speed setter
//   - set_jump_height                 : Jump apex height (cm) -> JumpZVelocity
//   - set_gravity_scale               : Direct gravity scale setter
//   - set_ground_friction             : Direct ground friction setter
//   - set_braking_deceleration        : Direct braking deceleration setter
//   - configure_crouch                : Crouch speed, half-height, canCrouch
//   - configure_sprint                : Sprint speed with state variable
//
// PAYLOAD/RESPONSE FORMATS:
// -------------------------
// create_character_blueprint:
//   Payload:  { "name": string, "path"?: string, "skeletalMeshPath"?: string }
//   Response: { "blueprintPath": string, "name": string, "parentClass": "Character" }
//
// configure_capsule_component:
//   Payload:  { "blueprintPath": string, "capsuleRadius"?: number, "capsuleHalfHeight"?: number }
//   Response: { "blueprintPath": string, "capsuleRadius": number, "capsuleHalfHeight": number }
//
// configure_mesh_component:
//   Payload:  { "blueprintPath": string, "skeletalMeshPath"?: string,
//               "animBlueprintPath"?: string, "meshOffset"?: {x,y,z}, "meshRotation"?: {pitch,yaw,roll} }
//   Response: { "blueprintPath": string, "skeletalMesh"?: string, "animBlueprint"?: string }
//
// configure_camera_component:
//   Payload:  { "blueprintPath": string, "springArmLength"?: number,
//               "cameraUsePawnControlRotation"?: bool, "springArmLagEnabled"?: bool,
//               "springArmLagSpeed"?: number }
//   Response: { "blueprintPath": string, "springArmLength": number,
//               "usePawnControlRotation": bool, "lagEnabled": bool }
//
// configure_movement_speeds:
//   Payload:  { "blueprintPath": string, "walkSpeed"?: number, "runSpeed"?: number,
//               "sprintSpeed"?: number, "crouchSpeed"?: number, "swimSpeed"?: number,
//               "flySpeed"?: number, "acceleration"?: number, "deceleration"?: number,
//               "groundFriction"?: number }
//   Response: { "blueprintPath": string, "walkSpeed": number, "runSpeed"?: number,
//               "runSpeedApplied"?: bool, "runSpeedTarget"?: string,
//               "sprintSpeed"?: number, "sprintSpeedTarget"?: string,
//               "variablesAdded"?: string[] }
//
// configure_jump:
//   Payload:  { "blueprintPath": string, "jumpHeight"?: number (apex height in cm,
//               converted to JumpZVelocity), "airControl"?: number,
//               "gravityScale"?: number, "fallingLateralFriction"?: number,
//               "maxJumpCount"?: number, "jumpHoldTime"?: number }
//   Response: { "blueprintPath": string, "requestedJumpHeight"?: number,
//               "appliedJumpZVelocity"?: number }
//
// configure_rotation:
//   Payload:  { "blueprintPath": string, "orientToMovement"?: bool,
//               "useControllerRotationYaw"?: bool, "useControllerRotationPitch"?: bool,
//               "useControllerRotationRoll"?: bool, "rotationRate"?: number }
//   Response: { "blueprintPath": string }
//
// add_custom_movement_mode:
//   Payload:  { "blueprintPath": string, "modeName"?: string, "modeId"?: number,
//               "customSpeed"?: number }
//   Response: { "blueprintPath": string, "modeName": string, "modeId": number,
//               "stateVariable": string, "speedVariable": string, "customSpeed": number }
//
// configure_nav_movement:
//   Payload:  { "blueprintPath": string, "navAgentRadius"?: number,
//               "navAgentHeight"?: number, "avoidanceEnabled"?: bool }
//   Response: { "blueprintPath": string }
//
// setup_mantling:
//   Payload:  { "blueprintPath": string, "mantleHeight"?: number,
//               "mantleReachDistance"?: number }
//   Response: { "blueprintPath": string, "mantleHeight": number,
//               "mantleReachDistance": number, "stateVariable": string, "targetVariable": string }
//
// setup_vaulting:
//   Payload:  { "blueprintPath": string, "vaultHeight"?: number,
//               "vaultDepth"?: number }
//   Response: { "blueprintPath": string, "vaultHeight": number, "vaultDepth": number,
//               "stateVariable": string }
//
// setup_climbing:
//   Payload:  { "blueprintPath": string, "climbSpeed"?: number,
//               "climbableTag"?: string }
//   Response: { "blueprintPath": string, "climbSpeed": number, "climbableTag": string,
//               "stateVariable": string }
//
// setup_sliding:
//   Payload:  { "blueprintPath": string, "slideSpeed"?: number, "slideDuration"?: number,
//               "slideCooldown"?: number }
//   Response: { "blueprintPath": string, "slideSpeed": number, "slideDuration": number,
//               "slideCooldown": number, "stateVariable": string }
//
// setup_wall_running:
//   Payload:  { "blueprintPath": string, "wallRunSpeed"?: number,
//               "wallRunDuration"?: number, "wallRunGravityScale"?: number }
//   Response: { "blueprintPath": string, "wallRunSpeed": number,
//               "wallRunDuration": number, "wallRunGravityScale": number,
//               "stateVariable": string }
//
// setup_grappling:
//   Payload:  { "blueprintPath": string, "grappleRange"?: number,
//               "grappleSpeed"?: number, "grappleTargetTag"?: string }
//   Response: { "blueprintPath": string, "grappleRange": number, "grappleSpeed": number,
//               "grappleTargetTag": string, "stateVariable": string }
//
// setup_footstep_system:
//   Payload:  { "blueprintPath": string, "footstepEnabled"?: bool,
//               "footstepSocketLeft"?: string, "footstepSocketRight"?: string,
//               "footstepTraceDistance"?: number }
//   Response: { "blueprintPath": string, "enabled": bool, "socketLeft": string,
//               "socketRight": string, "traceDistance": number }
//
// map_surface_to_sound:
//   Payload:  { "blueprintPath": string, "surfaceType": string, "soundPath": string }
//   Response: { "blueprintPath": string, "surfaceType": string, "soundPath": string,
//               "mapVariable": string, "mapSize": number }
//
// configure_footstep_fx:
//   Payload:  { "blueprintPath": string, "volumeMultiplier"?: number,
//               "particleScale"?: number }
//   Response: { "blueprintPath": string, "volumeMultiplier": number, "particleScale": number }
//
// get_character_info:
//   Payload:  { "blueprintPath": string }
//   Response: { "blueprintPath": string, "assetName": string, "capsuleRadius": number,
//               "capsuleHalfHeight": number, "walkSpeed": number, "jumpZVelocity": number,
//               "hasSpringArm": bool, "hasCamera": bool,
//               "springArmTemplates": object[], "cameraTemplates": object[],
//               "bFindCameraComponentWhenViewTarget"?: bool, "playerViewState": object,
//               "movementVariables"?: string[] }
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.7: Character movement component APIs stable across all versions.
// UE 5.1+:    Motion warping available.
// UE 5.3+:    Advanced locomotion system available.
// UE 5.7+:    SCS component templates must be created via SCS->CreateNode() + AddNode().
//             NO UPackage::SavePackage() — use McpSafeAssetSave() instead.
//             SetBlueprintVariableDefaultValue not available in UE 5.6/5.7.
//
// REFACTORING NOTES:
// ------------------
// - Security validation via IsValidAssetPath() for all blueprint paths.
// - Blueprint variable defaults use SetBPVarDefaultValue() with Blueprint metadata and CDO reflection.
// - McpSafeAssetSave() is used directly for UE 5.7+ compatibility.
// - AddBlueprintVariableChar in anonymous namespace to avoid Unity build collisions.
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

// =============================================================================
// Version Compatibility (MUST be first include)
// =============================================================================
#include "McpVersionCompatibility.h"
#include "McpHandlerUtils.h"

// =============================================================================
// Core Includes
// =============================================================================
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

// =============================================================================
// Editor-Only Includes
// =============================================================================
#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"

// =============================================================================
// Character & Movement Includes
// =============================================================================
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"

// =============================================================================
// Camera Includes
// =============================================================================
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

// =============================================================================
// Mesh & Animation Includes
// =============================================================================
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimBlueprint.h"

// =============================================================================
// Audio & Reflection Includes
// =============================================================================
#include "Sound/SoundBase.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

// =============================================================================
// Blueprint Graph Includes
// =============================================================================
#include "EdGraphSchema_K2.h"
#endif // WITH_EDITOR

// =============================================================================
// Logging Category
// =============================================================================
DEFINE_LOG_CATEGORY_STATIC(LogMcpCharacterHandlers, Log, All);

// =============================================================================
// JSON Helper Aliases
// =============================================================================
// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h

// =============================================================================
// Section 0: Helper Functions
// =============================================================================

#if WITH_EDITOR


static FString GetSceneComponentParentNameChar(const USceneComponent* Component)
{
    if (!Component)
    {
        return TEXT("");
    }

    const USceneComponent* Parent = Component->GetAttachParent();
    return Parent ? Parent->GetName() : TEXT("");
}

static TSharedPtr<FJsonObject> CreateCameraComponentReportChar(const UCameraComponent* Camera)
{
    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    if (!Camera)
    {
        return Report;
    }

    Report->SetStringField(TEXT("name"), Camera->GetName());
    Report->SetStringField(TEXT("class"), Camera->GetClass()->GetName());
    Report->SetStringField(TEXT("attachParent"), GetSceneComponentParentNameChar(Camera));
    Report->SetBoolField(TEXT("active"), Camera->IsActive());
    Report->SetBoolField(TEXT("visible"), Camera->IsVisible());
    Report->SetBoolField(TEXT("usePawnControlRotation"), Camera->bUsePawnControlRotation);
    Report->SetNumberField(TEXT("fieldOfView"), Camera->FieldOfView);
    Report->SetObjectField(TEXT("relativeLocation"), McpHandlerUtils::VectorToJson(Camera->GetRelativeLocation()));
    Report->SetObjectField(TEXT("relativeRotation"), McpHandlerUtils::RotatorToJson(Camera->GetRelativeRotation()));
    Report->SetObjectField(TEXT("worldLocation"), McpHandlerUtils::VectorToJson(Camera->GetComponentLocation()));
    Report->SetObjectField(TEXT("worldRotation"), McpHandlerUtils::RotatorToJson(Camera->GetComponentRotation()));
    return Report;
}

static TSharedPtr<FJsonObject> CreateSpringArmComponentReportChar(const USpringArmComponent* SpringArm)
{
    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    if (!SpringArm)
    {
        return Report;
    }

    Report->SetStringField(TEXT("name"), SpringArm->GetName());
    Report->SetStringField(TEXT("class"), SpringArm->GetClass()->GetName());
    Report->SetStringField(TEXT("attachParent"), GetSceneComponentParentNameChar(SpringArm));
    Report->SetBoolField(TEXT("active"), SpringArm->IsActive());
    Report->SetBoolField(TEXT("visible"), SpringArm->IsVisible());
    Report->SetNumberField(TEXT("targetArmLength"), SpringArm->TargetArmLength);
    Report->SetBoolField(TEXT("usePawnControlRotation"), SpringArm->bUsePawnControlRotation);
    Report->SetBoolField(TEXT("enableCameraLag"), SpringArm->bEnableCameraLag);
    Report->SetNumberField(TEXT("cameraLagSpeed"), SpringArm->CameraLagSpeed);
    Report->SetObjectField(TEXT("relativeLocation"), McpHandlerUtils::VectorToJson(SpringArm->GetRelativeLocation()));
    Report->SetObjectField(TEXT("relativeRotation"), McpHandlerUtils::RotatorToJson(SpringArm->GetRelativeRotation()));
    Report->SetObjectField(TEXT("worldLocation"), McpHandlerUtils::VectorToJson(SpringArm->GetComponentLocation()));
    Report->SetObjectField(TEXT("worldRotation"), McpHandlerUtils::RotatorToJson(SpringArm->GetComponentRotation()));
    return Report;
}

static void AddPlayerViewStateReportChar(UWorld* World, TSharedPtr<FJsonObject> Result)
{
    TSharedPtr<FJsonObject> ViewState = MakeShared<FJsonObject>();
    ViewState->SetBoolField(TEXT("isPIE"), World && World->WorldType == EWorldType::PIE);

    if (!World)
    {
        ViewState->SetStringField(TEXT("status"), TEXT("No active PIE world"));
        Result->SetObjectField(TEXT("playerViewState"), ViewState);
        return;
    }

    APlayerController* PlayerController = World->GetFirstPlayerController();
    if (!PlayerController)
    {
        ViewState->SetStringField(TEXT("status"), TEXT("No player controller"));
        Result->SetObjectField(TEXT("playerViewState"), ViewState);
        return;
    }

    ViewState->SetStringField(TEXT("playerController"), PlayerController->GetName());
    ViewState->SetStringField(TEXT("playerControllerClass"), PlayerController->GetClass()->GetName());

    APawn* Pawn = PlayerController->GetPawn();
    if (Pawn)
    {
        ViewState->SetStringField(TEXT("pawn"), Pawn->GetName());
        ViewState->SetStringField(TEXT("pawnClass"), Pawn->GetClass()->GetName());
        ViewState->SetBoolField(TEXT("bFindCameraComponentWhenViewTarget"), Pawn->bFindCameraComponentWhenViewTarget);
    }

    AActor* ViewTarget = PlayerController->GetViewTarget();
    if (ViewTarget)
    {
        ViewState->SetStringField(TEXT("viewTarget"), ViewTarget->GetName());
        ViewState->SetStringField(TEXT("viewTargetClass"), ViewTarget->GetClass()->GetName());
        ViewState->SetBoolField(TEXT("viewTargetFindsCameraComponent"), ViewTarget->bFindCameraComponentWhenViewTarget);
    }

    APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
    if (CameraManager)
    {
        TSharedPtr<FJsonObject> CameraManagerJson = MakeShared<FJsonObject>();
        CameraManagerJson->SetStringField(TEXT("name"), CameraManager->GetName());
        CameraManagerJson->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(CameraManager->GetCameraLocation()));
        CameraManagerJson->SetObjectField(TEXT("rotation"), McpHandlerUtils::RotatorToJson(CameraManager->GetCameraRotation()));
        CameraManagerJson->SetNumberField(TEXT("fov"), CameraManager->GetFOVAngle());
        ViewState->SetObjectField(TEXT("playerCameraManager"), CameraManagerJson);
    }

    Result->SetObjectField(TEXT("playerViewState"), ViewState);
}

#endif // WITH_EDITOR

/**
 * SetBPVarDefaultValue - Set blueprint variable default value (multi-version compatible)
 * 
 * SetBlueprintVariableDefaultValue doesn't exist in UE 5.6 or 5.7, so defaults are persisted by
 * updating the Blueprint variable description and importing the value into the generated CDO.
 * 
 * @param Blueprint     Target blueprint
 * @param VarName       Variable name to set default for
 * @param DefaultValue  String representation of the default value
 */
static void SetBPVarDefaultValue(UBlueprint* Blueprint, FName VarName, const FString& DefaultValue)
{
#if WITH_EDITOR
    if (!Blueprint)
    {
        return;
    }

    bool bUpdatedVariableDescription = false;
    for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        if (VarDesc.VarName == VarName)
        {
            VarDesc.DefaultValue = DefaultValue;
            bUpdatedVariableDescription = true;
            break;
        }
    }

    bool bAppliedToCDO = false;
    McpSafeCompileBlueprint(Blueprint);
    if (Blueprint->GeneratedClass)
    {
        if (UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject())
        {
            if (FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName))
            {
                void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CDO);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                Property->ImportText_Direct(*DefaultValue, ValuePtr, CDO, 0);
#else
                Property->ImportText(*DefaultValue, ValuePtr, PPF_None, CDO);
#endif
                bAppliedToCDO = true;
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    Blueprint->MarkPackageDirty();

    if (!bUpdatedVariableDescription && !bAppliedToCDO)
    {
        UE_LOG(LogMcpCharacterHandlers, Warning,
               TEXT("Variable '%s' default value could not be applied; variable was not found on Blueprint '%s'."),
               *VarName.ToString(), *Blueprint->GetName());
    }
#endif
}

#if WITH_EDITOR

/**
 * CreateCharacterBlueprint - Create a new Character Blueprint asset
 * 
 * Creates a Blueprint derived from ACharacter at the specified path.
 * Includes security validation via IsValidAssetPath() and duplicate detection.
 * 
 * @param Path      Asset directory path (e.g. "/Game/Characters")
 * @param Name      Blueprint asset name
 * @param OutError  Error message if creation fails
 * @return Created UBlueprint or nullptr on failure
 */
static UBlueprint* CreateCharacterBlueprint(const FString& Path, const FString& Name, FString& OutError)
{
    FString FullPath = Path / Name;

    // Validate path before CreatePackage (prevents crashes from // and path traversal)
    if (!IsValidAssetPath(FullPath))
    {
        OutError = FString::Printf(TEXT("Invalid asset path: '%s'. Path must start with '/', cannot contain '..' or '//'."), *FullPath);
        return nullptr;
    }

    // Check if asset already exists to prevent assertion failures
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Asset already exists at path: %s"), *FullPath);
        return nullptr;
    }

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ACharacter::StaticClass();

    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (!Blueprint)
    {
        OutError = TEXT("Failed to create character blueprint");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    Blueprint->MarkPackageDirty();
    return Blueprint;
}

/**
 * GetVectorFromJsonChar - Extract FVector from a JSON object
 * 
 * @param Obj  JSON object with "x", "y", "z" number fields
 * @return Parsed FVector or ZeroVector if invalid
 */
static FVector GetVectorFromJsonChar(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FVector::ZeroVector;
    return FVector(
        GetJsonNumberField(Obj, TEXT("x"), 0.0),
        GetJsonNumberField(Obj, TEXT("y"), 0.0),
        GetJsonNumberField(Obj, TEXT("z"), 0.0)
    );
}

/**
 * GetRotatorFromJsonChar - Extract FRotator from a JSON object
 * 
 * @param Obj  JSON object with "pitch", "yaw", "roll" number fields
 * @return Parsed FRotator or ZeroRotator if invalid
 */
static FRotator GetRotatorFromJsonChar(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FRotator::ZeroRotator;
    return FRotator(
        GetJsonNumberField(Obj, TEXT("pitch"), 0.0),
        GetJsonNumberField(Obj, TEXT("yaw"), 0.0),
        GetJsonNumberField(Obj, TEXT("roll"), 0.0)
    );
}

namespace {
/**
 * AddBlueprintVariableChar - Add a Blueprint variable with proper category
 * 
 * In anonymous namespace with "Char" suffix to avoid Unity build collisions
 * with identically-named helpers in other handler files.
 * 
 * @param Blueprint  Target blueprint
 * @param VarName    Variable name
 * @param PinType    Variable pin type (bool, float, int, struct, etc.)
 * @param Category   Optional variable category for organization
 * @return true if variable was added successfully
 */
static bool AddBlueprintVariableChar(UBlueprint* Blueprint, const FString& VarName, const FEdGraphPinType& PinType, const FString& Category = TEXT(""))
{
    if (!Blueprint) return false;
    
    bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
    
    if (bSuccess && !Category.IsEmpty())
    {
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*VarName), nullptr, FText::FromString(Category));
    }
    
    return bSuccess;
}
} // namespace
#endif // WITH_EDITOR

// =============================================================================
// Main Handler Entry Point: HandleManageCharacterAction
// =============================================================================
// Dispatches all manage_character sub-actions to their respective implementations.
// Requires editor build (WITH_EDITOR).
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageCharacterAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    if (Action != TEXT("manage_character"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(RequestingSocket, RequestId, TEXT("Character handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Common parameters
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game"));
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    // ============================================================
    // 14.1 CHARACTER CREATION
    // ============================================================

    // -------------------------------------------------------------------------
    // create_character_blueprint
    // -------------------------------------------------------------------------
    // Creates a new ACharacter-derived Blueprint at the given path.
    // Optionally sets a skeletal mesh on the character's mesh component.
    //
    // Payload:  { "name": string, "path"?: "/Game", "skeletalMeshPath"?: string }
    // Response: { "blueprintPath": string, "name": string, "parentClass": "Character" }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("create_character_blueprint"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
        USkeletalMesh* RequestedMesh = nullptr;
        if (!SkeletalMeshPath.IsEmpty())
        {
            RequestedMesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
            if (!RequestedMesh)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Skeletal mesh not found: %s"), *SkeletalMeshPath), TEXT("ASSET_NOT_FOUND"));
                return true;
            }
        }

        FString Error;
        UBlueprint* Blueprint = CreateCharacterBlueprint(Path, Name, Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        bool bSkeletalMeshAssigned = false;
        if (RequestedMesh)
        {
            if (Blueprint->SimpleConstructionScript)
            {
                for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
                {
                    if (Node && Node->ComponentTemplate &&
                        Node->ComponentTemplate->IsA<USkeletalMeshComponent>())
                    {
                        USkeletalMeshComponent* MeshComp = Cast<USkeletalMeshComponent>(Node->ComponentTemplate);
                        if (MeshComp)
                        {
                            MeshComp->SetSkeletalMesh(RequestedMesh);
                            bSkeletalMeshAssigned = true;
                        }
                        break;
                    }
                }
            }

            // ACharacter's mesh is an inherited component, so the SCS usually has no
            // skeletal mesh node. Fall back to the inherited mesh on the CDO.
            if (!bSkeletalMeshAssigned)
            {
                ACharacter* CharCDO = Blueprint->GeneratedClass
                    ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
                    : nullptr;
                if (CharCDO && CharCDO->GetMesh())
                {
                    CharCDO->GetMesh()->SetSkeletalMesh(RequestedMesh);
                    bSkeletalMeshAssigned = true;
                }
            }

            if (!bSkeletalMeshAssigned)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("No skeletal mesh component found to assign '%s' on blueprint '%s'."),
                        *SkeletalMeshPath, *(Path / Name)),
                    TEXT("NO_MESH_COMPONENT"));
                return true;
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Path / Name);
        Result->SetStringField(TEXT("name"), Name);
        Result->SetStringField(TEXT("parentClass"), TEXT("Character"));
        if (!SkeletalMeshPath.IsEmpty())
        {
            Result->SetStringField(TEXT("skeletalMesh"), SkeletalMeshPath);
            Result->SetBoolField(TEXT("skeletalMeshAssigned"), bSkeletalMeshAssigned);
        }
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Character blueprint created"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // configure_capsule_component
    // -------------------------------------------------------------------------
    // Configures the capsule collision component on a Character Blueprint CDO.
    //
    // Payload:  { "blueprintPath": string, "capsuleRadius"?: 42, "capsuleHalfHeight"?: 96 }
    // Response: { "blueprintPath": string, "capsuleRadius": number, "capsuleHalfHeight": number }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("configure_capsule_component"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        float CapsuleRadius = static_cast<float>(GetJsonNumberField(Payload, TEXT("capsuleRadius"), 42.0));
        float CapsuleHalfHeight = static_cast<float>(GetJsonNumberField(Payload, TEXT("capsuleHalfHeight"), 96.0));

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        if (!CharCDO)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint '%s' is not an ACharacter; no capsule to configure."), *BlueprintPath),
                TEXT("NOT_A_CHARACTER"));
            return true;
        }

        UCapsuleComponent* Capsule = CharCDO->GetCapsuleComponent();
        if (!Capsule)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Character blueprint '%s' has no capsule component."), *BlueprintPath),
                TEXT("NO_CAPSULE"));
            return true;
        }

        Capsule->SetCapsuleRadius(CapsuleRadius);
        Capsule->SetCapsuleHalfHeight(CapsuleHalfHeight);

        if (Capsule->GetUnscaledCapsuleRadius() != CapsuleRadius ||
            Capsule->GetUnscaledCapsuleHalfHeight() != CapsuleHalfHeight)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Capsule on '%s' did not retain the requested dimensions."), *BlueprintPath),
                TEXT("VERIFY_FAILED"));
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("capsuleRadius"), CapsuleRadius);
        Result->SetNumberField(TEXT("capsuleHalfHeight"), CapsuleHalfHeight);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Capsule configured"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // configure_mesh_component
    // -------------------------------------------------------------------------
    // Configures the skeletal mesh component on a Character Blueprint CDO.
    // Supports setting mesh asset, animation blueprint, position offset, and rotation.
    //
    // Payload:  { "blueprintPath": string, "skeletalMeshPath"?: string,
    //             "animBlueprintPath"?: string, "meshOffset"?: {x,y,z},
    //             "meshRotation"?: {pitch,yaw,roll} }
    // Response: { "blueprintPath": string, "skeletalMesh"?: string, "animBlueprint"?: string }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("configure_mesh_component"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
        FString AnimBPPath = GetJsonStringField(Payload, TEXT("animBlueprintPath"));
        USkeletalMesh* RequestedMesh = nullptr;
        UAnimBlueprint* RequestedAnimBP = nullptr;

        if (!SkeletalMeshPath.IsEmpty())
        {
            RequestedMesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
            if (!RequestedMesh)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Skeletal mesh not found: %s"), *SkeletalMeshPath), TEXT("ASSET_NOT_FOUND"));
                return true;
            }
        }

        if (!AnimBPPath.IsEmpty())
        {
            RequestedAnimBP = LoadObject<UAnimBlueprint>(nullptr, *AnimBPPath);
            if (!RequestedAnimBP || !RequestedAnimBP->GeneratedClass)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Animation Blueprint not found: %s"), *AnimBPPath), TEXT("ASSET_NOT_FOUND"));
                return true;
            }
        }

        bool bSkeletalMeshAssigned = false;
        bool bAnimBlueprintAssigned = false;

        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetMesh())
        {
            if (RequestedMesh)
            {
                CharCDO->GetMesh()->SetSkeletalMesh(RequestedMesh);
                bSkeletalMeshAssigned = true;
            }

            if (RequestedAnimBP)
            {
                CharCDO->GetMesh()->SetAnimInstanceClass(RequestedAnimBP->GeneratedClass);
                bAnimBlueprintAssigned = true;
            }

            // Handle offset
            const TSharedPtr<FJsonObject>* OffsetObj;
            if (Payload->TryGetObjectField(TEXT("meshOffset"), OffsetObj))
            {
                FVector Offset = GetVectorFromJsonChar(*OffsetObj);
                CharCDO->GetMesh()->SetRelativeLocation(Offset);
            }

            // Handle rotation
            const TSharedPtr<FJsonObject>* RotObj;
            if (Payload->TryGetObjectField(TEXT("meshRotation"), RotObj))
            {
                FRotator Rotation = GetRotatorFromJsonChar(*RotObj);
                CharCDO->GetMesh()->SetRelativeRotation(Rotation);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        if (!SkeletalMeshPath.IsEmpty())
        {
            Result->SetStringField(TEXT("skeletalMesh"), SkeletalMeshPath);
            Result->SetBoolField(TEXT("skeletalMeshAssigned"), bSkeletalMeshAssigned);
        }
        if (!AnimBPPath.IsEmpty())
        {
            Result->SetStringField(TEXT("animBlueprint"), AnimBPPath);
            Result->SetBoolField(TEXT("animBlueprintAssigned"), bAnimBlueprintAssigned);
        }
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mesh configured"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // configure_camera_component
    // -------------------------------------------------------------------------
    // Adds or configures a SpringArm + Camera setup via SCS on a Character Blueprint.
    // If spring arm doesn't exist, creates it with camera as child.
    // UE 5.7 fix: uses SetParent(USCS_Node*) for SCS node parenting.
    //
    // Payload:  { "blueprintPath": string, "springArmLength"?: 300,
    //             "cameraUsePawnControlRotation"?: true, "springArmLagEnabled"?: false,
    //             "springArmLagSpeed"?: 10 }
    // Response: { "blueprintPath": string, "springArmLength": number,
    //             "usePawnControlRotation": bool, "lagEnabled": bool }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("configure_camera_component"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        float SpringArmLength = static_cast<float>(GetJsonNumberField(Payload, TEXT("springArmLength"), 300.0));
        bool UsePawnControlRotation = GetJsonBoolField(Payload, TEXT("cameraUsePawnControlRotation"), true);
        bool LagEnabled = GetJsonBoolField(Payload, TEXT("springArmLagEnabled"), false);
        float LagSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("springArmLagSpeed"), 10.0));

        // Add spring arm + camera to SCS if not present
        bool bHasSpringArm = false;
        bool bHasCamera = false;

        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate)
            {
                if (Node->ComponentTemplate->IsA<USpringArmComponent>())
                {
                    bHasSpringArm = true;
                    USpringArmComponent* SpringArm = Cast<USpringArmComponent>(Node->ComponentTemplate);
                    SpringArm->TargetArmLength = SpringArmLength;
                    SpringArm->bUsePawnControlRotation = UsePawnControlRotation;
                    SpringArm->bEnableCameraLag = LagEnabled;
                    SpringArm->CameraLagSpeed = LagSpeed;
                }
                if (Node->ComponentTemplate->IsA<UCameraComponent>())
                {
                    bHasCamera = true;
                }
            }
        }

        // Add spring arm if not present
        if (!bHasSpringArm)
        {
            USCS_Node* SpringArmNode = Blueprint->SimpleConstructionScript->CreateNode(
                USpringArmComponent::StaticClass(), FName(TEXT("CameraBoom")));
            if (SpringArmNode)
            {
                USpringArmComponent* SpringArm = Cast<USpringArmComponent>(SpringArmNode->ComponentTemplate);
                if (SpringArm)
                {
                    SpringArm->TargetArmLength = SpringArmLength;
                    SpringArm->bUsePawnControlRotation = UsePawnControlRotation;
                    SpringArm->bEnableCameraLag = LagEnabled;
                    SpringArm->CameraLagSpeed = LagSpeed;
                }
                Blueprint->SimpleConstructionScript->AddNode(SpringArmNode);

                // Add camera as child of spring arm - UE 5.7 fix: use SetParent(USCS_Node*)
                USCS_Node* CameraNode = Blueprint->SimpleConstructionScript->CreateNode(
                    UCameraComponent::StaticClass(), FName(TEXT("FollowCamera")));
                if (CameraNode)
                {
                    CameraNode->SetParent(SpringArmNode);
                    Blueprint->SimpleConstructionScript->AddNode(CameraNode);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("springArmLength"), SpringArmLength);
        Result->SetBoolField(TEXT("usePawnControlRotation"), UsePawnControlRotation);
        Result->SetBoolField(TEXT("lagEnabled"), LagEnabled);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera configured"), Result);
        return true;
    }

    // ============================================================
    // 14.2 MOVEMENT COMPONENT
    // ============================================================

    // -------------------------------------------------------------------------
    // configure_movement_speeds
    // -------------------------------------------------------------------------
    // Configures movement speeds and physics on the CharacterMovementComponent CDO.
    // All fields are optional — only provided fields are applied. runSpeed writes
    // MaxWalkSpeed when walkSpeed is absent (UE has no separate run-speed field);
    // alongside walkSpeed it lands in a RunSpeed variable default. sprintSpeed
    // lands in a SprintSpeed variable default (matches configure_sprint).
    //
    // Payload:  { "blueprintPath": string, "walkSpeed"?: 600, "runSpeed"?: 600,
    //             "sprintSpeed"?: 900, "crouchSpeed"?: 300, "swimSpeed"?: 300,
    //             "flySpeed"?: 600, "acceleration"?: 2048, "deceleration"?: 2048,
    //             "groundFriction"?: 8 }
    // Response: { "blueprintPath": string, "walkSpeed": number, "runSpeed"?: number,
    //             "runSpeedApplied"?: bool, "runSpeedTarget"?: string,
    //             "sprintSpeed"?: number, "sprintSpeedTarget"?: string,
    //             "variablesAdded"?: string[] }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("configure_movement_speeds"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        UCharacterMovementComponent* Movement = CharCDO ? CharCDO->GetCharacterMovement() : nullptr;
        if (!Movement)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint '%s' is not an ACharacter; no CharacterMovementComponent to configure."), *BlueprintPath),
                TEXT("NOT_A_CHARACTER"));
            return true;
        }

        const bool bHasWalkSpeed = Payload->HasField(TEXT("walkSpeed"));
        const bool bHasRunSpeed = Payload->HasField(TEXT("runSpeed"));
        const bool bHasSprintSpeed = Payload->HasField(TEXT("sprintSpeed"));
        TArray<TSharedPtr<FJsonValue>> VarsAdded;

        // All CMC writes happen before any SetBPVarDefaultValue: it recompiles
        // the Blueprint, which reinstances the CDO and would orphan this pointer.
        if (bHasWalkSpeed)
        {
            Movement->MaxWalkSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("walkSpeed"), 600.0));
        }

        FString RunSpeedTarget;
        if (bHasRunSpeed && !bHasWalkSpeed)
        {
            Movement->MaxWalkSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("runSpeed"), 600.0));
            RunSpeedTarget = TEXT("MaxWalkSpeed");
        }

        if (Payload->HasField(TEXT("crouchSpeed")))
            Movement->MaxWalkSpeedCrouched = static_cast<float>(GetJsonNumberField(Payload, TEXT("crouchSpeed"), 300.0));
        if (Payload->HasField(TEXT("swimSpeed")))
            Movement->MaxSwimSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("swimSpeed"), 300.0));
        if (Payload->HasField(TEXT("flySpeed")))
            Movement->MaxFlySpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("flySpeed"), 600.0));
        if (Payload->HasField(TEXT("acceleration")))
            Movement->MaxAcceleration = static_cast<float>(GetJsonNumberField(Payload, TEXT("acceleration"), 2048.0));
        if (Payload->HasField(TEXT("deceleration")))
            Movement->BrakingDecelerationWalking = static_cast<float>(GetJsonNumberField(Payload, TEXT("deceleration"), 2048.0));
        if (Payload->HasField(TEXT("groundFriction")))
            Movement->GroundFriction = static_cast<float>(GetJsonNumberField(Payload, TEXT("groundFriction"), 8.0));

        const float AppliedMaxWalkSpeed = Movement->MaxWalkSpeed;

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

        if (bHasRunSpeed && bHasWalkSpeed)
        {
            if (AddBlueprintVariableChar(Blueprint, TEXT("RunSpeed"), FloatPinType, TEXT("Movement")))
            {
                VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("RunSpeed")));
            }
            SetBPVarDefaultValue(Blueprint, FName(TEXT("RunSpeed")), FString::SanitizeFloat(GetJsonNumberField(Payload, TEXT("runSpeed"), 600.0)));
            RunSpeedTarget = TEXT("RunSpeed");
        }

        if (bHasSprintSpeed)
        {
            if (AddBlueprintVariableChar(Blueprint, TEXT("SprintSpeed"), FloatPinType, TEXT("Sprint")))
            {
                VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("SprintSpeed")));
            }
            SetBPVarDefaultValue(Blueprint, FName(TEXT("SprintSpeed")), FString::SanitizeFloat(GetJsonNumberField(Payload, TEXT("sprintSpeed"), 900.0)));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("walkSpeed"), AppliedMaxWalkSpeed);
        if (bHasRunSpeed)
        {
            Result->SetNumberField(TEXT("runSpeed"), GetJsonNumberField(Payload, TEXT("runSpeed"), 600.0));
            Result->SetBoolField(TEXT("runSpeedApplied"), true);
            Result->SetStringField(TEXT("runSpeedTarget"), RunSpeedTarget);
        }
        if (bHasSprintSpeed)
        {
            Result->SetNumberField(TEXT("sprintSpeed"), GetJsonNumberField(Payload, TEXT("sprintSpeed"), 900.0));
            Result->SetStringField(TEXT("sprintSpeedTarget"), TEXT("SprintSpeed"));
        }
        if (VarsAdded.Num() > 0)
        {
            Result->SetArrayField(TEXT("variablesAdded"), VarsAdded);
        }
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Movement speeds configured"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // configure_jump
    // -------------------------------------------------------------------------
    // Configures jump parameters on the CharacterMovementComponent CDO.
    // All fields optional — only provided fields are applied. jumpHeight is the
    // desired apex height in cm, converted to JumpZVelocity via v = sqrt(2*g*h)
    // using the effective gravity (DefaultGravityZ * GravityScale); gravityScale
    // is applied first so a same-call change feeds the conversion.
    //
    // Payload:  { "blueprintPath": string, "jumpHeight"?: 600, "airControl"?: 0.35,
    //             "gravityScale"?: 1.0, "fallingLateralFriction"?: 0.0,
    //             "maxJumpCount"?: 1, "jumpHoldTime"?: 0.0 }
    // Response: { "blueprintPath": string, "requestedJumpHeight"?: number,
    //             "appliedJumpZVelocity"?: number }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("configure_jump"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        UCharacterMovementComponent* Movement = CharCDO ? CharCDO->GetCharacterMovement() : nullptr;
        if (!Movement)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint '%s' is not an ACharacter; no CharacterMovementComponent to configure."), *BlueprintPath),
                TEXT("NOT_A_CHARACTER"));
            return true;
        }

        if (Payload->HasField(TEXT("gravityScale")))
            Movement->GravityScale = static_cast<float>(GetJsonNumberField(Payload, TEXT("gravityScale"), 1.0));

        const bool bHasJumpHeight = Payload->HasField(TEXT("jumpHeight"));
        double RequestedJumpHeight = 0.0;
        if (bHasJumpHeight)
        {
            RequestedJumpHeight = GetJsonNumberField(Payload, TEXT("jumpHeight"), 600.0);
            if (RequestedJumpHeight < 0.0)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    TEXT("jumpHeight must be >= 0 (apex height in cm; converted to JumpZVelocity)."),
                    TEXT("INVALID_ARGUMENT"));
                return true;
            }
            const double GravityMagnitude = FMath::Abs(static_cast<double>(UPhysicsSettings::Get()->DefaultGravityZ) * Movement->GravityScale);
            if (GravityMagnitude <= KINDA_SMALL_NUMBER)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Cannot convert jumpHeight to JumpZVelocity: effective gravity is zero (gravityScale=%g). Pass a non-zero gravityScale."), Movement->GravityScale),
                    TEXT("INVALID_ARGUMENT"));
                return true;
            }
            Movement->JumpZVelocity = static_cast<float>(FMath::Sqrt(2.0 * GravityMagnitude * RequestedJumpHeight));
        }
        if (Payload->HasField(TEXT("airControl")))
            Movement->AirControl = static_cast<float>(GetJsonNumberField(Payload, TEXT("airControl"), 0.35));
        if (Payload->HasField(TEXT("fallingLateralFriction")))
            Movement->FallingLateralFriction = static_cast<float>(GetJsonNumberField(Payload, TEXT("fallingLateralFriction"), 0.0));
        if (Payload->HasField(TEXT("maxJumpCount")))
            CharCDO->JumpMaxCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxJumpCount"), 1));
        if (Payload->HasField(TEXT("jumpHoldTime")))
            CharCDO->JumpMaxHoldTime = static_cast<float>(GetJsonNumberField(Payload, TEXT("jumpHoldTime"), 0.0));

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        if (bHasJumpHeight)
        {
            Result->SetNumberField(TEXT("requestedJumpHeight"), RequestedJumpHeight);
            Result->SetNumberField(TEXT("appliedJumpZVelocity"), Movement->JumpZVelocity);
        }
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Jump configured"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // configure_rotation
    // -------------------------------------------------------------------------
    // Configures rotation behavior on the CharacterMovementComponent and Character CDO.
    // Controls orient-to-movement, controller rotation axes, and rotation rate.
    //
    // Payload:  { "blueprintPath": string, "orientToMovement"?: true,
    //             "useControllerRotationYaw"?: false, "useControllerRotationPitch"?: false,
    //             "useControllerRotationRoll"?: false, "rotationRate"?: 540 }
    // Response: { "blueprintPath": string }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("configure_rotation"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();

            if (Payload->HasField(TEXT("orientToMovement")))
                Movement->bOrientRotationToMovement = GetJsonBoolField(Payload, TEXT("orientToMovement"), true);
            if (Payload->HasField(TEXT("useControllerRotationYaw")))
                CharCDO->bUseControllerRotationYaw = GetJsonBoolField(Payload, TEXT("useControllerRotationYaw"), false);
            if (Payload->HasField(TEXT("useControllerRotationPitch")))
                CharCDO->bUseControllerRotationPitch = GetJsonBoolField(Payload, TEXT("useControllerRotationPitch"), false);
            if (Payload->HasField(TEXT("useControllerRotationRoll")))
                CharCDO->bUseControllerRotationRoll = GetJsonBoolField(Payload, TEXT("useControllerRotationRoll"), false);
            if (Payload->HasField(TEXT("rotationRate")))
                Movement->RotationRate = FRotator(0.0, GetJsonNumberField(Payload, TEXT("rotationRate"), 540.0), 0.0);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Rotation configured"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // add_custom_movement_mode
    // -------------------------------------------------------------------------
    // Adds a custom movement mode with state tracking Blueprint variables.
    // Creates: bIsIn{ModeName}Mode (bool), CustomModeId_{ModeName} (int),
    //          {ModeName}Speed (float). Also sets MaxCustomMovementSpeed on the CMC.
    //
    // Payload:  { "blueprintPath": string, "modeName"?: "Custom", "modeId"?: 0,
    //             "customSpeed"?: 600 }
    // Response: { "blueprintPath": string, "modeName": string, "modeId": number,
    //             "stateVariable": string, "speedVariable": string, "customSpeed": number }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("add_custom_movement_mode"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        FString ModeName = GetJsonStringField(Payload, TEXT("modeName"), TEXT("Custom"));
        int32 ModeId = static_cast<int32>(GetJsonNumberField(Payload, TEXT("modeId"), 0));
        float CustomSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("customSpeed"), 600.0));

        // Add state tracking variable (e.g., bIsInCustomMode_0)
        FString StateVarName = FString::Printf(TEXT("bIsIn%sMode"), *ModeName);
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        AddBlueprintVariableChar(Blueprint, StateVarName, BoolPinType, TEXT("Movement States"));

        // Add custom mode ID variable
        FString ModeIdVarName = FString::Printf(TEXT("CustomModeId_%s"), *ModeName);
        FEdGraphPinType IntPinType;
        IntPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        AddBlueprintVariableChar(Blueprint, ModeIdVarName, IntPinType, TEXT("Movement States"));

        // Add custom speed variable for this mode
        FString SpeedVarName = FString::Printf(TEXT("%sSpeed"), *ModeName);
        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, SpeedVarName, FloatPinType, TEXT("Movement States"));

        // Set default values for the variables
        SetBPVarDefaultValue(Blueprint, FName(*ModeIdVarName), FString::FromInt(ModeId));
        SetBPVarDefaultValue(Blueprint, FName(*SpeedVarName), FString::SanitizeFloat(CustomSpeed));

        // Configure CharacterMovementComponent if available
        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
            Movement->MaxCustomMovementSpeed = CustomSpeed;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("modeName"), ModeName);
        Result->SetNumberField(TEXT("modeId"), ModeId);
        Result->SetStringField(TEXT("stateVariable"), StateVarName);
        Result->SetStringField(TEXT("speedVariable"), SpeedVarName);
        Result->SetNumberField(TEXT("customSpeed"), CustomSpeed);
        Result->SetStringField(TEXT("cmcProperty"), TEXT("MaxCustomMovementSpeed"));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Custom movement mode added with state tracking variables"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // configure_nav_movement
    // -------------------------------------------------------------------------
    // Configures navigation agent properties on the CharacterMovementComponent.
    //
    // Payload:  { "blueprintPath": string, "navAgentRadius"?: 42,
    //             "navAgentHeight"?: 192, "avoidanceEnabled"?: false }
    // Response: { "blueprintPath": string }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("configure_nav_movement"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();

            if (Payload->HasField(TEXT("navAgentRadius")))
                Movement->NavAgentProps.AgentRadius = static_cast<float>(GetJsonNumberField(Payload, TEXT("navAgentRadius"), 42.0));
            if (Payload->HasField(TEXT("navAgentHeight")))
                Movement->NavAgentProps.AgentHeight = static_cast<float>(GetJsonNumberField(Payload, TEXT("navAgentHeight"), 192.0));
            if (Payload->HasField(TEXT("avoidanceEnabled")))
                Movement->bUseRVOAvoidance = GetJsonBoolField(Payload, TEXT("avoidanceEnabled"), false);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Nav movement configured"), Result);
        return true;
    }

    // ============================================================
    // 14.3 ADVANCED MOVEMENT
    // ============================================================

    // -------------------------------------------------------------------------
    // setup_mantling
    // -------------------------------------------------------------------------
    // Configures a mantling system by adding state and configuration Blueprint
    // variables: bIsMantling, bCanMantle, MantleHeight, MantleReachDistance,
    // MantleTargetLocation (FVector).
    //
    // Payload:  { "blueprintPath": string, "mantleHeight"?: 200,
    //             "mantleReachDistance"?: 100 }
    // Response: { "blueprintPath": string, "mantleHeight": number,
    //             "mantleReachDistance": number, "stateVariable": string,
    //             "targetVariable": string }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("setup_mantling"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        float MantleHeight = static_cast<float>(GetJsonNumberField(Payload, TEXT("mantleHeight"), 200.0));
        float MantleReach = static_cast<float>(GetJsonNumberField(Payload, TEXT("mantleReachDistance"), 100.0));
        // Add mantling state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        AddBlueprintVariableChar(Blueprint, TEXT("bIsMantling"), BoolPinType, TEXT("Mantling"));
        AddBlueprintVariableChar(Blueprint, TEXT("bCanMantle"), BoolPinType, TEXT("Mantling"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("MantleHeight"), FloatPinType, TEXT("Mantling"));
        AddBlueprintVariableChar(Blueprint, TEXT("MantleReachDistance"), FloatPinType, TEXT("Mantling"));

        // Set default values for mantling configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanMantle")), TEXT("true"));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("MantleHeight")), FString::SanitizeFloat(MantleHeight));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("MantleReachDistance")), FString::SanitizeFloat(MantleReach));

        // Add mantle target location variable
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        AddBlueprintVariableChar(Blueprint, TEXT("MantleTargetLocation"), VectorPinType, TEXT("Mantling"));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("mantleHeight"), MantleHeight);
        Result->SetNumberField(TEXT("mantleReachDistance"), MantleReach);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsMantling"));
        Result->SetStringField(TEXT("targetVariable"), TEXT("MantleTargetLocation"));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mantling system configured with state variables"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // setup_vaulting
    // -------------------------------------------------------------------------
    // Configures a vaulting system by adding state and configuration Blueprint
    // variables: bIsVaulting, bCanVault, VaultHeight, VaultDepth,
    // VaultStartLocation, VaultEndLocation (FVector).
    //
    // Payload:  { "blueprintPath": string, "vaultHeight"?: 100,
    //             "vaultDepth"?: 100 }
    // Response: { "blueprintPath": string, "vaultHeight": number, "vaultDepth": number,
    //             "stateVariable": string }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("setup_vaulting"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        float VaultHeight = static_cast<float>(GetJsonNumberField(Payload, TEXT("vaultHeight"), 100.0));
        float VaultDepth = static_cast<float>(GetJsonNumberField(Payload, TEXT("vaultDepth"), 100.0));
        // Add vaulting state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        AddBlueprintVariableChar(Blueprint, TEXT("bIsVaulting"), BoolPinType, TEXT("Vaulting"));
        AddBlueprintVariableChar(Blueprint, TEXT("bCanVault"), BoolPinType, TEXT("Vaulting"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("VaultHeight"), FloatPinType, TEXT("Vaulting"));
        AddBlueprintVariableChar(Blueprint, TEXT("VaultDepth"), FloatPinType, TEXT("Vaulting"));

        // Set default values for vaulting configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanVault")), TEXT("true"));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("VaultHeight")), FString::SanitizeFloat(VaultHeight));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("VaultDepth")), FString::SanitizeFloat(VaultDepth));

        // Add vault start and end location variables
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        AddBlueprintVariableChar(Blueprint, TEXT("VaultStartLocation"), VectorPinType, TEXT("Vaulting"));
        AddBlueprintVariableChar(Blueprint, TEXT("VaultEndLocation"), VectorPinType, TEXT("Vaulting"));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("vaultHeight"), VaultHeight);
        Result->SetNumberField(TEXT("vaultDepth"), VaultDepth);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsVaulting"));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Vaulting system configured with state variables"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // setup_climbing
    // -------------------------------------------------------------------------
    // Configures a climbing system by adding state and configuration Blueprint
    // variables: bIsClimbing, bCanClimb, ClimbSpeed, ClimbableTag, ClimbSurfaceNormal.
    // The climb speed lives in the ClimbSpeed variable default; no CharacterMovement
    // property is written (MaxCustomMovementSpeed belongs to add_custom_movement_mode).
    //
    // Payload:  { "blueprintPath": string, "climbSpeed"?: 300,
    //             "climbableTag"?: "Climbable" }
    // Response: { "blueprintPath": string, "climbSpeed": number,
    //             "climbableTag": string, "stateVariable": string }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("setup_climbing"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        float ClimbSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("climbSpeed"), 300.0));
        FString ClimbableTag = GetJsonStringField(Payload, TEXT("climbableTag"), TEXT("Climbable"));
        // Add climbing state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        AddBlueprintVariableChar(Blueprint, TEXT("bIsClimbing"), BoolPinType, TEXT("Climbing"));
        AddBlueprintVariableChar(Blueprint, TEXT("bCanClimb"), BoolPinType, TEXT("Climbing"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("ClimbSpeed"), FloatPinType, TEXT("Climbing"));

        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        AddBlueprintVariableChar(Blueprint, TEXT("ClimbableTag"), NamePinType, TEXT("Climbing"));

        // Add climb surface normal for proper orientation
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        AddBlueprintVariableChar(Blueprint, TEXT("ClimbSurfaceNormal"), VectorPinType, TEXT("Climbing"));

        // Set default values for climbing configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanClimb")), TEXT("true"));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("ClimbSpeed")), FString::SanitizeFloat(ClimbSpeed));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("ClimbableTag")), ClimbableTag);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("climbSpeed"), ClimbSpeed);
        Result->SetStringField(TEXT("climbableTag"), ClimbableTag);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsClimbing"));
        Result->SetStringField(TEXT("speedVariable"), TEXT("ClimbSpeed"));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Climbing system configured with state variables"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // setup_sliding
    // -------------------------------------------------------------------------
    // Configures a sliding system by adding state and timing Blueprint variables:
    // bIsSliding, bCanSlide, SlideSpeed, SlideDuration, SlideCooldown,
    // SlideTimeRemaining, SlideCooldownRemaining.
    //
    // Payload:  { "blueprintPath": string, "slideSpeed"?: 800, "slideDuration"?: 1.0,
    //             "slideCooldown"?: 0.5 }
    // Response: { "blueprintPath": string, "slideSpeed": number, "slideDuration": number,
    //             "slideCooldown": number, "stateVariable": string }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("setup_sliding"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        float SlideSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("slideSpeed"), 800.0));
        float SlideDuration = static_cast<float>(GetJsonNumberField(Payload, TEXT("slideDuration"), 1.0));
        float SlideCooldown = static_cast<float>(GetJsonNumberField(Payload, TEXT("slideCooldown"), 0.5));
        // Add sliding state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        AddBlueprintVariableChar(Blueprint, TEXT("bIsSliding"), BoolPinType, TEXT("Sliding"));
        AddBlueprintVariableChar(Blueprint, TEXT("bCanSlide"), BoolPinType, TEXT("Sliding"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("SlideSpeed"), FloatPinType, TEXT("Sliding"));
        AddBlueprintVariableChar(Blueprint, TEXT("SlideDuration"), FloatPinType, TEXT("Sliding"));
        AddBlueprintVariableChar(Blueprint, TEXT("SlideCooldown"), FloatPinType, TEXT("Sliding"));
        AddBlueprintVariableChar(Blueprint, TEXT("SlideTimeRemaining"), FloatPinType, TEXT("Sliding"));
        AddBlueprintVariableChar(Blueprint, TEXT("SlideCooldownRemaining"), FloatPinType, TEXT("Sliding"));

        // Set default values for sliding configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanSlide")), TEXT("true"));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("SlideSpeed")), FString::SanitizeFloat(SlideSpeed));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("SlideDuration")), FString::SanitizeFloat(SlideDuration));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("SlideCooldown")), FString::SanitizeFloat(SlideCooldown));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("slideSpeed"), SlideSpeed);
        Result->SetNumberField(TEXT("slideDuration"), SlideDuration);
        Result->SetNumberField(TEXT("slideCooldown"), SlideCooldown);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsSliding"));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sliding system configured with state and timing variables"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // setup_wall_running
    // -------------------------------------------------------------------------
    // Configures a wall running system by adding state and configuration Blueprint
    // variables: bIsWallRunning, bIsWallRunningLeft, bIsWallRunningRight,
    // WallRunSpeed, WallRunDuration, WallRunGravityScale, WallRunTimeRemaining,
    // WallRunNormal (FVector). Passed values land in the variable defaults; no
    // CharacterMovement property is written (MaxCustomMovementSpeed belongs to
    // add_custom_movement_mode).
    //
    // Payload:  { "blueprintPath": string, "wallRunSpeed"?: 600,
    //             "wallRunDuration"?: 2.0, "wallRunGravityScale"?: 0.25 }
    // Response: { "blueprintPath": string, "wallRunSpeed": number,
    //             "wallRunDuration": number, "wallRunGravityScale": number,
    //             "stateVariable": string }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("setup_wall_running"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        float WallRunSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("wallRunSpeed"), 600.0));
        float WallRunDuration = static_cast<float>(GetJsonNumberField(Payload, TEXT("wallRunDuration"), 2.0));
        float WallRunGravity = static_cast<float>(GetJsonNumberField(Payload, TEXT("wallRunGravityScale"), 0.25));
        // Add wall running state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        AddBlueprintVariableChar(Blueprint, TEXT("bIsWallRunning"), BoolPinType, TEXT("Wall Running"));
        AddBlueprintVariableChar(Blueprint, TEXT("bIsWallRunningLeft"), BoolPinType, TEXT("Wall Running"));
        AddBlueprintVariableChar(Blueprint, TEXT("bIsWallRunningRight"), BoolPinType, TEXT("Wall Running"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("WallRunSpeed"), FloatPinType, TEXT("Wall Running"));
        AddBlueprintVariableChar(Blueprint, TEXT("WallRunDuration"), FloatPinType, TEXT("Wall Running"));
        AddBlueprintVariableChar(Blueprint, TEXT("WallRunGravityScale"), FloatPinType, TEXT("Wall Running"));
        AddBlueprintVariableChar(Blueprint, TEXT("WallRunTimeRemaining"), FloatPinType, TEXT("Wall Running"));

        // Add wall normal for orientation
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        AddBlueprintVariableChar(Blueprint, TEXT("WallRunNormal"), VectorPinType, TEXT("Wall Running"));

        // Set default values for wall running configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("WallRunSpeed")), FString::SanitizeFloat(WallRunSpeed));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("WallRunDuration")), FString::SanitizeFloat(WallRunDuration));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("WallRunGravityScale")), FString::SanitizeFloat(WallRunGravity));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("wallRunSpeed"), WallRunSpeed);
        Result->SetNumberField(TEXT("wallRunDuration"), WallRunDuration);
        Result->SetNumberField(TEXT("wallRunGravityScale"), WallRunGravity);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsWallRunning"));
        Result->SetStringField(TEXT("speedVariable"), TEXT("WallRunSpeed"));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Wall running system configured with state variables"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // setup_grappling
    // -------------------------------------------------------------------------
    // Configures a grappling system by adding state and configuration Blueprint
    // variables: bIsGrappling, bHasGrappleTarget, GrappleRange, GrappleSpeed,
    // GrappleTargetTag (FName), GrappleTargetLocation (FVector).
    //
    // Payload:  { "blueprintPath": string, "grappleRange"?: 2000,
    //             "grappleSpeed"?: 1500, "grappleTargetTag"?: "Grapple" }
    // Response: { "blueprintPath": string, "grappleRange": number,
    //             "grappleSpeed": number, "grappleTargetTag": string,
    //             "stateVariable": string }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("setup_grappling"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        float GrappleRange = static_cast<float>(GetJsonNumberField(Payload, TEXT("grappleRange"), 2000.0));
        float GrappleSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("grappleSpeed"), 1500.0));
        FString GrappleTarget = GetJsonStringField(Payload, TEXT("grappleTargetTag"), TEXT("Grapple"));
        // Add grappling state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        AddBlueprintVariableChar(Blueprint, TEXT("bIsGrappling"), BoolPinType, TEXT("Grappling"));
        AddBlueprintVariableChar(Blueprint, TEXT("bHasGrappleTarget"), BoolPinType, TEXT("Grappling"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("GrappleRange"), FloatPinType, TEXT("Grappling"));
        AddBlueprintVariableChar(Blueprint, TEXT("GrappleSpeed"), FloatPinType, TEXT("Grappling"));

        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        AddBlueprintVariableChar(Blueprint, TEXT("GrappleTargetTag"), NamePinType, TEXT("Grappling"));

        // Add grapple target location
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        AddBlueprintVariableChar(Blueprint, TEXT("GrappleTargetLocation"), VectorPinType, TEXT("Grappling"));

        // Set default values for grappling configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("GrappleRange")), FString::SanitizeFloat(GrappleRange));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("GrappleSpeed")), FString::SanitizeFloat(GrappleSpeed));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("GrappleTargetTag")), GrappleTarget);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("grappleRange"), GrappleRange);
        Result->SetNumberField(TEXT("grappleSpeed"), GrappleSpeed);
        Result->SetStringField(TEXT("grappleTargetTag"), GrappleTarget);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsGrappling"));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Grappling system configured with state variables"), Result);
        return true;
    }

    // ============================================================
    // 14.4 FOOTSTEPS SYSTEM
    // ============================================================

    // -------------------------------------------------------------------------
    // setup_footstep_system
    // -------------------------------------------------------------------------
    // Configures a footstep system by adding tracking Blueprint variables:
    // bFootstepSystemEnabled, FootstepSocketLeft, FootstepSocketRight,
    // FootstepTraceDistance.
    //
    // Payload:  { "blueprintPath": string, "footstepEnabled"?: true,
    //             "footstepSocketLeft"?: "foot_l", "footstepSocketRight"?: "foot_r",
    //             "footstepTraceDistance"?: 50 }
    // Response: { "blueprintPath": string, "enabled": bool, "socketLeft": string,
    //             "socketRight": string, "traceDistance": number }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("setup_footstep_system"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        bool Enabled = GetJsonBoolField(Payload, TEXT("footstepEnabled"), true);
        FString SocketLeft = GetJsonStringField(Payload, TEXT("footstepSocketLeft"), TEXT("foot_l"));
        FString SocketRight = GetJsonStringField(Payload, TEXT("footstepSocketRight"), TEXT("foot_r"));
        float TraceDistance = static_cast<float>(GetJsonNumberField(Payload, TEXT("footstepTraceDistance"), 50.0));

        // Add footstep system variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        AddBlueprintVariableChar(Blueprint, TEXT("bFootstepSystemEnabled"), BoolPinType, TEXT("Footsteps"));

        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepSocketLeft"), NamePinType, TEXT("Footsteps"));
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepSocketRight"), NamePinType, TEXT("Footsteps"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepTraceDistance"), FloatPinType, TEXT("Footsteps"));

        // Set default values for footstep configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("bFootstepSystemEnabled")), Enabled ? TEXT("true") : TEXT("false"));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("FootstepSocketLeft")), SocketLeft);
        SetBPVarDefaultValue(Blueprint, FName(TEXT("FootstepSocketRight")), SocketRight);
        SetBPVarDefaultValue(Blueprint, FName(TEXT("FootstepTraceDistance")), FString::SanitizeFloat(TraceDistance));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetBoolField(TEXT("enabled"), Enabled);
        Result->SetStringField(TEXT("socketLeft"), SocketLeft);
        Result->SetStringField(TEXT("socketRight"), SocketRight);
        Result->SetNumberField(TEXT("traceDistance"), TraceDistance);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Footstep system configured with tracking variables"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // map_surface_to_sound
    // -------------------------------------------------------------------------
    // Writes a surfaceType -> sound entry into the FootstepSoundMap
    // (TMap<FName, TSoftObjectPtr<USoundBase>>) Blueprint variable default,
    // creating the variable if needed. Entries accumulate across calls;
    // re-mapping an existing surface replaces its sound.
    //
    // Payload:  { "blueprintPath": string, "surfaceType": string, "soundPath": string }
    // Response: { "blueprintPath": string, "surfaceType": string, "soundPath": string,
    //             "mapVariable": string, "mapSize": number }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("map_surface_to_sound"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        FString SurfaceType = GetJsonStringField(Payload, TEXT("surfaceType"));
        FString SoundPath = GetJsonStringField(Payload, TEXT("soundPath"));
        if (SurfaceType.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing 'surfaceType'. Provide the surface name to map (e.g. 'Grass')."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (SoundPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing 'soundPath'. Provide the sound asset (SoundBase) to map to this surface."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
        if (!Sound)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Sound asset not found: %s"), *SoundPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FEdGraphPinType MapPinType;
        MapPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        MapPinType.ContainerType = EPinContainerType::Map;
        MapPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_SoftObject;
        MapPinType.PinValueType.TerminalSubCategoryObject = USoundBase::StaticClass();
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepSoundMap"), MapPinType, TEXT("Footsteps"));

        McpSafeCompileBlueprint(Blueprint);

        UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
        FMapProperty* MapProperty = Blueprint->GeneratedClass
            ? FindFProperty<FMapProperty>(Blueprint->GeneratedClass, TEXT("FootstepSoundMap"))
            : nullptr;
        FSoftObjectProperty* SoundValueProperty = MapProperty ? CastField<FSoftObjectProperty>(MapProperty->ValueProp) : nullptr;
        if (!CDO || !MapProperty || !CastField<FNameProperty>(MapProperty->KeyProp) || !SoundValueProperty)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("FootstepSoundMap on '%s' is not a TMap<Name, SoftObject> variable; retype or remove the existing variable and retry."), *BlueprintPath),
                TEXT("VARIABLE_TYPE_MISMATCH"));
            return true;
        }

        void* MapPtr = MapProperty->ContainerPtrToValuePtr<void>(CDO);
        FScriptMapHelper MapHelper(MapProperty, MapPtr);
        FName SurfaceKey(*SurfaceType);
        FSoftObjectPtr SoundRef((FSoftObjectPath(Sound)));
        MapHelper.AddPair(&SurfaceKey, &SoundRef);

        // Keep the variable description's default in sync with the CDO so
        // recompiles that re-import DefaultValue reproduce the map.
        FString MapDefaultText;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        MapProperty->ExportTextItem_Direct(MapDefaultText, MapPtr, nullptr, CDO, PPF_None);
#else
        MapProperty->ExportTextItem(MapDefaultText, MapPtr, nullptr, CDO, PPF_None);
#endif
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == FName(TEXT("FootstepSoundMap")))
            {
                VarDesc.DefaultValue = MapDefaultText;
                break;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("surfaceType"), SurfaceType);
        Result->SetStringField(TEXT("soundPath"), SoundPath);
        Result->SetStringField(TEXT("mapVariable"), TEXT("FootstepSoundMap"));
        Result->SetNumberField(TEXT("mapSize"), MapHelper.Num());
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Surface mapped to footstep sound"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // configure_footstep_fx
    // -------------------------------------------------------------------------
    // Configures footstep FX parameters by adding volume and scale Blueprint variables.
    //
    // Payload:  { "blueprintPath": string, "volumeMultiplier"?: 1.0,
    //             "particleScale"?: 1.0 }
    // Response: { "blueprintPath": string, "volumeMultiplier": number,
    //             "particleScale": number }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("configure_footstep_fx"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        float VolumeMultiplier = static_cast<float>(GetJsonNumberField(Payload, TEXT("volumeMultiplier"), 1.0));
        float ParticleScale = static_cast<float>(GetJsonNumberField(Payload, TEXT("particleScale"), 1.0));

        // Add FX configuration variables
        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepVolumeMultiplier"), FloatPinType, TEXT("Footsteps"));
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepParticleScale"), FloatPinType, TEXT("Footsteps"));

        // Set default values for footstep FX configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("FootstepVolumeMultiplier")), FString::SanitizeFloat(VolumeMultiplier));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("FootstepParticleScale")), FString::SanitizeFloat(ParticleScale));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("volumeMultiplier"), VolumeMultiplier);
        Result->SetNumberField(TEXT("particleScale"), ParticleScale);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Footstep FX configured with scale variables"), Result);
        return true;
    }

    // ============================================================
    // UTILITY
    // ============================================================

    // -------------------------------------------------------------------------
    // get_character_info
    // -------------------------------------------------------------------------
    // Retrieves comprehensive info about a Character Blueprint including:
    // capsule dimensions, movement speeds, jump settings, Blueprint camera
    // templates, active camera discovery flags, PIE player view state,
    // and all movement-related Blueprint variables.
    //
    // Payload:  { "blueprintPath": string }
    // Response: { "blueprintPath": string, "assetName": string,
    //             "capsuleRadius": number, "capsuleHalfHeight": number,
    //             "walkSpeed": number, "jumpZVelocity": number, "airControl": number,
    //             "hasSpringArm": bool, "hasCamera": bool,
    //             "springArmTemplates": object[], "cameraTemplates": object[],
    //             "bFindCameraComponentWhenViewTarget"?: bool, "playerViewState": object,
    //             "movementVariables"?: string[] }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("get_character_info"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("assetName"), Blueprint->GetName());

        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO)
        {
            if (CharCDO->GetCapsuleComponent())
            {
                Result->SetNumberField(TEXT("capsuleRadius"), CharCDO->GetCapsuleComponent()->GetUnscaledCapsuleRadius());
                Result->SetNumberField(TEXT("capsuleHalfHeight"), CharCDO->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
            }

            if (CharCDO->GetCharacterMovement())
            {
                UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
                Result->SetNumberField(TEXT("walkSpeed"), Movement->MaxWalkSpeed);
                Result->SetNumberField(TEXT("jumpZVelocity"), Movement->JumpZVelocity);
                Result->SetNumberField(TEXT("airControl"), Movement->AirControl);
                Result->SetBoolField(TEXT("orientToMovement"), Movement->bOrientRotationToMovement);
                Result->SetNumberField(TEXT("gravityScale"), Movement->GravityScale);
                Result->SetNumberField(TEXT("customMovementSpeed"), Movement->MaxCustomMovementSpeed);
            }

            Result->SetNumberField(TEXT("maxJumpCount"), CharCDO->JumpMaxCount);
            Result->SetBoolField(TEXT("useControllerRotationYaw"), CharCDO->bUseControllerRotationYaw);
        }

        // Check for spring arm and camera templates on the Blueprint asset.
        bool bHasSpringArm = false;
        bool bHasCamera = false;
        TArray<TSharedPtr<FJsonValue>> SpringArmTemplates;
        TArray<TSharedPtr<FJsonValue>> CameraTemplates;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate)
            {
                if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(Node->ComponentTemplate))
                {
                    bHasSpringArm = true;
                    SpringArmTemplates.Add(MakeShared<FJsonValueObject>(CreateSpringArmComponentReportChar(SpringArm)));
                }
                else if (UCameraComponent* Camera = Cast<UCameraComponent>(Node->ComponentTemplate))
                {
                    bHasCamera = true;
                    CameraTemplates.Add(MakeShared<FJsonValueObject>(CreateCameraComponentReportChar(Camera)));
                }
            }
        }
        Result->SetBoolField(TEXT("hasSpringArm"), bHasSpringArm);
        Result->SetBoolField(TEXT("hasCamera"), bHasCamera);
        Result->SetArrayField(TEXT("springArmTemplates"), SpringArmTemplates);
        Result->SetArrayField(TEXT("cameraTemplates"), CameraTemplates);

        if (CharCDO)
        {
            Result->SetBoolField(TEXT("bFindCameraComponentWhenViewTarget"), CharCDO->bFindCameraComponentWhenViewTarget);
        }

        UWorld* PIEWorld = (GEditor && GEditor->PlayWorld) ? GEditor->PlayWorld.Get() : nullptr;
        AddPlayerViewStateReportChar(PIEWorld, Result);

        // List blueprint variables related to movement states
        TArray<TSharedPtr<FJsonValue>> MovementVars;
        for (const FBPVariableDescription& Var : Blueprint->NewVariables)
        {
            FString VarName = Var.VarName.ToString();
            if (VarName.StartsWith(TEXT("bIs")) || VarName.StartsWith(TEXT("bCan")) ||
                VarName.Contains(TEXT("Speed")) || VarName.Contains(TEXT("Movement")))
            {
                MovementVars.Add(MakeShared<FJsonValueString>(VarName));
            }
        }
        if (MovementVars.Num() > 0)
        {
            Result->SetArrayField(TEXT("movementVariables"), MovementVars);
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Character info retrieved"), Result);
        return true;
    }

    // ============================================================
    // ALIASES & CONVENIENCE SUB-ACTIONS
    // ============================================================

    // -------------------------------------------------------------------------
    // setup_movement (alias for configure_movement_speeds - subset)
    // -------------------------------------------------------------------------
    // Simplified movement setup with walkSpeed, runSpeed, and acceleration.
    //
    // Payload:  { "blueprintPath": string, "walkSpeed"?: 600, "runSpeed"?: 600,
    //             "acceleration"?: 2048 }
    // Response: { "blueprintPath": string }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("setup_movement"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
            // walkSpeed takes priority over runSpeed since both set MaxWalkSpeed in UE.
            if (Payload->HasField(TEXT("walkSpeed")))
                Movement->MaxWalkSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("walkSpeed"), 600.0));
            else if (Payload->HasField(TEXT("runSpeed")))
                Movement->MaxWalkSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("runSpeed"), 600.0));
            if (Payload->HasField(TEXT("acceleration")))
                Movement->MaxAcceleration = static_cast<float>(GetJsonNumberField(Payload, TEXT("acceleration"), 2048.0));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Movement configured"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // set_walk_speed
    // -------------------------------------------------------------------------
    // Direct setter for MaxWalkSpeed on the CharacterMovementComponent.
    //
    // Payload:  { "blueprintPath": string, "walkSpeed"?: 600 }
    // Response: { "blueprintPath": string, "walkSpeed": number }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("set_walk_speed"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        double WalkSpeed = GetJsonNumberField(Payload, TEXT("walkSpeed"), 600.0);

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            CharCDO->GetCharacterMovement()->MaxWalkSpeed = static_cast<float>(WalkSpeed);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("walkSpeed"), WalkSpeed);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Walk speed set"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // set_jump_height
    // -------------------------------------------------------------------------
    // Sets JumpZVelocity from a desired jump apex height in cm via v = sqrt(2*g*h),
    // using the effective gravity (DefaultGravityZ * GravityScale).
    //
    // Payload:  { "blueprintPath": string, "jumpHeight"?: 600 }
    // Response: { "blueprintPath": string, "jumpHeight": number,
    //             "appliedJumpZVelocity": number }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("set_jump_height"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        double JumpHeight = GetJsonNumberField(Payload, TEXT("jumpHeight"), 600.0);
        if (JumpHeight < 0.0)
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("jumpHeight must be >= 0 (apex height in cm; converted to JumpZVelocity)."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        UCharacterMovementComponent* Movement = CharCDO ? CharCDO->GetCharacterMovement() : nullptr;
        if (!Movement)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint '%s' is not an ACharacter; no CharacterMovementComponent to configure."), *BlueprintPath),
                TEXT("NOT_A_CHARACTER"));
            return true;
        }

        const double GravityMagnitude = FMath::Abs(static_cast<double>(UPhysicsSettings::Get()->DefaultGravityZ) * Movement->GravityScale);
        if (GravityMagnitude <= KINDA_SMALL_NUMBER)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Cannot convert jumpHeight to JumpZVelocity: effective gravity is zero (gravityScale=%g). Set a non-zero gravityScale first."), Movement->GravityScale),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Movement->JumpZVelocity = static_cast<float>(FMath::Sqrt(2.0 * GravityMagnitude * JumpHeight));

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("jumpHeight"), JumpHeight);
        Result->SetNumberField(TEXT("appliedJumpZVelocity"), Movement->JumpZVelocity);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Jump height set"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // set_gravity_scale
    // -------------------------------------------------------------------------
    // Direct setter for GravityScale on the CharacterMovementComponent.
    //
    // Payload:  { "blueprintPath": string, "gravityScale"?: 1.0 }
    // Response: { "blueprintPath": string, "gravityScale": number }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("set_gravity_scale"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        double GravityScale = GetJsonNumberField(Payload, TEXT("gravityScale"), 1.0);

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            CharCDO->GetCharacterMovement()->GravityScale = static_cast<float>(GravityScale);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("gravityScale"), GravityScale);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Gravity scale set"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // set_ground_friction
    // -------------------------------------------------------------------------
    // Direct setter for GroundFriction on the CharacterMovementComponent.
    //
    // Payload:  { "blueprintPath": string, "groundFriction"?: 8.0 }
    // Response: { "blueprintPath": string, "groundFriction": number }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("set_ground_friction"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        double GroundFriction = GetJsonNumberField(Payload, TEXT("groundFriction"), 8.0);

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            CharCDO->GetCharacterMovement()->GroundFriction = static_cast<float>(GroundFriction);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("groundFriction"), GroundFriction);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ground friction set"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // set_braking_deceleration
    // -------------------------------------------------------------------------
    // Direct setter for BrakingDecelerationWalking on the CharacterMovementComponent.
    //
    // Payload:  { "blueprintPath": string, "brakingDeceleration"?: 2048 }
    // Response: { "blueprintPath": string, "brakingDeceleration": number }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("set_braking_deceleration"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        double Deceleration = GetJsonNumberField(Payload, TEXT("brakingDeceleration"), 2048.0);

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            CharCDO->GetCharacterMovement()->BrakingDecelerationWalking = static_cast<float>(Deceleration);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("brakingDeceleration"), Deceleration);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Braking deceleration set"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // configure_crouch
    // -------------------------------------------------------------------------
    // Configures crouching parameters on the CharacterMovementComponent.
    //
    // Payload:  { "blueprintPath": string, "crouchSpeed"?: 300,
    //             "crouchedHalfHeight"?: 44, "canCrouch"?: true }
    // Response: { "blueprintPath": string, "crouchSpeed": number,
    //             "crouchedHalfHeight": number, "canCrouch": bool }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("configure_crouch"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        double CrouchSpeed = GetJsonNumberField(Payload, TEXT("crouchSpeed"), 300.0);
        double CrouchedHalfHeight = GetJsonNumberField(Payload, TEXT("crouchedHalfHeight"), 44.0);
        bool CanCrouch = GetJsonBoolField(Payload, TEXT("canCrouch"), true);

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
            Movement->MaxWalkSpeedCrouched = static_cast<float>(CrouchSpeed);
            Movement->SetCrouchedHalfHeight(static_cast<float>(CrouchedHalfHeight));
            Movement->NavAgentProps.bCanCrouch = CanCrouch;
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("crouchSpeed"), CrouchSpeed);
        Result->SetNumberField(TEXT("crouchedHalfHeight"), CrouchedHalfHeight);
        Result->SetBoolField(TEXT("canCrouch"), CanCrouch);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Crouch configured"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // configure_sprint
    // -------------------------------------------------------------------------
    // Configures sprinting by adding state variables (bIsSprinting, SprintSpeed)
    // and setting the SprintSpeed variable default. No CharacterMovement property
    // is written; sprint logic must apply SprintSpeed at runtime.
    //
    // Payload:  { "blueprintPath": string, "sprintSpeed"?: 900 }
    // Response: { "blueprintPath": string, "sprintSpeed": number,
    //             "stateVariable": string, "speedVariable": string }
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("configure_sprint"))
    {
        UBlueprint *Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
        if (!Blueprint) return true;

        double SprintSpeed = GetJsonNumberField(Payload, TEXT("sprintSpeed"), 900.0);

        // Add sprint state variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        AddBlueprintVariableChar(Blueprint, TEXT("bIsSprinting"), BoolPinType, TEXT("Sprint"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("SprintSpeed"), FloatPinType, TEXT("Sprint"));

        SetBPVarDefaultValue(Blueprint, FName(TEXT("SprintSpeed")), FString::SanitizeFloat(SprintSpeed));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("sprintSpeed"), SprintSpeed);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsSprinting"));
        Result->SetStringField(TEXT("speedVariable"), TEXT("SprintSpeed"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sprint configured; SprintSpeed stored as variable default"), Result);
        return true;
    }

    // ============================================================
    // UNKNOWN SUB-ACTION FALLBACK
    // ============================================================
    SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Unknown character subAction: %s"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;

#endif // WITH_EDITOR
}

// =============================================================================
// Cleanup: Undefine local macro aliases
// =============================================================================
#undef GetJsonStringField
#undef GetJsonNumberField
#undef GetJsonBoolField
