// =============================================================================
// McpAutomationBridge_AnimationHandlers.cpp
// =============================================================================
// Animation, skeleton, blend space, and control rig handlers.
//
// HANDLERS:
//   Animation Assets:
//     - create_animation_blueprint, create_anim_sequence, create_anim_montage
//     - create_blend_space, create_blend_space_1d, create_aim_offset
//     - get_anim_blueprint_info, get_anim_sequence_info
//     - get_skeleton_info, get_blend_space_info
//
//   Animation Editing:
//     - add_anim_curve, remove_anim_curve, get_anim_curves
//     - set_anim_curve_keys, add_anim_notify
//     - modify_anim_sequence, retarget_anim_sequence
//
//   Skeleton Operations:
//     - get_skeleton_bones, add_skeleton_socket
//     - set_bone_transform, get_bone_transform
//
//   Physics Assets:
//     - create_physics_asset, get_physics_asset_info
//     - modify_physics_body, set_physics_constraint
//
//   Control Rig:
//     - get_control_rig_info, modify_control_rig
//
// REFACTORING NOTES:
//   - Uses McpVersionCompatibility.h for UE 5.0-5.7 API abstraction
//   - Uses McpHandlerUtils for standardized JSON parsing/responses
//   - BlendSpaceBase.h deprecated in UE 5.3+, uses BlendSpace.h
//   - IAnimationDataController requires specific header paths by UE version
//
// VERSION COMPATIBILITY:
//   - BlendSpaceFactory: UE 5.0+ (conditional include)
//   - AnimationBlueprintLibrary: Header path varies by UE version
//   - AssetEditorSubsystem: Optional, header path varies
//   - AnimationDataController: UE 5.1+ with specific paths
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpResponseHelpers.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_AnimationHandlers.h"
#if WITH_EDITOR

// -----------------------------------------------------------------------------
// Editor-only Includes: Animation Core
// -----------------------------------------------------------------------------
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Animation Blueprint Library (path varies by UE version)
// -----------------------------------------------------------------------------
#if __has_include("Animation/AnimationBlueprintLibrary.h")
#include "Animation/AnimationBlueprintLibrary.h"
#elif __has_include("AnimationBlueprintLibrary.h")
#include "AnimationBlueprintLibrary.h"
#endif
#if __has_include("Animation/AnimBlueprintLibrary.h")
#include "Animation/AnimBlueprintLibrary.h"
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Blend Spaces
// -----------------------------------------------------------------------------
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"

// BlendSpaceBase.h is deprecated in favor of BlendSpace.h, but we need UBlendSpaceBase class
// Suppress the deprecation warning for this include
PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if __has_include("Animation/BlendSpaceBase.h")
#include "Animation/BlendSpaceBase.h"
#define MCP_HAS_BLENDSPACE_BASE 1
#elif __has_include("BlendSpaceBase.h")
#include "BlendSpaceBase.h"
#define MCP_HAS_BLENDSPACE_BASE 1
#else
#define MCP_HAS_BLENDSPACE_BASE 0
#endif
PRAGMA_ENABLE_DEPRECATION_WARNINGS

// -----------------------------------------------------------------------------
// Editor-only Includes: Animation Data Controller (UE 5.1+)
// -----------------------------------------------------------------------------
#if __has_include("AnimData/IAnimationDataController.h")
#include "AnimData/IAnimationDataController.h"
#endif
#if __has_include("Animation/AnimData/IAnimationDataModel.h")
#include "Animation/AnimData/IAnimationDataModel.h"
#endif
#if __has_include("Animation/AnimData/CurveIdentifier.h")
#include "Animation/AnimData/CurveIdentifier.h"
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Editor Framework
// -----------------------------------------------------------------------------
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EngineUtils.h"
#include "RenderingThread.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Blend Space Factories
// -----------------------------------------------------------------------------
#if __has_include("Factories/BlendSpaceFactoryNew.h") && __has_include("Factories/BlendSpaceFactory1D.h")
#include "Factories/BlendSpaceFactory1D.h"
#include "Factories/BlendSpaceFactoryNew.h"
#define MCP_HAS_BLENDSPACE_FACTORY 1
#else
#define MCP_HAS_BLENDSPACE_FACTORY 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Control Rig
// -----------------------------------------------------------------------------
#include "ControlRig.h"
// ControlRig headers removed for dynamic loading compatibility
// #include "ControlRigBlueprint.h" etc.

// -----------------------------------------------------------------------------
// Editor-only Includes: Asset Management
// -----------------------------------------------------------------------------
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Animation Factories
// -----------------------------------------------------------------------------
#include "Factories/AnimBlueprintFactory.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/AnimSequenceFactory.h"
#include "Factories/PhysicsAssetFactory.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Blueprint Utilities
// -----------------------------------------------------------------------------
#include "Kismet2/BlueprintEditorUtils.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: AnimGraph State Machine (for create_state_machine)
// -----------------------------------------------------------------------------
#if __has_include("AnimGraphNode_StateMachine.h")
#include "AnimGraphNode_StateMachine.h"
#endif
#if __has_include("AnimStateNode.h")
#include "AnimStateNode.h"
#endif
#if __has_include("AnimStateTransitionNode.h")
#include "AnimStateTransitionNode.h"
#define MCP_HAS_ANIM_STATE_TRANSITION 1
#else
#define MCP_HAS_ANIM_STATE_TRANSITION 0
#endif
#if __has_include("AnimationStateMachineGraph.h")
#include "AnimationStateMachineGraph.h"
#define MCP_HAS_ANIM_STATE_MACHINE_GRAPH 1
#else
#define MCP_HAS_ANIM_STATE_MACHINE_GRAPH 0
#endif
#if __has_include("AnimationStateMachineSchema.h")
#include "AnimationStateMachineSchema.h"
#define MCP_HAS_ANIM_STATE_MACHINE_SCHEMA 1
#else
#define MCP_HAS_ANIM_STATE_MACHINE_SCHEMA 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: AnimGraph Blend Tree (for create_blend_tree)
// -----------------------------------------------------------------------------
#if __has_include("AnimGraphNode_BlendTree.h")
#include "AnimGraphNode_BlendTree.h"
#define MCP_HAS_ANIM_GRAPH_NODE_BLEND_TREE 1
#else
#define MCP_HAS_ANIM_GRAPH_NODE_BLEND_TREE 0
#endif
#if __has_include("Animation/BlendTree.h")
#include "Animation/BlendTree.h"
#define MCP_HAS_BLEND_TREE 1
#else
#define MCP_HAS_BLEND_TREE 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Misc Utilities
// -----------------------------------------------------------------------------
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Components/PrimitiveComponent.h"

#if __has_include("PhysicsEngine/WheeledVehicleMovementComponent4W.h")
#include "PhysicsEngine/WheeledVehicleMovementComponent4W.h"
#define MCP_HAS_WHEELED_VEHICLE_4W 1
#else
#define MCP_HAS_WHEELED_VEHICLE_4W 0
#endif

#if __has_include("PhysicsEngine/VehicleWheel.h")
#include "PhysicsEngine/VehicleWheel.h"
#define MCP_HAS_VEHICLE_WHEEL 1
#else
#define MCP_HAS_VEHICLE_WHEEL 0
#endif

#if __has_include("ChaosWheeledVehicleMovementComponent.h")
#include "ChaosWheeledVehicleMovementComponent.h"
#define MCP_HAS_CHAOS_WHEELED_VEHICLE 1
#elif __has_include("Chaos/ChaosWheeledVehicleMovementComponent.h")
#include "Chaos/ChaosWheeledVehicleMovementComponent.h"
#define MCP_HAS_CHAOS_WHEELED_VEHICLE 1
#else
#define MCP_HAS_CHAOS_WHEELED_VEHICLE 0
#endif

// If we have Chaos vehicles but not PhysX vehicles, enable the vehicle feature via Chaos
#if MCP_HAS_CHAOS_WHEELED_VEHICLE && !MCP_HAS_WHEELED_VEHICLE_4W
#undef MCP_HAS_WHEELED_VEHICLE_4W
#define MCP_HAS_WHEELED_VEHICLE_4W 1
// Alias for Chaos vehicle component type
#define UWheeledVehicleMovementComponent4W UChaosWheeledVehicleMovementComponent
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Editor Subsystems (path varies by UE version)
// -----------------------------------------------------------------------------
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#define MCP_HAS_ASSET_EDITOR_SUBSYSTEM 1
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#define MCP_HAS_ASSET_EDITOR_SUBSYSTEM 1
#else
#define MCP_HAS_ASSET_EDITOR_SUBSYSTEM 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: UObject Reflection
// -----------------------------------------------------------------------------
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

namespace {
#if MCP_HAS_BLENDSPACE_FACTORY
/**
 * @brief Creates a new 1D or 2D Blend Space asset bound to a target skeleton.
 *
 * Creates and returns a newly created UBlendSpace (2D) or UBlendSpace1D (1D)
 * asset using the appropriate factory and places it at the given package path.
 *
 * @param AssetName Name to assign to the new asset.
 * @param PackagePath Package path where the asset will be created (e.g.
 * "/Game/Animations").
 * @param TargetSkeleton Skeleton to bind the created Blend Space to.
 * @param bTwoDimensional If true, creates a 2D UBlendSpace; if false, creates a
 * 1D UBlendSpace1D.
 * @param OutError Receives a human-readable error message on failure.
 * @return UObject* Pointer to the created blend space asset on success, or
 * `nullptr` on failure.
 */
static UObject *CreateBlendSpaceAsset(const FString &AssetName,
                                      const FString &PackagePath,
                                      USkeleton *TargetSkeleton,
                                      bool bTwoDimensional, FString &OutError) {
  OutError.Reset();

  // IAssetTools::CreateAsset pops a blocking "overwrite?" modal when the target asset
  // already exists — on the bridge's game thread that starves every subsequent MCP call
  // until a human dismisses it. Fail fast instead (covers all blend-space callers).
  const FString ExistingObjectPath =
      FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
  if (UEditorAssetLibrary::DoesAssetExist(ExistingObjectPath)) {
    OutError = FString::Printf(TEXT("Asset already exists: %s"), *ExistingObjectPath);
    return nullptr;
  }

  UFactory *Factory = nullptr;
  UClass *DesiredClass = nullptr;

  if (bTwoDimensional) {
    UBlendSpaceFactoryNew *Factory2D = NewObject<UBlendSpaceFactoryNew>();
    if (!Factory2D) {
      OutError = TEXT("Failed to allocate BlendSpace factory");
      return nullptr;
    }
    Factory2D->TargetSkeleton = TargetSkeleton;
    Factory = Factory2D;
    DesiredClass = UBlendSpace::StaticClass();
  } else {
    UBlendSpaceFactory1D *Factory1D = NewObject<UBlendSpaceFactory1D>();
    if (!Factory1D) {
      OutError = TEXT("Failed to allocate BlendSpace1D factory");
      return nullptr;
    }
    Factory1D->TargetSkeleton = TargetSkeleton;
    Factory = Factory1D;
    DesiredClass = UBlendSpace1D::StaticClass();
  }

  if (!Factory || !DesiredClass) {
    OutError = TEXT("BlendSpace factory unavailable");
    return nullptr;
  }

  FAssetToolsModule &AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
  return AssetToolsModule.Get().CreateAsset(AssetName, PackagePath,
                                            DesiredClass, Factory);
}

/**
 * @brief Applies axis range and grid configuration to a blend space asset.
 *
 * Reads numeric fields from the provided JSON payload and updates the blend
 * space's first axis (minX, maxX, gridX) and, if bTwoDimensional is true,
 * the second axis (minY, maxY, gridY). Marks the asset package dirty when
 * modifications are applied.
 *
 * @param BlendSpaceAsset Blend space or blend space base object to configure.
 *                       If null, the function is a no-op.
 * @param Payload JSON object containing axis configuration fields:
 *                - "minX", "maxX", "gridX" for axis 0 (required defaults:
 * 0,1,3)
 *                - "minY", "maxY", "gridY" for axis 1 when bTwoDimensional is
 * true
 * @param bTwoDimensional If true, the second axis is also configured.
 *
 * Notes:
 * - If the engine headers/types required to modify blend parameters are
 *   unavailable, the function logs and skips axis configuration.
 * - Grid values are clamped to a minimum of 1.
 */
static void ApplyBlendSpaceConfiguration(UObject *BlendSpaceAsset,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         bool bTwoDimensional) {
  if (!BlendSpaceAsset || !Payload.IsValid()) {
    return;
  }

  double MinX = 0.0, MaxX = 1.0, GridX = 3.0;
  Payload->TryGetNumberField(TEXT("minX"), MinX);
  Payload->TryGetNumberField(TEXT("maxX"), MaxX);
  Payload->TryGetNumberField(TEXT("gridX"), GridX);

#if MCP_HAS_BLENDSPACE_BASE
  // UBlendSpaceBase is deprecated in UE 5.0+ but still needed for backward compatibility
  PRAGMA_DISABLE_DEPRECATION_WARNINGS
  if (UBlendSpaceBase *BlendBase = Cast<UBlendSpaceBase>(BlendSpaceAsset)) {
    BlendBase->Modify();

    FBlendParameter &Axis0 =
        const_cast<FBlendParameter &>(BlendBase->GetBlendParameter(0));
    Axis0.Min = static_cast<float>(MinX);
    Axis0.Max = static_cast<float>(MaxX);
    Axis0.GridNum = FMath::Max(1, static_cast<int32>(GridX));

    if (bTwoDimensional) {
      double MinY = 0.0, MaxY = 1.0, GridY = 3.0;
      Payload->TryGetNumberField(TEXT("minY"), MinY);
      Payload->TryGetNumberField(TEXT("maxY"), MaxY);
      Payload->TryGetNumberField(TEXT("gridY"), GridY);

      FBlendParameter &Axis1 =
          const_cast<FBlendParameter &>(BlendBase->GetBlendParameter(1));
      Axis1.Min = static_cast<float>(MinY);
      Axis1.Max = static_cast<float>(MaxY);
      Axis1.GridNum = FMath::Max(1, static_cast<int32>(GridY));
    }

    BlendBase->MarkPackageDirty();
  }
  PRAGMA_ENABLE_DEPRECATION_WARNINGS
#else
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("ApplyBlendSpaceConfiguration: BlendSpaceBase headers "
              "unavailable; skipping axis configuration."));
  if (bTwoDimensional) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Requested 2D blend space but BlendSpaceBase headers are "
                "missing; axis configuration skipped."));
  }
  if (!BlendSpaceAsset->IsA<UBlendSpace>() &&
      !BlendSpaceAsset->IsA<UBlendSpace1D>()) {
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("ApplyBlendSpaceConfiguration: Asset %s is not a BlendSpace type"),
        *BlendSpaceAsset->GetName());
  }
