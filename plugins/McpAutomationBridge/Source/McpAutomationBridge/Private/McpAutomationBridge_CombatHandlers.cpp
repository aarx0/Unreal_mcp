// =============================================================================
// McpAutomationBridge_CombatHandlers.cpp
// =============================================================================
// Combat & Weapons System Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED (31+ actions):
// -----------------------------------
// Section 1: Weapon Creation
//   - create_weapon_blueprint       : Create AWeapon base blueprint
//   - add_weapon_mesh               : Add skeletal/static mesh to weapon
//   - configure_weapon_stats        : Set damage, fire rate, range, etc.
//
// Section 2: Firing Modes
//   - set_fire_mode                 : Configure fire mode (Auto/Semi/Burst)
//   - add_muzzle_flash              : Add muzzle flash particle/light
//   - add_firing_sound              : Add fire sound cue
//   - configure_ammo_system         : Setup ammo/reload mechanics
//
// Section 3: Projectiles
//   - create_projectile_blueprint   : Create AProjectile blueprint
//   - add_projectile_movement       : Add UProjectileMovementComponent
//   - configure_projectile_damage   : Set damage type and values
//   - add_projectile_collision      : Configure collision response
//
// Section 4: Damage System
//   - create_damage_type            : Create UDamageType class
//   - configure_damage_response     : Set damage response behavior
//   - add_damage_indicator          : Add visual/audio feedback
//
// Section 5: Melee Combat
//   - create_melee_weapon           : Create melee weapon blueprint
//   - configure_melee_collision     : Setup attack trace/hitbox
//   - add_combo_system              : Configure combo attacks
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.7: All handlers supported
// - Weapon/projectile APIs stable across versions
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpResponseHelpers.h"

class UProjectileMovementComponent;
class USphereComponent;
class UBoxComponent;

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundCue.h"
#include "Materials/Material.h"
#include "Animation/AnimMontage.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#endif

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h

#if WITH_EDITOR
// Helper to create Actor blueprint
static UBlueprint* CreateActorBlueprint(UClass* ParentClass, const FString& Path, const FString& Name, FString& OutError)
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
    Factory->ParentClass = ParentClass;

    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (!Blueprint)
    {
        OutError = TEXT("Failed to create blueprint");
        return nullptr;
    }

    McpSafeAssetSave(Blueprint);
    return Blueprint;
}

// Helper to get or create SCS component
template<typename T>
T* GetOrCreateSCSComponent(UBlueprint* Blueprint, const FString& ComponentName, const FString& AttachTo = TEXT(""))
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return nullptr;
    }

    // Try to find existing component
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<T>())
        {
            if (ComponentName.IsEmpty() || Node->GetVariableName().ToString() == ComponentName)
            {
                return Cast<T>(Node->ComponentTemplate);
            }
        }
    }

    // UE 5.7+ Fix: SCS->CreateNode() creates and owns the ComponentTemplate internally.
    // DO NOT create component with NewObject then assign to NewNode->ComponentTemplate.
    // This causes access violation crashes due to incorrect object ownership.
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    USCS_Node* NewNode = SCS->CreateNode(T::StaticClass(), FName(*ComponentName));
    if (!NewNode || !NewNode->ComponentTemplate)
    {
        return nullptr;
    }
    
    T* NewComp = Cast<T>(NewNode->ComponentTemplate);
    if (!NewComp)
    {
        return nullptr;
    }
    
    // UE 5.7 SCS fix: Always add nodes directly via SCS->AddNode() 
    // Use SetParent(USCS_Node*) for hierarchy instead of SetupAttachment
    // SetupAttachment creates cross-package references that crash on save
    if (!AttachTo.IsEmpty())
    {
        for (USCS_Node* ParentNode : SCS->GetAllNodes())
        {
            if (ParentNode && ParentNode->GetVariableName().ToString() == AttachTo)
            {
                // Set up attachment via SetParent(USCS_Node*)
                NewNode->SetParent(ParentNode);
                break;
            }
        }
    }
    // Always add directly to SCS (never via AddChildNode)
    SCS->AddNode(NewNode);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return NewComp;
}

// Helper to get Vector from JSON
static FVector GetVectorFromJsonCombat(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FVector::ZeroVector;
    return FVector(
        GetJsonNumberField(Obj, TEXT("x"), 0.0),
        GetJsonNumberField(Obj, TEXT("y"), 0.0),
        GetJsonNumberField(Obj, TEXT("z"), 0.0)
    );
}

namespace {
// Helper to add a Blueprint variable with a specific type
static bool AddBlueprintVariableCombat(UBlueprint* Blueprint, const FName& VarName, const FEdGraphPinType& PinType)
{
    if (!Blueprint)
    {
        return false;
    }
    
    // Check if variable already exists
    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        if (Var.VarName == VarName)
        {
            return true; // Already exists
        }
    }
    
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, PinType);
    return true;
}

// Helper to create pin types
static FEdGraphPinType MakeFloatPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
    PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
    return PinType;
}

static FEdGraphPinType MakeBoolPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    return PinType;
}

static FEdGraphPinType MakeStringPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    return PinType;
}

static FEdGraphPinType MakeNamePinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
    return PinType;
}

static void SetVariablesAdded(const TSharedPtr<FJsonObject>& Result, const TArray<FString>& VarNames)
{
    TArray<TSharedPtr<FJsonValue>> VarsAdded;
    for (const FString& VarName : VarNames)
    {
        VarsAdded.Add(MakeShared<FJsonValueString>(VarName));
    }
    Result->SetArrayField(TEXT("variablesAdded"), VarsAdded);
}

// Reads each Blueprint member variable's current generated-class CDO value into
// Info.variableValues (bool as bool, numeric as number, enum as name string,
// else export-text). A variable not yet compiled into the class reads as null.
static void AddVariableValueReadbackCombat(const TSharedPtr<FJsonObject>& Info, UBlueprint* Blueprint)
{
    constexpr int32 MaxVariableValues = 256;
    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
    TSharedPtr<FJsonObject> Values = MakeShared<FJsonObject>();
    bool bTruncated = false;
    int32 Emitted = 0;
    for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        if (Emitted >= MaxVariableValues)
        {
            bTruncated = true;
            break;
        }
        ++Emitted;

        const FString VarName = VarDesc.VarName.ToString();
        FProperty* Property = CDO ? FindFProperty<FProperty>(CDO->GetClass(), VarDesc.VarName) : nullptr;
        if (!Property)
        {
            Values->SetField(VarName, MakeShared<FJsonValueNull>());
            continue;
        }
        if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
        {
            Values->SetBoolField(VarName, BoolProp->GetPropertyValue_InContainer(CDO));
            continue;
        }
        if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
        {
            if (const UEnum* Enum = EnumProp->GetEnum())
            {
                const void* ValuePtr = EnumProp->ContainerPtrToValuePtr<void>(CDO);
                Values->SetStringField(VarName, Enum->GetNameStringByValue(
                    EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr)));
                continue;
            }
        }
        // Legacy BP enums compile to byte properties carrying a UEnum.
        const FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        if (ByteProp && ByteProp->Enum)
        {
            Values->SetStringField(VarName, ByteProp->Enum->GetNameStringByValue(
                ByteProp->GetPropertyValue_InContainer(CDO)));
            continue;
        }
        if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
        {
            const void* ValuePtr = NumProp->ContainerPtrToValuePtr<void>(CDO);
            Values->SetNumberField(VarName,
                NumProp->IsFloatingPoint()
                    ? NumProp->GetFloatingPointPropertyValue(ValuePtr)
                    : static_cast<double>(NumProp->GetSignedIntPropertyValue(ValuePtr)));
            continue;
        }
        FString ValueString;
        FBlueprintEditorUtils::PropertyValueToString(Property, reinterpret_cast<const uint8*>(CDO), ValueString, CDO);
        Values->SetStringField(VarName, ValueString);
    }
    Info->SetObjectField(TEXT("variableValues"), Values);
    Info->SetNumberField(TEXT("variableCount"), Blueprint->NewVariables.Num());
    Info->SetBoolField(TEXT("variableValuesTruncated"), bTruncated);
}
} // namespace
#endif

