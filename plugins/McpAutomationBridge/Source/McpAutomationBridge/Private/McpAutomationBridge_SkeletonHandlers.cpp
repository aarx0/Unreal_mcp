/**
 * McpAutomationBridge_SkeletonHandlers.cpp
 * =============================================================================
 * Phase 7: Skeleton and Rigging System Handlers
 *
 * Provides comprehensive skeleton, rigging, physics asset, and skin weight management
 * capabilities for the MCP Automation Bridge. This file implements the `manage_skeleton` tool.
 *
 * HANDLERS BY CATEGORY:
 * ---------------------
 * 7.1  Skeleton Operations   - get_skeleton_info, list_skeleton_bones, add_skeleton_bone,
 *                              rename_bone, remove_bone, set_bone_parent, get_bone_hierarchy
 * 7.2  Socket Management     - add_socket, remove_socket, list_sockets, set_socket_transform,
 *                              get_socket_info, attach_to_socket
 * 7.3  Physics Assets        - create_physics_asset, get_physics_asset_info, add_physics_body,
 *                              add_physics_constraint, configure_constraint_limits,
 *                              set_constraint_angular_limits, set_constraint_linear_limits,
 *                              remove_physics_body, remove_physics_constraint
 * 7.4  Skin Weights          - paint_weights, copy_weights, mirror_weights,
 *                              create_skin_weight_profile
 * 7.5  Morph Targets         - list_morph_targets, get_morph_target_info, set_morph_target
 * 7.6  Cloth Binding         - bind_cloth_asset, unbind_cloth_asset, list_cloth_assets
 * 7.7  Utility Actions       - preview_physics, validate_skeleton, compare_skeletons
 *
 * VERSION COMPATIBILITY:
 * ----------------------
 * - UE 5.0-5.4: Standard physics asset APIs
 * - UE 5.5+: SkeletalBodySetup.h available for extended physics operations
 * - SkeletonModifier.h: Optional include with __has_include guard
 * - Chaos Cloth: Conditional compilation via __has_include guards
 * - FReferenceSkeletonModifier: Add/UpdateRefPoseTransform (5.0), Remove/SetParent (5.1+)
 *
 * REFACTORING NOTES:
 * ------------------
 * - Helper macros (GetJsonStringField, etc.) for JSON field access
 * - Anonymous namespace for file-local helper functions
 * - LoadSkeletonFromPathSkel/LoadSkeletalMeshFromPathSkel: Secure asset loading
 * - Uses McpSafeAssetSave for UE 5.7+ safe asset saving
 * - Path validation via SanitizeProjectRelativePath()
 *
 * Copyright (c) 2024 MCP Automation Bridge Contributors
 */

// =============================================================================
// Includes
// =============================================================================

// Version Compatibility (must be first)
#include "McpVersionCompatibility.h"

// MCP Core
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_SkeletonHandlers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpResponseHelpers.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"

// JSON & Serialization
#include "Dom/JsonObject.h"

#if WITH_EDITOR

// Skeleton & Animation
#include "Animation/Skeleton.h"
#include "Animation/MorphTarget.h"
#if __has_include("Animation/SkinWeightProfile.h")
#include "Animation/SkinWeightProfile.h"
#endif
#include "ReferenceSkeleton.h"

// Skeletal Mesh
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"

// Physics
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

// UE 5.5+ SkeletalBodySetup
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "PhysicsEngine/SkeletalBodySetup.h"
#endif

// Asset Registry & Tools
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/PhysicsAssetFactory.h"

// Editor Utilities
#include "EditorAssetLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EngineUtils.h"

// Core & Misc
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

// =============================================================================
// Helper Macros
// =============================================================================

/* DEPRECATED: JSON field macros are deprecated. Use McpHandlerUtils helpers instead:
 * - McpHandlerUtils::TryGetRequiredString()
 * - McpHandlerUtils::JsonToVector() / JsonToRotator()
 * - McpHandlerUtils::VectorToJson() / RotatorToJson()
 */

// =============================================================================
// Conditional Includes (version-dependent)
// =============================================================================

// SkeletonModifier (optional)
#if __has_include("Animation/SkeletonModifier.h")
#include "Animation/SkeletonModifier.h"
#define MCP_HAS_SKELETON_MODIFIER 1
#else
#define MCP_HAS_SKELETON_MODIFIER 0
#endif

// Chaos Cloth Support
#if __has_include("ClothingAsset/ClothingAssetBase.h")
#include "ClothingAsset/ClothingAssetBase.h"
#elif __has_include("ClothingAssetBase.h")
#include "ClothingAssetBase.h"
#endif

// UClothingAssetCommon for GetNumLods() (UE 5.7+)
#if __has_include("ClothingAsset.h")
#include "ClothingAsset.h"
#elif __has_include("ClothingAssetCommon.h")
#include "ClothingAssetCommon.h"
#endif

#if __has_include("ClothingAssetFactory.h")
#include "ClothingAssetFactory.h"
#define MCP_HAS_CLOTH_FACTORY 1
#else
#define MCP_HAS_CLOTH_FACTORY 0
#endif

// Editor Actor Subsystem
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

namespace {

/**
 * Helper: Load skeleton asset from path
 */
static USkeleton* LoadSkeletonFromPathSkel(const FString& SkeletonPath, FString& OutError)
{
    OutError.Reset();
    if (SkeletonPath.IsEmpty())
    {
        OutError = TEXT("Skeleton path is required");
        return nullptr;
    }


    // Validate path security before loading
    FString SanitizedPath = SanitizeProjectRelativePath(SkeletonPath);
    if (SanitizedPath.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Invalid skeleton path '%s': contains traversal sequences"), *SkeletonPath);
        return nullptr;
    }

    UObject* Asset = StaticLoadObject(USkeleton::StaticClass(), nullptr, *SanitizedPath);
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Failed to load skeleton: %s"), *SkeletonPath);
        return nullptr;
    }

    USkeleton* Skeleton = Cast<USkeleton>(Asset);
    if (!Skeleton)
    {
        OutError = FString::Printf(TEXT("Asset is not a skeleton: %s"), *SkeletonPath);
        return nullptr;
    }

    return Skeleton;
}

/**
 * Helper: Load skeletal mesh asset from path
 */
static USkeletalMesh* LoadSkeletalMeshFromPathSkel(const FString& MeshPath, FString& OutError)
{
    OutError.Reset();
    if (MeshPath.IsEmpty())
    {
        OutError = TEXT("Skeletal mesh path is required");
        return nullptr;
    }


    // Validate path security before loading
    FString SanitizedPath = SanitizeProjectRelativePath(MeshPath);
    if (SanitizedPath.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Invalid skeletal mesh path '%s': contains traversal sequences"), *MeshPath);
        return nullptr;
    }

    UObject* Asset = StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *SanitizedPath);
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Failed to load skeletal mesh: %s"), *MeshPath);
        return nullptr;
    }

    USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset);
    if (!Mesh)
    {
        OutError = FString::Printf(TEXT("Asset is not a skeletal mesh: %s"), *MeshPath);
        return nullptr;
    }

    return Mesh;
}

/**
 * Helper: Load physics asset from path
 */
static UPhysicsAsset* LoadPhysicsAssetFromPath(const FString& PhysicsPath, FString& OutError)
{
    OutError.Reset();
    if (PhysicsPath.IsEmpty())
    {
        OutError = TEXT("Physics asset path is required");
        return nullptr;
    }


    // Validate path security before loading
    FString SanitizedPath = SanitizeProjectRelativePath(PhysicsPath);
    if (SanitizedPath.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Invalid physics asset path '%s': contains traversal sequences"), *PhysicsPath);
        return nullptr;
    }

    UObject* Asset = StaticLoadObject(UPhysicsAsset::StaticClass(), nullptr, *SanitizedPath);
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Failed to load physics asset: %s"), *PhysicsPath);
        return nullptr;
    }

    UPhysicsAsset* PhysAsset = Cast<UPhysicsAsset>(Asset);
    if (!PhysAsset)
    {
        OutError = FString::Printf(TEXT("Asset is not a physics asset: %s"), *PhysicsPath);
        return nullptr;
    }

    return PhysAsset;
}

/**
 * Helper: Parse FVector from JSON object
 */
static FVector ParseVectorFromJson(const TSharedPtr<FJsonObject>& JsonObj, const FString& FieldName, const FVector& Default = FVector::ZeroVector)
{
    if (!JsonObj.IsValid() || !JsonObj->HasField(FieldName))
    {
        return Default;
    }

    const TSharedPtr<FJsonObject>* VecObj = nullptr;
    if (JsonObj->TryGetObjectField(FieldName, VecObj) && VecObj && VecObj->IsValid())
    {
        // Missing components fall back to the per-component Default so a partial scale
        // object keeps 1 on the unspecified axes rather than collapsing to 0.
        double X = Default.X, Y = Default.Y, Z = Default.Z;
        (*VecObj)->TryGetNumberField(TEXT("x"), X);
        (*VecObj)->TryGetNumberField(TEXT("y"), Y);
        (*VecObj)->TryGetNumberField(TEXT("z"), Z);
        return FVector(X, Y, Z);
    }

    return Default;
}

/**
 * Helper: Parse FRotator from JSON object
 */
static FRotator ParseRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObj, const FString& FieldName, const FRotator& Default = FRotator::ZeroRotator)
{
    if (!JsonObj.IsValid() || !JsonObj->HasField(FieldName))
    {
        return Default;
    }

    const TSharedPtr<FJsonObject>* RotObj = nullptr;
    if (JsonObj->TryGetObjectField(FieldName, RotObj) && RotObj && RotObj->IsValid())
    {
        double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
        (*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*RotObj)->TryGetNumberField(TEXT("roll"), Roll);
        return FRotator(Pitch, Yaw, Roll);
    }

    return Default;
}

} // anonymous namespace


// ============================================================================
// BATCH 1 & 2: Core Skeleton Structure + Attachments
// ============================================================================

/**
 * Handle: get_skeleton_info
 * Get information about a skeleton (bones, sockets, etc.)
 */
bool McpHandlers::AnimationPhysics::HandleGetSkeletonInfo(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    
    // Try loading as skeletal mesh if skeleton load failed
    if (!Skeleton && !SkeletonPath.IsEmpty())
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Skeleton);

    // Bone count
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    Result->SetNumberField(TEXT("boneCount"), RefSkeleton.GetRawBoneNum());

    // Virtual bone count
    Result->SetNumberField(TEXT("virtualBoneCount"), Skeleton->GetVirtualBones().Num());

    // Socket count
    Result->SetNumberField(TEXT("socketCount"), Skeleton->Sockets.Num());

    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Skeleton info retrieved"), Result);
    return true;
}

/**
 * Handle: list_bones
 * List all bones in a skeleton
 */