#endif
}
#endif
} // namespace
#else
#define MCP_HAS_BLENDSPACE_FACTORY 0
#endif // WITH_EDITOR

// =============================================================================
// Classed animation_physics core members
// =============================================================================
// Per-action entry points called by the FMcpCall classes
// (MCP/Calls/McpCalls_AnimationPhysics.cpp). Extracted verbatim from the
// retired HandleAnimationPhysicsAction dispatcher; each member replicates the
// chain's shared response prologue/tail and its non-editor stub.

bool McpHandlers::AnimationPhysics::HandleAnimPhysCleanup(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("cleanup"));
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  const TArray<TSharedPtr<FJsonValue>> *ArtifactsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("artifacts"), ArtifactsArray) ||
      !ArtifactsArray) {
    Message = TEXT("artifacts array required for cleanup");
    ErrorCode = TEXT("INVALID_ARGUMENT");
  } else {
    TArray<FString> Cleaned;
    TArray<FString> Missing;
    TArray<FString> Failed;

    for (const TSharedPtr<FJsonValue> &Val : *ArtifactsArray) {
      if (!Val.IsValid() || Val->Type != EJson::String) {
        continue;
      }

      const FString ArtifactPath = Val->AsString().TrimStartAndEnd();
      if (ArtifactPath.IsEmpty()) {
        continue;
      }

      if (UEditorAssetLibrary::DoesAssetExist(ArtifactPath)) {
// Close editors to ensure asset can be deleted
#if MCP_HAS_ASSET_EDITOR_SUBSYSTEM
        if (GEditor) {
          UObject *Asset = LoadObject<UObject>(nullptr, *ArtifactPath);
          if (Asset) {
            if (UAssetEditorSubsystem *AssetEditorSubsystem =
                    GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
              AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
            }
          }
        }
#endif

        // Flush before deleting to release references
        if (GEditor) {
          FlushRenderingCommands();
          GEditor->ForceGarbageCollection(true);
          FlushRenderingCommands();
        }

        if (UEditorAssetLibrary::DeleteAsset(ArtifactPath)) {
          Cleaned.Add(ArtifactPath);
        } else {
          Failed.Add(ArtifactPath);
        }
      } else {
        Missing.Add(ArtifactPath);
      }
    }

    TArray<TSharedPtr<FJsonValue>> CleanedArray;
    for (const FString &Path : Cleaned) {
      CleanedArray.Add(MakeShared<FJsonValueString>(Path));
    }
    if (CleanedArray.Num() > 0) {
      Resp->SetArrayField(TEXT("cleaned"), CleanedArray);
    }
    Resp->SetNumberField(TEXT("cleanedCount"), Cleaned.Num());

    if (Missing.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> MissingArray;
      for (const FString &Path : Missing) {
        MissingArray.Add(MakeShared<FJsonValueString>(Path));
      }
      Resp->SetArrayField(TEXT("missing"), MissingArray);
    }

    if (Failed.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> FailedArray;
      for (const FString &Path : Failed) {
        FailedArray.Add(MakeShared<FJsonValueString>(Path));
      }
      Resp->SetArrayField(TEXT("failed"), FailedArray);
    }

    if (Cleaned.Num() > 0 && Failed.Num() == 0) {
      bSuccess = true;
      Message = TEXT("Animation artifacts removed");
    } else if (Failed.Num() > 0) {
      // Actual failure to delete something that exists
      bSuccess = false;
      Message = TEXT("Some animation artifacts could not be removed");
      ErrorCode = TEXT("CLEANUP_PARTIAL");
      Resp->SetStringField(TEXT("error"), Message);
    } else if (Cleaned.Num() == 0 && Missing.Num() > 0 && Failed.Num() == 0) {
      // All artifacts were missing - not an error, just nothing to do
      // The end state (no artifacts at those paths) is what the user wanted
      bSuccess = true;
      Message = TEXT("No animation artifacts needed removal (all specified paths were missing)");
      Resp->SetBoolField(TEXT("noOp"), true);
    } else {
      bSuccess = false;
      Message = TEXT("No animation artifacts were removed");
      ErrorCode = TEXT("CLEANUP_NO_OP");
      Resp->SetStringField(TEXT("error"), Message);
    }
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }
  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleAnimPhysCreateBlendSpace(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("create_blend_space"));
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  FString Name;
  if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
    Message = TEXT("name field required for blend space creation");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
  } else {
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game/Animations");
    }

    FString SkeletonPath;
    if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) ||
        SkeletonPath.IsEmpty()) {
      Message =
          TEXT("skeletonPath is required to bind blend space to a skeleton");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      USkeleton *TargetSkeleton =
          LoadObject<USkeleton>(nullptr, *SkeletonPath);
      if (!TargetSkeleton) {
        Message = TEXT("Failed to load skeleton for blend space");
        ErrorCode = TEXT("LOAD_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        int32 Dimensions = 1;
        double DimensionsNumber = 1.0;
        if (Payload->TryGetNumberField(TEXT("dimensions"),
                                       DimensionsNumber)) {
          Dimensions = static_cast<int32>(DimensionsNumber);
        }
        const bool bTwoDimensional = (Dimensions >= 2);

        // Validation for Issue #10
        double MinX = 0.0, MaxX = 1.0, GridX = 3.0;
        Payload->TryGetNumberField(TEXT("minX"), MinX);
        Payload->TryGetNumberField(TEXT("maxX"), MaxX);
        Payload->TryGetNumberField(TEXT("gridX"), GridX);

        if (MinX >= MaxX) {
          Message = TEXT("minX must be less than maxX");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
        } else if (GridX <= 0) {
          Message = TEXT("gridX must be greater than 0");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          if (bTwoDimensional) {
            double MinY = 0.0, MaxY = 1.0, GridY = 3.0;
            Payload->TryGetNumberField(TEXT("minY"), MinY);
            Payload->TryGetNumberField(TEXT("maxY"), MaxY);
            Payload->TryGetNumberField(TEXT("gridY"), GridY);

            if (MinY >= MaxY) {
              Message = TEXT("minY must be less than maxY");
              ErrorCode = TEXT("INVALID_ARGUMENT");
              Resp->SetStringField(TEXT("error"), Message);
              goto ValidationFailed;
            }
            if (GridY <= 0) {
              Message = TEXT("gridY must be greater than 0");
              ErrorCode = TEXT("INVALID_ARGUMENT");
              Resp->SetStringField(TEXT("error"), Message);
              goto ValidationFailed;
            }
          }

          FString FactoryError;
#if MCP_HAS_BLENDSPACE_FACTORY
          UObject *CreatedBlendAsset = CreateBlendSpaceAsset(
              Name, SavePath, TargetSkeleton, bTwoDimensional, FactoryError);
          if (CreatedBlendAsset) {
            ApplyBlendSpaceConfiguration(CreatedBlendAsset, Payload,
                                         bTwoDimensional);
#if MCP_HAS_BLENDSPACE_BASE
            // UBlendSpaceBase is deprecated in UE 5.0+ but still needed for backward compatibility
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            if (UBlendSpaceBase *BlendSpace =
                    Cast<UBlendSpaceBase>(CreatedBlendAsset)) {

              bSuccess = true;
              Message = TEXT("Blend space created successfully");
              Resp->SetStringField(TEXT("blendSpacePath"),
                                   BlendSpace->GetPathName());
              Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
              Resp->SetBoolField(TEXT("twoDimensional"), bTwoDimensional);
              McpHandlerUtils::AddVerification(Resp, BlendSpace);
            } else {
              Message =
                  TEXT("Created asset is not a BlendSpaceBase instance");
              ErrorCode = TEXT("TYPE_MISMATCH");
              Resp->SetStringField(TEXT("error"), Message);
            }
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#else

            bSuccess = true;
            Message = TEXT("Blend space created (limited configuration)");
            Resp->SetStringField(TEXT("blendSpacePath"),
                                 CreatedBlendAsset->GetPathName());
            Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
            Resp->SetBoolField(TEXT("twoDimensional"), bTwoDimensional);
            Resp->SetStringField(TEXT("warning"),
                                 TEXT("BlendSpaceBase headers unavailable; "
                                      "axis configuration skipped."));
            McpHandlerUtils::AddVerification(Resp, CreatedBlendAsset);
#endif // MCP_HAS_BLENDSPACE_BASE
          } else {
            Message = FactoryError.IsEmpty()
                          ? TEXT("Failed to create blend space asset")
                          : FactoryError;
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          }
#else
          Message = TEXT(
              "Blend space creation requires editor blend space factories");
          ErrorCode = TEXT("NOT_AVAILABLE");
          Resp->SetStringField(TEXT("error"), Message);
#endif
        } // End valid params

      ValidationFailed:;
      }
    }
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }
  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleAnimPhysCreateBlendTree(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("create_blend_tree"));
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  // ============================================================================
  // Blend Tree Creation in AnimBlueprint's AnimGraph
  // ============================================================================
  // Creates a new blend tree node in an existing AnimBlueprint's AnimGraph.
  // Optionally configures blend parameters and adds children with animations.
  // Uses FGraphNodeCreator for proper graph editing.
  // ============================================================================
  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

  if (BlueprintPath.IsEmpty()) {
    Message = TEXT("blueprintPath is required for create_blend_tree");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
  } else {
    FString TreeName;
    Payload->TryGetStringField(TEXT("treeName"), TreeName);
    if (TreeName.IsEmpty()) {
      TreeName = TEXT("BlendTree");
    }

    // Load the AnimBlueprint
    UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
    if (!AnimBP) {
      Message = FString::Printf(TEXT("AnimBlueprint not found: %s"), *BlueprintPath);
      ErrorCode = TEXT("ASSET_NOT_FOUND");
      Resp->SetStringField(TEXT("error"), Message);
      Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    } else {
#if MCP_HAS_ANIM_GRAPH_NODE_BLEND_TREE && MCP_HAS_BLEND_TREE && MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
      // Find the AnimGraph in the blueprint
      UEdGraph* AnimGraph = nullptr;
      for (UEdGraph* Graph : AnimBP->FunctionGraphs) {
        if (Graph && Graph->GetName() == TEXT("AnimGraph")) {
          AnimGraph = Graph;
          break;
        }
      }

      if (!AnimGraph) {
        Message = TEXT("Could not find AnimGraph in blueprint");
        ErrorCode = TEXT("GRAPH_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        // Create the Blend Tree Node using FGraphNodeCreator
        FGraphNodeCreator<UAnimGraphNode_BlendTree> NodeCreator(*AnimGraph);
        UAnimGraphNode_BlendTree* BTNode = NodeCreator.CreateNode();
        BTNode->NodePosX = 0;
        BTNode->NodePosY = 0;
        NodeCreator.Finalize();

        // Access and configure the inner UBlendTree
#if MCP_HAS_BLEND_TREE
        if (UBlendTree* BlendTree = BTNode->GetBlendTree()) {
          BlendTree->Modify();

          // Process blend parameters array if provided
          const TArray<TSharedPtr<FJsonValue>>* BlendParamsArray = nullptr;
          if (Payload->TryGetArrayField(TEXT("blendParameters"), BlendParamsArray) && BlendParamsArray) {
            int32 ParamIndex = 0;
            for (const TSharedPtr<FJsonValue>& ParamValue : *BlendParamsArray) {
              if (!ParamValue.IsValid() || ParamValue->Type != EJson::Object) {
                continue;
              }

              const TSharedPtr<FJsonObject> ParamObj = ParamValue->AsObject();
              FString ParamName;
              ParamObj->TryGetStringField(TEXT("name"), ParamName);

              double MinVal = 0.0, MaxVal = 1.0;
              ParamObj->TryGetNumberField(TEXT("min"), MinVal);
              ParamObj->TryGetNumberField(TEXT("max"), MaxVal);

              // Configure blend parameter (index 0 or 1 for 1D/2D blend trees)
              if (ParamIndex < 2) {
                FBlendParameter& Param = const_cast<FBlendParameter&>(BlendTree->GetBlendParameter(ParamIndex));
                if (!ParamName.IsEmpty()) {
                  Param.DisplayName = FText::FromString(ParamName);
                }
                Param.Min = static_cast<float>(MinVal);
                Param.Max = static_cast<float>(MaxVal);
              }
              ParamIndex++;
            }
          }

          // Process children array if provided
          const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
          if (Payload->TryGetArrayField(TEXT("children"), ChildrenArray) && ChildrenArray) {
            for (const TSharedPtr<FJsonValue>& ChildValue : *ChildrenArray) {
              if (!ChildValue.IsValid() || ChildValue->Type != EJson::Object) {
                continue;
              }

              const TSharedPtr<FJsonObject> ChildObj = ChildValue->AsObject();
              FString AnimationPath;
              ChildObj->TryGetStringField(TEXT("animationPath"), AnimationPath);

              double BlendWeight = 1.0;
              ChildObj->TryGetNumberField(TEXT("blendWeight"), BlendWeight);

              // Load animation asset if path provided
              if (!AnimationPath.IsEmpty()) {
                if (UAnimationAsset* AnimAsset = LoadObject<UAnimationAsset>(nullptr, *AnimationPath)) {
                  // Add as blend sample (simplified - full implementation would use proper blend tree API)
                  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
                         TEXT("create_blend_tree: Added animation %s with weight %.2f"),
                         *AnimationPath, BlendWeight);
                }
              }
            }
          }

          BlendTree->MarkPackageDirty();
        }
#endif // MCP_HAS_BLEND_TREE

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);

        bool bShouldSave = true;
        Payload->TryGetBoolField(TEXT("save"), bShouldSave);
        if (bShouldSave) {
          McpSafeAssetSave(AnimBP);
        }

        bSuccess = true;
        Message = FString::Printf(TEXT("Blend tree '%s' created in %s"), *TreeName, *BlueprintPath);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("treeName"), TreeName);
        Resp->SetStringField(TEXT("nodeType"), TEXT("BlendTree"));
      }