// =============================================================================
// Combat Member Handlers
// =============================================================================
// Dispatch lives in the manage_combat FMcpCall classes
// (Private/MCP/Calls/McpCalls_ManageCombat.cpp); each HandleCombat*
// member here implements one advertised action.

// ============================================================
// 15.1 WEAPON BASE
// ============================================================

// create_weapon_blueprint
bool UMcpAutomationBridgeSubsystem::HandleCombatCreateWeaponBlueprint(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game"));

    if (Name.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString Error;
    UBlueprint* Blueprint = CreateActorBlueprint(AActor::StaticClass(), Path, Name, Error);
    if (!Blueprint)
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    // Add static mesh component for weapon mesh
    UStaticMeshComponent* WeaponMesh = GetOrCreateSCSComponent<UStaticMeshComponent>(Blueprint, TEXT("WeaponMesh"));
    if (WeaponMesh)
    {
        FString MeshPath = GetJsonStringField(Payload, TEXT("weaponMeshPath"));
        if (!MeshPath.IsEmpty())
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
            if (Mesh)
            {
                WeaponMesh->SetStaticMesh(Mesh);
            }
        }
    }

    // Set base damage as default variable if needed
    double BaseDamage = GetJsonNumberField(Payload, TEXT("baseDamage"), 25.0);
    double FireRate = GetJsonNumberField(Payload, TEXT("fireRate"), 600.0);
    double Range = GetJsonNumberField(Payload, TEXT("range"), 10000.0);
    double Spread = GetJsonNumberField(Payload, TEXT("spread"), 2.0);

    // Apply weapon stats as Blueprint variables using FBlueprintEditorUtils
    AddBlueprintVariableCombat(Blueprint, TEXT("BaseDamage"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("FireRate"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("Range"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("Spread"), MakeFloatPinType());
    
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    
    // Set default values for the variables using CDO
    if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
    {
        if (UObject* CDO = BPGC->GetDefaultObject())
        {
            // Set via reflection
            if (FDoubleProperty* DamageProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("BaseDamage")))
            {
                DamageProp->SetPropertyValue_InContainer(CDO, BaseDamage);
            }
            if (FDoubleProperty* RateProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("FireRate")))
            {
                RateProp->SetPropertyValue_InContainer(CDO, FireRate);
            }
            if (FDoubleProperty* RangeProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("Range")))
            {
                RangeProp->SetPropertyValue_InContainer(CDO, Range);
            }
            if (FDoubleProperty* SpreadProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("Spread")))
            {
                SpreadProp->SetPropertyValue_InContainer(CDO, Spread);
            }
        }
    }

    McpSafeAssetSave(Blueprint);

    // Build response using standardized helper
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Result->SetNumberField(TEXT("baseDamage"), BaseDamage);
    Result->SetNumberField(TEXT("fireRate"), FireRate);
    Result->SetNumberField(TEXT("range"), Range);
    Result->SetNumberField(TEXT("spread"), Spread);
    SetVariablesAdded(Result, {TEXT("BaseDamage"), TEXT("FireRate"), TEXT("Range"), TEXT("Spread")});

    SendAutomationResponse(Socket, RequestId, true, TEXT("Weapon blueprint created successfully."), Result);
    return true;
#endif // WITH_EDITOR
}

// configure_weapon_mesh
bool UMcpAutomationBridgeSubsystem::HandleCombatConfigureWeaponMesh(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    FString MeshPath = GetJsonStringField(Payload, TEXT("weaponMeshPath"));
    if (!MeshPath.IsEmpty())
    {
        UStaticMeshComponent* WeaponMesh = GetOrCreateSCSComponent<UStaticMeshComponent>(Blueprint, TEXT("WeaponMesh"));
        if (WeaponMesh)
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
            if (Mesh)
            {
                WeaponMesh->SetStaticMesh(Mesh);
            }
        }
    }

    McpSafeCompileBlueprint(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Result->SetStringField(TEXT("meshPath"), MeshPath);
    
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Weapon mesh configured."), Result);
    return true;
#endif // WITH_EDITOR
}

// configure_weapon_sockets

// set_weapon_stats
bool UMcpAutomationBridgeSubsystem::HandleCombatSetWeaponStats(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    McpHandlerUtils::FMcpWriteReport Report;
    TArray<FString> VarsAdded;

    const bool bHasBaseDamage = Payload->HasField(TEXT("baseDamage"));
    const bool bHasFireRate = Payload->HasField(TEXT("fireRate"));
    const bool bHasRange = Payload->HasField(TEXT("range"));
    const bool bHasSpread = Payload->HasField(TEXT("spread"));

    double BaseDamage = GetJsonNumberField(Payload, TEXT("baseDamage"), 25.0);
    double FireRate = GetJsonNumberField(Payload, TEXT("fireRate"), 600.0);
    double Range = GetJsonNumberField(Payload, TEXT("range"), 10000.0);
    double Spread = GetJsonNumberField(Payload, TEXT("spread"), 2.0);

    // Add only the requested variables so an absent field never clobbers an existing default.
    if (bHasBaseDamage) { AddBlueprintVariableCombat(Blueprint, TEXT("BaseDamage"), MakeFloatPinType()); VarsAdded.Add(TEXT("BaseDamage")); }
    if (bHasFireRate) { AddBlueprintVariableCombat(Blueprint, TEXT("FireRate"), MakeFloatPinType()); VarsAdded.Add(TEXT("FireRate")); }
    if (bHasRange) { AddBlueprintVariableCombat(Blueprint, TEXT("Range"), MakeFloatPinType()); VarsAdded.Add(TEXT("Range")); }
    if (bHasSpread) { AddBlueprintVariableCombat(Blueprint, TEXT("Spread"), MakeFloatPinType()); VarsAdded.Add(TEXT("Spread")); }

    if (bHasBaseDamage || bHasFireRate || bHasRange || bHasSpread)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
        UObject* CDO = BPGC ? BPGC->GetDefaultObject() : nullptr;
        if (bHasBaseDamage)
        {
            if (FDoubleProperty* DamageProp = CDO ? FindFProperty<FDoubleProperty>(BPGC, TEXT("BaseDamage")) : nullptr)
            { DamageProp->SetPropertyValue_InContainer(CDO, BaseDamage); Report.MarkApplied(TEXT("baseDamage")); }
            else Report.MarkFailed(TEXT("baseDamage"), TEXT("BaseDamage property not found after compile"));
        }
        if (bHasFireRate)
        {
            if (FDoubleProperty* RateProp = CDO ? FindFProperty<FDoubleProperty>(BPGC, TEXT("FireRate")) : nullptr)
            { RateProp->SetPropertyValue_InContainer(CDO, FireRate); Report.MarkApplied(TEXT("fireRate")); }
            else Report.MarkFailed(TEXT("fireRate"), TEXT("FireRate property not found after compile"));
        }
        if (bHasRange)
        {
            if (FDoubleProperty* RangeProp = CDO ? FindFProperty<FDoubleProperty>(BPGC, TEXT("Range")) : nullptr)
            { RangeProp->SetPropertyValue_InContainer(CDO, Range); Report.MarkApplied(TEXT("range")); }
            else Report.MarkFailed(TEXT("range"), TEXT("Range property not found after compile"));
        }
        if (bHasSpread)
        {
            if (FDoubleProperty* SpreadProp = CDO ? FindFProperty<FDoubleProperty>(BPGC, TEXT("Spread")) : nullptr)
            { SpreadProp->SetPropertyValue_InContainer(CDO, Spread); Report.MarkApplied(TEXT("spread")); }
            else Report.MarkFailed(TEXT("spread"), TEXT("Spread property not found after compile"));
        }
    }

    if (Report.AnyApplied())
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    if (bHasBaseDamage) Result->SetNumberField(TEXT("baseDamage"), BaseDamage);
    if (bHasFireRate) Result->SetNumberField(TEXT("fireRate"), FireRate);
    if (bHasRange) Result->SetNumberField(TEXT("range"), Range);
    if (bHasSpread) Result->SetNumberField(TEXT("spread"), Spread);
    if (VarsAdded.Num() > 0)
    {
        SetVariablesAdded(Result, VarsAdded);
    }

    TSharedPtr<FJsonObject> IgnoredParams = MakeShared<FJsonObject>();
    if (Payload->HasField(TEXT("criticalMultiplier")))
    {
        IgnoredParams->SetStringField(TEXT("criticalMultiplier"), TEXT("Not consumed by set_weapon_stats; use configure_damage_execution."));
    }
    if (Payload->HasField(TEXT("headshotMultiplier")))
    {
        IgnoredParams->SetStringField(TEXT("headshotMultiplier"), TEXT("Not consumed by set_weapon_stats; use configure_damage_execution."));
    }
    if (IgnoredParams->Values.Num() > 0)
    {
        Result->SetObjectField(TEXT("ignoredParams"), IgnoredParams);
    }

    return SendWriteReportResponse(this, Socket, RequestId, Report, Result,
                                   TEXT("Weapon stats configured."), Blueprint);