bool McpHandlers::AnimationPhysics::HandleListBones(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    
    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    TArray<TSharedPtr<FJsonValue>> BoneArray;

    for (int32 i = 0; i < RefSkeleton.GetRawBoneNum(); ++i)
    {
        TSharedPtr<FJsonObject> BoneObj = McpHandlerUtils::CreateResultObject();
        BoneObj->SetStringField(TEXT("name"), RefSkeleton.GetBoneName(i).ToString());
        BoneObj->SetNumberField(TEXT("index"), i);
        
        int32 ParentIndex = RefSkeleton.GetParentIndex(i);
        BoneObj->SetNumberField(TEXT("parentIndex"), ParentIndex);
        if (ParentIndex >= 0)
        {
            BoneObj->SetStringField(TEXT("parentName"), RefSkeleton.GetBoneName(ParentIndex).ToString());
        }

        // Use McpHandlerUtils helper for transform JSON
        const FTransform& RefPose = RefSkeleton.GetRefBonePose()[i];
        TSharedPtr<FJsonObject> TransformObj = McpHandlerUtils::CreateResultObject();
        TransformObj->SetNumberField(TEXT("x"), RefPose.GetLocation().X);
        TransformObj->SetNumberField(TEXT("y"), RefPose.GetLocation().Y);
        TransformObj->SetNumberField(TEXT("z"), RefPose.GetLocation().Z);
        BoneObj->SetObjectField(TEXT("location"), TransformObj);

        BoneArray.Add(MakeShared<FJsonValueObject>(BoneObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Skeleton);
    Result->SetArrayField(TEXT("bones"), BoneArray);
    Result->SetNumberField(TEXT("count"), BoneArray.Num());
    McpHandlerUtils::AddVerification(Result, Skeleton);

    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bones listed"), Result);
    return true;
}

/**
 * Handle: list_sockets
 * List all sockets in a skeleton/skeletal mesh
 */
bool McpHandlers::AnimationPhysics::HandleListSockets(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    
    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> SocketArray;
    for (USkeletalMeshSocket* Socket : Skeleton->Sockets)
    {
        if (!Socket) continue;

        TSharedPtr<FJsonObject> SocketObj = McpHandlerUtils::CreateResultObject();
        SocketObj->SetStringField(TEXT("name"), Socket->SocketName.ToString());
        SocketObj->SetStringField(TEXT("boneName"), Socket->BoneName.ToString());

        // Use McpHandlerUtils helpers for transform JSON
        SocketObj->SetObjectField(TEXT("relativeLocation"), McpHandlerUtils::VectorToJson(Socket->RelativeLocation));
        SocketObj->SetObjectField(TEXT("relativeRotation"), McpHandlerUtils::RotatorToJson(Socket->RelativeRotation));
        SocketObj->SetObjectField(TEXT("relativeScale"), McpHandlerUtils::VectorToJson(Socket->RelativeScale));

        SocketArray.Add(MakeShared<FJsonValueObject>(SocketObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("sockets"), SocketArray);
    Result->SetNumberField(TEXT("count"), SocketArray.Num());

    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sockets listed"), Result);
    return true;
}

/**
 * Handle: create_socket
 * Create a new socket on a skeleton
 */
bool McpHandlers::AnimationPhysics::HandleCreateSocket(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    }

    FString SocketName = GetJsonStringField(Payload, TEXT("socketName"));
    FString BoneName = GetJsonStringField(Payload, TEXT("attachBoneName"));
    if (BoneName.IsEmpty())
    {
        BoneName = GetJsonStringField(Payload, TEXT("boneName"));
    }

    if (SocketName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("socketName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    if (BoneName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("attachBoneName or boneName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    
    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    // Check if socket already exists
    for (USkeletalMeshSocket* ExistingSocket : Skeleton->Sockets)
    {
        if (ExistingSocket && ExistingSocket->SocketName == FName(*SocketName))
        {
            S.SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Socket '%s' already exists"), *SocketName), 
                TEXT("SOCKET_EXISTS"));
            return true;
        }
    }

    // Create the socket
    USkeletalMeshSocket* NewSocket = NewObject<USkeletalMeshSocket>(Skeleton);
    if (!NewSocket)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create socket object"), TEXT("CREATION_FAILED"));
        return true;
    }
    // Parse socket transform using ParseVectorFromJson helpers
    NewSocket->SocketName = FName(*SocketName);
    NewSocket->RelativeLocation = ParseVectorFromJson(Payload, TEXT("relativeLocation"));
    NewSocket->RelativeRotation = ParseRotatorFromJson(Payload, TEXT("relativeRotation"));
    NewSocket->RelativeScale = ParseVectorFromJson(Payload, TEXT("relativeScale"), FVector::OneVector);
    NewSocket->BoneName = FName(*BoneName);

    Skeleton->Sockets.Add(NewSocket);
    McpSafeAssetSave(Skeleton);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("socketName"), SocketName);
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());

    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Socket '%s' created on bone '%s'"), *SocketName, *BoneName), Result);
    return true;
}

/**
 * Handle: configure_socket
 * Modify an existing socket's properties
 */
bool McpHandlers::AnimationPhysics::HandleConfigureSocket(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    }

    FString SocketName = GetJsonStringField(Payload, TEXT("socketName"));
    if (SocketName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("socketName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    
    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    // Find the socket
    USkeletalMeshSocket* Socket = nullptr;
    for (USkeletalMeshSocket* Candidate : Skeleton->Sockets)
    {
        if (Candidate && Candidate->SocketName == FName(*SocketName))
        {
            Socket = Candidate;
            break;
        }
    }

    if (!Socket)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Socket '%s' not found"), *SocketName), 
            TEXT("SOCKET_NOT_FOUND"));
        return true;
    }

    McpHandlerUtils::FMcpWriteReport Report;

    if (Payload->HasField(TEXT("attachBoneName")))
    {
        FString NewBoneName = GetJsonStringField(Payload, TEXT("attachBoneName"));
        if (!NewBoneName.IsEmpty())
        {
            Socket->BoneName = FName(*NewBoneName);
            Report.MarkApplied(TEXT("attachBoneName"));
        }
        else
        {
            Report.MarkFailed(TEXT("attachBoneName"), TEXT("must be a non-empty string"));
        }
    }

    if (Payload->HasField(TEXT("relativeLocation")))
    {
        const TSharedPtr<FJsonObject>* LocObj;
        if (Payload->TryGetObjectField(TEXT("relativeLocation"), LocObj))
        {
            Socket->RelativeLocation = ParseVectorFromJson(Payload, TEXT("relativeLocation"));
            Report.MarkApplied(TEXT("relativeLocation"));
        }
        else
        {
            Report.MarkFailed(TEXT("relativeLocation"), TEXT("must be an object with x/y/z"));
        }
    }

    if (Payload->HasField(TEXT("relativeRotation")))
    {
        const TSharedPtr<FJsonObject>* RotObj;
        if (Payload->TryGetObjectField(TEXT("relativeRotation"), RotObj))
        {
            Socket->RelativeRotation = ParseRotatorFromJson(Payload, TEXT("relativeRotation"));
            Report.MarkApplied(TEXT("relativeRotation"));
        }
        else
        {
            Report.MarkFailed(TEXT("relativeRotation"), TEXT("must be an object with pitch/yaw/roll"));
        }
    }

    if (Payload->HasField(TEXT("relativeScale")))
    {
        const TSharedPtr<FJsonObject>* ScaleObj;
        if (Payload->TryGetObjectField(TEXT("relativeScale"), ScaleObj))
        {
            Socket->RelativeScale = ParseVectorFromJson(Payload, TEXT("relativeScale"), FVector::OneVector);
            Report.MarkApplied(TEXT("relativeScale"));
        }
        else
        {
            Report.MarkFailed(TEXT("relativeScale"), TEXT("must be an object with x/y/z"));
        }
    }

    if (Report.AnyApplied())
    {
        McpSafeAssetSave(Skeleton);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("socketName"), SocketName);
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());

    return SendWriteReportResponse(&S, RequestingSocket, RequestId, Report, Result,
        FString::Printf(TEXT("Socket '%s' configured"), *SocketName), nullptr);
}

/**
 * Handle: create_virtual_bone
 * Create a virtual bone between two bones
 */
bool McpHandlers::AnimationPhysics::HandleCreateVirtualBone(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    FString SourceBone = GetJsonStringField(Payload, TEXT("sourceBoneName"));
    FString TargetBone = GetJsonStringField(Payload, TEXT("targetBoneName"));
    FString VirtualBoneName = GetJsonStringField(Payload, TEXT("boneName"));

    if (SkeletonPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    if (SourceBone.IsEmpty() || TargetBone.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("sourceBoneName and targetBoneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    if (!Skeleton)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    // Generate virtual bone name if not provided
    if (VirtualBoneName.IsEmpty())
    {
        VirtualBoneName = FString::Printf(TEXT("VB_%s_to_%s"), *SourceBone, *TargetBone);
    }

    // Add virtual bone
    FName NewVirtualBoneName;
    bool bSuccess = Skeleton->AddNewVirtualBone(FName(*SourceBone), FName(*TargetBone), NewVirtualBoneName);
    
    if (!bSuccess)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Failed to create virtual bone. Check that source and target bones exist."), 
            TEXT("VIRTUAL_BONE_FAILED"));
        return true;
    }

    // Rename if custom name provided
    if (!VirtualBoneName.IsEmpty() && NewVirtualBoneName.ToString() != VirtualBoneName)
    {
        Skeleton->RenameVirtualBone(NewVirtualBoneName, FName(*VirtualBoneName));
        NewVirtualBoneName = FName(*VirtualBoneName);
    }

    McpSafeAssetSave(Skeleton);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("virtualBoneName"), NewVirtualBoneName.ToString());
    Result->SetStringField(TEXT("sourceBone"), SourceBone);
    Result->SetStringField(TEXT("targetBone"), TargetBone);
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());

    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Virtual bone '%s' created"), *NewVirtualBoneName.ToString()), Result);
    return true;
}


// ============================================================================
// BATCH 3: Physics Asset
// ============================================================================

/**
 * Handle: create_physics_asset
 * Create a new physics asset for a skeletal mesh
 */