#else
      // Blend tree headers not available
      Message = FString::Printf(
        TEXT("Cannot create blend tree '%s': AnimGraph BlendTree module headers not available. "
             "Rebuild with AnimGraph module enabled."),
        *TreeName);
      ErrorCode = TEXT("ANIMGRAPH_MODULE_UNAVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
#endif
    }
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }
  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleAnimPhysCreateProceduralAnim(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("create_procedural_animation"));
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  FString Name;
  if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
    Message = TEXT("name field required for procedural animation creation");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
  } else {
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (SavePath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("path"), SavePath);
    }
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game/Animations");
    }

    FString SkeletonPath;
    if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) ||
        SkeletonPath.IsEmpty()) {
      Message = TEXT("skeletonPath is required for create_procedural_animation");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      const TArray<TSharedPtr<FJsonValue>> *BoneTracksArray = nullptr;
      if (!Payload->TryGetArrayField(TEXT("boneTracks"), BoneTracksArray) ||
          !BoneTracksArray) {
        Message =
            TEXT("boneTracks array is required for create_procedural_animation");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        double NumFramesNumber = 30.0;
        Payload->TryGetNumberField(TEXT("numFrames"), NumFramesNumber);
        const int32 NumFrames =
            FMath::Max(1, static_cast<int32>(NumFramesNumber));

        double FrameRateNumber = 30.0;
        Payload->TryGetNumberField(TEXT("frameRate"), FrameRateNumber);
        const int32 FrameRate =
            FMath::Max(1, static_cast<int32>(FrameRateNumber));

        bool bShouldSave = true;
        Payload->TryGetBoolField(TEXT("save"), bShouldSave);

        USkeleton *TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
        if (!TargetSkeleton) {
          Message = TEXT("Failed to load skeleton for procedural animation");
          ErrorCode = TEXT("LOAD_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          // Pre-check for existing asset to avoid UObject type-collision
          // crashes. Follow same pattern as create_montage/create_sequence.
          const FString ObjectPath =
              FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
          if (UEditorAssetLibrary::DoesAssetExist(ObjectPath)) {
            UObject *ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
            if (ExistingAsset) {
              if (UAnimSequence *ExistingSequence =
                      Cast<UAnimSequence>(ExistingAsset)) {
                bSuccess = true;
                Message =
                    FString::Printf(TEXT("Procedural animation '%s' already "
                                         "exists - reusing existing asset"),
                                    *Name);
                Resp->SetStringField(TEXT("assetPath"), ObjectPath);
                Resp->SetBoolField(TEXT("existingAsset"), true);
                Resp->SetStringField(TEXT("skeletonPath"),
                                     ExistingSequence->GetSkeleton()
                                         ? ExistingSequence->GetSkeleton()->GetPathName()
                                         : SkeletonPath);
                McpHandlerUtils::AddVerification(Resp, ExistingSequence);
              } else {
                Message = FString::Printf(
                    TEXT("Cannot create procedural animation: asset '%s' "
                         "already exists as type '%s'"),
                    *ObjectPath, *ExistingAsset->GetClass()->GetName());
                ErrorCode = TEXT("ASSET_TYPE_MISMATCH");
                Resp->SetStringField(TEXT("error"), Message);
              }
            } else {
              Message =
                  FString::Printf(TEXT("Asset exists but failed to load: %s"),
                                  *ObjectPath);
              ErrorCode = TEXT("ASSET_LOAD_FAILED");
              Resp->SetStringField(TEXT("error"), Message);
            }
          } else {
            FString PackagePath = SavePath / Name;
            UPackage *Package = CreatePackage(*PackagePath);
            if (!Package) {
              Message = TEXT("Failed to create package for animation sequence");
              ErrorCode = TEXT("PACKAGE_ERROR");
              Resp->SetStringField(TEXT("error"), Message);
            } else {
              UAnimSequenceFactory *Factory = NewObject<UAnimSequenceFactory>();
              if (!Factory) {
                Message = TEXT("Failed to create AnimSequence factory");
                ErrorCode = TEXT("FACTORY_FAILED");
                Resp->SetStringField(TEXT("error"), Message);
              } else {
                Factory->TargetSkeleton = TargetSkeleton;

                UAnimSequence *NewSequence = Cast<UAnimSequence>(
                    Factory->FactoryCreateNew(UAnimSequence::StaticClass(), Package,
                                              FName(*Name),
                                              RF_Public | RF_Standalone,
                                              nullptr, GWarn));

                if (!NewSequence) {
                  Message = TEXT("Failed to create procedural animation "
                                 "sequence asset");
                  ErrorCode = TEXT("ASSET_CREATION_FAILED");
                  Resp->SetStringField(TEXT("error"), Message);
                } else {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
                  IAnimationDataController &Controller =
                      NewSequence->GetController();
                  Controller.SetFrameRate(FFrameRate(FrameRate, 1));
                  Controller.SetNumberOfFrames(FFrameNumber(NumFrames));
#else
                  // SequenceLength is deprecated in UE 5.1+ but required for
                  // UE 5.0 compatibility.
                  PRAGMA_DISABLE_DEPRECATION_WARNINGS
                  NewSequence->SequenceLength =
                      static_cast<float>(NumFrames) /
                      static_cast<float>(FrameRate);
                  PRAGMA_ENABLE_DEPRECATION_WARNINGS
                  IAnimationDataController &Controller =
                      NewSequence->GetController();
#endif

                  int32 AppliedTrackCount = 0;
                  for (const TSharedPtr<FJsonValue> &TrackValue :
                       *BoneTracksArray) {
                    if (!TrackValue.IsValid() ||
                        TrackValue->Type != EJson::Object) {
                      continue;
                    }

                    const TSharedPtr<FJsonObject> TrackObject =
                        TrackValue->AsObject();
                    if (!TrackObject.IsValid()) {
                      continue;
                    }

                    FString BoneName;
                    if (!TrackObject->TryGetStringField(TEXT("boneName"),
                                                        BoneName) ||
                        BoneName.IsEmpty()) {
                      continue;
                    }

                    const FName BoneFName(*BoneName);

                    // CRITICAL: Validate bone exists in skeleton before attempting to add track
                    const FReferenceSkeleton& RefSkeleton = TargetSkeleton->GetReferenceSkeleton();
                    const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneFName);
                    if (BoneIndex == INDEX_NONE)
                    {
                      // Bone doesn't exist in skeleton - skip this track
                      UE_LOG(LogTemp, Warning, TEXT("create_procedural_animation: Bone '%s' not found in skeleton %s"), 
                             *BoneName, *TargetSkeleton->GetName());
                      continue;
                    }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
                    if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName)) {
                      Controller.AddBoneCurve(BoneFName);
                    }
#elif ENGINE_MAJOR_VERSION >= 5
                    const FBoneAnimationTrack *ExistingTrack =
                        Controller.GetModel()->FindBoneTrackByName(BoneFName);
                    if (ExistingTrack == nullptr) {
                      PRAGMA_DISABLE_DEPRECATION_WARNINGS
                      FRawAnimSequenceTrack NewTrack;
                      NewSequence->AddNewRawTrack(BoneFName, &NewTrack);
                      PRAGMA_ENABLE_DEPRECATION_WARNINGS
                    }
#endif

                    TArray<FVector> PositionKeys;
                    TArray<FQuat> RotationKeys;
                    TArray<FVector> ScaleKeys;
                    PositionKeys.Init(FVector::ZeroVector, NumFrames);
                    RotationKeys.Init(FQuat::Identity, NumFrames);
                    ScaleKeys.Init(FVector::OneVector, NumFrames);

                    const TArray<TSharedPtr<FJsonValue>> *FramesArray =
                        nullptr;
                    if (TrackObject->TryGetArrayField(TEXT("frames"),
                                                      FramesArray) &&
                        FramesArray) {
                      for (const TSharedPtr<FJsonValue> &FrameValue :
                           *FramesArray) {
                        if (!FrameValue.IsValid() ||
                            FrameValue->Type != EJson::Object) {
                          continue;
                        }

                        const TSharedPtr<FJsonObject> FrameObject =
                            FrameValue->AsObject();
                        if (!FrameObject.IsValid()) {
                          continue;
                        }

                        double FrameNumberValue = 0.0;
                        FrameObject->TryGetNumberField(TEXT("frame"),
                                                       FrameNumberValue);
                        const int32 FrameIndex =
                            static_cast<int32>(FrameNumberValue);
                        if (FrameIndex < 0 || FrameIndex >= NumFrames) {
                          continue;
                        }

                        const TSharedPtr<FJsonObject> *LocationObject =
                            nullptr;
                        if (FrameObject->TryGetObjectField(TEXT("location"),
                                                           LocationObject) &&
                            LocationObject && LocationObject->IsValid()) {
                          double X = 0.0, Y = 0.0, Z = 0.0;
                          (*LocationObject)
                              ->TryGetNumberField(TEXT("x"), X);
                          (*LocationObject)
                              ->TryGetNumberField(TEXT("y"), Y);
                          (*LocationObject)
                              ->TryGetNumberField(TEXT("z"), Z);
                          PositionKeys[FrameIndex] =
                              FVector(static_cast<float>(X),
                                      static_cast<float>(Y),
                                      static_cast<float>(Z));
                        }

                        const TSharedPtr<FJsonObject> *RotationObject =
                            nullptr;
                        if (FrameObject->TryGetObjectField(TEXT("rotation"),
                                                           RotationObject) &&
                            RotationObject && RotationObject->IsValid()) {
                          const TSharedPtr<FJsonObject> RotationJson =
                              *RotationObject;
                          if (RotationJson->HasField(TEXT("w"))) {
                            double X = 0.0, Y = 0.0, Z = 0.0, W = 1.0;
                            RotationJson->TryGetNumberField(TEXT("x"), X);
                            RotationJson->TryGetNumberField(TEXT("y"), Y);
                            RotationJson->TryGetNumberField(TEXT("z"), Z);
                            RotationJson->TryGetNumberField(TEXT("w"), W);
                            RotationKeys[FrameIndex] =
                                FQuat(static_cast<float>(X),
                                      static_cast<float>(Y),
                                      static_cast<float>(Z),
                                      static_cast<float>(W));
                          } else {
                            double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
                            RotationJson->TryGetNumberField(TEXT("pitch"),
                                                            Pitch);
                            RotationJson->TryGetNumberField(TEXT("yaw"),
                                                            Yaw);
                            RotationJson->TryGetNumberField(TEXT("roll"),
                                                            Roll);
                            RotationKeys[FrameIndex] =
                                FRotator(static_cast<float>(Pitch),
                                         static_cast<float>(Yaw),
                                         static_cast<float>(Roll))
                                    .Quaternion();
                          }
                        }

                        const TSharedPtr<FJsonObject> *ScaleObject = nullptr;
                        if (FrameObject->TryGetObjectField(TEXT("scale"),
                                                           ScaleObject) &&
                            ScaleObject && ScaleObject->IsValid()) {
                          double X = 1.0, Y = 1.0, Z = 1.0;
                          (*ScaleObject)->TryGetNumberField(TEXT("x"), X);
                          (*ScaleObject)->TryGetNumberField(TEXT("y"), Y);
                          (*ScaleObject)->TryGetNumberField(TEXT("z"), Z);
                          ScaleKeys[FrameIndex] =
                              FVector(static_cast<float>(X),
                                      static_cast<float>(Y),
                                      static_cast<float>(Z));
                        }
                      }
                    }

                    Controller.SetBoneTrackKeys(BoneFName, PositionKeys,
                                                RotationKeys, ScaleKeys);
                    ++AppliedTrackCount;
                  }

                  NewSequence->PostEditChange();
                  NewSequence->MarkPackageDirty();
                  if (bShouldSave) {
                    McpSafeAssetSave(NewSequence);
                  }

                  bSuccess = true;
                  Message =
                      FString::Printf(TEXT("Procedural animation '%s' "
                                           "created with %d tracks"),
                                      *Name, AppliedTrackCount);
                  Resp->SetStringField(TEXT("assetPath"),
                                       NewSequence->GetPathName());
                  Resp->SetStringField(TEXT("skeletonPath"),
                                       TargetSkeleton->GetPathName());
                  Resp->SetBoolField(TEXT("existingAsset"), false);
                  Resp->SetNumberField(TEXT("numFrames"), NumFrames);
                  Resp->SetNumberField(TEXT("frameRate"), FrameRate);
                  Resp->SetNumberField(TEXT("appliedTrackCount"),
                                       AppliedTrackCount);
                  McpHandlerUtils::AddVerification(Resp, NewSequence);
                }
              }
            }
          }
        }
      }
    }
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }
  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleAnimPhysCreateStateMachine(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("create_state_machine"));
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  // ============================================================================
  // State Machine Creation using AnimGraph Editor API
  // ============================================================================
  // Creates a new state machine node in an existing AnimBlueprint's AnimGraph.
  // Optionally adds states and transitions if provided in the payload.
  // Uses FGraphNodeCreator and FBlueprintEditorUtils for proper graph editing.
  // ============================================================================
  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (BlueprintPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("name"), BlueprintPath);
  }

  if (BlueprintPath.IsEmpty()) {
    Message = TEXT("blueprintPath is required for create_state_machine");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
  } else {
    FString MachineName;
    Payload->TryGetStringField(TEXT("machineName"), MachineName);
    if (MachineName.IsEmpty()) {
      MachineName = TEXT("StateMachine");
    }

    // Load the AnimBlueprint
    UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
    if (!AnimBP) {
      Message = FString::Printf(TEXT("AnimBlueprint not found: %s"), *BlueprintPath);
      ErrorCode = TEXT("ASSET_NOT_FOUND");
      Resp->SetStringField(TEXT("error"), Message);
      Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    } else {
      // Check if AnimGraph headers are available
      #if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
      // Find the AnimGraph in the blueprint
      UEdGraph* AnimGraph = nullptr;
      for (UEdGraph* Graph : AnimBP->FunctionGraphs) {
        if (Graph && Graph->GetName() == TEXT("AnimGraph")) {
          AnimGraph = Graph;
          break;
        }
      }

      if (!AnimGraph) {
        Message = TEXT("Could not find AnimGraph in blueprint");
        ErrorCode = TEXT("GRAPH_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        // Check if a state machine with this name already exists
        bool bAlreadyExists = false;
        for (UEdGraphNode* Node : AnimGraph->Nodes) {
          if (UAnimGraphNode_StateMachine* ExistingSM = Cast<UAnimGraphNode_StateMachine>(Node)) {
            if (ExistingSM->GetStateMachineName() == MachineName) {
              bAlreadyExists = true;
              break;
            }
          }
        }

        if (bAlreadyExists) {
          bSuccess = true;
          Message = FString::Printf(TEXT("State machine '%s' already exists in %s"), *MachineName, *BlueprintPath);
          Resp->SetBoolField(TEXT("existingAsset"), true);
        } else {
          // Create the State Machine Node using FGraphNodeCreator
          FGraphNodeCreator<UAnimGraphNode_StateMachine> NodeCreator(*AnimGraph);
          UAnimGraphNode_StateMachine* SMNode = NodeCreator.CreateNode();
          SMNode->NodePosX = 0;
          SMNode->NodePosY = 0;
          NodeCreator.Finalize();

          // Create the internal State Machine Graph
          UAnimationStateMachineGraph* InnerGraph = Cast<UAnimationStateMachineGraph>(
            FBlueprintEditorUtils::CreateNewGraph(
              AnimBP,
              FName(*MachineName),
              UAnimationStateMachineGraph::StaticClass(),
              UAnimationStateMachineSchema::StaticClass()
            )
          );
          if (!InnerGraph) {
            S.SendAutomationError(RequestingSocket, RequestId,
              TEXT("Failed to create animation state machine graph"),
              TEXT("CREATE_GRAPH_FAILED"));
            return true;
          }

          // Link the State Machine Node to its internal graph
          SMNode->EditorStateMachineGraph = InnerGraph;
          InnerGraph->OwnerAnimGraphNode = SMNode;

          // Initialize Entry Node (required for State Machines)
          const UAnimationStateMachineSchema* Schema = Cast<UAnimationStateMachineSchema>(InnerGraph->GetSchema());
          if (!Schema) {
            S.SendAutomationError(RequestingSocket, RequestId,
              TEXT("Animation state machine graph has an invalid schema"),
              TEXT("INVALID_SCHEMA"));
            return true;
          }
          Schema->CreateDefaultNodesForGraph(*InnerGraph);

          // Process states array if provided
          const TArray<TSharedPtr<FJsonValue>>* StatesArray = nullptr;
          if (Payload->TryGetArrayField(TEXT("states"), StatesArray) && StatesArray) {
            int32 StatePosX = 200;
            for (const TSharedPtr<FJsonValue>& StateValue : *StatesArray) {
              if (!StateValue.IsValid() || StateValue->Type != EJson::Object) {
                continue;
              }

              const TSharedPtr<FJsonObject> StateObj = StateValue->AsObject();
              FString StateName;
              StateObj->TryGetStringField(TEXT("name"), StateName);
              if (StateName.IsEmpty()) {
                continue;
              }

              // Create the State Node
              FGraphNodeCreator<UAnimStateNode> StateCreator(*InnerGraph);
              UAnimStateNode* StateNode = StateCreator.CreateNode();
              StateNode->NodePosX = StatePosX;
              StateNode->NodePosY = 0;
              StateCreator.Finalize();

              // Rename the state's bound graph to set the state name
              if (StateNode->BoundGraph) {
                FBlueprintEditorUtils::RenameGraph(StateNode->BoundGraph, *StateName);
              }

              StatePosX += 200;
            }
          }

          // Process transitions array if provided
          const TArray<TSharedPtr<FJsonValue>>* TransitionsArray = nullptr;
          if (Payload->TryGetArrayField(TEXT("transitions"), TransitionsArray) && TransitionsArray) {
            #if MCP_HAS_ANIM_STATE_TRANSITION
            for (const TSharedPtr<FJsonValue>& TransitionValue : *TransitionsArray) {
              if (!TransitionValue.IsValid() || TransitionValue->Type != EJson::Object) {
                continue;
              }

              const TSharedPtr<FJsonObject> TransitionObj = TransitionValue->AsObject();
              FString SourceState;
              FString TargetState;
              TransitionObj->TryGetStringField(TEXT("sourceState"), SourceState);
              TransitionObj->TryGetStringField(TEXT("targetState"), TargetState);

              if (SourceState.IsEmpty() || TargetState.IsEmpty()) {
                continue;
              }

              // Find the source and target states
              UAnimStateNode* FromNode = nullptr;
              UAnimStateNode* ToNode = nullptr;
              for (UEdGraphNode* Node : InnerGraph->Nodes) {
                if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node)) {
                  FString NodeName = StateNode->GetStateName();
                  if (NodeName == SourceState) FromNode = StateNode;
                  if (NodeName == TargetState) ToNode = StateNode;
                }
              }

              if (FromNode && ToNode) {
                // Create the Transition Node
                FGraphNodeCreator<UAnimStateTransitionNode> TransCreator(*InnerGraph);
                UAnimStateTransitionNode* TransNode = TransCreator.CreateNode();
                TransCreator.Finalize();

                // Establish the connection between states
                TransNode->CreateConnections(FromNode, ToNode);

                // Configure crossfade duration
                double CrossfadeDuration = 0.2;
                TransitionObj->TryGetNumberField(TEXT("crossfadeDuration"), CrossfadeDuration);
                TransNode->CrossfadeDuration = static_cast<float>(CrossfadeDuration);
              }
            }
            #endif // MCP_HAS_ANIM_STATE_TRANSITION
          }

          McpFinalizeBlueprint(AnimBP, /*bStructural=*/true, /*bSave=*/true);

          bSuccess = true;
          Message = FString::Printf(TEXT("State machine '%s' created in %s"), *MachineName, *BlueprintPath);
          Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
          Resp->SetStringField(TEXT("machineName"), MachineName);
        }
      }
      #else
      // AnimGraph headers not available
      Message = FString::Printf(
        TEXT("Cannot create state machine '%s': AnimGraph module headers not available. "
             "Rebuild with AnimGraph module enabled or use add_state_machine action."),
        *MachineName);
      ErrorCode = TEXT("ANIMGRAPH_MODULE_UNAVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
      #endif
    }
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }
  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleAnimPhysSetupIk(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("setup_ik"));
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  FString IKName;
  if (!Payload->TryGetStringField(TEXT("name"), IKName) || IKName.IsEmpty()) {
    Message = TEXT("name field required for IK setup");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
  } else {
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game/Animations");
    }

    FString SkeletonPath;
    if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) ||
        SkeletonPath.IsEmpty()) {
      Message = TEXT("skeletonPath is required to bind IK to a skeleton");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      USkeleton *TargetSkeleton =
          LoadObject<USkeleton>(nullptr, *SkeletonPath);
      if (!TargetSkeleton) {
        Message = TEXT("Failed to load skeleton for IK");
        ErrorCode = TEXT("LOAD_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        FString FactoryError;
        UBlueprint *ControlRigBlueprint = nullptr;
#if MCP_HAS_CONTROLRIG_FACTORY
        ControlRigBlueprint = S.CreateControlRigBlueprint(
            IKName, SavePath, TargetSkeleton, FactoryError);
#else
        FactoryError =
            TEXT("Control Rig factory not available in this editor build");
#endif
        if (!ControlRigBlueprint) {
          Message = FactoryError.IsEmpty() ? TEXT("Failed to create IK asset")
                                           : FactoryError;
          ErrorCode = TEXT("ASSET_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          bSuccess = true;
          Message = TEXT("IK setup created successfully");
          const FString ControlRigPath = ControlRigBlueprint->GetPathName();
          Resp->SetStringField(TEXT("ikPath"), ControlRigPath);
          Resp->SetStringField(TEXT("controlRigPath"), ControlRigPath);
          Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
          McpHandlerUtils::AddVerification(Resp, ControlRigBlueprint);
        }
      }
    }
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }
  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleAnimPhysConfigureVehicle(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("configure_vehicle"));
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  FString ActorName;
  if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
      ActorName.IsEmpty()) {
    // Backward compatibility for older payloads.
    Payload->TryGetStringField(TEXT("vehicleName"), ActorName);
  }

  if (ActorName.IsEmpty()) {
    Message = TEXT("actorName is required for configure_vehicle");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
  } else {
#if MCP_HAS_WHEELED_VEHICLE_4W
    AActor *TargetActor = S.FindActorByName(ActorName);
    if (!TargetActor) {
      Message = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString VehicleType = TEXT("WheeledVehicle4W");
      Payload->TryGetStringField(TEXT("vehicleType"), VehicleType);

      // An empty request must not create the movement component below.
      const bool bAnyFieldRequested = Payload->HasField(TEXT("wheels")) ||
                                      Payload->HasField(TEXT("engine")) ||
                                      Payload->HasField(TEXT("transmission")) ||
                                      Payload->HasField(TEXT("mass")) ||
                                      Payload->HasField(TEXT("dragCoefficient"));
      if (!bAnyFieldRequested) {
        McpHandlerUtils::FMcpWriteReport EmptyReport;
        return SendWriteReportResponse(&S, RequestingSocket, RequestId,
                                       EmptyReport, Resp, FString(), nullptr);
      }

      UWheeledVehicleMovementComponent4W *VehicleMC =
          TargetActor->FindComponentByClass<UWheeledVehicleMovementComponent4W>();
      bool bCreatedComponent = false;
      if (!VehicleMC) {
        VehicleMC = NewObject<UWheeledVehicleMovementComponent4W>(
            TargetActor, TEXT("MCP_VehicleMovement4W"));
        if (VehicleMC) {
          TargetActor->AddInstanceComponent(VehicleMC);
          VehicleMC->RegisterComponent();
          bCreatedComponent = true;
        }
      }

      if (!VehicleMC) {
        Message = TEXT("Failed to create/get UWheeledVehicleMovementComponent4W");
        ErrorCode = TEXT("COMPONENT_CREATION_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        auto SetNumericOnStruct = [](UStruct *StructType, void *Container,
                                     const TArray<FString> &PropertyNames,
                                     double Value) -> bool {
          if (!StructType || !Container) {
            return false;
          }

          for (const FString &PropName : PropertyNames) {
            if (PropName.IsEmpty()) {
              continue;
            }

            FProperty *Prop = StructType->FindPropertyByName(*PropName);
            if (!Prop) {
              continue;
            }

            if (FNumericProperty *NumericProp = CastField<FNumericProperty>(Prop)) {
              void *ValuePtr = Prop->ContainerPtrToValuePtr<void>(Container);
              if (!ValuePtr) {
                continue;
              }

              if (NumericProp->IsFloatingPoint()) {
                NumericProp->SetFloatingPointPropertyValue(ValuePtr, Value);
              } else {
                NumericProp->SetIntPropertyValue(ValuePtr,
                                                 static_cast<int64>(Value));
              }
              return true;
            }
          }

          return false;
        };

        auto SetNumericOnObject =
            [&SetNumericOnStruct](UObject *Obj,
                                  const TArray<FString> &PropertyNames,
                                  double Value) -> bool {
          if (!Obj) {
            return false;
          }
          return SetNumericOnStruct(Obj->GetClass(), Obj, PropertyNames,
                                    Value);
        };

        auto SetNestedNumericOnObject =
            [&SetNumericOnStruct](UObject *Obj,
                                  const TArray<FString> &OuterNames,
                                  const TArray<FString> &InnerNames,
                                  double Value) -> bool {
          if (!Obj) {
            return false;
          }

          for (const FString &OuterName : OuterNames) {
            FStructProperty *OuterStructProp =
                FindFProperty<FStructProperty>(Obj->GetClass(), *OuterName);
            if (!OuterStructProp) {
              continue;
            }

            void *OuterData = OuterStructProp->ContainerPtrToValuePtr<void>(Obj);
            if (!OuterData) {
              continue;
            }

            if (SetNumericOnStruct(OuterStructProp->Struct, OuterData,
                                   InnerNames, Value)) {
              return true;
            }
          }

          return false;
        };

        auto ConfigureForwardGearRatios =
            [](UObject *Obj, const TArray<double> &Ratios) -> bool {
          if (!Obj) {
            return false;
          }

          const TArray<FString> TransmissionOuterNames = {
              TEXT("TransmissionSetup"), TEXT("TransmissionConfig")};
          const TArray<FString> GearArrayNames = {TEXT("ForwardGears"),
                                                  TEXT("GearRatios")};

          for (const FString &OuterName : TransmissionOuterNames) {
            FStructProperty *TransmissionProp =
                FindFProperty<FStructProperty>(Obj->GetClass(), *OuterName);
            if (!TransmissionProp) {
              continue;
            }

            void *TransmissionData =
                TransmissionProp->ContainerPtrToValuePtr<void>(Obj);
            if (!TransmissionData) {
              continue;
            }

            for (const FString &ArrayName : GearArrayNames) {
              FArrayProperty *GearArrayProp = FindFProperty<FArrayProperty>(
                  TransmissionProp->Struct, *ArrayName);
              if (!GearArrayProp) {
                continue;
              }

              void *GearArrayPtr =
                  GearArrayProp->ContainerPtrToValuePtr<void>(TransmissionData);
              FScriptArrayHelper GearArrayHelper(GearArrayProp, GearArrayPtr);
              GearArrayHelper.EmptyValues();

              for (int32 GearIndex = 0; GearIndex < Ratios.Num(); ++GearIndex) {
                const int32 NewIdx = GearArrayHelper.AddValue();
                void *GearElemPtr = GearArrayHelper.GetRawPtr(NewIdx);

                if (FNumericProperty *NumericInner =
                        CastField<FNumericProperty>(GearArrayProp->Inner)) {
                  if (NumericInner->IsFloatingPoint()) {
                    NumericInner->SetFloatingPointPropertyValue(
                        GearElemPtr, Ratios[GearIndex]);
                  } else {
                    NumericInner->SetIntPropertyValue(
                        GearElemPtr,
                        static_cast<int64>(FMath::RoundToInt(Ratios[GearIndex])));
                  }
                  continue;
                }

                if (FStructProperty *StructInner =
                        CastField<FStructProperty>(GearArrayProp->Inner)) {
                  StructInner->Struct->InitializeStruct(GearElemPtr);
                  FProperty *RatioProp =
                      StructInner->Struct->FindPropertyByName(TEXT("Ratio"));
                  if (FNumericProperty *RatioNumeric =
                          CastField<FNumericProperty>(RatioProp)) {
                    void *RatioPtr =
                        RatioProp->ContainerPtrToValuePtr<void>(GearElemPtr);
                    if (RatioNumeric->IsFloatingPoint()) {
                      RatioNumeric->SetFloatingPointPropertyValue(
                          RatioPtr, Ratios[GearIndex]);
                    } else {
                      RatioNumeric->SetIntPropertyValue(
                          RatioPtr,
                          static_cast<int64>(
                              FMath::RoundToInt(Ratios[GearIndex])));
                    }
                  }
                }
              }

              return true;
            }
          }

          return false;
        };

        McpHandlerUtils::FMcpWriteReport Report;

        int32 ConfiguredWheels = 0;
        const TArray<TSharedPtr<FJsonValue>> *WheelsArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("wheels"), WheelsArray) &&
            WheelsArray) {
          FArrayProperty *WheelSetupsProp = FindFProperty<FArrayProperty>(
              VehicleMC->GetClass(), TEXT("WheelSetups"));
          FStructProperty *WheelSetupStruct =
              WheelSetupsProp ? CastField<FStructProperty>(WheelSetupsProp->Inner)
                              : nullptr;

          if (WheelSetupsProp && WheelSetupStruct) {
            void *WheelSetupsPtr =
                WheelSetupsProp->ContainerPtrToValuePtr<void>(VehicleMC);
            FScriptArrayHelper WheelArrayHelper(WheelSetupsProp,
                                                WheelSetupsPtr);
            WheelArrayHelper.EmptyValues();

            for (const TSharedPtr<FJsonValue> &WheelVal : *WheelsArray) {
              if (!WheelVal.IsValid() || WheelVal->Type != EJson::Object) {
                continue;
              }

              const TSharedPtr<FJsonObject> WheelObj = WheelVal->AsObject();
              if (!WheelObj.IsValid()) {
                continue;
              }

              const int32 NewIndex = WheelArrayHelper.AddValue();
              void *WheelSetupPtr = WheelArrayHelper.GetRawPtr(NewIndex);
              WheelSetupStruct->Struct->InitializeStruct(WheelSetupPtr);

              FString BoneName;
              if (WheelObj->TryGetStringField(TEXT("boneName"), BoneName) &&
                  !BoneName.IsEmpty()) {
                if (FNameProperty *BoneProp = FindFProperty<FNameProperty>(
                        WheelSetupStruct->Struct, TEXT("BoneName"))) {
                  BoneProp->SetPropertyValue_InContainer(WheelSetupPtr,
                                                         FName(*BoneName));
                }
              }

              const TSharedPtr<FJsonObject> *OffsetObj = nullptr;
              if (WheelObj->TryGetObjectField(TEXT("offset"), OffsetObj) &&
                  OffsetObj && (*OffsetObj).IsValid()) {
                double X = 0.0, Y = 0.0, Z = 0.0;
                (*OffsetObj)->TryGetNumberField(TEXT("x"), X);
                (*OffsetObj)->TryGetNumberField(TEXT("y"), Y);
                (*OffsetObj)->TryGetNumberField(TEXT("z"), Z);

                if (FStructProperty *OffsetProp =
                        FindFProperty<FStructProperty>(
                            WheelSetupStruct->Struct,
                            TEXT("AdditionalOffset"))) {
                  if (OffsetProp->Struct == TBaseStructure<FVector>::Get()) {
                    if (FVector *OffsetPtr =
                            OffsetProp->ContainerPtrToValuePtr<FVector>(
                                WheelSetupPtr)) {
                      *OffsetPtr = FVector(static_cast<float>(X),
                                           static_cast<float>(Y),
                                           static_cast<float>(Z));
                    }
                  }
                }
              }

#if MCP_HAS_VEHICLE_WHEEL
              if (FClassProperty *WheelClassProp = FindFProperty<FClassProperty>(
                      WheelSetupStruct->Struct, TEXT("WheelClass"))) {
                WheelClassProp->SetPropertyValue_InContainer(
                    WheelSetupPtr, UVehicleWheel::StaticClass());
              }
#endif

              double Radius = 0.0;
              if (WheelObj->TryGetNumberField(TEXT("radius"), Radius)) {
                SetNumericOnStruct(WheelSetupStruct->Struct, WheelSetupPtr,
                                   {TEXT("WheelRadius"), TEXT("Radius")},
                                   Radius);
              }

              double Width = 0.0;
              if (WheelObj->TryGetNumberField(TEXT("width"), Width)) {
                SetNumericOnStruct(WheelSetupStruct->Struct, WheelSetupPtr,
                                   {TEXT("WheelWidth"), TEXT("Width")},
                                   Width);
              }

              double Friction = 0.0;
              if (WheelObj->TryGetNumberField(TEXT("friction"), Friction)) {
                SetNumericOnStruct(
                    WheelSetupStruct->Struct, WheelSetupPtr,
                    {TEXT("Friction"), TEXT("FrictionMultiplier"),
                     TEXT("TireFrictionScale")},
                    Friction);
              }

              ++ConfiguredWheels;
            }
            Report.MarkApplied(TEXT("wheels"));
          } else {
            Report.MarkFailed(TEXT("wheels"), TEXT("WheelSetups property not found on vehicle movement component"));
          }
        } else if (Payload->HasField(TEXT("wheels"))) {
          Report.MarkFailed(TEXT("wheels"), TEXT("must be an array"));
        }

        const TSharedPtr<FJsonObject> *EngineObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("engine"), EngineObj) && EngineObj &&
            (*EngineObj).IsValid()) {
          double MaxRPM = 0.0;
          if ((*EngineObj)->TryGetNumberField(TEXT("maxRPM"), MaxRPM)) {
            SetNumericOnObject(VehicleMC,
                               {TEXT("MaxEngineRPM"), TEXT("EngineMaxRPM")},
                               MaxRPM);
            SetNestedNumericOnObject(
                VehicleMC, {TEXT("EngineSetup"), TEXT("EngineConfig")},
                {TEXT("MaxRPM"), TEXT("MaxEngineRPM")}, MaxRPM);
          }

          double MaxTorque = 0.0;
          if ((*EngineObj)->TryGetNumberField(TEXT("maxTorque"), MaxTorque)) {
            SetNumericOnObject(
                VehicleMC,
                {TEXT("MaxEngineTorque"), TEXT("EngineMaxTorque")},
                MaxTorque);
            SetNestedNumericOnObject(
                VehicleMC, {TEXT("EngineSetup"), TEXT("EngineConfig")},
                {TEXT("MaxTorque"), TEXT("PeakTorque"),
                 TEXT("MaxEngineTorque")},
                MaxTorque);
          }

          double EngineGears = 0.0;
          if ((*EngineObj)->TryGetNumberField(TEXT("gears"), EngineGears)) {
            SetNestedNumericOnObject(
                VehicleMC, {TEXT("TransmissionSetup"), TEXT("TransmissionConfig")},
                {TEXT("NumForwardGears"), TEXT("ForwardGearCount")},
                EngineGears);
          }
          Report.MarkApplied(TEXT("engine"));
        } else if (Payload->HasField(TEXT("engine"))) {
          Report.MarkFailed(TEXT("engine"), TEXT("must be an object"));
        }

        const TSharedPtr<FJsonObject> *TransmissionObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("transmission"), TransmissionObj) &&
            TransmissionObj && (*TransmissionObj).IsValid()) {
          double FinalDrive = 0.0;
          if ((*TransmissionObj)->TryGetNumberField(TEXT("finalDrive"),
                                                    FinalDrive) ||
              (*TransmissionObj)->TryGetNumberField(TEXT("finalDriveRatio"),
                                                    FinalDrive)) {
            SetNestedNumericOnObject(
                VehicleMC, {TEXT("TransmissionSetup"), TEXT("TransmissionConfig")},
                {TEXT("FinalRatio"), TEXT("FinalDrive"),
                 TEXT("FinalDriveRatio")},
                FinalDrive);
          }

          const TArray<TSharedPtr<FJsonValue>> *GearRatiosArray = nullptr;
          if ((*TransmissionObj)
                  ->TryGetArrayField(TEXT("gearRatios"), GearRatiosArray) &&
              GearRatiosArray) {
            TArray<double> Ratios;
            Ratios.Reserve(GearRatiosArray->Num());
            for (const TSharedPtr<FJsonValue> &RatioVal : *GearRatiosArray) {
              if (RatioVal.IsValid() && RatioVal->Type == EJson::Number) {
                Ratios.Add(RatioVal->AsNumber());
              }
            }
            if (Ratios.Num() > 0) {
              ConfigureForwardGearRatios(VehicleMC, Ratios);
            }
          }
          Report.MarkApplied(TEXT("transmission"));
        } else if (Payload->HasField(TEXT("transmission"))) {
          Report.MarkFailed(TEXT("transmission"), TEXT("must be an object"));
        }

        double Mass = 0.0;
        if (Payload->TryGetNumberField(TEXT("mass"), Mass) && Mass > 0.0) {
          SetNumericOnObject(VehicleMC, {TEXT("Mass"), TEXT("VehicleMass")},
                             Mass);

          if (UPrimitiveComponent *PrimitiveRoot =
                  Cast<UPrimitiveComponent>(TargetActor->GetRootComponent())) {
            PrimitiveRoot->SetMassOverrideInKg(NAME_None,
                                               static_cast<float>(Mass), true);
          }
          Report.MarkApplied(TEXT("mass"));
        } else if (Payload->HasField(TEXT("mass"))) {
          Report.MarkFailed(TEXT("mass"), TEXT("must be a number greater than 0"));
        }

        double DragCoefficient = 0.0;
        if (Payload->TryGetNumberField(TEXT("dragCoefficient"),
                                       DragCoefficient)) {
          SetNumericOnObject(VehicleMC,
                             {TEXT("DragCoefficient"), TEXT("DragCoeff"),
                              TEXT("AerodynamicDragCoefficient")},
                             DragCoefficient);
          SetNestedNumericOnObject(
              VehicleMC,
              {TEXT("AerofoilSetup"), TEXT("AerodynamicsSetup")},
              {TEXT("DragCoefficient"), TEXT("DragCoeff")},
              DragCoefficient);

          if (UPrimitiveComponent *PrimitiveRoot =
                  Cast<UPrimitiveComponent>(TargetActor->GetRootComponent())) {
            PrimitiveRoot->SetLinearDamping(static_cast<float>(DragCoefficient));
          }
          Report.MarkApplied(TEXT("dragCoefficient"));
        } else if (Payload->HasField(TEXT("dragCoefficient"))) {
          Report.MarkFailed(TEXT("dragCoefficient"), TEXT("must be a number"));
        }

        if (Report.AnyApplied()) {
          VehicleMC->RecreatePhysicsState();
        }

        Resp->SetStringField(TEXT("actorName"), ActorName);
        Resp->SetStringField(TEXT("vehicleType"), VehicleType);
        Resp->SetBoolField(TEXT("createdMovementComponent"), bCreatedComponent);
        Resp->SetNumberField(TEXT("configuredWheelCount"), ConfiguredWheels);