#endif // WITH_EDITOR
}

// ============================================================
// 15.2 FIRING MODES
// ============================================================

// configure_hitscan

// configure_projectile

// configure_spread_pattern

// configure_recoil_pattern

// configure_aim_down_sights

// ============================================================
// 15.3 PROJECTILES
// ============================================================

// create_projectile_blueprint
bool UMcpAutomationBridgeSubsystem::HandleCombatCreateProjectileBlueprint(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game"));

    if (Name.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString Error;
    UBlueprint* Blueprint = CreateActorBlueprint(AActor::StaticClass(), Path, Name, Error);
    if (!Blueprint)
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    // Add collision sphere
    USphereComponent* CollisionComp = GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("CollisionComponent"));
    if (CollisionComp)
    {
        double CollisionRadius = GetJsonNumberField(Payload, TEXT("collisionRadius"), 5.0);
        CollisionComp->SetSphereRadius(static_cast<float>(CollisionRadius));
        CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
    }

    FString ProjectileMeshPath = GetJsonStringField(Payload, TEXT("projectileMeshPath"));
    bool bProjectileMeshLoaded = false;

    // Add static mesh for visual
    UStaticMeshComponent* MeshComp = GetOrCreateSCSComponent<UStaticMeshComponent>(Blueprint, TEXT("ProjectileMesh"), TEXT("CollisionComponent"));
    if (MeshComp)
    {
        if (!ProjectileMeshPath.IsEmpty())
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ProjectileMeshPath);
            if (Mesh)
            {
                MeshComp->SetStaticMesh(Mesh);
                bProjectileMeshLoaded = true;
            }
        }
    }

    // Add projectile movement component
    UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
    if (MovementComp)
    {
        double Speed = GetJsonNumberField(Payload, TEXT("projectileSpeed"), 5000.0);
        double GravityScale = GetJsonNumberField(Payload, TEXT("projectileGravityScale"), 0.0);
        
        MovementComp->InitialSpeed = static_cast<float>(Speed);
        MovementComp->MaxSpeed = static_cast<float>(Speed);
        MovementComp->ProjectileGravityScale = static_cast<float>(GravityScale);
    }

    McpSafeCompileBlueprint(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Result->SetStringField(TEXT("projectileMeshPath"), ProjectileMeshPath);
    Result->SetBoolField(TEXT("projectileMeshLoaded"), bProjectileMeshLoaded);
    
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Projectile blueprint created successfully."), Result);
    return true;
#endif // WITH_EDITOR
}

// configure_projectile_movement
bool UMcpAutomationBridgeSubsystem::HandleCombatConfigureProjectileMovement(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    McpHandlerUtils::FMcpWriteReport Report;

    const bool bHasSpeed = Payload->HasField(TEXT("projectileSpeed"));
    const bool bHasGravity = Payload->HasField(TEXT("projectileGravityScale"));
    const bool bHasLifespan = Payload->HasField(TEXT("projectileLifespan"));

    if (bHasSpeed || bHasGravity)
    {
        UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
        if (MovementComp)
        {
            if (bHasSpeed)
            {
                const float Speed = static_cast<float>(GetJsonNumberField(Payload, TEXT("projectileSpeed"), 5000.0));
                MovementComp->InitialSpeed = Speed;
                MovementComp->MaxSpeed = Speed;
                Report.MarkApplied(TEXT("projectileSpeed"));
            }
            if (bHasGravity)
            {
                MovementComp->ProjectileGravityScale = static_cast<float>(GetJsonNumberField(Payload, TEXT("projectileGravityScale"), 0.0));
                Report.MarkApplied(TEXT("projectileGravityScale"));
            }
        }
        else
        {
            if (bHasSpeed) Report.MarkFailed(TEXT("projectileSpeed"), TEXT("could not get or create ProjectileMovement component"));
            if (bHasGravity) Report.MarkFailed(TEXT("projectileGravityScale"), TEXT("could not get or create ProjectileMovement component"));
        }
    }

    if (Report.AnyApplied())
    {
        McpSafeCompileBlueprint(Blueprint);
    }

    // projectileLifespan lands on the projectile actor's InitialLifeSpan (the
    // movement component has no lifespan); set after any compile so it survives.
    if (bHasLifespan)
    {
        AActor* ActorCDO = Blueprint->GeneratedClass
            ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        if (ActorCDO)
        {
            ActorCDO->InitialLifeSpan = static_cast<float>(GetJsonNumberField(Payload, TEXT("projectileLifespan"), 5.0));
            Report.MarkApplied(TEXT("projectileLifespan"));
        }
        else
        {
            Report.MarkFailed(TEXT("projectileLifespan"), TEXT("blueprint has no actor CDO to hold InitialLifeSpan"));
        }
    }

    if (Report.AnyApplied())
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

    return SendWriteReportResponse(this, Socket, RequestId, Report, Result,
                                   TEXT("Projectile movement configured."), Blueprint);
#endif // WITH_EDITOR
}

// configure_projectile_collision
bool UMcpAutomationBridgeSubsystem::HandleCombatConfigureProjectileCollision(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    McpHandlerUtils::FMcpWriteReport Report;

    const bool bHasRadius = Payload->HasField(TEXT("collisionRadius"));
    const bool bHasBounce = Payload->HasField(TEXT("bounceEnabled"));
    const bool bHasBounceRatio = Payload->HasField(TEXT("bounceVelocityRatio"));

    if (bHasRadius)
    {
        USphereComponent* CollisionComp = GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("CollisionComponent"));
        if (CollisionComp)
        {
            CollisionComp->SetSphereRadius(static_cast<float>(GetJsonNumberField(Payload, TEXT("collisionRadius"), 5.0)));
            Report.MarkApplied(TEXT("collisionRadius"));
        }
        else
        {
            Report.MarkFailed(TEXT("collisionRadius"), TEXT("could not get or create CollisionComponent sphere"));
        }
    }

    if (bHasBounce || bHasBounceRatio)
    {
        UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
        if (MovementComp)
        {
            if (bHasBounce)
            {
                MovementComp->bShouldBounce = GetJsonBoolField(Payload, TEXT("bounceEnabled"), false);
                Report.MarkApplied(TEXT("bounceEnabled"));
            }
            if (bHasBounceRatio)
            {
                MovementComp->Bounciness = static_cast<float>(GetJsonNumberField(Payload, TEXT("bounceVelocityRatio"), 0.6));
                Report.MarkApplied(TEXT("bounceVelocityRatio"));
            }
        }
        else
        {
            if (bHasBounce) Report.MarkFailed(TEXT("bounceEnabled"), TEXT("could not get or create ProjectileMovement component"));
            if (bHasBounceRatio) Report.MarkFailed(TEXT("bounceVelocityRatio"), TEXT("could not get or create ProjectileMovement component"));
        }
    }

    if (Report.AnyApplied())
    {
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

    return SendWriteReportResponse(this, Socket, RequestId, Report, Result,
                                   TEXT("Projectile collision configured."), nullptr);
#endif // WITH_EDITOR
}