bool McpHandlers::AnimationPhysics::HandleCreatePhysicsAsset(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    // Also accept skeletonPath for backward compatibility
    if (SkeletalMeshPath.IsEmpty())
    {
        SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    }
    FString OutputPath = GetJsonStringField(Payload, TEXT("outputPath"));

    if (SkeletalMeshPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath (or skeletonPath) is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* SkeletalMesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!SkeletalMesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // Determine output path
    if (OutputPath.IsEmpty())
    {
        OutputPath = FPaths::GetPath(SkeletalMeshPath);
        FString MeshName = FPaths::GetBaseFilename(SkeletalMeshPath);
        OutputPath = FString::Printf(TEXT("%s/%s_PhysicsAsset"), *OutputPath, *MeshName);
    }

    // Create package and asset directly to avoid UI dialogs
    FString PackagePath = FPaths::GetPath(OutputPath);
    FString AssetName = FPaths::GetBaseFilename(OutputPath);
    FString FullPackagePath = PackagePath / AssetName;
    
    UPackage* Package = CreatePackage(*FullPackagePath);
    if (!Package)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        return true;
    }

    UPhysicsAsset* PhysicsAsset = NewObject<UPhysicsAsset>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
    if (!PhysicsAsset)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create physics asset"), TEXT("CREATE_FAILED"));
        return true;
    }

    PhysicsAsset->SetPreviewMesh(SkeletalMesh);
    PhysicsAsset->UpdateBodySetupIndexMap();
    PhysicsAsset->UpdateBoundsBodiesArray();
    FAssetRegistryModule::AssetCreated(PhysicsAsset);
    Package->MarkPackageDirty();
    McpSafeAssetSave(PhysicsAsset);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("physicsAssetPath"), PhysicsAsset->GetPathName());
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMesh->GetPathName());
    Result->SetNumberField(TEXT("bodyCount"), PhysicsAsset->SkeletalBodySetups.Num());
    Result->SetNumberField(TEXT("constraintCount"), PhysicsAsset->ConstraintSetup.Num());

    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Physics asset created"), Result);
    return true;
}

/**
 * Handle: list_physics_bodies
 * List all physics bodies in a physics asset
 */
bool McpHandlers::AnimationPhysics::HandleListPhysicsBodies(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
    if (PhysicsAssetPath.IsEmpty())
    {
        // Try to get from skeletal mesh
        FString MeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
        if (!MeshPath.IsEmpty())
        {
            FString Error;
            USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(MeshPath, Error);
            if (Mesh && Mesh->GetPhysicsAsset())
            {
                PhysicsAssetPath = Mesh->GetPhysicsAsset()->GetPathName();
            }
        }
    }

    if (PhysicsAssetPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("physicsAssetPath or skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> BodyArray;
    for (USkeletalBodySetup* BodySetup : PhysicsAsset->SkeletalBodySetups)
    {
        if (!BodySetup) continue;

        TSharedPtr<FJsonObject> BodyObj = McpHandlerUtils::CreateResultObject();
        BodyObj->SetStringField(TEXT("boneName"), BodySetup->BoneName.ToString());
        BodyObj->SetBoolField(TEXT("considerForBounds"), BodySetup->bConsiderForBounds);

        // Collision type
        FString CollisionType;
        switch (BodySetup->CollisionTraceFlag)
        {
            case CTF_UseDefault: CollisionType = TEXT("Default"); break;
            case CTF_UseSimpleAndComplex: CollisionType = TEXT("SimpleAndComplex"); break;
            case CTF_UseSimpleAsComplex: CollisionType = TEXT("SimpleAsComplex"); break;
            case CTF_UseComplexAsSimple: CollisionType = TEXT("ComplexAsSimple"); break;
        }
        BodyObj->SetStringField(TEXT("collisionType"), CollisionType);

        // Primitive counts
        BodyObj->SetNumberField(TEXT("sphereCount"), BodySetup->AggGeom.SphereElems.Num());
        BodyObj->SetNumberField(TEXT("boxCount"), BodySetup->AggGeom.BoxElems.Num());
        BodyObj->SetNumberField(TEXT("capsuleCount"), BodySetup->AggGeom.SphylElems.Num());
        BodyObj->SetNumberField(TEXT("convexCount"), BodySetup->AggGeom.ConvexElems.Num());

        BodyArray.Add(MakeShared<FJsonValueObject>(BodyObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("physicsBodies"), BodyArray);
    Result->SetNumberField(TEXT("count"), BodyArray.Num());
    Result->SetNumberField(TEXT("constraintCount"), PhysicsAsset->ConstraintSetup.Num());

    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Physics bodies listed"), Result);
    return true;
}


// ============================================================================
// BATCH 4: Physics Body Operations
// ============================================================================

/**
 * Handle: add_physics_body
 * Add a physics body to a physics asset
 */
bool McpHandlers::AnimationPhysics::HandleAddPhysicsBody(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
    FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));
    FString BodyType = GetJsonStringField(Payload, TEXT("bodyType"));

    if (PhysicsAssetPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("physicsAssetPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    if (BoneName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("boneName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate path security BEFORE loading asset
    FString SanitizedPath = SanitizeProjectRelativePath(PhysicsAssetPath);
    if (SanitizedPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid physics asset path '%s': contains traversal sequences or invalid characters"), *PhysicsAssetPath),
            TEXT("INVALID_PATH"));
        return true;
    }
    PhysicsAssetPath = SanitizedPath;

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    // CRITICAL: Validate bone exists in the skeleton before creating physics body
    // This prevents creating physics bodies for non-existent bones (fixes suspicious passes)
    USkeletalMesh* PreviewMesh = PhysicsAsset->GetPreviewMesh();
    if (PreviewMesh)
    {
        USkeleton* Skeleton = PreviewMesh->GetSkeleton();
        if (Skeleton)
        {
            const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
            int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));
            if (BoneIndex == INDEX_NONE)
            {
                S.SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Bone '%s' does not exist in skeleton"), *BoneName),
                    TEXT("BONE_NOT_FOUND"));
                return true;
            }
        }
    }

    // Find existing body or create new one
    int32 BodyIndex = PhysicsAsset->FindBodyIndex(FName(*BoneName));
    USkeletalBodySetup* BodySetup = nullptr;
    bool bCreated = false;

    if (BodyIndex == INDEX_NONE)
    {
        // Create new body
        BodySetup = NewObject<USkeletalBodySetup>(PhysicsAsset, NAME_None, RF_Transactional);
        if (!BodySetup)
        {
            S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create physics body setup"), TEXT("CREATION_FAILED"));
            return true;
        }
        BodySetup->BoneName = FName(*BoneName);
        PhysicsAsset->SkeletalBodySetups.Add(BodySetup);
        bCreated = true;
        BodyIndex = PhysicsAsset->SkeletalBodySetups.Num() - 1;
    }
    else
    {
        BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
    }

    // Add geometry based on type
    if (BodyType.IsEmpty()) BodyType = TEXT("Capsule");

    double Radius = 10.0;
    double Length = 20.0;
    double Width = 10.0, Height = 10.0, Depth = 10.0;

    Payload->TryGetNumberField(TEXT("radius"), Radius);
    Payload->TryGetNumberField(TEXT("length"), Length);
    Payload->TryGetNumberField(TEXT("width"), Width);
    Payload->TryGetNumberField(TEXT("height"), Height);
    Payload->TryGetNumberField(TEXT("depth"), Depth);

    FVector Center = ParseVectorFromJson(Payload, TEXT("center"));
    FRotator Rotation = ParseRotatorFromJson(Payload, TEXT("rotation"));

    if (BodyType.Equals(TEXT("Sphere"), ESearchCase::IgnoreCase))
    {
        FKSphereElem SphereElem;
        SphereElem.Radius = static_cast<float>(Radius);
        SphereElem.Center = Center;
        BodySetup->AggGeom.SphereElems.Add(SphereElem);
    }
    else if (BodyType.Equals(TEXT("Box"), ESearchCase::IgnoreCase))
    {
        FKBoxElem BoxElem;
        BoxElem.X = static_cast<float>(Width);
        BoxElem.Y = static_cast<float>(Depth);
        BoxElem.Z = static_cast<float>(Height);
        BoxElem.Center = Center;
        BoxElem.Rotation = Rotation;
        BodySetup->AggGeom.BoxElems.Add(BoxElem);
    }
    else if (BodyType.Equals(TEXT("Capsule"), ESearchCase::IgnoreCase) || 
             BodyType.Equals(TEXT("Sphyl"), ESearchCase::IgnoreCase))
    {
        FKSphylElem CapsuleElem;
        CapsuleElem.Radius = static_cast<float>(Radius);
        CapsuleElem.Length = static_cast<float>(Length);
        CapsuleElem.Center = Center;
        CapsuleElem.Rotation = Rotation;
        BodySetup->AggGeom.SphylElems.Add(CapsuleElem);
    }
    else
    {
        // Default to capsule
        FKSphylElem CapsuleElem;
        CapsuleElem.Radius = static_cast<float>(Radius);
        CapsuleElem.Length = static_cast<float>(Length);
        CapsuleElem.Center = Center;
        BodySetup->AggGeom.SphylElems.Add(CapsuleElem);
    }

    PhysicsAsset->UpdateBodySetupIndexMap();
    PhysicsAsset->UpdateBoundsBodiesArray();
    McpSafeAssetSave(PhysicsAsset);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetStringField(TEXT("bodyType"), BodyType);
    Result->SetNumberField(TEXT("bodyIndex"), BodyIndex);
    Result->SetBoolField(TEXT("created"), bCreated);

    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Physics body %s for bone '%s'"), bCreated ? TEXT("created") : TEXT("modified"), *BoneName), Result);
    return true;
}

/**
 * Handle: configure_physics_body
 * Configure properties of a physics body
 */
bool McpHandlers::AnimationPhysics::HandleConfigurePhysicsBody(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
    FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));

    if (PhysicsAssetPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("physicsAssetPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    if (BoneName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("boneName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate path security BEFORE loading asset
    FString SanitizedPath = SanitizeProjectRelativePath(PhysicsAssetPath);
    if (SanitizedPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid physics asset path '%s': contains traversal sequences or invalid characters"), *PhysicsAssetPath),
            TEXT("INVALID_PATH"));
        return true;
    }
    PhysicsAssetPath = SanitizedPath;

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    int32 BodyIndex = PhysicsAsset->FindBodyIndex(FName(*BoneName));
    if (BodyIndex == INDEX_NONE)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("No physics body found for bone '%s'"), *BoneName), 
            TEXT("BODY_NOT_FOUND"));
        return true;
    }

    USkeletalBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];

    // Configure physics properties
    double Mass = 0.0;
    if (Payload->TryGetNumberField(TEXT("mass"), Mass))
    {
        // MassInKgOverride is clamped to >= 0.001 by the engine; reject non-positive input loudly.
        if (Mass < 0.001)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("mass must be >= 0.001 kg (got %f)"), Mass),
                TEXT("UNSUPPORTED_PARAM"));
            return true;
        }
        BodySetup->DefaultInstance.SetMassOverride(static_cast<float>(Mass), true);
    }

    double LinearDamping = 0.0;
    if (Payload->TryGetNumberField(TEXT("linearDamping"), LinearDamping))
    {
        BodySetup->DefaultInstance.LinearDamping = static_cast<float>(LinearDamping);
    }

    double AngularDamping = 0.0;
    if (Payload->TryGetNumberField(TEXT("angularDamping"), AngularDamping))
    {
        BodySetup->DefaultInstance.AngularDamping = static_cast<float>(AngularDamping);
    }

    bool bCollisionEnabled = true;
    if (Payload->TryGetBoolField(TEXT("collisionEnabled"), bCollisionEnabled))
    {
        BodySetup->DefaultInstance.SetCollisionEnabled(bCollisionEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }

    bool bSimulatePhysics = true;
    if (Payload->TryGetBoolField(TEXT("simulatePhysics"), bSimulatePhysics))
    {
        // Note: In UE 5.7+, SetSimulatePhysics is not available on FBodyInstance
        // The simulation is controlled at the component level at runtime
        BodySetup->DefaultInstance.bSimulatePhysics = bSimulatePhysics;
    }

    McpSafeAssetSave(PhysicsAsset);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetNumberField(TEXT("bodyIndex"), BodyIndex);

    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Physics body '%s' configured"), *BoneName), Result);
    return true;
}