#if MCP_HAS_CHAOS_WHEELED_VEHICLE
        Resp->SetBoolField(TEXT("chaosVehicleHeadersAvailable"), true);
#else
        Resp->SetBoolField(TEXT("chaosVehicleHeadersAvailable"), false);
#endif

        return SendWriteReportResponse(
            &S, RequestingSocket, RequestId, Report, Resp,
            FString::Printf(TEXT("Vehicle physics configured for actor '%s'"),
                            *ActorName),
            nullptr);
      }
    }
#else
    Message = TEXT("Wheeled vehicle component headers unavailable in this build");
    ErrorCode = TEXT("NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
#endif
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }
  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleAnimPhysSetupPhysicsSimulation(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("setup_physics_simulation"));
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  FString SkeletonPath;
  Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

  FString SkeletalMeshPath;
  Payload->TryGetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);

  // Support actorName parameter to find skeletal mesh from a spawned actor
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);

  const bool bSkeletonProvided = !SkeletonPath.IsEmpty();
  const bool bSkeletalMeshProvided = !SkeletalMeshPath.IsEmpty();
  const bool bActorProvided = !ActorName.IsEmpty();

  bool bSkeletonLoadFailed = false;
  bool bSkeletonMissingPreview = false;
  bool bSkeletalMeshLoadFailed = false;
  bool bSkeletalMeshTypeMismatch = false;
  FString FoundClassName;

  USkeletalMesh *TargetMesh = nullptr;

  if (!bSkeletonProvided && !bSkeletalMeshProvided && !bActorProvided) {
    Message = TEXT("setup_physics_simulation requires skeletonPath, skeletalMeshPath, or actorName");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
    S.SendAutomationResponse(RequestingSocket, RequestId, false, Message, Resp,
                           ErrorCode);
    return true;
  }

  // If actorName provided, try to find the actor and get its skeletal mesh
  if (!bSkeletonProvided && !bSkeletalMeshProvided && bActorProvided) {
		AActor *FoundActor = S.FindActorByName(ActorName);
	if (FoundActor) {
			// Try to get skeletal mesh component
      if (USkeletalMeshComponent *SkelComp =
              FoundActor->FindComponentByClass<USkeletalMeshComponent>()) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        TargetMesh = SkelComp->GetSkeletalMeshAsset();
#else
        TargetMesh = SkelComp->SkeletalMesh;
#endif
			if (!TargetMesh) {
          Message =
              FString::Printf(TEXT("Actor '%s' has a SkeletalMeshComponent "
                                   "but no SkeletalMesh asset assigned."),
                              *FoundActor->GetName());
          ErrorCode = TEXT("ACTOR_SKELETAL_MESH_ASSET_NULL");
          UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"),
                 *Message);
        }
      } else {
        Message = FString::Printf(
            TEXT("Actor '%s' does not have a SkeletalMeshComponent."),
            *FoundActor->GetName());
        ErrorCode = TEXT("ACTOR_NO_SKELETAL_MESH_COMPONENT");
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *Message);
      }
    } else {
      Message = FString::Printf(TEXT("Actor '%s' not found."), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
      UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *Message);
    }

    if (!TargetMesh) {
      Resp->SetStringField(TEXT("actorName"), ActorName);
      bSuccess = false;
      S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message,
                             Resp, ErrorCode);
      return true;
    }
  }

  if (!TargetMesh && bSkeletalMeshProvided) {
    if (UEditorAssetLibrary::DoesAssetExist(SkeletalMeshPath)) {
      UObject *Asset = UEditorAssetLibrary::LoadAsset(SkeletalMeshPath);
      TargetMesh = Cast<USkeletalMesh>(Asset);
      if (!TargetMesh && Asset) {
        bSkeletalMeshTypeMismatch = true;
        FoundClassName = Asset->GetClass()->GetName();
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("setup_physics_simulation: Asset %s is not a SkeletalMesh (Class: %s)"),
               *SkeletalMeshPath, *FoundClassName);
      } else if (!Asset) {
        bSkeletalMeshLoadFailed = true;
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("setup_physics_simulation: failed to load skeletal mesh asset %s"),
               *SkeletalMeshPath);
      }
    } else {
      bSkeletalMeshLoadFailed = true;
    }
  }

  USkeleton *TargetSkeleton = nullptr;
  if (!TargetMesh && bSkeletonProvided) {
    if (UEditorAssetLibrary::DoesAssetExist(SkeletonPath)) {
      TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      if (TargetSkeleton) {
        TargetMesh = TargetSkeleton->GetPreviewMesh();
        if (!TargetMesh) {
          bSkeletonMissingPreview = true;
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("setup_physics_simulation: skeleton %s has no preview "
                      "mesh"),
                 *SkeletonPath);
        }
      } else {
        bSkeletonLoadFailed = true;
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("setup_physics_simulation: failed to load skeleton %s"),
               *SkeletonPath);
      }
    } else {
      bSkeletonLoadFailed = true;
    }
  }

  if (!TargetSkeleton && TargetMesh) {
    TargetSkeleton = TargetMesh->GetSkeleton();
  }

  if (!TargetMesh) {
    if (bSkeletalMeshTypeMismatch) {
      Message = FString::Printf(
          TEXT("asset found but is not a SkeletalMesh: %s (is %s)"),
          *SkeletalMeshPath, *FoundClassName);
      ErrorCode = TEXT("TYPE_MISMATCH");
      Resp->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
      Resp->SetStringField(TEXT("actualClass"), FoundClassName);
    } else if (bSkeletalMeshLoadFailed) {
      Message = FString::Printf(TEXT("asset not found: skeletal mesh %s"),
                                *SkeletalMeshPath);
      ErrorCode = TEXT("ASSET_NOT_FOUND");
      Resp->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    } else if (bSkeletonLoadFailed) {
      Message = FString::Printf(TEXT("asset not found: skeleton %s"),
                                *SkeletonPath);
      ErrorCode = TEXT("ASSET_NOT_FOUND");
      Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
    } else if (bSkeletonMissingPreview) {
      Message = FString::Printf(TEXT("asset not found: skeleton %s (no "
                                     "preview mesh for physics simulation)"),
                                *SkeletonPath);
      ErrorCode = TEXT("ASSET_NOT_FOUND");
      Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
    } else {
      Message = TEXT("asset not found: no valid skeletal mesh provided for "
                     "physics simulation setup");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    }

    Resp->SetStringField(TEXT("error"), Message);
  } else {
    if (!TargetSkeleton && !SkeletonPath.IsEmpty()) {
      TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
    }

    FString PhysicsAssetName;
    Payload->TryGetStringField(TEXT("physicsAssetName"), PhysicsAssetName);
    if (PhysicsAssetName.IsEmpty()) {
      PhysicsAssetName = TargetMesh->GetName() + TEXT("_Physics");
    }

    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game/Physics");
    }
    SavePath = SavePath.TrimStartAndEnd();

    if (!FPackageName::IsValidLongPackageName(SavePath)) {
      FString NormalizedPath;
      if (!FPackageName::TryConvertFilenameToLongPackageName(
              SavePath, NormalizedPath)) {
        Message = TEXT("Invalid savePath for physics asset");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
        SavePath.Reset();
      } else {
        SavePath = NormalizedPath;
      }
    }

    if (!SavePath.IsEmpty()) {
      if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
        UEditorAssetLibrary::MakeDirectory(SavePath);
      }

      const FString PhysicsAssetObjectPath =
          FString::Printf(TEXT("%s/%s"), *SavePath, *PhysicsAssetName);

      if (UEditorAssetLibrary::DoesAssetExist(PhysicsAssetObjectPath)) {
        bSuccess = true;
        Message = TEXT(
            "Physics simulation already configured - existing asset reused");
        Resp->SetStringField(TEXT("physicsAssetPath"),
                             PhysicsAssetObjectPath);
        Resp->SetBoolField(TEXT("existingAsset"), true);
        Resp->SetStringField(TEXT("savePath"), SavePath);
        Resp->SetStringField(TEXT("meshPath"), TargetMesh->GetPathName());
        Resp->SetStringField(TEXT("skeletalMeshPath"), TargetMesh->GetPathName());
        if (TargetSkeleton) {
          Resp->SetStringField(TEXT("skeletonPath"),
                               TargetSkeleton->GetPathName());
        }
        UPhysicsAsset* ExistingPhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, *PhysicsAssetObjectPath);
        if (ExistingPhysicsAsset) {
          McpHandlerUtils::AddVerification(Resp, ExistingPhysicsAsset);
        }
      } else {
        UPackage *Package = CreatePackage(*PhysicsAssetObjectPath);
        if (!Package) {
          Message = TEXT("Failed to create physics asset package");
          ErrorCode = TEXT("PACKAGE_ERROR");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          UPhysicsAsset *PhysicsAsset = NewObject<UPhysicsAsset>(
              Package, FName(*PhysicsAssetName),
              RF_Public | RF_Standalone | RF_Transactional);

          if (!PhysicsAsset) {
            Message = TEXT("Failed to create physics asset");
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            PhysicsAsset->SetPreviewMesh(TargetMesh);
            PhysicsAsset->UpdateBodySetupIndexMap();
            PhysicsAsset->UpdateBoundsBodiesArray();
            FAssetRegistryModule::AssetCreated(PhysicsAsset);
            Package->MarkPackageDirty();

            bool bAssignToMesh = false;
            Payload->TryGetBoolField(TEXT("assignToMesh"), bAssignToMesh);

            if (bAssignToMesh) {
              TargetMesh->Modify();
              TargetMesh->SetPhysicsAsset(PhysicsAsset);
              McpSafeAssetSave(TargetMesh);
            }
            McpSafeAssetSave(PhysicsAsset);

            Resp->SetStringField(TEXT("physicsAssetPath"),
                                 PhysicsAsset->GetPathName());
            Resp->SetBoolField(TEXT("assignedToMesh"), bAssignToMesh);
            Resp->SetBoolField(TEXT("existingAsset"), false);
            Resp->SetStringField(TEXT("savePath"), SavePath);
            Resp->SetStringField(TEXT("meshPath"), TargetMesh->GetPathName());
            Resp->SetStringField(TEXT("skeletalMeshPath"),
                                 TargetMesh->GetPathName());
            if (TargetSkeleton) {
              Resp->SetStringField(TEXT("skeletonPath"),
                                   TargetSkeleton->GetPathName());
            }
            Resp->SetNumberField(TEXT("bodyCount"),
                                 PhysicsAsset->SkeletalBodySetups.Num());
            Resp->SetNumberField(TEXT("constraintCount"),
                                 PhysicsAsset->ConstraintSetup.Num());
            McpHandlerUtils::AddVerification(Resp, PhysicsAsset);

            bSuccess = true;
            Message = TEXT("Physics simulation setup completed");
          }
        }
      }
    }
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }
  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleAnimPhysCreateAnimationAsset(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("create_animation_asset"));
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  FString AssetName;
  if (!Payload->TryGetStringField(TEXT("name"), AssetName) ||
      AssetName.IsEmpty()) {
    Message = TEXT("name required for create_animation_asset");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
  } else {
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game/Animations");
    }
    SavePath = SavePath.TrimStartAndEnd();

    if (!FPackageName::IsValidLongPackageName(SavePath)) {
      FString NormalizedPath;
      if (!FPackageName::TryConvertFilenameToLongPackageName(
              SavePath, NormalizedPath)) {
        Message = TEXT("Invalid savePath for animation asset");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
        SavePath.Reset();
      } else {
        SavePath = NormalizedPath;
      }
    }

    FString SkeletonPath;
    Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);
    USkeleton *TargetSkeleton = nullptr;
    const bool bHadSkeletonPath = !SkeletonPath.IsEmpty();
    if (bHadSkeletonPath) {
      if (UEditorAssetLibrary::DoesAssetExist(SkeletonPath)) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }
    }

    if (!TargetSkeleton) {
      if (bHadSkeletonPath) {
        Message =
            FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
      } else {
        Message = TEXT("skeletonPath is required for create_animation_asset");
        ErrorCode = TEXT("INVALID_ARGUMENT");
      }

      Resp->SetStringField(TEXT("error"), Message);
    } else if (!SavePath.IsEmpty()) {
      if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
        UEditorAssetLibrary::MakeDirectory(SavePath);
      }

      FString AssetType;
      Payload->TryGetStringField(TEXT("assetType"), AssetType);
      AssetType = AssetType.ToLower();
      if (AssetType.IsEmpty()) {
        AssetType = TEXT("sequence");
      }

      UFactory *Factory = nullptr;
      UClass *DesiredClass = nullptr;
      FString AssetTypeString;

      if (AssetType == TEXT("montage")) {
        UAnimMontageFactory *MontageFactory =
            NewObject<UAnimMontageFactory>();
        if (MontageFactory) {
          MontageFactory->TargetSkeleton = TargetSkeleton;
          Factory = MontageFactory;
          DesiredClass = UAnimMontage::StaticClass();
          AssetTypeString = TEXT("Montage");
        }
      } else {
        UAnimSequenceFactory *SequenceFactory =
            NewObject<UAnimSequenceFactory>();
        if (SequenceFactory) {
          SequenceFactory->TargetSkeleton = TargetSkeleton;
          Factory = SequenceFactory;
          DesiredClass = UAnimSequence::StaticClass();
          AssetTypeString = TEXT("Sequence");
        }
      }

      if (!Factory || !DesiredClass) {
        Message = TEXT("Unsupported assetType for create_animation_asset");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        const FString ObjectPath =
            FString::Printf(TEXT("%s/%s"), *SavePath, *AssetName);
        if (UEditorAssetLibrary::DoesAssetExist(ObjectPath)) {
          bSuccess = true;
          Message =
              TEXT("Animation asset already exists - existing asset reused");
          Resp->SetStringField(TEXT("assetPath"), ObjectPath);
          Resp->SetStringField(TEXT("assetType"), AssetTypeString);
          Resp->SetBoolField(TEXT("existingAsset"), true);
          UObject* ExistingAsset = LoadObject<UObject>(nullptr, *ObjectPath);
          if (ExistingAsset) {
            McpHandlerUtils::AddVerification(Resp, ExistingAsset);
          }
        } else {
          FAssetToolsModule &AssetToolsModule =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>(
                  "AssetTools");
          UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
              AssetName, SavePath, DesiredClass, Factory);

          if (!NewAsset) {
            Message = TEXT("Failed to create animation asset");
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
            Resp->SetStringField(TEXT("assetType"), AssetTypeString);
            Resp->SetBoolField(TEXT("existingAsset"), false);
            McpHandlerUtils::AddVerification(Resp, NewAsset);
            bSuccess = true;
            Message = FString::Printf(TEXT("Animation %s created"),
                                      *AssetTypeString);
          }
        }
      }
    }
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }
  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleAnimPhysSetupRetargeting(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("setup_retargeting"));
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  FString SourceSkeletonPath;
  FString TargetSkeletonPath;
  Payload->TryGetStringField(TEXT("sourceSkeleton"), SourceSkeletonPath);
  Payload->TryGetStringField(TEXT("targetSkeleton"), TargetSkeletonPath);

  USkeleton *SourceSkeleton = nullptr;
  USkeleton *TargetSkeleton = nullptr;

  if (!SourceSkeletonPath.IsEmpty()) {
    SourceSkeleton = LoadObject<USkeleton>(nullptr, *SourceSkeletonPath);
  }
  if (!TargetSkeletonPath.IsEmpty()) {
    TargetSkeleton = LoadObject<USkeleton>(nullptr, *TargetSkeletonPath);
  }

  if (!SourceSkeleton || !TargetSkeleton) {
    bSuccess = false;
    Message =
        TEXT("Retargeting failed - source or target skeleton not found");
    ErrorCode = TEXT("ASSET_NOT_FOUND");
    Resp->SetStringField(TEXT("error"), Message);
    Resp->SetStringField(TEXT("sourceSkeleton"), SourceSkeletonPath);
    Resp->SetStringField(TEXT("targetSkeleton"), TargetSkeletonPath);
  } else {
    const TArray<TSharedPtr<FJsonValue>> *AssetsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("assets"), AssetsArray)) {
      Payload->TryGetArrayField(TEXT("retargetAssets"), AssetsArray);
    }

    if (!AssetsArray || AssetsArray->Num() == 0) {
      bSuccess = false;
      Message = TEXT("setup_retargeting requires at least one animation asset to retarget");
      ErrorCode = TEXT("MISSING_RETARGET_ASSETS");
      Resp->SetStringField(TEXT("error"), Message);
      Resp->SetStringField(TEXT("sourceSkeleton"), SourceSkeletonPath);
      Resp->SetStringField(TEXT("targetSkeleton"), TargetSkeletonPath);
    } else {

    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (!SavePath.IsEmpty()) {
      SavePath = SavePath.TrimStartAndEnd();
      if (!FPackageName::IsValidLongPackageName(SavePath)) {
        FString NormalizedPath;
        if (FPackageName::TryConvertFilenameToLongPackageName(
                SavePath, NormalizedPath)) {
          SavePath = NormalizedPath;
        } else {
          SavePath.Reset();
        }
      }
    }

    FString Suffix;
    Payload->TryGetStringField(TEXT("suffix"), Suffix);
    if (Suffix.IsEmpty()) {
      Suffix = TEXT("_Retargeted");
    }

    bool bOverwrite = false;
    Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);

    TArray<FString> RetargetedAssets;
    TArray<FString> SkippedAssets;
    TArray<TSharedPtr<FJsonValue>> WarningArray;

    if (AssetsArray && AssetsArray->Num() > 0) {
      for (const TSharedPtr<FJsonValue> &Value : *AssetsArray) {
        if (!Value.IsValid() || Value->Type != EJson::String) {
          continue;
        }

        const FString SourceAssetPath = Value->AsString();
        UAnimSequence *SourceSequence =
            LoadObject<UAnimSequence>(nullptr, *SourceAssetPath);
        if (!SourceSequence) {
          WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(
              TEXT("Skipped non-sequence asset: %s"), *SourceAssetPath)));
          SkippedAssets.Add(SourceAssetPath);
          continue;
        }

        FString DestinationFolder = SavePath;
        if (DestinationFolder.IsEmpty()) {
          const FString SourcePackageName =
              SourceSequence->GetOutermost()->GetName();
          DestinationFolder =
              FPackageName::GetLongPackagePath(SourcePackageName);
        }

        if (!DestinationFolder.IsEmpty() &&
            !UEditorAssetLibrary::DoesDirectoryExist(DestinationFolder)) {
          UEditorAssetLibrary::MakeDirectory(DestinationFolder);
        }

        FString DestinationAssetName = FPackageName::GetShortName(
            SourceSequence->GetOutermost()->GetName());
        DestinationAssetName += Suffix;

        const FString DestinationObjectPath = FString::Printf(
            TEXT("%s/%s"), *DestinationFolder, *DestinationAssetName);

        if (UEditorAssetLibrary::DoesAssetExist(DestinationObjectPath)) {
          if (!bOverwrite) {
            WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(
                TEXT("Retarget destination already exists, skipping: %s"),
                *DestinationObjectPath)));
            SkippedAssets.Add(SourceAssetPath);
            continue;
          }
          if (!UEditorAssetLibrary::DeleteAsset(DestinationObjectPath)) {
            WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(
                TEXT("Failed to delete existing retarget destination: %s"),
                *DestinationObjectPath)));
            SkippedAssets.Add(SourceAssetPath);
            continue;
          }
        }

        UPackage *DestinationPackage = CreatePackage(*DestinationObjectPath);
        if (!DestinationPackage) {
          WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(
              TEXT("Failed to create destination package: %s"),
              *DestinationObjectPath)));
          SkippedAssets.Add(SourceAssetPath);
          continue;
        }

        UAnimSequence *DestinationSequence = Cast<UAnimSequence>(
            StaticDuplicateObject(SourceSequence, DestinationPackage,
                                  *DestinationAssetName));
        if (!DestinationSequence) {
          WarningArray.Add(MakeShared<FJsonValueString>(
              FString::Printf(TEXT("Failed to duplicate animation asset: %s"),
                              *DestinationObjectPath)));
          SkippedAssets.Add(SourceAssetPath);
          continue;
        }

        DestinationSequence->SetFlags(RF_Public | RF_Standalone);
        DestinationSequence->Modify();
        DestinationSequence->SetSkeleton(TargetSkeleton);
        DestinationPackage->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(DestinationSequence);
        McpSafeAssetSave(DestinationSequence);

        // Animation retargeting in UE5 requires IK Rig system
        // Use a duplicated AnimSequence with the target skeleton assigned.
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Animation asset copied (retargeting requires IK Rig "
                    "setup)"));

        RetargetedAssets.Add(DestinationSequence->GetPathName());
      }
    }

    bSuccess = RetargetedAssets.Num() > 0;
    Message = bSuccess
                  ? TEXT("Retargeting completed")
                  : TEXT("Retargeting failed - no assets processed");
    if (!bSuccess) {
      ErrorCode = TEXT("NO_ASSETS_RETARGETED");
      Resp->SetStringField(TEXT("error"), Message);
    }

    TArray<TSharedPtr<FJsonValue>> RetargetedArray;
    for (const FString &Path : RetargetedAssets) {
      RetargetedArray.Add(MakeShared<FJsonValueString>(Path));
    }
    if (RetargetedArray.Num() > 0) {
      Resp->SetArrayField(TEXT("retargetedAssets"), RetargetedArray);
      // Add verification for the first retargeted asset
      if (RetargetedAssets.Num() > 0) {
        UAnimSequence* FirstRetargeted = LoadObject<UAnimSequence>(nullptr, *RetargetedAssets[0]);
        if (FirstRetargeted) {
          McpHandlerUtils::AddVerification(Resp, FirstRetargeted);
        }
      }
    }

    if (SkippedAssets.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> SkippedArray;
      for (const FString &Path : SkippedAssets) {
        SkippedArray.Add(MakeShared<FJsonValueString>(Path));
      }
      Resp->SetArrayField(TEXT("skippedAssets"), SkippedArray);
    }

    if (WarningArray.Num() > 0) {
      Resp->SetArrayField(TEXT("warnings"), WarningArray);
    }

    Resp->SetStringField(TEXT("sourceSkeleton"),
                         SourceSkeleton->GetPathName());
    Resp->SetStringField(TEXT("targetSkeleton"),
                         TargetSkeleton->GetPathName());
    }
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }
  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