// configure_projectile_homing
bool UMcpAutomationBridgeSubsystem::HandleCombatConfigureProjectileHoming(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    McpHandlerUtils::FMcpWriteReport Report;

    const bool bHasHoming = Payload->HasField(TEXT("homingEnabled"));
    const bool bHasAccel = Payload->HasField(TEXT("homingAcceleration"));

    if (bHasHoming || bHasAccel)
    {
        UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
        if (MovementComp)
        {
            if (bHasHoming)
            {
                MovementComp->bIsHomingProjectile = GetJsonBoolField(Payload, TEXT("homingEnabled"), true);
                Report.MarkApplied(TEXT("homingEnabled"));
            }
            if (bHasAccel)
            {
                MovementComp->HomingAccelerationMagnitude = static_cast<float>(GetJsonNumberField(Payload, TEXT("homingAcceleration"), 20000.0));
                Report.MarkApplied(TEXT("homingAcceleration"));
            }
        }
        else
        {
            if (bHasHoming) Report.MarkFailed(TEXT("homingEnabled"), TEXT("could not get or create ProjectileMovement component"));
            if (bHasAccel) Report.MarkFailed(TEXT("homingAcceleration"), TEXT("could not get or create ProjectileMovement component"));
        }
    }

    if (Report.AnyApplied())
    {
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

    return SendWriteReportResponse(this, Socket, RequestId, Report, Result,
                                   TEXT("Projectile homing configured."), nullptr);
#endif // WITH_EDITOR
}

// ============================================================
// 15.4 DAMAGE SYSTEM
// ============================================================

// create_damage_type
bool UMcpAutomationBridgeSubsystem::HandleCombatCreateDamageType(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game"));

    if (Name.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString Error;
    UBlueprint* Blueprint = CreateActorBlueprint(UDamageType::StaticClass(), Path, Name, Error);
    if (!Blueprint)
    {
        // Try creating as UObject-based blueprint
        FString FullPath = Path / Name;

        // Validate path before fallback CreatePackage
        if (!IsValidAssetPath(FullPath))
        {
            SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Invalid asset path: '%s'. Path must start with '/', cannot contain '..' or '//'."), *FullPath),
                TEXT("INVALID_PATH"));
            return true;
        }

        // Check if asset already exists
        if (UEditorAssetLibrary::DoesAssetExist(FullPath))
        {
            SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Asset already exists at path: %s"), *FullPath),
                TEXT("ASSET_EXISTS"));
            return true;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(Socket, RequestId, TEXT("Failed to create damage type package."), TEXT("CREATION_FAILED"));
            return true;
        }

        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        Factory->ParentClass = UDamageType::StaticClass();

        Blueprint = Cast<UBlueprint>(
            Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                      RF_Public | RF_Standalone, nullptr, GWarn));
        
        if (!Blueprint)
        {
            SendAutomationError(Socket, RequestId, TEXT("Failed to create damage type blueprint."), TEXT("CREATION_FAILED"));
            return true;
        }

        FAssetRegistryModule::AssetCreated(Blueprint);
        Blueprint->MarkPackageDirty();
    }

    McpSafeCompileBlueprint(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("damageTypePath"), Blueprint->GetPathName());
    
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Damage type created successfully."), Result);
    return true;
#endif // WITH_EDITOR
}

// configure_damage_execution