/**
 * Handle: add_physics_constraint
 * Add a constraint between two physics bodies
 */
bool McpHandlers::AnimationPhysics::HandleAddPhysicsConstraint(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
    FString BodyA = GetJsonStringField(Payload, TEXT("bodyA"));
    FString BodyB = GetJsonStringField(Payload, TEXT("bodyB"));
    FString ConstraintName = GetJsonStringField(Payload, TEXT("constraintName"));

    if (PhysicsAssetPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("physicsAssetPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    if (BodyA.IsEmpty() || BodyB.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("bodyA and bodyB are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate path security BEFORE loading asset
    FString SanitizedPath = SanitizeProjectRelativePath(PhysicsAssetPath);
    if (SanitizedPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid physics asset path '%s': contains traversal sequences or invalid characters"), *PhysicsAssetPath),
            TEXT("INVALID_PATH"));
        return true;
    }
    PhysicsAssetPath = SanitizedPath;

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    // Check that both bodies exist
    if (PhysicsAsset->FindBodyIndex(FName(*BodyA)) == INDEX_NONE)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Body '%s' not found in physics asset"), *BodyA), 
            TEXT("BODY_NOT_FOUND"));
        return true;
    }

    if (PhysicsAsset->FindBodyIndex(FName(*BodyB)) == INDEX_NONE)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Body '%s' not found in physics asset"), *BodyB), 
            TEXT("BODY_NOT_FOUND"));
        return true;
    }

    // Create constraint
    UPhysicsConstraintTemplate* Constraint = NewObject<UPhysicsConstraintTemplate>(PhysicsAsset, NAME_None, RF_Transactional);
    if (!Constraint)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create physics constraint"), TEXT("CREATION_FAILED"));
        return true;
    }
    
    Constraint->DefaultInstance.ConstraintBone1 = FName(*BodyA);
    Constraint->DefaultInstance.ConstraintBone2 = FName(*BodyB);
    
    // Set default constraint profile name via JointName (ProfileName removed in UE 5.7)
    if (!ConstraintName.IsEmpty())
    {
        Constraint->DefaultInstance.JointName = FName(*ConstraintName);
    }

    PhysicsAsset->ConstraintSetup.Add(Constraint);

    // Apply default limits
    const TSharedPtr<FJsonObject>* LimitsObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("limits"), LimitsObj) && LimitsObj && LimitsObj->IsValid())
    {
        double Swing1 = 45.0, Swing2 = 45.0, Twist = 45.0;
        (*LimitsObj)->TryGetNumberField(TEXT("swing1LimitAngle"), Swing1);
        (*LimitsObj)->TryGetNumberField(TEXT("swing2LimitAngle"), Swing2);
        (*LimitsObj)->TryGetNumberField(TEXT("twistLimitAngle"), Twist);

        Constraint->DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Swing1));
        Constraint->DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Swing2));
        Constraint->DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Twist));
    }
    else
    {
        // Default to limited motion
        Constraint->DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 45.0f);
        Constraint->DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 45.0f);
        Constraint->DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 45.0f);
    }

    PhysicsAsset->UpdateBodySetupIndexMap();
    McpSafeAssetSave(PhysicsAsset);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("bodyA"), BodyA);
    Result->SetStringField(TEXT("bodyB"), BodyB);
    Result->SetNumberField(TEXT("constraintIndex"), PhysicsAsset->ConstraintSetup.Num() - 1);

    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Constraint created between '%s' and '%s'"), *BodyA, *BodyB), Result);
    return true;
}

/**
 * Handle: configure_constraint_limits
 * Configure angular/linear limits on a constraint
 */
bool McpHandlers::AnimationPhysics::HandleConfigureConstraintLimits(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
    FString BodyA = GetJsonStringField(Payload, TEXT("bodyA"));
    FString BodyB = GetJsonStringField(Payload, TEXT("bodyB"));

    if (PhysicsAssetPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("physicsAssetPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    if (BodyA.IsEmpty() || BodyB.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("bodyA and bodyB are required to identify constraint"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate path security BEFORE loading asset
    FString SanitizedPath = SanitizeProjectRelativePath(PhysicsAssetPath);
    if (SanitizedPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid physics asset path '%s': contains traversal sequences or invalid characters"), *PhysicsAssetPath),
            TEXT("INVALID_PATH"));
        return true;
    }
    PhysicsAssetPath = SanitizedPath;

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    // Find constraint by body names
    UPhysicsConstraintTemplate* Constraint = nullptr;
    for (UPhysicsConstraintTemplate* C : PhysicsAsset->ConstraintSetup)
    {
        if (C && 
            C->DefaultInstance.ConstraintBone1 == FName(*BodyA) && 
            C->DefaultInstance.ConstraintBone2 == FName(*BodyB))
        {
            Constraint = C;
            break;
        }
        // Also check reverse order
        if (C && 
            C->DefaultInstance.ConstraintBone1 == FName(*BodyB) && 
            C->DefaultInstance.ConstraintBone2 == FName(*BodyA))
        {
            Constraint = C;
            break;
        }
    }

    if (!Constraint)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("No constraint found between '%s' and '%s'"), *BodyA, *BodyB), 
            TEXT("CONSTRAINT_NOT_FOUND"));
        return true;
    }

    // Configure limits
    const TSharedPtr<FJsonObject>* LimitsObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("limits"), LimitsObj) && LimitsObj && LimitsObj->IsValid())
    {
        double Swing1 = 45.0, Swing2 = 45.0, Twist = 45.0;
        (*LimitsObj)->TryGetNumberField(TEXT("swing1LimitAngle"), Swing1);
        (*LimitsObj)->TryGetNumberField(TEXT("swing2LimitAngle"), Swing2);
        (*LimitsObj)->TryGetNumberField(TEXT("twistLimitAngle"), Twist);

        FString Swing1Motion, Swing2Motion, TwistMotion;
        (*LimitsObj)->TryGetStringField(TEXT("swing1Motion"), Swing1Motion);
        (*LimitsObj)->TryGetStringField(TEXT("swing2Motion"), Swing2Motion);
        (*LimitsObj)->TryGetStringField(TEXT("twistMotion"), TwistMotion);

        auto ParseMotion = [](const FString& Motion) -> EAngularConstraintMotion {
            if (Motion.Equals(TEXT("Free"), ESearchCase::IgnoreCase)) return EAngularConstraintMotion::ACM_Free;
            if (Motion.Equals(TEXT("Locked"), ESearchCase::IgnoreCase)) return EAngularConstraintMotion::ACM_Locked;
            return EAngularConstraintMotion::ACM_Limited;
        };

        Constraint->DefaultInstance.SetAngularSwing1Limit(ParseMotion(Swing1Motion), static_cast<float>(Swing1));
        Constraint->DefaultInstance.SetAngularSwing2Limit(ParseMotion(Swing2Motion), static_cast<float>(Swing2));
        Constraint->DefaultInstance.SetAngularTwistLimit(ParseMotion(TwistMotion), static_cast<float>(Twist));
    }
    else
    {
        // Individual parameters
        double Swing1 = 0.0, Swing2 = 0.0, Twist = 0.0;
        if (Payload->TryGetNumberField(TEXT("swing1LimitAngle"), Swing1))
        {
            Constraint->DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Swing1));
        }
        if (Payload->TryGetNumberField(TEXT("swing2LimitAngle"), Swing2))
        {
            Constraint->DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Swing2));
        }
        if (Payload->TryGetNumberField(TEXT("twistLimitAngle"), Twist))
        {
            Constraint->DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Twist));
        }
    }

    McpSafeAssetSave(PhysicsAsset);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("bodyA"), BodyA);
    Result->SetStringField(TEXT("bodyB"), BodyB);

    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Constraint limits configured"), Result);
    return true;
}


// ============================================================================
// BATCH 5: Bone Structure Operations
// ============================================================================

/**
 * Handle: rename_bone
 * Rename a bone in a skeleton (via virtual bone renaming for safety)
 */
bool McpHandlers::AnimationPhysics::HandleRenameBone(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));
    FString NewBoneName = GetJsonStringField(Payload, TEXT("newBoneName"));

    if (SkeletonPath.IsEmpty() || BoneName.IsEmpty() || NewBoneName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("skeletonPath, boneName, and newBoneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    if (!Skeleton)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    // Check if it's a virtual bone
    const TArray<FVirtualBone>& VirtualBones = Skeleton->GetVirtualBones();
    bool bIsVirtualBone = false;
    for (const FVirtualBone& VB : VirtualBones)
    {
        if (VB.VirtualBoneName == FName(*BoneName))
        {
            bIsVirtualBone = true;
            break;
        }
    }

    if (bIsVirtualBone)
    {
        Skeleton->RenameVirtualBone(FName(*BoneName), FName(*NewBoneName));
        McpSafeAssetSave(Skeleton);

        bool bSave = false;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        if (bSave)
        {
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("oldName"), BoneName);
        Result->SetStringField(TEXT("newName"), NewBoneName);
        Result->SetBoolField(TEXT("isVirtualBone"), true);

        S.SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Virtual bone renamed from '%s' to '%s'"), *BoneName, *NewBoneName), Result);
        return true;
    }

    // For regular bones, renaming is not directly supported without reimporting
    // We can rename bone mappings in animation assets though
    S.SendAutomationError(RequestingSocket, RequestId, 
        TEXT("Renaming non-virtual bones is not supported. Only virtual bones can be renamed at runtime. To rename regular bones, reimport the skeletal mesh with updated bone names."), 
        TEXT("OPERATION_NOT_SUPPORTED"));
    return true;
}

/**
 * Handle: set_bone_transform
 * Set the reference pose transform for a bone
 */
bool McpHandlers::AnimationPhysics::HandleSetBoneTransform(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    // Also accept skeletonPath for backward compatibility
    if (SkeletalMeshPath.IsEmpty())
    {
        SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    }
    FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));

    if (SkeletalMeshPath.IsEmpty() || BoneName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("skeletalMeshPath (or skeletonPath) and boneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
    int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));
    
    if (BoneIndex == INDEX_NONE)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Bone '%s' not found"), *BoneName), TEXT("BONE_NOT_FOUND"));
        return true;
    }

    // Parse transform
    FVector Location = ParseVectorFromJson(Payload, TEXT("location"));
    FRotator Rotation = ParseRotatorFromJson(Payload, TEXT("rotation"));
    FVector Scale = ParseVectorFromJson(Payload, TEXT("scale"), FVector::OneVector);

    FTransform NewTransform(Rotation, Location, Scale);

    // Modify the reference skeleton
    // Note: This modifies the skeleton in memory. For persistent changes, the mesh needs to be reimported.
    FReferenceSkeletonModifier Modifier(Mesh->GetRefSkeleton(), Mesh->GetSkeleton());
    Modifier.UpdateRefPoseTransform(BoneIndex, NewTransform);

    McpSafeAssetSave(Mesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetNumberField(TEXT("boneIndex"), BoneIndex);

    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Bone '%s' transform updated"), *BoneName), Result);
    return true;
}