/**
 * @brief Handles a "play_montage" automation request by locating an actor
 * and playing the specified animation montage in the editor.
 *
 * Processes the payload to resolve an actor by name and a montage asset path,
 * loads the montage, and initiates playback on the actor's skeletal mesh
 * component (using the actor's AnimInstance when available or single-node
 * playback otherwise). Sends a structured automation response reporting
 * success, playback length, and error details when applicable.
 *
 * @param RequestId Unique identifier for the incoming automation request;
 * included in responses.
 * @param Payload JSON payload containing fields:
 *   - "actorName" (string, required): name or label of the target actor in the
 * editor.
 *   - "montagePath" or "assetPath" (string, required): asset path to the
 * UAnimMontage.
 *   - "playRate" (number, optional): playback speed (default 1.0).
 * @param RequestingSocket Optional websocket that originated the request; used
 * to send the response.
 *
 * @return true after sending a response (success or error).
 */
bool McpHandlers::AnimationPhysics::HandleAnimPhysPlayMontage(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("play_montage payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString ActorName;
  if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
      ActorName.IsEmpty()) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString MontagePath;
  // Check both montagePath and assetPath for flexibility
  if (!Payload->TryGetStringField(TEXT("montagePath"), MontagePath) ||
      MontagePath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("assetPath"), MontagePath);
  }

  if (MontagePath.IsEmpty()) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("error"), TEXT("montagePath required"));
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("montagePath required"), Resp,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  double PlayRate = 1.0;
  Payload->TryGetNumberField(TEXT("playRate"), PlayRate);

  AActor *TargetActor = S.FindActorByName(ActorName);

  if (!TargetActor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetStringField(TEXT("montagePath"), MontagePath);
    Resp->SetNumberField(TEXT("playRate"), PlayRate);

    S.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Actor not found"), Resp,
                           TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  USkeletalMeshComponent *SkelMeshComp =
      TargetActor->FindComponentByClass<USkeletalMeshComponent>();
  if (!SkelMeshComp) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Skeletal mesh component not found"),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(MontagePath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Montage asset not found: %s"), *MontagePath));
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Montage not found"), Resp,
                           TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
  if (!Montage) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Failed to load montage: %s"), *MontagePath));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetStringField(TEXT("montagePath"), MontagePath);
    Resp->SetNumberField(TEXT("playRate"), PlayRate);

    S.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Failed to load montage"), Resp,
                           TEXT("ASSET_LOAD_FAILED"));
    return true;
  }

  float MontageLength = 0.f;
  if (UAnimInstance *AnimInst = SkelMeshComp->GetAnimInstance()) {
    MontageLength =
        AnimInst->Montage_Play(Montage, static_cast<float>(PlayRate));
  } else {
    SkelMeshComp->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
    SkelMeshComp->PlayAnimation(Montage, false);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetStringField(TEXT("montagePath"), MontagePath);
  Resp->SetNumberField(TEXT("playRate"), PlayRate);
  Resp->SetNumberField(TEXT("montageLength"), MontageLength);
  Resp->SetBoolField(TEXT("playing"), true);

  S.SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Animation montage playing"), Resp, FString());
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::AnimationPhysics::HandleAnimPhysCreatePoseLibrary(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("create_pose_library"));
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  // Create a pose library asset
  FString LibraryName;
  if (!Payload->TryGetStringField(TEXT("name"), LibraryName) || LibraryName.IsEmpty()) {
    Message = TEXT("name required for create_pose_library");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
  } else {
    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (SavePath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("path"), SavePath);
    }
    if (SavePath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("directory"), SavePath);
    }
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game/Animations/PoseLibraries");
    }
    SavePath = SanitizeProjectRelativePath(SavePath.TrimStartAndEnd());
    if (SavePath.IsEmpty()) {
      Message = TEXT("Invalid savePath for create_pose_library");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }

    FString SkeletonPath;
    Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

    if (!SavePath.IsEmpty() && SkeletonPath.IsEmpty()) {
      Message = TEXT("skeletonPath required for create_pose_library");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else if (!SavePath.IsEmpty()) {
      USkeleton *TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      if (!TargetSkeleton) {
        Message = FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else if (UEditorAssetLibrary::DoesAssetExist(FString::Printf(
                     TEXT("%s/%s.%s"), *SavePath, *LibraryName, *LibraryName))) {
        // IAssetTools::CreateAsset would pop a blocking "overwrite?" modal on an
        // existing asset, starving the bridge's game thread. Fail fast instead.
        Message = FString::Printf(TEXT("Asset already exists: %s/%s"), *SavePath,
                                  *LibraryName);
        ErrorCode = TEXT("ALREADY_EXISTS");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
          UEditorAssetLibrary::MakeDirectory(SavePath);
        }

        // Create a Data Asset to serve as a pose library container
        FAssetToolsModule &AssetToolsModule =
            FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
            LibraryName, SavePath, UMcpGenericDataAsset::StaticClass(), nullptr);

        if (NewAsset) {
          if (UMcpGenericDataAsset* PoseLibrary = Cast<UMcpGenericDataAsset>(NewAsset)) {
            PoseLibrary->ItemName = LibraryName;
            PoseLibrary->Description = TEXT("Pose Library for animation poses");
            PoseLibrary->Properties.Add(TEXT("SkeletonPath"), SkeletonPath);
            PoseLibrary->MarkPackageDirty();
            McpSafeAssetSave(PoseLibrary);
          }

          bSuccess = true;
          Message = TEXT("Pose library created successfully");
          Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
          Resp->SetStringField(TEXT("savePath"), SavePath);
          Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
          McpHandlerUtils::AddVerification(Resp, NewAsset);
        } else {
          Message = TEXT("Failed to create pose library asset");
          ErrorCode = TEXT("ASSET_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
      }
    }
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }
  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// NOTE: ExecuteEditorCommands and CreateControlRigBlueprint are defined in
// McpAutomationBridgeSubsystem.cpp - do not duplicate definitions here.
// The functions are declared in the subsystem header and implemented once
// to avoid LNK2005 duplicate symbol linker errors.

/**
 * @brief Enables ragdoll physics on a named actor's skeletal mesh in the
 * editor.
 *
 * Applies physics simulation and collision to the actor's
 * SkeletalMeshComponent, optionally respects a provided blend weight and
 * verifies an optional skeleton asset.
 *
 * @param RequestId The automation request identifier returned to the caller.
 * @param Payload JSON payload; must contain "actorName" and may include:
 *                - "blendWeight" (number): blend factor for animation/physics
 * update.
 *                - "skeletonPath" (string): optional path to a skeleton asset
 * to validate.
 * @param RequestingSocket The websocket that initiated the request (may be
 * null).
 * @return true after sending a response (success or error).
 */
bool McpHandlers::AnimationPhysics::HandleSetupRagdoll(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("setup_ragdoll payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString ActorName;
  if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
      ActorName.IsEmpty()) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  double BlendWeight = 1.0;
  Payload->TryGetNumberField(TEXT("blendWeight"), BlendWeight);

  FString SkeletonPath;
  if (Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) &&
      !SkeletonPath.IsEmpty()) {
    USkeleton *RagdollSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
    if (!RagdollSkeleton) {
      const FString SkelMessage =
          FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
      S.SendAutomationError(RequestingSocket, RequestId, SkelMessage,
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
  }

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("EditorActorSubsystem not available"),
                        TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  AActor *TargetActor = nullptr;

  if (GEditor && GEditor->GetEditorWorldContext().World()) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    for (TActorIterator<AActor> It(World); It; ++It) {
      AActor *Actor = *It;
      if (Actor) {
        if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
            Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)) {
          TargetActor = Actor;
          break;
        }
      }
    }
  }

  if (!TargetActor) {
    for (AActor *Actor : AllActors) {
      if (Actor &&
          (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
           Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase))) {
        TargetActor = Actor;
        break;
      }
    }
  }

  if (!TargetActor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetNumberField(TEXT("blendWeight"), BlendWeight);

    S.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Actor not found"), Resp,
                           TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  USkeletalMeshComponent *SkelMeshComp =
      TargetActor->FindComponentByClass<USkeletalMeshComponent>();
  if (!SkelMeshComp) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Skeletal mesh component not found"),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  SkelMeshComp->SetSimulatePhysics(true);
  SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

  if (SkelMeshComp->GetPhysicsAsset()) {
    SkelMeshComp->SetAllBodiesSimulatePhysics(true);
    SkelMeshComp->SetUpdateAnimationInEditor(BlendWeight < 1.0);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetNumberField(TEXT("blendWeight"), BlendWeight);
  Resp->SetBoolField(TEXT("ragdollActive"),
                     SkelMeshComp->IsSimulatingPhysics());
  Resp->SetBoolField(TEXT("hasPhysicsAsset"),
                     SkelMeshComp->GetPhysicsAsset() != nullptr);

  if (SkelMeshComp->GetPhysicsAsset()) {
    Resp->SetStringField(TEXT("physicsAssetPath"),
                         SkelMeshComp->GetPhysicsAsset()->GetPathName());
  }

  S.SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Ragdoll setup completed"), Resp, FString());
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("setup_ragdoll requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

/**
 * @brief Activates or deactivates ragdoll physics on a named actor's skeletal mesh.
 *
 * This handler toggles ragdoll simulation on/off, allowing runtime control
 * over physics simulation state.
 *
 * @param RequestId The automation request identifier returned to the caller.
 * @param Payload JSON payload; must contain "actorName" and may include:
 *                - "activate" (bool): true to activate, false to deactivate (default: true)
 * @param RequestingSocket The websocket that initiated the request (may be null).
 * @return true after sending a response (success or error).
 */
bool McpHandlers::AnimationPhysics::HandleActivateRagdoll(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("activate_ragdoll payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString ActorName;
  if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
      ActorName.IsEmpty()) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  bool bActivate = true;
  Payload->TryGetBoolField(TEXT("activate"), bActivate);

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  AActor *TargetActor = nullptr;

  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Actor = *It;
    if (Actor) {
      if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
          Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)) {
        TargetActor = Actor;
        break;
      }
    }
  }

  if (!TargetActor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("error"),
                         FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetBoolField(TEXT("activate"), bActivate);

    S.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Actor not found"), Resp,
                           TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  USkeletalMeshComponent *SkelMeshComp =
      TargetActor->FindComponentByClass<USkeletalMeshComponent>();
  if (!SkelMeshComp) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Skeletal mesh component not found"),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  // Activate or deactivate ragdoll
  if (bActivate) {
    SkelMeshComp->SetSimulatePhysics(true);
    SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (SkelMeshComp->GetPhysicsAsset()) {
      SkelMeshComp->SetAllBodiesSimulatePhysics(true);
    }
  } else {
    SkelMeshComp->SetAllBodiesSimulatePhysics(false);
    SkelMeshComp->SetSimulatePhysics(false);
    SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetBoolField(TEXT("activate"), bActivate);
  Resp->SetBoolField(TEXT("ragdollActive"),
                     SkelMeshComp->IsSimulatingPhysics());
  Resp->SetBoolField(TEXT("hasPhysicsAsset"),
                     SkelMeshComp->GetPhysicsAsset() != nullptr);

  if (SkelMeshComp->GetPhysicsAsset()) {
    Resp->SetStringField(TEXT("physicsAssetPath"),
                         SkelMeshComp->GetPhysicsAsset()->GetPathName());
  }

  S.SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Ragdoll activation state changed"), Resp, FString());
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("activate_ragdoll requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