// setup_hitbox_component
bool UMcpAutomationBridgeSubsystem::HandleCombatSetupHitboxComponent(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    McpHandlerUtils::FMcpWriteReport Report;
    TArray<FString> VarsAdded;

    const bool bHasType = Payload->HasField(TEXT("hitboxType"));
    const bool bHasSize = Payload->HasField(TEXT("hitboxSize"));
    const bool bHasHead = Payload->HasField(TEXT("isDamageZoneHead"));
    const bool bHasMult = Payload->HasField(TEXT("damageMultiplier"));
    const bool bHasBone = Payload->HasField(TEXT("hitboxBoneName"));

    FString HitboxType = GetJsonStringField(Payload, TEXT("hitboxType"), TEXT("Capsule"));
    FString BoneName = GetJsonStringField(Payload, TEXT("hitboxBoneName"), TEXT(""));
    bool bIsDamageZoneHead = GetJsonBoolField(Payload, TEXT("isDamageZoneHead"), false);
    double DamageMultiplier = GetJsonNumberField(Payload, TEXT("damageMultiplier"), 1.0);
    FString ComponentName = Name.IsEmpty()
        ? FString::Printf(TEXT("Hitbox%s"), *HitboxType)
        : Name;
    TSharedPtr<FJsonObject> AppliedHitboxSize = MakeShared<FJsonObject>();

    // Create the collision component (and size it) only when the type or size was
    // requested. An unknown hitboxType fails loudly instead of silently no-op'ing.
    if (bHasType || bHasSize)
    {
        if (HitboxType == TEXT("Capsule"))
        {
            UCapsuleComponent* Hitbox = GetOrCreateSCSComponent<UCapsuleComponent>(Blueprint, ComponentName);
            if (!Hitbox)
            {
                if (bHasType) Report.MarkFailed(TEXT("hitboxType"), TEXT("could not get or create capsule hitbox component"));
                if (bHasSize) Report.MarkFailed(TEXT("hitboxSize"), TEXT("could not get or create capsule hitbox component"));
            }
            else
            {
                if (bHasType) Report.MarkApplied(TEXT("hitboxType"));
                if (bHasSize)
                {
                    const TSharedPtr<FJsonObject>* SizeObj;
                    if (Payload->TryGetObjectField(TEXT("hitboxSize"), SizeObj))
                    {
                        double Radius = GetJsonNumberField(*SizeObj, TEXT("radius"), 34.0);
                        double HalfHeight = GetJsonNumberField(*SizeObj, TEXT("halfHeight"), 88.0);
                        Hitbox->SetCapsuleRadius(static_cast<float>(Radius));
                        Hitbox->SetCapsuleHalfHeight(static_cast<float>(HalfHeight));
                        AppliedHitboxSize->SetNumberField(TEXT("radius"), Radius);
                        AppliedHitboxSize->SetNumberField(TEXT("halfHeight"), HalfHeight);
                        Report.MarkApplied(TEXT("hitboxSize"));
                    }
                    else
                    {
                        Report.MarkFailed(TEXT("hitboxSize"), TEXT("must be an object with radius/halfHeight"));
                    }
                }
            }
        }
        else if (HitboxType == TEXT("Box"))
        {
            UBoxComponent* Hitbox = GetOrCreateSCSComponent<UBoxComponent>(Blueprint, ComponentName);
            if (!Hitbox)
            {
                if (bHasType) Report.MarkFailed(TEXT("hitboxType"), TEXT("could not get or create box hitbox component"));
                if (bHasSize) Report.MarkFailed(TEXT("hitboxSize"), TEXT("could not get or create box hitbox component"));
            }
            else
            {
                if (bHasType) Report.MarkApplied(TEXT("hitboxType"));
                if (bHasSize)
                {
                    const TSharedPtr<FJsonObject>* SizeObj;
                    const TSharedPtr<FJsonObject>* ExtentObj;
                    if (Payload->TryGetObjectField(TEXT("hitboxSize"), SizeObj) &&
                        (*SizeObj)->TryGetObjectField(TEXT("extent"), ExtentObj))
                    {
                        FVector Extent = GetVectorFromJsonCombat(*ExtentObj);
                        Hitbox->SetBoxExtent(Extent);
                        TSharedPtr<FJsonObject> ExtentResult = MakeShared<FJsonObject>();
                        ExtentResult->SetNumberField(TEXT("x"), Extent.X);
                        ExtentResult->SetNumberField(TEXT("y"), Extent.Y);
                        ExtentResult->SetNumberField(TEXT("z"), Extent.Z);
                        AppliedHitboxSize->SetObjectField(TEXT("extent"), ExtentResult);
                        Report.MarkApplied(TEXT("hitboxSize"));
                    }
                    else
                    {
                        Report.MarkFailed(TEXT("hitboxSize"), TEXT("Box hitboxSize must contain an 'extent' object with x/y/z"));
                    }
                }
            }
        }
        else if (HitboxType == TEXT("Sphere"))
        {
            USphereComponent* Hitbox = GetOrCreateSCSComponent<USphereComponent>(Blueprint, ComponentName);
            if (!Hitbox)
            {
                if (bHasType) Report.MarkFailed(TEXT("hitboxType"), TEXT("could not get or create sphere hitbox component"));
                if (bHasSize) Report.MarkFailed(TEXT("hitboxSize"), TEXT("could not get or create sphere hitbox component"));
            }
            else
            {
                if (bHasType) Report.MarkApplied(TEXT("hitboxType"));
                if (bHasSize)
                {
                    const TSharedPtr<FJsonObject>* SizeObj;
                    if (Payload->TryGetObjectField(TEXT("hitboxSize"), SizeObj))
                    {
                        double Radius = GetJsonNumberField(*SizeObj, TEXT("radius"), 50.0);
                        Hitbox->SetSphereRadius(static_cast<float>(Radius));
                        AppliedHitboxSize->SetNumberField(TEXT("radius"), Radius);
                        Report.MarkApplied(TEXT("hitboxSize"));
                    }
                    else
                    {
                        Report.MarkFailed(TEXT("hitboxSize"), TEXT("must be an object with radius"));
                    }
                }
            }
        }
        else
        {
            if (bHasType) Report.MarkFailed(TEXT("hitboxType"), FString::Printf(TEXT("unknown hitbox type '%s'; expected Capsule, Box, or Sphere"), *HitboxType));
            if (bHasSize) Report.MarkFailed(TEXT("hitboxSize"), FString::Printf(TEXT("unknown hitbox type '%s'; cannot create a component to size"), *HitboxType));
        }
    }

    // Add only the requested metadata variables so an absent field never clobbers an existing default.
    if (bHasHead) { AddBlueprintVariableCombat(Blueprint, TEXT("bIsHeadshotZone"), MakeBoolPinType()); VarsAdded.Add(TEXT("bIsHeadshotZone")); }
    if (bHasMult) { AddBlueprintVariableCombat(Blueprint, TEXT("HitboxDamageMultiplier"), MakeFloatPinType()); VarsAdded.Add(TEXT("HitboxDamageMultiplier")); }
    if (bHasBone) { AddBlueprintVariableCombat(Blueprint, TEXT("HitboxBoneName"), MakeNamePinType()); VarsAdded.Add(TEXT("HitboxBoneName")); }

    const bool bNeedsCompile = Report.AnyApplied() || bHasHead || bHasMult || bHasBone;
    if (bNeedsCompile)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
    }

    if (bHasHead || bHasMult || bHasBone)
    {
        UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
        UObject* CDO = BPGC ? BPGC->GetDefaultObject() : nullptr;
        if (bHasHead)
        {
            if (FBoolProperty* HeadProp = CDO ? FindFProperty<FBoolProperty>(BPGC, TEXT("bIsHeadshotZone")) : nullptr)
            { HeadProp->SetPropertyValue_InContainer(CDO, bIsDamageZoneHead); Report.MarkApplied(TEXT("isDamageZoneHead")); }
            else Report.MarkFailed(TEXT("isDamageZoneHead"), TEXT("bIsHeadshotZone property not found after compile"));
        }
        if (bHasMult)
        {
            if (FDoubleProperty* MultProp = CDO ? FindFProperty<FDoubleProperty>(BPGC, TEXT("HitboxDamageMultiplier")) : nullptr)
            { MultProp->SetPropertyValue_InContainer(CDO, DamageMultiplier); Report.MarkApplied(TEXT("damageMultiplier")); }
            else Report.MarkFailed(TEXT("damageMultiplier"), TEXT("HitboxDamageMultiplier property not found after compile"));
        }
        if (bHasBone)
        {
            if (FNameProperty* BoneProp = CDO ? FindFProperty<FNameProperty>(BPGC, TEXT("HitboxBoneName")) : nullptr)
            { BoneProp->SetPropertyValue_InContainer(CDO, FName(*BoneName)); Report.MarkApplied(TEXT("hitboxBoneName")); }
            else Report.MarkFailed(TEXT("hitboxBoneName"), TEXT("HitboxBoneName property not found after compile"));
        }
    }

    if (Report.AnyApplied())
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Result->SetStringField(TEXT("hitboxType"), HitboxType);
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetObjectField(TEXT("hitboxSize"), AppliedHitboxSize);
    if (bHasBone) Result->SetStringField(TEXT("hitboxBoneName"), BoneName);
    if (bHasHead) Result->SetBoolField(TEXT("isDamageZoneHead"), bIsDamageZoneHead);
    if (bHasMult) Result->SetNumberField(TEXT("damageMultiplier"), DamageMultiplier);
    if (VarsAdded.Num() > 0)
    {
        SetVariablesAdded(Result, VarsAdded);
    }

    return SendWriteReportResponse(this, Socket, RequestId, Report, Result,
                                   TEXT("Hitbox component configured."), Blueprint);
#endif // WITH_EDITOR
}

// ============================================================
// 15.5 WEAPON FEATURES
// ============================================================

// setup_reload_system

// setup_ammo_system

// setup_attachment_system
bool UMcpAutomationBridgeSubsystem::HandleCombatSetupAttachmentSystem(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    // Parse attachment slots and create actual SceneComponent attach points
    const TArray<TSharedPtr<FJsonValue>>* AttachmentSlotsArray;
    TArray<FString> SlotNames;
    TArray<FString> CreatedComponents;
    
    if (Payload->TryGetArrayField(TEXT("attachmentSlots"), AttachmentSlotsArray))
    {
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (SCS)
        {
            for (const auto& SlotValue : *AttachmentSlotsArray)
            {
                if (SlotValue->Type == EJson::Object)
                {
                    auto SlotObj = SlotValue->AsObject();
                    FString SlotName = GetJsonStringField(SlotObj, TEXT("slotName"));
                    FString SlotType = GetJsonStringField(SlotObj, TEXT("slotType"), TEXT("Optic"));
                    
                    if (!SlotName.IsEmpty())
                    {
                        SlotNames.Add(SlotName);
                        
                        // Create actual SceneComponent as attachment point
                        FString ComponentName = FString::Printf(TEXT("AttachPoint_%s"), *SlotName);
                        USceneComponent* AttachPoint = GetOrCreateSCSComponent<USceneComponent>(Blueprint, ComponentName, TEXT("WeaponMesh"));
                        if (AttachPoint)
                        {
                            CreatedComponents.Add(ComponentName);
                        }
                    }
                }
                else if (SlotValue->Type == EJson::String)
                {
                    // Simple string slot name
                    FString SlotName = SlotValue->AsString();
                    if (!SlotName.IsEmpty())
                    {
                        SlotNames.Add(SlotName);
                        
                        FString ComponentName = FString::Printf(TEXT("AttachPoint_%s"), *SlotName);
                        USceneComponent* AttachPoint = GetOrCreateSCSComponent<USceneComponent>(Blueprint, ComponentName, TEXT("WeaponMesh"));
                        if (AttachPoint)
                        {
                            CreatedComponents.Add(ComponentName);
                        }
                    }
                }
            }
        }
    }

    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, /*bSave=*/true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    
    TArray<TSharedPtr<FJsonValue>> SlotsJsonArray;
    for (const FString& Slot : SlotNames)
    {
        SlotsJsonArray.Add(MakeShared<FJsonValueString>(Slot));
    }
    Result->SetArrayField(TEXT("attachmentSlots"), SlotsJsonArray);
    
    TArray<TSharedPtr<FJsonValue>> ComponentsJsonArray;
    for (const FString& Comp : CreatedComponents)
    {
        ComponentsJsonArray.Add(MakeShared<FJsonValueString>(Comp));
    }
    Result->SetArrayField(TEXT("componentsCreated"), ComponentsJsonArray);
    
    SendAutomationResponse(Socket, RequestId, true, TEXT("Attachment system configured with SceneComponent attach points."), Result);
    return true;
#endif // WITH_EDITOR
}