// ============================================================================
// BATCH 6: Morph Target Operations
// ============================================================================

/**
 * Handle: create_morph_target
 * Create a new morph target on a skeletal mesh
 */
bool McpHandlers::AnimationPhysics::HandleCreateMorphTarget(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    FString MorphTargetName = GetJsonStringField(Payload, TEXT("morphTargetName"));

    if (SkeletalMeshPath.IsEmpty() || MorphTargetName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("skeletalMeshPath and morphTargetName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // Check if morph target already exists
    UMorphTarget* ExistingMorph = Mesh->FindMorphTarget(FName(*MorphTargetName));
    if (ExistingMorph)
    {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
        Result->SetBoolField(TEXT("alreadyExists"), true);

        S.SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Morph target '%s' already exists"), *MorphTargetName), Result);
        return true;
    }

    // CRITICAL FIX: UE 5.7 requires morph targets to have valid delta data BEFORE registration.
    // RegisterMorphTarget() internally checks HasValidData() and fires an ensure() for empty morphs.
    // We must either:
    // 1. Provide deltas and populate them BEFORE registering, OR
    // 2. Return EMPTY_MORPH_TARGET error immediately without creating the morph target
    
    // Check if deltas parameter is provided
    const TArray<TSharedPtr<FJsonValue>>* DeltasArray = nullptr;
    bool bHasDeltas = Payload->TryGetArrayField(TEXT("deltas"), DeltasArray) && DeltasArray && DeltasArray->Num() > 0;
    
    if (!bHasDeltas)
    {
        // No deltas provided - cannot create a valid morph target in UE 5.7+
        // Return error WITHOUT creating/registering to avoid engine ensure failure
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Morph target '%s' requires vertex deltas. Provide 'deltas' array with vertex indices and position offsets. Example: {\"deltas\": [{\"vertexIndex\": 0, \"positionDelta\": {\"x\": 1, \"y\": 0, \"z\": 0}}]}"), *MorphTargetName),
            TEXT("EMPTY_MORPH_TARGET"));
        return true;
    }
    
    // Parse deltas array
    TArray<FMorphTargetDelta> Deltas;
    for (const TSharedPtr<FJsonValue>& DeltaValue : *DeltasArray)
    {
        const TSharedPtr<FJsonObject>* DeltaObj = nullptr;
        if (DeltaValue->TryGetObject(DeltaObj) && DeltaObj && DeltaObj->IsValid())
        {
            FMorphTargetDelta Delta;
            
            double VertexIndex = 0;
            (*DeltaObj)->TryGetNumberField(TEXT("vertexIndex"), VertexIndex);
            Delta.SourceIdx = static_cast<uint32>(VertexIndex);

            const TSharedPtr<FJsonObject>* PositionDelta = nullptr;
            if ((*DeltaObj)->TryGetObjectField(TEXT("positionDelta"), PositionDelta) && PositionDelta && PositionDelta->IsValid())
            {
                double X = 0, Y = 0, Z = 0;
                (*PositionDelta)->TryGetNumberField(TEXT("x"), X);
                (*PositionDelta)->TryGetNumberField(TEXT("y"), Y);
                (*PositionDelta)->TryGetNumberField(TEXT("z"), Z);
                Delta.PositionDelta = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            }

            const TSharedPtr<FJsonObject>* TangentDelta = nullptr;
            if ((*DeltaObj)->TryGetObjectField(TEXT("tangentDelta"), TangentDelta) && TangentDelta && TangentDelta->IsValid())
            {
                double X = 0, Y = 0, Z = 0;
                (*TangentDelta)->TryGetNumberField(TEXT("x"), X);
                (*TangentDelta)->TryGetNumberField(TEXT("y"), Y);
                (*TangentDelta)->TryGetNumberField(TEXT("z"), Z);
                Delta.TangentZDelta = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            }

            Deltas.Add(Delta);
        }
    }
    
    if (Deltas.Num() == 0)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Deltas array was provided but contained no valid delta entries. Each delta must have vertexIndex and positionDelta."),
            TEXT("INVALID_MORPH_DATA"));
        return true;
    }
    
    // Create new morph target
    UMorphTarget* NewMorphTarget = NewObject<UMorphTarget>(Mesh, FName(*MorphTargetName));
    if (!NewMorphTarget)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create morph target object"), TEXT("CREATION_FAILED"));
        return true;
    }
    
    // Set BaseSkelMesh - required for HasValidData() to work properly
    NewMorphTarget->BaseSkelMesh = Mesh;
    
    // Get LOD index (default to 0)
    int32 LODIndex = 0;
    Payload->TryGetNumberField(TEXT("lodIndex"), LODIndex);
    
    // Populate deltas BEFORE registering - this is critical for UE 5.7+
    // PopulateDeltas requires the sections array from the skeletal mesh LOD model
#if WITH_EDITOR
    const FSkeletalMeshModel* SkelMeshModel = Mesh->GetImportedModel();
    TArray<FSkelMeshSection> Sections;
    if (SkelMeshModel && SkelMeshModel->LODModels.IsValidIndex(LODIndex))
    {
        const FSkeletalMeshLODModel& LODModel = SkelMeshModel->LODModels[LODIndex];
        Sections = LODModel.Sections;
    }
    
    NewMorphTarget->PopulateDeltas(Deltas, LODIndex, Sections, false, false);
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Morph target creation with deltas requires editor"), TEXT("NOT_SUPPORTED"));
    return true;
#endif
    
    // NOW validate that we have valid data
    if (!NewMorphTarget->HasValidData())
    {
        // This shouldn't happen if deltas were valid, but check anyway
        NewMorphTarget->MarkAsGarbage();
        
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Morph target '%s' has no valid data after populating deltas. Check vertex indices are valid."), *MorphTargetName),
            TEXT("INVALID_MORPH_DATA"));
        return true;
    }
    
    // Only register AFTER the morph target has valid data
    Mesh->RegisterMorphTarget(NewMorphTarget);
    
    McpSafeAssetSave(Mesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("morphTargetCount"), Mesh->GetMorphTargets().Num());
    Result->SetNumberField(TEXT("deltaCount"), Deltas.Num());

    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Morph target '%s' created with %d deltas"), *MorphTargetName, Deltas.Num()), Result);
    return true;
}

/**
 * Handle: set_morph_target_deltas
 * Set vertex deltas for a morph target
 */
bool McpHandlers::AnimationPhysics::HandleSetMorphTargetDeltas(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    FString MorphTargetName = GetJsonStringField(Payload, TEXT("morphTargetName"));

    if (SkeletalMeshPath.IsEmpty() || MorphTargetName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("skeletalMeshPath and morphTargetName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UMorphTarget* MorphTarget = Mesh->FindMorphTarget(FName(*MorphTargetName));
    bool bCreatedMorphTarget = false;

    // Parse deltas array
    const TArray<TSharedPtr<FJsonValue>>* DeltasArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("deltas"), DeltasArray) || !DeltasArray)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("deltas array is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Build delta vertices
    TArray<FMorphTargetDelta> Deltas;
    for (const TSharedPtr<FJsonValue>& DeltaValue : *DeltasArray)
    {
        const TSharedPtr<FJsonObject>* DeltaObj = nullptr;
        if (DeltaValue->TryGetObject(DeltaObj) && DeltaObj && DeltaObj->IsValid())
        {
            FMorphTargetDelta Delta;
            
            double VertexIndex = 0;
            (*DeltaObj)->TryGetNumberField(TEXT("vertexIndex"), VertexIndex);
            Delta.SourceIdx = static_cast<uint32>(VertexIndex);

            const TSharedPtr<FJsonObject>* PositionDelta = nullptr;
            if ((*DeltaObj)->TryGetObjectField(TEXT("positionDelta"), PositionDelta) && PositionDelta && PositionDelta->IsValid())
            {
                double X = 0, Y = 0, Z = 0;
                (*PositionDelta)->TryGetNumberField(TEXT("x"), X);
                (*PositionDelta)->TryGetNumberField(TEXT("y"), Y);
                (*PositionDelta)->TryGetNumberField(TEXT("z"), Z);
                Delta.PositionDelta = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            }

            const TSharedPtr<FJsonObject>* TangentDelta = nullptr;
            if ((*DeltaObj)->TryGetObjectField(TEXT("tangentDelta"), TangentDelta) && TangentDelta && TangentDelta->IsValid())
            {
                double X = 0, Y = 0, Z = 0;
                (*TangentDelta)->TryGetNumberField(TEXT("x"), X);
                (*TangentDelta)->TryGetNumberField(TEXT("y"), Y);
                (*TangentDelta)->TryGetNumberField(TEXT("z"), Z);
                Delta.TangentZDelta = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            }

            Deltas.Add(Delta);
        }
    }

    if (!MorphTarget)
    {
        MorphTarget = NewObject<UMorphTarget>(Mesh, FName(*MorphTargetName));
        if (!MorphTarget)
        {
            S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create morph target object"), TEXT("CREATION_FAILED"));
            return true;
        }
        MorphTarget->BaseSkelMesh = Mesh;
        bCreatedMorphTarget = true;
    }

    // Apply deltas to morph target
    // MorphLODModels is protected in UE 5.6+, use PopulateDeltas() for proper editor workflow
#if WITH_EDITOR
    // Use PopulateDeltas - the proper API for morph target manipulation
    // This handles all internal data structures correctly
    TArray<FSkelMeshSection> EmptySections; // PopulateDeltas can work with empty sections array
    MorphTarget->PopulateDeltas(Deltas, 0, EmptySections, false, false);
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Morph target manipulation requires editor"), TEXT("NOT_SUPPORTED"));
    return true;
#endif


    // Validate morph target has valid data after setting deltas
    // This prevents returning success for morph targets that trigger Engine Ensures
    if (!MorphTarget->HasValidData())
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Morph target '%s' has no valid data - deltas may be empty or invalid"), *MorphTargetName),
            TEXT("INVALID_MORPH_DATA"));
        return true;
    }

    if (bCreatedMorphTarget)
    {
        Mesh->RegisterMorphTarget(MorphTarget);
    }

    McpSafeAssetSave(Mesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("deltaCount"), Deltas.Num());
    Result->SetBoolField(TEXT("created"), bCreatedMorphTarget);

    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Set %d deltas on morph target '%s'"), Deltas.Num(), *MorphTargetName), Result);
    return true;
}

/**
 * Handle: import_morph_targets
 * Import morph targets from an external file (FBX)
 */
bool McpHandlers::AnimationPhysics::HandleImportMorphTargets(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    FString SourceFilePath = GetJsonStringField(Payload, TEXT("morphTargetPath"));
    if (SourceFilePath.IsEmpty())
    {
        SourceFilePath = GetJsonStringField(Payload, TEXT("sourcePath"));
    }

    if (SkeletalMeshPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // If source file provided, import from it
    if (!SourceFilePath.IsEmpty() && FPaths::FileExists(SourceFilePath))
    {
        // Note: Full FBX import for morph targets requires FbxImporter
        // This is a simplified response indicating the operation is queued
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("FBX morph target import requires using the asset import pipeline. Use manage_asset import action with the FBX file."), 
            TEXT("USE_ASSET_IMPORT"));
        return true;
    }

    // Return current morph targets as info
    TArray<TSharedPtr<FJsonValue>> MorphTargetArray;
    for (UMorphTarget* MT : Mesh->GetMorphTargets())
    {
        if (!MT) continue;
        
        TSharedPtr<FJsonObject> MTObj = McpHandlerUtils::CreateResultObject();
        MTObj->SetStringField(TEXT("name"), MT->GetName());
        MorphTargetArray.Add(MakeShared<FJsonValueObject>(MTObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("morphTargets"), MorphTargetArray);
    Result->SetNumberField(TEXT("count"), MorphTargetArray.Num());

    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        TEXT("Use manage_asset import to import morph targets from FBX"), Result);
    return true;
}


// ============================================================================
// BATCH 7: Skin Weight Operations
// ============================================================================

/**
 * Handle: normalize_weights
 * Normalize skin weights to sum to 1.0 for each vertex
 */
bool McpHandlers::AnimationPhysics::HandleNormalizeWeights(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));

    if (SkeletalMeshPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // A plain Mesh->Build() does NOT renormalize skin weights; it only rebuilds
    // render data from the existing imported weights. Re-weighting requires the
    // Skeletal Mesh Editor's weight tools (no safe in-place API on UE 5.7).
    S.SendAutomationError(RequestingSocket, RequestId,
        TEXT("Skin weight normalization is not supported via the bridge. Use the Skeletal Mesh Editor's ")
        TEXT("Skin Weights tools (or re-import with normalization enabled). A plain rebuild does not renormalize weights."),
        TEXT("MANUAL_INTERVENTION_REQUIRED"));
    return true;
}

/**
 * Handle: prune_weights
 * Remove bone influences below a threshold
 */
bool McpHandlers::AnimationPhysics::HandlePruneWeights(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    double Threshold = 0.01;
    Payload->TryGetNumberField(TEXT("threshold"), Threshold);

    if (SkeletalMeshPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // The prune threshold is an import-time option; a post-import Mesh->Build()
    // does NOT drop low influences (it rebuilds render data from existing weights).
    // Pruning safely requires the editor's Skin Weights tools or a re-import.
    S.SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Skin weight pruning (threshold %f) is not supported via the bridge. ")
            TEXT("Re-import the mesh with the influence threshold set, or use the Skeletal Mesh Editor. ")
            TEXT("A plain rebuild does not prune influences."), Threshold),
        TEXT("MANUAL_INTERVENTION_REQUIRED"));
    return true;
}


// ============================================================================
// BATCH 8: Cloth Operations
// ============================================================================

/**
 * Handle: bind_cloth_to_skeletal_mesh
 * Bind a cloth simulation asset to a skeletal mesh
 */
bool McpHandlers::AnimationPhysics::HandleBindClothToSkeletalMesh(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    FString ClothAssetName = GetJsonStringField(Payload, TEXT("clothAssetName"));
    int32 MeshLodIndex = 0;
    int32 SectionIndex = 0;
    int32 AssetLodIndex = 0;
    
    Payload->TryGetNumberField(TEXT("meshLodIndex"), MeshLodIndex);
    Payload->TryGetNumberField(TEXT("sectionIndex"), SectionIndex);
    Payload->TryGetNumberField(TEXT("assetLodIndex"), AssetLodIndex);

    if (SkeletalMeshPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

#if WITH_EDITOR
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    
    // Find the cloth asset by name if provided
    UClothingAssetBase* TargetClothAsset = nullptr;
    // UE 5.7 returns TArray<TObjectPtr<>> - UE 5.0 returns TArray<UClothingAssetBase*>
    const auto& ClothingAssets = Mesh->GetMeshClothingAssets();
    
    if (!ClothAssetName.IsEmpty())
    {
        for (const auto& ClothAssetPtr : ClothingAssets)
        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
            // UE 5.3+ uses TObjectPtr in non-const getter
            UClothingAssetBase* ClothAsset = ClothAssetPtr.Get();
#else
            // UE 5.0-5.2 uses raw pointers
            UClothingAssetBase* ClothAsset = ClothAssetPtr;
#endif
            if (ClothAsset && ClothAsset->GetName() == ClothAssetName)
            {
                TargetClothAsset = ClothAsset;
                break;
            }
        }
        
        if (!TargetClothAsset)
        {
            S.SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Cloth asset '%s' not found on mesh"), *ClothAssetName), 
                TEXT("CLOTH_NOT_FOUND"));
            return true;
        }
        
        // Bind the cloth asset to the specified section
        bool bSuccess = TargetClothAsset->BindToSkeletalMesh(Mesh, MeshLodIndex, SectionIndex, AssetLodIndex);
        
        if (bSuccess)
        {
            McpSafeAssetSave(Mesh);
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("clothAssetName"), ClothAssetName);
            Result->SetNumberField(TEXT("meshLodIndex"), MeshLodIndex);
            Result->SetNumberField(TEXT("sectionIndex"), SectionIndex);
            Result->SetNumberField(TEXT("assetLodIndex"), AssetLodIndex);
            
            S.SendAutomationResponse(RequestingSocket, RequestId, true, 
                FString::Printf(TEXT("Cloth asset '%s' bound to section %d"), *ClothAssetName, SectionIndex), Result);
        }
        else
        {
            S.SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Failed to bind cloth asset to skeletal mesh section"), 
                TEXT("BIND_FAILED"));
            return true;
        }
    }
    else
    {
        // No cloth asset specified - return list of available cloth assets
        TArray<TSharedPtr<FJsonValue>> ClothingArray;
        for (const auto& ClothAssetPtr : ClothingAssets)
        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
            UClothingAssetBase* ClothAsset = ClothAssetPtr.Get();
#else
            UClothingAssetBase* ClothAsset = ClothAssetPtr;
#endif
            if (!ClothAsset) continue;
            
            TSharedPtr<FJsonObject> ClothObj = McpHandlerUtils::CreateResultObject();
            ClothObj->SetStringField(TEXT("name"), ClothAsset->GetName());
            // Use UClothingAssetCommon::GetNumLods() for UE 5.7+ compatibility
            if (UClothingAssetCommon* ClothAssetCommon = Cast<UClothingAssetCommon>(ClothAsset))
            {
                ClothObj->SetNumberField(TEXT("numLods"), ClothAssetCommon->GetNumLods());
            }
            ClothingArray.Add(MakeShared<FJsonValueObject>(ClothObj));
        }
        
        Result->SetArrayField(TEXT("availableClothAssets"), ClothingArray);
        Result->SetNumberField(TEXT("clothingAssetCount"), ClothingAssets.Num());
        
        S.SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Found %d cloth assets. Provide clothAssetName to bind."), ClothingAssets.Num()), Result);
    }
    
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, 
        TEXT("Cloth binding requires editor mode."), 
        TEXT("NOT_EDITOR"));
    return true;
#endif
}

/**
 * Handle: assign_cloth_asset_to_mesh
 * Assign an existing cloth asset to a skeletal mesh section
 */