// setup_weapon_switching

// ============================================================
// 15.6 EFFECTS
// ============================================================

// configure_muzzle_flash

// configure_tracer

// configure_impact_effects

// configure_shell_ejection

// ============================================================
// 15.7 MELEE COMBAT
// ============================================================

// create_melee_trace
bool UMcpAutomationBridgeSubsystem::HandleCombatCreateMeleeTrace(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    FString TraceStartSocket = GetJsonStringField(Payload, TEXT("meleeTraceStartSocket"), TEXT("WeaponBase"));
    FString TraceEndSocket = GetJsonStringField(Payload, TEXT("meleeTraceEndSocket"), TEXT("WeaponTip"));
    double TraceRadius = GetJsonNumberField(Payload, TEXT("meleeTraceRadius"), 10.0);

    // Add variables
    AddBlueprintVariableCombat(Blueprint, TEXT("MeleeTraceStartSocket"), MakeNamePinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("MeleeTraceEndSocket"), MakeNamePinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("MeleeTraceRadius"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("bIsTracing"), MakeBoolPinType());

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);

    // Set values
    if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
    {
        if (UObject* CDO = BPGC->GetDefaultObject())
        {
            if (FNameProperty* StartProp = FindFProperty<FNameProperty>(BPGC, TEXT("MeleeTraceStartSocket")))
            {
                StartProp->SetPropertyValue_InContainer(CDO, FName(*TraceStartSocket));
            }
            if (FNameProperty* EndProp = FindFProperty<FNameProperty>(BPGC, TEXT("MeleeTraceEndSocket")))
            {
                EndProp->SetPropertyValue_InContainer(CDO, FName(*TraceEndSocket));
            }
            if (FDoubleProperty* RadiusProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MeleeTraceRadius")))
            {
                RadiusProp->SetPropertyValue_InContainer(CDO, TraceRadius);
            }
            if (FBoolProperty* TracingProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsTracing")))
            {
                TracingProp->SetPropertyValue_InContainer(CDO, false);
            }
        }
    }

    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Result->SetStringField(TEXT("traceStartSocket"), TraceStartSocket);
    Result->SetStringField(TEXT("traceEndSocket"), TraceEndSocket);
    Result->SetNumberField(TEXT("traceRadius"), TraceRadius);
    SetVariablesAdded(Result, {TEXT("MeleeTraceStartSocket"), TEXT("MeleeTraceEndSocket"), TEXT("MeleeTraceRadius"), TEXT("bIsTracing")});
    Result->SetBoolField(TEXT("scaffoldOnly"), true);

    SendAutomationResponse(Socket, RequestId, true, TEXT("Melee trace variables scaffolded; game code must perform the trace."), Result);
    return true;
#endif // WITH_EDITOR
}

// configure_combo_system

// create_hit_pause (hitstop)
bool UMcpAutomationBridgeSubsystem::HandleCombatCreateHitPause(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    double HitPauseDuration = GetJsonNumberField(Payload, TEXT("hitPauseDuration"), 0.05);
    double TimeDilation = GetJsonNumberField(Payload, TEXT("hitPauseTimeDilation"), 0.1);

    // Add variables
    AddBlueprintVariableCombat(Blueprint, TEXT("HitPauseDuration"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("HitPauseTimeDilation"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("bEnableHitPause"), MakeBoolPinType());

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);

    // Set values
    if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
    {
        if (UObject* CDO = BPGC->GetDefaultObject())
        {
            if (FDoubleProperty* DurationProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitPauseDuration")))
            {
                DurationProp->SetPropertyValue_InContainer(CDO, HitPauseDuration);
            }
            if (FDoubleProperty* DilationProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitPauseTimeDilation")))
            {
                DilationProp->SetPropertyValue_InContainer(CDO, TimeDilation);
            }
            if (FBoolProperty* EnableProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bEnableHitPause")))
            {
                EnableProp->SetPropertyValue_InContainer(CDO, true);
            }
        }
    }

    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Result->SetNumberField(TEXT("hitPauseDuration"), HitPauseDuration);
    Result->SetNumberField(TEXT("timeDilation"), TimeDilation);
    SetVariablesAdded(Result, {TEXT("HitPauseDuration"), TEXT("HitPauseTimeDilation"), TEXT("bEnableHitPause")});
    Result->SetBoolField(TEXT("scaffoldOnly"), true);

    SendAutomationResponse(Socket, RequestId, true, TEXT("Hit pause variables scaffolded; game code must apply the time dilation."), Result);
    return true;
#endif // WITH_EDITOR
}

// configure_hit_reaction

// setup_parry_block_system

// configure_weapon_trails

// ============================================================
// UTILITY
// ============================================================

// get_info
bool UMcpAutomationBridgeSubsystem::HandleCombatGetInfo(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    TSharedPtr<FJsonObject> Info = McpHandlerUtils::CreateResultObject();
    Info->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Info->SetStringField(TEXT("parentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("Unknown"));
    
    // Check for components
    bool bHasWeaponMesh = false;
    bool bHasProjectileMovement = false;
    bool bHasCollision = false;
    TArray<TSharedPtr<FJsonValue>> ComponentList;
    
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate)
            {
                ComponentList.Add(MakeShared<FJsonValueString>(Node->GetVariableName().ToString()));
                
                if (Node->ComponentTemplate->IsA<UStaticMeshComponent>() ||
                    Node->ComponentTemplate->IsA<USkeletalMeshComponent>())
                {
                    bHasWeaponMesh = true;
                }
                if (Node->ComponentTemplate->IsA<UProjectileMovementComponent>())
                {
                    bHasProjectileMovement = true;
                }
                if (Node->ComponentTemplate->IsA<USphereComponent>() ||
                    Node->ComponentTemplate->IsA<UCapsuleComponent>() ||
                    Node->ComponentTemplate->IsA<UBoxComponent>())
                {
                    bHasCollision = true;
                }
            }
        }
    }

    Info->SetBoolField(TEXT("hasWeaponMesh"), bHasWeaponMesh);
    Info->SetBoolField(TEXT("hasProjectileMovement"), bHasProjectileMovement);
    Info->SetBoolField(TEXT("hasCollision"), bHasCollision);
    Info->SetArrayField(TEXT("components"), ComponentList);
    
    // List Blueprint variables
    TArray<TSharedPtr<FJsonValue>> VariableList;
    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        VariableList.Add(MakeShared<FJsonValueString>(Var.VarName.ToString()));
    }
    Info->SetArrayField(TEXT("variables"), VariableList);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetObjectField(TEXT("combatInfo"), Info);
    
    SendAutomationResponse(Socket, RequestId, true, TEXT("Combat info retrieved."), Result);
    return true;
#endif // WITH_EDITOR
}

// ============================================================
// ALIASES
// ============================================================

// configure_hit_detection -> alias for setup_hitbox_component
bool UMcpAutomationBridgeSubsystem::HandleCombatConfigureHitDetection(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    McpHandlerUtils::FMcpWriteReport Report;
    TArray<FString> VarsAdded;

    const bool bHasType = Payload->HasField(TEXT("hitboxType"));
    const bool bHasMult = Payload->HasField(TEXT("damageMultiplier"));

    FString HitboxType = GetJsonStringField(Payload, TEXT("hitboxType"), TEXT("Capsule"));
    double DamageMultiplier = GetJsonNumberField(Payload, TEXT("damageMultiplier"), 1.0);

    // Create the collision component only when a type was requested; an unknown
    // type fails loudly instead of silently defaulting to a sphere.
    if (bHasType)
    {
        if (HitboxType == TEXT("Capsule"))
        {
            if (GetOrCreateSCSComponent<UCapsuleComponent>(Blueprint, TEXT("HitboxCapsule"))) Report.MarkApplied(TEXT("hitboxType"));
            else Report.MarkFailed(TEXT("hitboxType"), TEXT("could not get or create capsule hitbox component"));
        }
        else if (HitboxType == TEXT("Box"))
        {
            if (GetOrCreateSCSComponent<UBoxComponent>(Blueprint, TEXT("HitboxBox"))) Report.MarkApplied(TEXT("hitboxType"));
            else Report.MarkFailed(TEXT("hitboxType"), TEXT("could not get or create box hitbox component"));
        }
        else if (HitboxType == TEXT("Sphere"))
        {
            if (GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("HitboxSphere"))) Report.MarkApplied(TEXT("hitboxType"));
            else Report.MarkFailed(TEXT("hitboxType"), TEXT("could not get or create sphere hitbox component"));
        }
        else
        {
            Report.MarkFailed(TEXT("hitboxType"), FString::Printf(TEXT("unknown hitbox type '%s'; expected Capsule, Box, or Sphere"), *HitboxType));
        }
    }

    if (bHasMult)
    {
        AddBlueprintVariableCombat(Blueprint, TEXT("HitboxDamageMultiplier"), MakeFloatPinType());
        VarsAdded.Add(TEXT("HitboxDamageMultiplier"));
    }

    const bool bNeedsCompile = Report.AnyApplied() || bHasMult;
    if (bNeedsCompile)
    {
        McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, /*bSave=*/false);
    }

    if (bHasMult)
    {
        UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
        UObject* CDO = BPGC ? BPGC->GetDefaultObject() : nullptr;
        if (FDoubleProperty* MultProp = CDO ? FindFProperty<FDoubleProperty>(BPGC, TEXT("HitboxDamageMultiplier")) : nullptr)
        { MultProp->SetPropertyValue_InContainer(CDO, DamageMultiplier); Report.MarkApplied(TEXT("damageMultiplier")); }
        else Report.MarkFailed(TEXT("damageMultiplier"), TEXT("HitboxDamageMultiplier property not found after compile"));
    }

    if (Report.AnyApplied())
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    if (bHasType) Result->SetStringField(TEXT("hitboxType"), HitboxType);
    if (bHasMult) Result->SetNumberField(TEXT("damageMultiplier"), DamageMultiplier);
    if (VarsAdded.Num() > 0)
    {
        SetVariablesAdded(Result, VarsAdded);
    }
    return SendWriteReportResponse(this, Socket, RequestId, Report, Result,
                                   TEXT("Hit detection configured."), nullptr);
#endif // WITH_EDITOR
}

// get_stats -> alias for get_info
bool UMcpAutomationBridgeSubsystem::HandleCombatGetStats(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    TSharedPtr<FJsonObject> Info = McpHandlerUtils::CreateResultObject();
    Info->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Info->SetStringField(TEXT("parentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("Unknown"));

    TArray<TSharedPtr<FJsonValue>> VariableList;
    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        VariableList.Add(MakeShared<FJsonValueString>(Var.VarName.ToString()));
    }
    Info->SetArrayField(TEXT("variables"), VariableList);
    AddVariableValueReadbackCombat(Info, Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetObjectField(TEXT("combatInfo"), Info);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Combat stats retrieved."), Result);
    return true;
#endif // WITH_EDITOR
}

// ============================================================
// NEW SUB-ACTIONS
// ============================================================

// create_damage_effect - creates a blueprint with damage effect variables
bool UMcpAutomationBridgeSubsystem::HandleCombatCreateDamageEffect(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game"));

    if (Name.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString Error;
    UBlueprint* Blueprint = CreateActorBlueprint(AActor::StaticClass(), Path, Name, Error);
    if (!Blueprint)
    {
        SendAutomationError(Socket, RequestId, Error.IsEmpty() ? TEXT("Failed to create damage effect.") : Error, TEXT("CREATION_FAILED"));
        return true;
    }

    double Duration = GetJsonNumberField(Payload, TEXT("duration"), 5.0);
    double DamagePerSecond = GetJsonNumberField(Payload, TEXT("damagePerSecond"), 10.0);
    FString EffectType = GetJsonStringField(Payload, TEXT("effectType"), TEXT("DamageOverTime"));

    AddBlueprintVariableCombat(Blueprint, TEXT("EffectDuration"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("DamagePerSecond"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("EffectType"), MakeStringPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("bIsActive"), MakeBoolPinType());

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);

    if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
    {
        if (UObject* CDO = BPGC->GetDefaultObject())
        {
            if (FDoubleProperty* DurProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("EffectDuration")))
                DurProp->SetPropertyValue_InContainer(CDO, Duration);
            if (FDoubleProperty* DpsProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("DamagePerSecond")))
                DpsProp->SetPropertyValue_InContainer(CDO, DamagePerSecond);
            if (FStrProperty* TypeProp = FindFProperty<FStrProperty>(BPGC, TEXT("EffectType")))
                TypeProp->SetPropertyValue_InContainer(CDO, EffectType);
        }
    }

    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Result->SetNumberField(TEXT("duration"), Duration);
    Result->SetNumberField(TEXT("damagePerSecond"), DamagePerSecond);
    Result->SetStringField(TEXT("effectType"), EffectType);
    SetVariablesAdded(Result, {TEXT("EffectDuration"), TEXT("DamagePerSecond"), TEXT("EffectType"), TEXT("bIsActive")});
    Result->SetBoolField(TEXT("scaffoldOnly"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Damage effect blueprint created with scaffold variables; game code must implement the damage-over-time logic."), Result);
    return true;
#endif // WITH_EDITOR
}

// apply_damage - adds damage application variables to a blueprint
bool UMcpAutomationBridgeSubsystem::HandleCombatApplyDamage(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    double DamageAmount = GetJsonNumberField(Payload, TEXT("damageAmount"), 25.0);
    FString DamageTypeName = GetJsonStringField(Payload, TEXT("damageType"), TEXT("Default"));

    AddBlueprintVariableCombat(Blueprint, TEXT("AppliedDamageAmount"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("AppliedDamageType"), MakeStringPinType());

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);

    if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
    {
        if (UObject* CDO = BPGC->GetDefaultObject())
        {
            if (FDoubleProperty* AmtProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("AppliedDamageAmount")))
                AmtProp->SetPropertyValue_InContainer(CDO, DamageAmount);
            if (FStrProperty* TypeProp = FindFProperty<FStrProperty>(BPGC, TEXT("AppliedDamageType")))
                TypeProp->SetPropertyValue_InContainer(CDO, DamageTypeName);
        }
    }

    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Result->SetNumberField(TEXT("damageAmount"), DamageAmount);
    Result->SetStringField(TEXT("damageType"), DamageTypeName);
    SetVariablesAdded(Result, {TEXT("AppliedDamageAmount"), TEXT("AppliedDamageType")});
    Result->SetBoolField(TEXT("scaffoldOnly"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Damage variables scaffolded; no damage was applied at runtime."), Result);
    return true;
#endif // WITH_EDITOR
}

// heal - adds healing variables to a blueprint
bool UMcpAutomationBridgeSubsystem::HandleCombatHeal(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    double HealAmount = GetJsonNumberField(Payload, TEXT("healAmount"), 25.0);
    double MaxHealth = GetJsonNumberField(Payload, TEXT("maxHealth"), 100.0);

    AddBlueprintVariableCombat(Blueprint, TEXT("CurrentHealth"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("MaxHealth"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("HealAmount"), MakeFloatPinType());

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);

    if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
    {
        if (UObject* CDO = BPGC->GetDefaultObject())
        {
            if (FDoubleProperty* HealthProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("CurrentHealth")))
                HealthProp->SetPropertyValue_InContainer(CDO, MaxHealth);
            if (FDoubleProperty* MaxProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MaxHealth")))
                MaxProp->SetPropertyValue_InContainer(CDO, MaxHealth);
            if (FDoubleProperty* HealProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HealAmount")))
                HealProp->SetPropertyValue_InContainer(CDO, HealAmount);
        }
    }

    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Result->SetNumberField(TEXT("healAmount"), HealAmount);
    Result->SetNumberField(TEXT("maxHealth"), MaxHealth);
    SetVariablesAdded(Result, {TEXT("CurrentHealth"), TEXT("MaxHealth"), TEXT("HealAmount")});
    Result->SetBoolField(TEXT("scaffoldOnly"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Healing variables scaffolded; no healing was applied at runtime."), Result);
    return true;
#endif // WITH_EDITOR
}

// create_shield - adds shield/barrier variables to a blueprint
bool UMcpAutomationBridgeSubsystem::HandleCombatCreateShield(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    double ShieldAmount = GetJsonNumberField(Payload, TEXT("shieldAmount"), 50.0);
    double MaxShield = GetJsonNumberField(Payload, TEXT("maxShield"), 100.0);
    double ShieldRegenRate = GetJsonNumberField(Payload, TEXT("shieldRegenRate"), 5.0);
    double ShieldRegenDelay = GetJsonNumberField(Payload, TEXT("shieldRegenDelay"), 3.0);

    AddBlueprintVariableCombat(Blueprint, TEXT("CurrentShield"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("MaxShield"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("ShieldRegenRate"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("ShieldRegenDelay"), MakeFloatPinType());
    AddBlueprintVariableCombat(Blueprint, TEXT("bShieldActive"), MakeBoolPinType());

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);

    if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
    {
        if (UObject* CDO = BPGC->GetDefaultObject())
        {
            if (FDoubleProperty* CurProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("CurrentShield")))
                CurProp->SetPropertyValue_InContainer(CDO, ShieldAmount);
            if (FDoubleProperty* MaxProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MaxShield")))
                MaxProp->SetPropertyValue_InContainer(CDO, MaxShield);
            if (FDoubleProperty* RegenProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ShieldRegenRate")))
                RegenProp->SetPropertyValue_InContainer(CDO, ShieldRegenRate);
            if (FDoubleProperty* DelayProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ShieldRegenDelay")))
                DelayProp->SetPropertyValue_InContainer(CDO, ShieldRegenDelay);
            if (FBoolProperty* ActiveProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bShieldActive")))
                ActiveProp->SetPropertyValue_InContainer(CDO, true);
        }
    }

    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    Result->SetNumberField(TEXT("shieldAmount"), ShieldAmount);
    Result->SetNumberField(TEXT("maxShield"), MaxShield);
    Result->SetNumberField(TEXT("shieldRegenRate"), ShieldRegenRate);
    Result->SetNumberField(TEXT("shieldRegenDelay"), ShieldRegenDelay);
    SetVariablesAdded(Result, {TEXT("CurrentShield"), TEXT("MaxShield"), TEXT("ShieldRegenRate"), TEXT("ShieldRegenDelay"), TEXT("bShieldActive")});

    TSharedPtr<FJsonObject> AppliedDefaults = MakeShared<FJsonObject>();
    AppliedDefaults->SetNumberField(TEXT("CurrentShield"), ShieldAmount);
    AppliedDefaults->SetNumberField(TEXT("MaxShield"), MaxShield);
    AppliedDefaults->SetNumberField(TEXT("ShieldRegenRate"), ShieldRegenRate);
    AppliedDefaults->SetNumberField(TEXT("ShieldRegenDelay"), ShieldRegenDelay);
    AppliedDefaults->SetBoolField(TEXT("bShieldActive"), true);
    Result->SetObjectField(TEXT("appliedDefaults"), AppliedDefaults);

    Result->SetBoolField(TEXT("scaffoldOnly"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Shield variables scaffolded (shieldAmount stored as CurrentShield); game code must implement absorption and regen."), Result);
    return true;
#endif // WITH_EDITOR
}

// modify_armor - adds armor/damage reduction variables to a blueprint
bool UMcpAutomationBridgeSubsystem::HandleCombatModifyArmor(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = ResolveBlueprintOrError(BlueprintPath, RequestId, Socket);
    if (!Blueprint) return true;

    McpHandlerUtils::FMcpWriteReport Report;
    TArray<FString> VarsAdded;

    const bool bHasArmor = Payload->HasField(TEXT("armorValue"));
    const bool bHasReduction = Payload->HasField(TEXT("damageReduction"));

    double ArmorValue = GetJsonNumberField(Payload, TEXT("armorValue"), 50.0);
    double DamageReduction = GetJsonNumberField(Payload, TEXT("damageReduction"), 0.25);

    // Add only the requested variables so an absent field never clobbers an existing default.
    if (bHasArmor) { AddBlueprintVariableCombat(Blueprint, TEXT("ArmorValue"), MakeFloatPinType()); VarsAdded.Add(TEXT("ArmorValue")); }
    if (bHasReduction) { AddBlueprintVariableCombat(Blueprint, TEXT("ArmorDamageReduction"), MakeFloatPinType()); VarsAdded.Add(TEXT("ArmorDamageReduction")); }

    if (bHasArmor || bHasReduction)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
        UObject* CDO = BPGC ? BPGC->GetDefaultObject() : nullptr;
        if (bHasArmor)
        {
            if (FDoubleProperty* ArmorProp = CDO ? FindFProperty<FDoubleProperty>(BPGC, TEXT("ArmorValue")) : nullptr)
            { ArmorProp->SetPropertyValue_InContainer(CDO, ArmorValue); Report.MarkApplied(TEXT("armorValue")); }
            else Report.MarkFailed(TEXT("armorValue"), TEXT("ArmorValue property not found after compile"));
        }
        if (bHasReduction)
        {
            if (FDoubleProperty* RedProp = CDO ? FindFProperty<FDoubleProperty>(BPGC, TEXT("ArmorDamageReduction")) : nullptr)
            { RedProp->SetPropertyValue_InContainer(CDO, DamageReduction); Report.MarkApplied(TEXT("damageReduction")); }
            else Report.MarkFailed(TEXT("damageReduction"), TEXT("ArmorDamageReduction property not found after compile"));
        }
    }

    if (Report.AnyApplied())
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    if (bHasArmor) Result->SetNumberField(TEXT("armorValue"), ArmorValue);
    if (bHasReduction) Result->SetNumberField(TEXT("damageReduction"), DamageReduction);
    if (VarsAdded.Num() > 0)
    {
        SetVariablesAdded(Result, VarsAdded);
    }
    Result->SetBoolField(TEXT("scaffoldOnly"), true);
    return SendWriteReportResponse(this, Socket, RequestId, Report, Result,
                                   TEXT("Armor variables scaffolded; game code must apply the damage reduction."), nullptr);
#endif // WITH_EDITOR
}

#undef GetJsonStringField
#undef GetJsonNumberField
#undef GetJsonBoolField