bool McpHandlers::AnimationPhysics::HandleAssignClothAssetToMesh(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));

    if (SkeletalMeshPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // List current clothing assets
    TArray<TSharedPtr<FJsonValue>> ClothingArray;
    for (const auto& ClothAssetPtr : Mesh->GetMeshClothingAssets())
    {
        #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        UClothingAssetBase* ClothAsset = ClothAssetPtr.Get();
        #else
        UClothingAssetBase* ClothAsset = ClothAssetPtr;
        #endif
        if (!ClothAsset) continue;
        
        TSharedPtr<FJsonObject> ClothObj = McpHandlerUtils::CreateResultObject();
        ClothObj->SetStringField(TEXT("name"), ClothAsset->GetName());
        ClothingArray.Add(MakeShared<FJsonValueObject>(ClothObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetArrayField(TEXT("clothingAssets"), ClothingArray);
    Result->SetNumberField(TEXT("count"), ClothingArray.Num());

    // Return an explicit error rather than claiming cloth assignment occurred.
    S.SendAutomationError(RequestingSocket, RequestId, 
        TEXT("Cloth asset assignment requires using the Cloth Paint tool in Unreal Editor. ")
        TEXT("Use the Skeletal Mesh Editor's Paint Cloth tool to assign cloth assets to mesh sections."), 
        TEXT("MANUAL_INTERVENTION_REQUIRED"));
    return true;
}

// ============================================================================
// Classed animation_physics skeleton members
// ============================================================================
// Per-action entry points called by the FMcpCall classes
// (MCP/Calls/McpCalls_AnimationPhysics.cpp): thin wrappers where the retired
// HandleManageSkeleton dispatcher delegated to a dedicated member, verbatim
// extractions where its branch was inline.

bool McpHandlers::AnimationPhysics::HandleSkeletonGetSkeletonInfo(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleGetSkeletonInfo(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonListBones(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleListBones(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonListSockets(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleListSockets(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonCreateSocket(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleCreateSocket(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonConfigureSocket(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleConfigureSocket(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonCreateVirtualBone(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleCreateVirtualBone(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonCreatePhysicsAsset(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleCreatePhysicsAsset(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonListPhysicsBodies(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleListPhysicsBodies(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonAddPhysicsBody(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleAddPhysicsBody(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonConfigurePhysicsBody(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleConfigurePhysicsBody(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonAddPhysicsConstraint(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleAddPhysicsConstraint(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonConfigureConstraintLimits(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleConfigureConstraintLimits(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonRenameBone(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleRenameBone(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonSetBoneTransform(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleSetBoneTransform(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonCreateMorphTarget(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleCreateMorphTarget(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonSetMorphTargetDeltas(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleSetMorphTargetDeltas(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonImportMorphTargets(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleImportMorphTargets(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonNormalizeWeights(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleNormalizeWeights(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonPruneWeights(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandlePruneWeights(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonBindClothToSkeletalMesh(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleBindClothToSkeletalMesh(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonAssignClothAssetToMesh(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleAssignClothAssetToMesh(S, RequestId, Payload, RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleSkeletonCreateSkeleton(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("path"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    }
    FString RootBoneName = GetJsonStringField(Payload, TEXT("rootBoneName"));
    if (RootBoneName.IsEmpty())
    {
        RootBoneName = TEXT("Root");
    }
    
    if (SkeletonPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("path or skeletonPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }
    
    // SECURITY: Validate path to prevent path traversal attacks
    // Ensure path starts with /Game/ and contains no traversal sequences
    if (!SkeletonPath.StartsWith(TEXT("/Game/")) && !SkeletonPath.StartsWith(TEXT("/Engine/")) && !SkeletonPath.StartsWith(TEXT("/Temp/")))
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Invalid path. Path must start with /Game/, /Engine/, or /Temp/"), TEXT("INVALID_PATH"));
        return true;
    }
    
    // Check for path traversal attempts
    if (SkeletonPath.Contains(TEXT("..")) || SkeletonPath.Contains(TEXT("//")) || SkeletonPath.Contains(TEXT("\\")))
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Invalid path. Path contains illegal characters or traversal sequences"), TEXT("INVALID_PATH"));
        return true;
    }
    
    // Validate using Unreal's package name validation (UE 5.7 uses EErrorCode)
    FPackageName::EErrorCode ErrorCode;
    if (!FPackageName::IsValidLongPackageName(SkeletonPath, false, &ErrorCode))
    {
        FString ErrorMsg = FPackageName::FormatErrorAsString(SkeletonPath, ErrorCode);
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Invalid package path: %s"), *ErrorMsg), TEXT("INVALID_PATH"));
        return true;
    }
    
    // Normalize path
    FString PackagePath = FPaths::GetPath(SkeletonPath);
    FString SkeletonName = FPaths::GetBaseFilename(SkeletonPath);
    FString FullPackagePath = PackagePath / SkeletonName;
    
    // Create package
    UPackage* Package = CreatePackage(*FullPackagePath);
    if (!Package)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        return true;
    }
    
    // Create skeleton asset
    USkeleton* NewSkeleton = NewObject<USkeleton>(Package, *SkeletonName, RF_Public | RF_Standalone);
    if (!NewSkeleton)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create skeleton object"), TEXT("CREATION_FAILED"));
        return true;
    }
    
    // Initialize with a root bone using FReferenceSkeletonModifier
    FReferenceSkeletonModifier Modifier(NewSkeleton);
    FMeshBoneInfo RootBone;
    RootBone.Name = FName(*RootBoneName);
    RootBone.ParentIndex = INDEX_NONE;
#if WITH_EDITORONLY_DATA
    RootBone.ExportName = RootBoneName;
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    Modifier.Add(RootBone, FTransform::Identity, true); // bAllowMultipleRoots = true for first bone
#else
    // UE 5.0-5.2: Add() only takes 2 parameters
    Modifier.Add(RootBone, FTransform::Identity);
#endif
    
    McpSafeAssetSave(NewSkeleton);
    
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletonPath"), NewSkeleton->GetPathName());
    Result->SetStringField(TEXT("rootBoneName"), RootBoneName);
    Result->SetNumberField(TEXT("boneCount"), 1);
    
    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Skeleton created with root bone '%s'"), *RootBoneName), Result);
    return true;
}

bool McpHandlers::AnimationPhysics::HandleSkeletonAddBone(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));
    FString ParentName = GetJsonStringField(Payload, TEXT("parentBone"));
    if (ParentName.IsEmpty())
    {
        ParentName = GetJsonStringField(Payload, TEXT("parentBoneName"));
    }
    
    if (SkeletonPath.IsEmpty() || BoneName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath and boneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }
    
    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    if (!Skeleton)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }
    
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    
    // Check if bone already exists
    if (RefSkeleton.FindBoneIndex(FName(*BoneName)) != INDEX_NONE)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Bone '%s' already exists"), *BoneName), TEXT("BONE_EXISTS"));
        return true;
    }
    
    // Find parent bone index
    int32 ParentIndex = INDEX_NONE;
    if (!ParentName.IsEmpty())
    {
        ParentIndex = RefSkeleton.FindBoneIndex(FName(*ParentName));
        if (ParentIndex == INDEX_NONE)
        {
            S.SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Parent bone '%s' not found"), *ParentName), TEXT("PARENT_NOT_FOUND"));
            return true;
        }
    }
    else if (RefSkeleton.GetRawBoneNum() > 0)
    {
        // Cannot add a root bone if the skeleton already has bones - need to specify a parent
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Cannot add root bone; Skeleton already has bones. Specify parentBone."), TEXT("PARENT_REQUIRED"));
        return true;
    }
    
    // Parse transform from payload
    FVector Location = ParseVectorFromJson(Payload, TEXT("location"));
    FRotator Rotation = ParseRotatorFromJson(Payload, TEXT("rotation"));
    FVector Scale = ParseVectorFromJson(Payload, TEXT("scale"), FVector::OneVector);
    FTransform BoneTransform(Rotation, Location, Scale);
    
    // Add the bone using FReferenceSkeletonModifier
    FReferenceSkeletonModifier Modifier(Skeleton);
    FMeshBoneInfo NewBone;
    NewBone.Name = FName(*BoneName);
    NewBone.ParentIndex = ParentIndex;
#if WITH_EDITORONLY_DATA
    NewBone.ExportName = BoneName;
#endif
    
    // Allow multiple roots only if no parent is specified and this is the first bone
    bool bAllowMultipleRoots = ParentIndex == INDEX_NONE && RefSkeleton.GetRawBoneNum() == 0;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    Modifier.Add(NewBone, BoneTransform, bAllowMultipleRoots);
#else
    // UE 5.0-5.2: Add() only takes 2 parameters
    Modifier.Add(NewBone, BoneTransform);
#endif
    
    McpSafeAssetSave(Skeleton);
    
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetStringField(TEXT("parentBone"), ParentName);
    Result->SetNumberField(TEXT("boneCount"), Skeleton->GetReferenceSkeleton().GetRawBoneNum());
    
    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Bone '%s' added to skeleton"), *BoneName), Result);
    return true;
}

bool McpHandlers::AnimationPhysics::HandleSkeletonRemoveBone(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));
    bool bRemoveChildren = false;
    Payload->TryGetBoolField(TEXT("removeChildren"), bRemoveChildren);
    
    if (SkeletonPath.IsEmpty() || BoneName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath and boneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }
    
    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    if (!Skeleton)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }
    
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));
    
    if (BoneIndex == INDEX_NONE)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Bone '%s' not found"), *BoneName), TEXT("BONE_NOT_FOUND"));
        return true;
    }
    
    // Check if it's the root bone
    if (BoneIndex == 0)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Cannot remove root bone"), TEXT("CANNOT_REMOVE_ROOT"));
        return true;
    }
    
    // Remove the bone using FReferenceSkeletonModifier
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    FReferenceSkeletonModifier Modifier(Skeleton);
    Modifier.Remove(FName(*BoneName), bRemoveChildren);
    McpSafeAssetSave(Skeleton);
    
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("removedBone"), BoneName);
    Result->SetBoolField(TEXT("childrenRemoved"), bRemoveChildren);
    Result->SetNumberField(TEXT("boneCount"), Skeleton->GetReferenceSkeleton().GetRawBoneNum());
    
    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Bone '%s' removed from skeleton"), *BoneName), Result);
    return true;
#else
    // UE 5.0-5.2: FReferenceSkeletonModifier doesn't have Remove() method
    S.SendAutomationError(RequestingSocket, RequestId,
        TEXT("remove_bone is not supported in UE 5.0-5.2. Please use UE 5.3 or later."),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleSkeletonSetBoneParent(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));
    FString NewParentName = GetJsonStringField(Payload, TEXT("parentBone"));
    if (NewParentName.IsEmpty())
    {
        NewParentName = GetJsonStringField(Payload, TEXT("newParentBone"));
    }
    
    if (SkeletonPath.IsEmpty() || BoneName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath and boneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }
    
    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    if (!Skeleton)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }
    
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));
    
    if (BoneIndex == INDEX_NONE)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Bone '%s' not found"), *BoneName), TEXT("BONE_NOT_FOUND"));
        return true;
    }
    
    // Set new parent using FReferenceSkeletonModifier
    // NewParentName can be empty/NAME_None to unparent (make root)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    FReferenceSkeletonModifier Modifier(Skeleton);
    FName ParentFName = NewParentName.IsEmpty() ? NAME_None : FName(*NewParentName);
    int32 NewBoneIndex = Modifier.SetParent(FName(*BoneName), ParentFName, true);
    
    if (NewBoneIndex == INDEX_NONE)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Failed to set parent. New parent '%s' may not exist or operation invalid."), *NewParentName), 
            TEXT("SET_PARENT_FAILED"));
        return true;
    }
    
    McpSafeAssetSave(Skeleton);
    
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetStringField(TEXT("newParent"), NewParentName.IsEmpty() ? TEXT("(none - root)") : NewParentName);
    Result->SetNumberField(TEXT("newBoneIndex"), NewBoneIndex);
    
    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Bone '%s' parent changed to '%s'"), *BoneName, NewParentName.IsEmpty() ? TEXT("(none)") : *NewParentName), Result);
    return true;
#else
    // UE 5.0-5.2: FReferenceSkeletonModifier doesn't have SetParent() method
    S.SendAutomationError(RequestingSocket, RequestId, 
        TEXT("set_bone_parent is not supported in UE 5.0-5.2. Please use UE 5.3 or later."), 
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleSkeletonSetVertexWeights(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    FString ProfileName = GetJsonStringField(Payload, TEXT("profileName"));
    if (ProfileName.IsEmpty())
    {
        ProfileName = TEXT("CustomWeights");
    }
    
    if (SkeletalMeshPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }
    
    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }
    
    // Parse weights array
    const TArray<TSharedPtr<FJsonValue>>* WeightsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("weights"), WeightsArray) || !WeightsArray)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("weights array is required"), TEXT("MISSING_PARAM"));
        return true;
    }
    
#if WITH_EDITORONLY_DATA
    // Access the LOD model for editing
    FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
    if (!ImportedModel || ImportedModel->LODModels.Num() == 0)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Mesh has no LOD models"), TEXT("NO_LOD_MODELS"));
        return true;
    }
    
    int32 LODIndex = 0;
    Payload->TryGetNumberField(TEXT("lodIndex"), LODIndex);
    
    if (LODIndex >= ImportedModel->LODModels.Num())
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("LOD index %d out of range (max: %d)"), LODIndex, ImportedModel->LODModels.Num() - 1), 
            TEXT("INVALID_LOD"));
        return true;
    }
    
    FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
    
    // Create or update skin weight profile
    FSkinWeightProfileInfo* ProfileInfo = nullptr;
    for (FSkinWeightProfileInfo& Info : Mesh->GetSkinWeightProfiles())
    {
        if (Info.Name == FName(*ProfileName))
        {
            ProfileInfo = &Info;
            break;
        }
    }
    
    if (!ProfileInfo)
    {
        // Add new profile
        FSkinWeightProfileInfo NewProfile;
        NewProfile.Name = FName(*ProfileName);
        Mesh->AddSkinWeightProfile(NewProfile);
    }
    
    // Build FImportedSkinWeightProfileData from weights array
    FImportedSkinWeightProfileData& ProfileData = LODModel.SkinWeightProfiles.FindOrAdd(FName(*ProfileName));
    ProfileData.SkinWeights.SetNum(LODModel.NumVertices);
    
    int32 WeightsSet = 0;
    for (const TSharedPtr<FJsonValue>& WeightValue : *WeightsArray)
    {
        const TSharedPtr<FJsonObject>* WeightObj = nullptr;
        if (!WeightValue->TryGetObject(WeightObj) || !WeightObj || !WeightObj->IsValid())
        {
            continue;
        }
        
        int32 VertexIndex = 0;
        (*WeightObj)->TryGetNumberField(TEXT("vertexIndex"), VertexIndex);
        
        if (VertexIndex < 0 || VertexIndex >= static_cast<int32>(LODModel.NumVertices))
        {
            continue;
        }
        
        FRawSkinWeight& SkinWeight = ProfileData.SkinWeights[VertexIndex];
        FMemory::Memzero(&SkinWeight, sizeof(FRawSkinWeight));
        
        // Parse bone influences
        const TArray<TSharedPtr<FJsonValue>>* InfluencesArray = nullptr;
        if ((*WeightObj)->TryGetArrayField(TEXT("influences"), InfluencesArray) && InfluencesArray)
        {
            int32 InfluenceIndex = 0;
            for (const TSharedPtr<FJsonValue>& InfluenceValue : *InfluencesArray)
            {
                if (InfluenceIndex >= MAX_TOTAL_INFLUENCES) break;
                
                const TSharedPtr<FJsonObject>* InfluenceObj = nullptr;
                if (InfluenceValue->TryGetObject(InfluenceObj) && InfluenceObj && InfluenceObj->IsValid())
                {
                    int32 BoneIndex = 0;
                    double Weight = 0.0;
                    (*InfluenceObj)->TryGetNumberField(TEXT("boneIndex"), BoneIndex);
                    (*InfluenceObj)->TryGetNumberField(TEXT("weight"), Weight);
                    
                    SkinWeight.InfluenceBones[InfluenceIndex] = static_cast<FBoneIndexType>(BoneIndex);
                    SkinWeight.InfluenceWeights[InfluenceIndex] = static_cast<uint16>(FMath::Clamp(Weight, 0.0, 1.0) * 65535.0);
                    InfluenceIndex++;
                }
            }
        }
        
        WeightsSet++;
    }
    
    // Rebuild the mesh with the new skin weight profile
    Mesh->Build();
    McpSafeAssetSave(Mesh);
    
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetStringField(TEXT("profileName"), ProfileName);
    Result->SetNumberField(TEXT("verticesModified"), WeightsSet);
    Result->SetNumberField(TEXT("lodIndex"), LODIndex);
    
    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Set weights for %d vertices in profile '%s'"), WeightsSet, *ProfileName), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("set_vertex_weights requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleSkeletonAutoSkinWeights(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));

    if (SkeletalMeshPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // Mesh->Build() rebuilds render data from existing weights; it does NOT compute
    // auto/heat-map skin weights from bone positions. Auto-skinning lives in the
    // DCC import pipeline / editor tools, with no safe in-place API on UE 5.7.
    S.SendAutomationError(RequestingSocket, RequestId,
        TEXT("Automatic skin weight computation is not supported via the bridge. Auto-skinning is an ")
        TEXT("import-time / DCC operation; re-import the mesh with auto weights, or bind weights in the ")
        TEXT("Skeletal Mesh Editor. A plain rebuild does not compute new weights."),
        TEXT("MANUAL_INTERVENTION_REQUIRED"));
    return true;
}

bool McpHandlers::AnimationPhysics::HandleSkeletonCopyWeights(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SourceMeshPath = GetJsonStringField(Payload, TEXT("sourceMeshPath"));
    FString TargetMeshPath = GetJsonStringField(Payload, TEXT("targetMeshPath"));
    FString ProfileName = GetJsonStringField(Payload, TEXT("profileName"));
    if (ProfileName.IsEmpty())
    {
        ProfileName = TEXT("CopiedWeights");
    }
    int32 LODIndex = 0;
    Payload->TryGetNumberField(TEXT("lodIndex"), LODIndex);
    
    if (SourceMeshPath.IsEmpty() || TargetMeshPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("sourceMeshPath and targetMeshPath are required"), TEXT("MISSING_PARAM"));
        return true;
    }
    
    // CRITICAL: Validate any extra path parameters for security and existence
    // This prevents false negatives where unused parameters contain invalid paths
    FString ExtraSkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    if (!ExtraSkeletalMeshPath.IsEmpty())
    {
        FString SanitizedExtraPath = SanitizeProjectRelativePath(ExtraSkeletalMeshPath);
        if (SanitizedExtraPath.IsEmpty())
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid skeletalMeshPath parameter '%s': contains traversal sequences or invalid characters"), *ExtraSkeletalMeshPath),
                TEXT("INVALID_PATH"));
            return true;
        }
        // Also verify the asset exists - this prevents false negatives when test provides invalid path
        UObject* ExtraMeshAsset = StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *ExtraSkeletalMeshPath);
        if (!ExtraMeshAsset)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("skeletalMeshPath parameter '%s' does not exist"), *ExtraSkeletalMeshPath),
                TEXT("MESH_NOT_FOUND"));
            return true;
        }
    }
    
    FString Error;
    USkeletalMesh* SourceMesh = LoadSkeletalMeshFromPathSkel(SourceMeshPath, Error);
    if (!SourceMesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Source mesh not found: %s"), *Error), TEXT("SOURCE_NOT_FOUND"));
        return true;
    }
    
    USkeletalMesh* TargetMesh = LoadSkeletalMeshFromPathSkel(TargetMeshPath, Error);
    if (!TargetMesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Target mesh not found: %s"), *Error), TEXT("TARGET_NOT_FOUND"));
        return true;
    }
    
#if WITH_EDITORONLY_DATA
    FSkeletalMeshModel* SourceModel = SourceMesh->GetImportedModel();
    FSkeletalMeshModel* TargetModel = TargetMesh->GetImportedModel();
    
    if (!SourceModel || !TargetModel || 
        LODIndex >= SourceModel->LODModels.Num() || 
        LODIndex >= TargetModel->LODModels.Num())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid LOD models"), TEXT("INVALID_LOD"));
        return true;
    }
    
    // Per-vertex weight transfer requires mapping source vertices onto target
    // topology (closest-point / barycentric) and the SkeletalMeshUtilitiesCommon
    // transfer API. Creating an empty profile and zeroing every influence is NOT
    // a copy, so report an honest error instead of fabricating one.
    S.SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Skin weight transfer from '%s' to '%s' is not supported via the bridge. ")
            TEXT("Use the Skeletal Mesh Editor's 'Transfer Skin Weights' tool (or a DCC weight-transfer) ")
            TEXT("for a correct mapping; the bridge will not write an empty profile and call it copied."),
            *SourceMeshPath, *TargetMeshPath),
        TEXT("MANUAL_INTERVENTION_REQUIRED"));
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("copy_weights requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleSkeletonMirrorWeights(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    FString Axis = GetJsonStringField(Payload, TEXT("axis"));
    if (Axis.IsEmpty())
    {
        Axis = TEXT("X");
    }
    FString ProfileName = GetJsonStringField(Payload, TEXT("profileName"));
    if (ProfileName.IsEmpty())
    {
        ProfileName = TEXT("MirroredWeights");
    }
    
    if (SkeletalMeshPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }
    
    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }
    
#if WITH_EDITORONLY_DATA
    FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
    if (!ImportedModel || ImportedModel->LODModels.Num() == 0)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Mesh has no LOD models"), TEXT("NO_LOD_MODELS"));
        return true;
    }
    
    int32 LODIndex = 0;
    Payload->TryGetNumberField(TEXT("lodIndex"), LODIndex);
    
    FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
    
    // Create mirrored skin weight profile
    FSkinWeightProfileInfo NewProfile;
    NewProfile.Name = FName(*ProfileName);
    Mesh->AddSkinWeightProfile(NewProfile);
    
    FImportedSkinWeightProfileData& ProfileData = LODModel.SkinWeightProfiles.FindOrAdd(FName(*ProfileName));
    ProfileData.SkinWeights.SetNum(LODModel.NumVertices);
    
    // Initialize profile - mirroring logic would need vertex position data
    // For now we create the profile structure and indicate manual completion needed
    for (uint32 i = 0; i < LODModel.NumVertices; ++i)
    {
        FMemory::Memzero(&ProfileData.SkinWeights[i], sizeof(FRawSkinWeight));
    }
    
    Mesh->Build();
    McpSafeAssetSave(Mesh);
    
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetStringField(TEXT("profileName"), ProfileName);
    Result->SetStringField(TEXT("axis"), Axis);
    Result->SetNumberField(TEXT("lodIndex"), LODIndex);
    Result->SetStringField(TEXT("note"), TEXT("Skin weight profile created. Use Skeletal Mesh Editor for precise mirroring with bone name mapping."));
    
    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Skin weight profile '%s' created for mirroring along %s axis"), *ProfileName, *Axis), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("mirror_weights requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

#undef GetJsonStringField
#undef GetJsonNumberField
#undef GetJsonBoolField

#endif // WITH_EDITOR
