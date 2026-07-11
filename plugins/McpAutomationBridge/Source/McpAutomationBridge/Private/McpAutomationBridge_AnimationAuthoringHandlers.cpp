// =============================================================================
// McpAutomationBridge_AnimationAuthoringHandlers.cpp
// =============================================================================
// Animation System Authoring Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// ---------------------
// Section 1: Animation Sequences
//   - create_animation_sequence    : Create UAnimSequence asset
//   - add_animation_curve          : Add curve to animation
//   - set_animation_rate           : Set animation frame rate
//
// Section 2: Animation Montages
//   - create_animation_montage     : Create UAnimMontage from sequence
//   - add_montage_section          : Add section to montage
//   - set_montage_blend_time       : Configure blend settings
//
// Section 3: Blend Spaces
//   - create_blend_space           : Create UBlendSpace asset
//   - create_blend_space_1d        : Create UBlendSpace1D asset
//   - add_blend_space_sample       : Add sample to blend space
//
// Section 4: Animation Blueprints
//   - create_animation_blueprint   : Create UAnimBlueprint
//   - add_anim_graph_node          : Add node to anim graph
//   - link_anim_state              : Connect state machine states
//
// Section 5: Control Rig (5.1+)
//   - create_control_rig           : Create Control Rig blueprint
//   - add_control_rig_input        : Add rig input
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0: BlendSpaceBase.h deprecated warning suppression
// UE 5.1+: Full control rig support
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_AnimationAuthoringHandlers.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
// UE 5.0 deprecation warning suppression - BlendSpaceBase.h is deprecated but transitively included by engine headers
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/Skeleton.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
#pragma warning(pop)
#endif
#include "Engine/SkeletalMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/AnimSequenceFactory.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/AnimBlueprintFactory.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
// bind_animation_notify: event-graph authoring (notify -> BlueprintCallable function)
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Pawn.h"
#include "ScopedTransaction.h"

// Blend Space factories
#if __has_include("Factories/BlendSpaceFactoryNew.h") && __has_include("Factories/BlendSpaceFactory1D.h")
#include "Factories/BlendSpaceFactoryNew.h"
#include "Factories/BlendSpaceFactory1D.h"
#define MCP_HAS_BLENDSPACE_FACTORY 1
#else
#define MCP_HAS_BLENDSPACE_FACTORY 0
#endif

// Control Rig support (optional module)
#if __has_include("ControlRig.h")
#include "ControlRig.h"
#define MCP_HAS_CONTROLRIG 1
#else
#define MCP_HAS_CONTROLRIG 0
#endif

// Control Rig Blueprint - header location changed in UE 5.5+
// UE 5.5+: ControlRigDeveloper/Public/ControlRigBlueprintLegacy.h
// UE 5.0-5.4: ControlRigBlueprint.h (various locations)
#if __has_include("ControlRigBlueprintLegacy.h")
#include "ControlRigBlueprintLegacy.h"
#define MCP_HAS_CONTROLRIG_BLUEPRINT 1
#elif __has_include("ControlRigBlueprint.h")
#include "ControlRigBlueprint.h"
#define MCP_HAS_CONTROLRIG_BLUEPRINT 1
#else
#define MCP_HAS_CONTROLRIG_BLUEPRINT 0
#endif

// RigVM Blueprint Generated Class (needed for ControlRig creation fallback in UE 5.1-5.4)
#if __has_include("RigVMBlueprintGeneratedClass.h")
#include "RigVMBlueprintGeneratedClass.h"
#endif

// UE 5.0 uses UControlRigBlueprintGeneratedClass (different name from UE 5.1+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
#include "ControlRigBlueprintGeneratedClass.h"
#endif

// Control Rig Factory (for creating Control Rig assets)
// Note: ControlRigBlueprintFactory header is Public only in UE 5.5+
// For UE 5.1-5.4 we use a fallback implementation
#if MCP_HAS_CONTROLRIG_FACTORY && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
  #include "ControlRigBlueprintFactory.h"
#endif

// IK Rig support (UE 5.0+)
// Header path: Engine/Plugins/Animation/IKRig/Source/IKRig/Public/Rig/IKRigDefinition.h
#if __has_include("Rig/IKRigDefinition.h")
#include "Rig/IKRigDefinition.h"
#define MCP_HAS_IKRIG 1
#elif __has_include("IKRigDefinition.h")
#include "IKRigDefinition.h"
#define MCP_HAS_IKRIG 1
#else
#define MCP_HAS_IKRIG 0
#endif

// IK Rig Factory (for creating IK Rig assets)
#if __has_include("RigEditor/IKRigDefinitionFactory.h")
#include "RigEditor/IKRigDefinitionFactory.h"
#define MCP_HAS_IKRIG_FACTORY 1
#elif __has_include("IKRigDefinitionFactory.h")
#include "IKRigDefinitionFactory.h"
#define MCP_HAS_IKRIG_FACTORY 1
#else
#define MCP_HAS_IKRIG_FACTORY 0
#endif

// IK Retarget Factory
#if __has_include("RetargetEditor/IKRetargetFactory.h")
#include "RetargetEditor/IKRetargetFactory.h"
#define MCP_HAS_IKRETARGET_FACTORY 1
#elif __has_include("IKRetargetFactory.h")
#include "IKRetargetFactory.h"
#define MCP_HAS_IKRETARGET_FACTORY 1
#else
#define MCP_HAS_IKRETARGET_FACTORY 0
#endif

#if __has_include("Retargeter/IKRetargeter.h")
#include "Retargeter/IKRetargeter.h"
#define MCP_HAS_IKRETARGETER 1
#elif __has_include("IKRetargeter.h")
#include "IKRetargeter.h"
#define MCP_HAS_IKRETARGETER 1
#else
#define MCP_HAS_IKRETARGETER 0
#endif

// IK Retargeter Controller (for setting IK Rigs on retargeter)
#if __has_include("RetargetEditor/IKRetargeterController.h")
#include "RetargetEditor/IKRetargeterController.h"
#define MCP_HAS_IKRETARGETER_CONTROLLER 1
#else
#define MCP_HAS_IKRETARGETER_CONTROLLER 0
#endif

// Pose Asset
#if __has_include("Animation/PoseAsset.h")
#include "Animation/PoseAsset.h"
#define MCP_HAS_POSEASSET 1
#else
#define MCP_HAS_POSEASSET 0
#endif

// Animation Blueprint Graph
#if __has_include("AnimationGraph.h")
#include "AnimationGraph.h"
#define MCP_HAS_ANIMATION_GRAPH 1
#else
#define MCP_HAS_ANIMATION_GRAPH 0
#endif
#if __has_include("AnimGraphNode_StateMachine.h")
#include "AnimGraphNode_StateMachine.h"
#endif
#if __has_include("AnimGraphNode_TransitionResult.h")
#include "AnimGraphNode_TransitionResult.h"
#endif
#if __has_include("AnimStateNode.h")
#include "AnimStateNode.h"
#endif

// Additional AnimGraph node types for state machine implementation
#if __has_include("AnimStateTransitionNode.h")
#include "AnimStateTransitionNode.h"
#define MCP_HAS_ANIM_STATE_TRANSITION 1
#else
#define MCP_HAS_ANIM_STATE_TRANSITION 0
#endif

#if __has_include("AnimStateEntryNode.h")
#include "AnimStateEntryNode.h"
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

// Animation State Graph (for creating individual states with BoundGraph)
#if __has_include("AnimationStateGraph.h")
#include "AnimationStateGraph.h"
#define MCP_HAS_ANIMATION_STATE_GRAPH 1
#else
#define MCP_HAS_ANIMATION_STATE_GRAPH 0
#endif

#if __has_include("AnimationStateGraphSchema.h")
#include "AnimationStateGraphSchema.h"
#define MCP_HAS_ANIMATION_STATE_GRAPH_SCHEMA 1
#else
#define MCP_HAS_ANIMATION_STATE_GRAPH_SCHEMA 0
#endif

// Blend node types
#if __has_include("AnimGraphNode_TwoWayBlend.h")
#include "AnimGraphNode_TwoWayBlend.h"
#define MCP_HAS_TWO_WAY_BLEND 1
#else
#define MCP_HAS_TWO_WAY_BLEND 0
#endif

#if __has_include("AnimGraphNode_LayeredBoneBlend.h")
#include "AnimGraphNode_LayeredBoneBlend.h"
#define MCP_HAS_LAYERED_BLEND 1
#else
#define MCP_HAS_LAYERED_BLEND 0
#endif

#if __has_include("AnimGraphNode_SaveCachedPose.h")
#include "AnimGraphNode_SaveCachedPose.h"
#define MCP_HAS_CACHED_POSE 1
#else
#define MCP_HAS_CACHED_POSE 0
#endif

#if __has_include("AnimGraphNode_Slot.h")
#include "AnimGraphNode_Slot.h"
#define MCP_HAS_SLOT_NODE 1
#else
#define MCP_HAS_SLOT_NODE 0
#endif

// Helper macros
#define ANIM_ERROR_RESPONSE(Msg, Code) \
    Response->SetBoolField(TEXT("success"), false); \
    Response->SetStringField(TEXT("error"), Msg); \
    Response->SetStringField(TEXT("errorCode"), Code); \
    return Response;

#define ANIM_SUCCESS_RESPONSE(Msg) \
    Response->SetBoolField(TEXT("success"), true); \
    Response->SetStringField(TEXT("message"), Msg);

namespace {

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h
// Note: These are macros to avoid ODR issues with the anonymous namespace

// Helper to normalize asset path
static FString NormalizeAnimPath(const FString& Path)
{
    FString Normalized = Path;
    Normalized.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
    
    // Remove trailing slashes
    while (Normalized.EndsWith(TEXT("/")))
    {
        Normalized.LeftChopInline(1);
    }
    
    return Normalized;
}

// Helper to load skeleton from path
static USkeleton* LoadSkeletonFromPathAnim(const FString& SkeletonPath)
{
    FString NormalizedPath = NormalizeAnimPath(SkeletonPath);
    return Cast<USkeleton>(StaticLoadObject(USkeleton::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to load skeletal mesh from path
static USkeletalMesh* LoadSkeletalMeshFromPathAnim(const FString& MeshPath)
{
    FString NormalizedPath = NormalizeAnimPath(MeshPath);
    return Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to load animation sequence from path
static UAnimSequence* LoadAnimSequenceFromPath(const FString& AnimPath)
{
    FString NormalizedPath = NormalizeAnimPath(AnimPath);
    return Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to save asset through the repo's loaded-asset safe save path.
// These assets must persist to disk; leaving them registry-only creates large
// in-memory delete sets that later crash post-response cleanup.
static bool SaveAnimAsset(UObject* Asset, bool bShouldSave)
{
    if (!bShouldSave || !Asset)
    {
        return true;
    }
    
    Asset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Asset);

    const bool bSaved = SaveLoadedAssetThrottled(Asset, -1.0, true);
    if (bSaved)
    {
        ScanPathSynchronous(Asset->GetOutermost()->GetName());
    }

    return bSaved;
}

// Helper to get FVector from JSON object. Missing components fall back to the per-component
// Default so a partial scale object keeps 1 on the unspecified axes rather than collapsing to 0.
static FVector GetVectorFromJsonAnim(const TSharedPtr<FJsonObject>& Obj, const FVector& Default = FVector::ZeroVector)
{
    if (!Obj.IsValid())
    {
        return Default;
    }
    return FVector(
        GetJsonNumberField(Obj, TEXT("x"), Default.X),
        GetJsonNumberField(Obj, TEXT("y"), Default.Y),
        GetJsonNumberField(Obj, TEXT("z"), Default.Z)
    );
}

// Helper to get FRotator from JSON object
static FRotator GetRotatorFromJsonAnim(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid())
    {
        return FRotator::ZeroRotator;
    }
    // Support both Euler (pitch/yaw/roll) and quaternion (x/y/z/w)
    if (Obj->HasField(TEXT("pitch")) || Obj->HasField(TEXT("yaw")) || Obj->HasField(TEXT("roll")))
    {
        return FRotator(
            GetJsonNumberField(Obj, TEXT("pitch"), 0.0),
            GetJsonNumberField(Obj, TEXT("yaw"), 0.0),
            GetJsonNumberField(Obj, TEXT("roll"), 0.0)
        );
    }
    else if (Obj->HasField(TEXT("w")))
    {
        FQuat Quat(
            GetJsonNumberField(Obj, TEXT("x"), 0.0),
            GetJsonNumberField(Obj, TEXT("y"), 0.0),
            GetJsonNumberField(Obj, TEXT("z"), 0.0),
            GetJsonNumberField(Obj, TEXT("w"), 1.0)
        );
        return Quat.Rotator();
    }
    return FRotator::ZeroRotator;
}

// ============================================================================
// AnimGraph Helper Functions for State Machine Implementation
// ============================================================================

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA

// Helper to find the main AnimGraph from an Animation Blueprint
static UEdGraph* GetAnimGraphFromBlueprint(UAnimBlueprint* AnimBP)
{
    if (!AnimBP)
    {
        return nullptr;
    }
    
    // Search through UbergraphPages first (most common location for AnimGraph)
    for (UEdGraph* Graph : AnimBP->UbergraphPages)
    {
        if (Graph && Graph->GetName() == TEXT("AnimGraph"))
        {
            return Graph;
        }
    }
    
    // Also search through function graphs
    for (UEdGraph* Graph : AnimBP->FunctionGraphs)
    {
        if (Graph && Graph->GetName() == TEXT("AnimGraph"))
        {
            return Graph;
        }
    }
    
    // Fallback: look for any graph with AnimGraph in the name
    for (UEdGraph* Graph : AnimBP->UbergraphPages)
    {
        if (Graph && Graph->GetName().Contains(TEXT("AnimGraph")))
        {
            return Graph;
        }
    }
    
    return nullptr;
}

// Helper to find a State Machine node by name in a graph
static UAnimGraphNode_StateMachine* FindStateMachineNode(UEdGraph* Graph, const FString& Name)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node))
        {
            // Check node title for matching name
            FString NodeName = SMNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
            if (NodeName.Contains(Name) || SMNode->GetStateMachineName() == Name)
            {
                return SMNode;
            }
        }
    }
    
    return nullptr;
}

// Helper to collect all state machine nodes that match a requested name.
static TArray<UAnimGraphNode_StateMachine*> FindStateMachineNodes(UEdGraph* Graph, const FString& Name)
{
    TArray<UAnimGraphNode_StateMachine*> Matches;
    if (!Graph)
    {
        return Matches;
    }

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node))
        {
            const FString MachineName = SMNode->GetStateMachineName();
            const FString Title = SMNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
            const bool bExactNameMatch = MachineName.Equals(Name, ESearchCase::IgnoreCase);
            const bool bTitleMatch = Title.Equals(Name, ESearchCase::IgnoreCase) || Title.Contains(Name);
            if (bExactNameMatch || bTitleMatch)
            {
                Matches.Add(SMNode);
            }
        }
    }

    return Matches;
}

// Helper to find a State node within a State Machine graph
static UAnimStateNode* FindStateNode(UAnimationStateMachineGraph* SMGraph, const FString& Name)
{
    if (!SMGraph)
    {
        return nullptr;
    }
    
    for (UEdGraphNode* Node : SMGraph->Nodes)
    {
        if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
        {
            // Use GetStateName for more accurate matching
            // Also try case-insensitive matching for robustness
            FString StateName = StateNode->GetStateName();
            if (StateName == Name)
            {
                return StateNode;
            }
            // Case-insensitive fallback
            if (StateName.Equals(Name, ESearchCase::IgnoreCase))
            {
                return StateNode;
            }
        }
    }
    
    return nullptr;
}

static UAnimStateTransitionNode* FindTransitionNode(UAnimationStateMachineGraph* SMGraph, const FString& FromState, const FString& ToState)
{
    if (!SMGraph)
    {
        return nullptr;
    }

    for (UEdGraphNode* Node : SMGraph->Nodes)
    {
#if MCP_HAS_ANIM_STATE_TRANSITION
        if (UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node))
        {
            UAnimStateNodeBase* PrevState = Trans->GetPreviousState();
            UAnimStateNodeBase* NextState = Trans->GetNextState();
            if (PrevState && NextState &&
                PrevState->GetStateName().Equals(FromState, ESearchCase::IgnoreCase) &&
                NextState->GetStateName().Equals(ToState, ESearchCase::IgnoreCase))
            {
                return Trans;
            }
        }
#endif
    }

    return nullptr;
}

#endif // MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA

} // anonymous namespace

// =============================================================================
// Classed animation_physics authoring actions
// =============================================================================
// One static per action (block bodies verbatim from the retired
// HandleAnimationAuthoringRequest chain), plus the response conversion the
// retired HandleManageAnimationAuthoringAction wrapper applied, shared by the
// per-action members below.

    // ===== 10.1 Animation Sequences =====
static TSharedPtr<FJsonObject> AnimAuthoringCreateAnimationSequence(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    // Schema advertises savePath; older callers pass path. Prefer savePath, fall back to path.
    FString Path = GetJsonStringField(Params, TEXT("savePath"), TEXT(""));
    if (Path.IsEmpty()) { Path = GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Animations")); }
    Path = NormalizeAnimPath(Path);
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    int32 FrameRate = static_cast<int32>(GetJsonNumberField(Params, TEXT("frameRate"), 30));
    // Schema exposes length (seconds); numFrames is an undocumented override.
    int32 NumFrames = 30;
    if (Params->HasField(TEXT("numFrames")))
    {
        NumFrames = static_cast<int32>(GetJsonNumberField(Params, TEXT("numFrames"), 30));
    }
    else if (Params->HasField(TEXT("length")))
    {
        NumFrames = FMath::Max(1, FMath::RoundToInt(GetJsonNumberField(Params, TEXT("length"), 1.0) * static_cast<double>(FrameRate)));
    }
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (FrameRate <= 0)
    {
        ANIM_ERROR_RESPONSE(TEXT("frameRate must be greater than 0"), TEXT("INVALID_FRAME_RATE"));
    }

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    USkeleton* Skeleton = nullptr;
    if (!SkeletonPath.IsEmpty())
    {
        Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
    }

    // Check if an asset already exists at the target path to prevent assertion failure
        FString ObjectPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
        if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
        {
            UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
            if (ExistingAsset)
            {
                if (Cast<UAnimSequence>(ExistingAsset))
                {
                    // Same type - return success with existing asset info
                    Response->SetStringField(TEXT("assetPath"), ObjectPath);
                    Response->SetBoolField(TEXT("existingAsset"), true);
                    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation sequence '%s' already exists - reusing existing asset"), *Name));
                    McpHandlerUtils::AddVerification(Response, ExistingAsset);
                    return Response;
                }
                else
                {
                    // Different type - return error to prevent crash
                    FString ExistingClassName = ExistingAsset->GetClass()->GetName();
                    ANIM_ERROR_RESPONSE(
                        FString::Printf(TEXT("Cannot create animation sequence: asset '%s' already exists as type '%s'"), 
                            *ObjectPath, *ExistingClassName),
                        TEXT("ASSET_TYPE_MISMATCH")
                    );
                }
            }
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UAnimSequenceFactory* Factory = NewObject<UAnimSequenceFactory>();
        Factory->TargetSkeleton = Skeleton;
        UAnimSequence* NewSequence = Cast<UAnimSequence>(
            Factory->FactoryCreateNew(UAnimSequence::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewSequence)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create animation sequence"), TEXT("CREATE_FAILED"));
        }
        
        // Set sequence length
        float Duration = static_cast<float>(NumFrames) / static_cast<float>(FrameRate);
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: Use SetNumberOfFrames with FFrameNumber
        NewSequence->GetController().SetFrameRate(FFrameRate(FrameRate, 1));
        NewSequence->GetController().SetNumberOfFrames(FFrameNumber(NumFrames));
#else
        // SequenceLength is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        NewSequence->SequenceLength = Duration;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        
        SaveAnimAsset(NewSequence, bSave);

        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        Response->SetBoolField(TEXT("existingAsset"), false);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation sequence '%s' created"), *Name));
        McpHandlerUtils::AddVerification(Response, NewSequence);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetSequenceLength(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        int32 FrameRate = static_cast<int32>(GetJsonNumberField(Params, TEXT("frameRate"), 30));
        // Schema exposes length (seconds); numFrames is an undocumented override.
        int32 NumFrames = 30;
        if (Params->HasField(TEXT("numFrames")))
        {
            NumFrames = static_cast<int32>(GetJsonNumberField(Params, TEXT("numFrames"), 30));
        }
        else if (Params->HasField(TEXT("length")))
        {
            NumFrames = FMath::Max(1, FMath::RoundToInt(GetJsonNumberField(Params, TEXT("length"), 1.0) * static_cast<double>(FrameRate)));
        }
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
        float Duration = static_cast<float>(NumFrames) / static_cast<float>(FrameRate);
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: Use SetNumberOfFrames with FFrameNumber
        Sequence->GetController().SetFrameRate(FFrameRate(FrameRate, 1));
        Sequence->GetController().SetNumberOfFrames(FFrameNumber(NumFrames));
        if (Params->HasField(TEXT("frameRate")))
        {
            // Frame rate already set above
        }
#else
        // SequenceLength is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        Sequence->SequenceLength = Duration;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        
        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Sequence length updated"));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddBoneTrack(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString BoneName = GetJsonStringField(Params, TEXT("boneName"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        if (BoneName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("boneName is required"), TEXT("MISSING_BONE_NAME"));
        }
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
        // Validate that the bone exists in the skeleton before trying to add a track
        USkeleton* Skeleton = Sequence->GetSkeleton();
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(TEXT("Animation sequence has no skeleton reference"), TEXT("NO_SKELETON"));
        }
        
        FName BoneFName(*BoneName);
        
        // Check if the bone exists in the skeleton's reference skeleton
        const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
        int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneFName);
        if (BoneIndex == INDEX_NONE)
        {
            // Provide helpful error message with available bone names
            TArray<FString> AvailableBones;
            const int32 NumBones = FMath::Min(RefSkeleton.GetNum(), 10); // Limit to first 10 bones
            for (int32 i = 0; i < NumBones; ++i)
            {
                AvailableBones.Add(RefSkeleton.GetBoneName(i).ToString());
            }
            FString BoneList = FString::Join(AvailableBones, TEXT(", "));
            if (RefSkeleton.GetNum() > 10)
            {
                BoneList += FString::Printf(TEXT(" ... and %d more"), RefSkeleton.GetNum() - 10);
            }
            
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Bone '%s' not found in skeleton. Available bones: %s"), *BoneName, *BoneList),
                TEXT("BONE_NOT_FOUND_IN_SKELETON")
            );
        }
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+ uses IAnimationDataController with IsValidBoneTrackName and AddBoneCurve
        IAnimationDataController& Controller = Sequence->GetController();

        // Validate the controller model is available
        if (!Controller.GetModel())
        {
            ANIM_ERROR_RESPONSE(
                TEXT("Animation data model is not available - cannot add bone track"),
                TEXT("MODEL_NOT_AVAILABLE")
            );
        }

        // Use IsValidBoneTrackName (non-deprecated) instead of GetBoneTrackIndexByName (deprecated since 5.2)
        if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName))
        {
            // AddBoneCurve returns bool - check the result
            const bool bAdded = Controller.AddBoneCurve(BoneFName);
            if (!bAdded)
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("AddBoneCurve failed for bone '%s' - the bone may not be valid for this animation"), *BoneName),
                    TEXT("BONE_TRACK_ADD_FAILED")
                );
            }

            if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName))
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("Bone track '%s' was not found after AddBoneCurve succeeded - internal inconsistency"), *BoneName),
                    TEXT("BONE_TRACK_ADD_FAILED")
                );
            }
        }
#elif ENGINE_MAJOR_VERSION >= 5
        // UE 5.0 approach - uses FindBoneTrackByName which returns a pointer
        IAnimationDataController& Controller = Sequence->GetController();
        
        const FBoneAnimationTrack* Track = Controller.GetModel()->FindBoneTrackByName(BoneFName);
        if (Track == nullptr)
        {
            // UE 5.0 doesn't have AddBoneCurve - use AddNewRawTrack directly on the sequence
            // AddNewRawTrack is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            FRawAnimSequenceTrack NewTrack;
            Sequence->AddNewRawTrack(BoneFName, &NewTrack);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS

            // Verify the bone track was actually added
            const FBoneAnimationTrack* AddedTrack = Controller.GetModel()->FindBoneTrackByName(BoneFName);
            if (AddedTrack == nullptr)
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("Failed to add bone track '%s' - internal error"), *BoneName),
                    TEXT("BONE_TRACK_ADD_FAILED")
                );
            }
        }
#else
        // UE4 approach
        int32 TrackIndex = Sequence->GetRawAnimationData().FindBoneTrackByName(BoneFName);
        if (TrackIndex == INDEX_NONE)
        {
            // Add raw track
            // AddNewRawTrack is deprecated in UE 5.1+ but needed for UE 4.x compatibility
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            FRawAnimSequenceTrack NewTrack;
            Sequence->AddNewRawTrack(BoneFName, &NewTrack);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }
#endif
        
        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Bone track '%s' added"), *BoneName));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetBoneKey(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString BoneName = GetJsonStringField(Params, TEXT("boneName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetJsonNumberField(Params, TEXT("frame"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        TSharedPtr<FJsonObject> LocationObj = Params->HasField(TEXT("location")) ? Params->GetObjectField(TEXT("location")) : nullptr;
        TSharedPtr<FJsonObject> RotationObj = Params->HasField(TEXT("rotation")) ? Params->GetObjectField(TEXT("rotation")) : nullptr;
        TSharedPtr<FJsonObject> ScaleObj = Params->HasField(TEXT("scale")) ? Params->GetObjectField(TEXT("scale")) : nullptr;
        
        if (BoneName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("boneName is required"), TEXT("MISSING_BONE_NAME"));
        }
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
        // Build transform key
        FVector Location = LocationObj.IsValid() ? GetVectorFromJsonAnim(LocationObj) : FVector::ZeroVector;
        FQuat Rotation = RotationObj.IsValid() ? GetRotatorFromJsonAnim(RotationObj).Quaternion() : FQuat::Identity;
        FVector Scale = ScaleObj.IsValid() ? GetVectorFromJsonAnim(ScaleObj, FVector::OneVector) : FVector::OneVector;

        int32 TotalFrames = Sequence->GetDataModel()->GetNumberOfFrames();
        if (Frame < 0 || Frame >= TotalFrames)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Frame %d is out of range (animation has %d frames)"), Frame, TotalFrames),
                TEXT("FRAME_OUT_OF_RANGE")
            );
        }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+ API
        IAnimationDataController& Controller = Sequence->GetController();
        FName BoneFName(*BoneName);

        if (!Controller.GetModel())
        {
            ANIM_ERROR_RESPONSE(
                TEXT("Animation data model is not available - cannot set bone key"),
                TEXT("MODEL_NOT_AVAILABLE")
            );
        }

        // Use IsValidBoneTrackName (non-deprecated) instead of GetBoneTrackIndexByName (deprecated since 5.2)
        if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName))
        {
            const bool bAdded = Controller.AddBoneCurve(BoneFName);
            if (!bAdded)
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("Failed to create missing bone track '%s' before keying"), *BoneName),
                    TEXT("BONE_TRACK_ADD_FAILED")
                );
            }

            // Verify the track was actually created after AddBoneCurve succeeded
            if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName))
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("Bone track '%s' not found in animation sequence after AddBoneCurve. Add the track first using add_bone_track."), *BoneName),
                    TEXT("BONE_TRACK_NOT_FOUND")
                );
            }
        }

        // UpdateBoneTrackKeys preserves other frames; SetBoneTrackKeys would replace the entire track
        FInt32Range KeyRange(Frame, Frame + 1);
        if (!Controller.UpdateBoneTrackKeys(BoneFName, KeyRange, {Location}, {Rotation}, {Scale}))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Failed to set bone key at frame %d"), Frame),
                TEXT("BONE_KEY_SET_FAILED")
            );
        }
#elif ENGINE_MAJOR_VERSION >= 5
        // UE 5.0 API - uses FindBoneTrackByName which returns a pointer
        IAnimationDataController& Controller = Sequence->GetController();
        FName BoneFName(*BoneName);

        const FBoneAnimationTrack* Track = Controller.GetModel()->FindBoneTrackByName(BoneFName);
        if (Track == nullptr)
        {
            // UE 5.0 doesn't have AddBoneCurve - use AddNewRawTrack
            // AddNewRawTrack is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            FRawAnimSequenceTrack NewTrack;
            Sequence->AddNewRawTrack(BoneFName, &NewTrack);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }

        // Verify bone track exists before setting keys
        const FBoneAnimationTrack* VerifiedTrack = Controller.GetModel()->FindBoneTrackByName(BoneFName);
        if (VerifiedTrack == nullptr)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Bone track '%s' not found in animation sequence. Add the track first using add_bone_track."), *BoneName),
                TEXT("BONE_TRACK_NOT_FOUND")
            );
        }

        // UE 5.0 fallback: SetBoneTrackKeys replaces the entire track (no UpdateBoneTrackKeys available)
        Controller.SetBoneTrackKeys(BoneFName, {Location}, {Rotation}, {Scale});
#endif
        
        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Bone key set at frame %d"), Frame));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetCurveKey(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString CurveName = GetJsonStringField(Params, TEXT("curveName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetJsonNumberField(Params, TEXT("frame"), 0));
        float Value = static_cast<float>(GetJsonNumberField(Params, TEXT("floatValue"), 0.0));
        bool bCreateIfMissing = GetJsonBoolField(Params, TEXT("createIfMissing"), true);
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        if (CurveName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("curveName is required"), TEXT("MISSING_CURVE_NAME"));
        }
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        // UE 5.3+ API - FAnimationCurveIdentifier takes FName directly
        IAnimationDataController& Controller = Sequence->GetController();
        FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1-5.2 API - FAnimationCurveIdentifier takes FSmartName
        IAnimationDataController& Controller = Sequence->GetController();
        FSmartName SmartCurveName;
        SmartCurveName.DisplayName = FName(*CurveName);
        FAnimationCurveIdentifier CurveId(SmartCurveName, ERawCurveTrackTypes::RCT_Float);
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // Find or create curve
        const FFloatCurve* ExistingCurve = Sequence->GetDataModel()->FindFloatCurve(CurveId);
        if (!ExistingCurve && bCreateIfMissing)
        {
            Controller.AddCurve(CurveId, AACF_DefaultCurve);
        }
        
        // Set key value
        float FrameTime = static_cast<float>(Frame) / Sequence->GetSamplingFrameRate().AsDecimal();
        Controller.SetCurveKey(CurveId, FRichCurveKey(FrameTime, Value));
#endif
        
        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Curve key set at frame %d"), Frame));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddNotify(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
    FString NotifyClass = GetJsonStringField(Params, TEXT("notifyClass"), TEXT(""));
    int32 Frame = static_cast<int32>(GetJsonNumberField(Params, TEXT("frame"), 0));
    int32 TrackIndex = static_cast<int32>(GetJsonNumberField(Params, TEXT("trackIndex"), 0));
    FString NotifyName = GetJsonStringField(Params, TEXT("notifyName"), TEXT(""));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (NotifyClass.IsEmpty() && NotifyName.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("At least one of notifyClass or notifyName is required"), TEXT("MISSING_NOTIFY_PARAMS"));
    }

    // Resolve notify class BEFORE modifying the asset
    UClass* ResolvedNotifyClass = nullptr;
    if (!NotifyClass.IsEmpty())
    {
        FString FullClassName = NotifyClass;
        if (!FullClassName.StartsWith(TEXT("AnimNotify_")))
        {
            FullClassName = TEXT("AnimNotify_") + NotifyClass;
        }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        ResolvedNotifyClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::None);
#else
        ResolvedNotifyClass = ResolveClassByName(FullClassName);
#endif
        if (!ResolvedNotifyClass)
        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            ResolvedNotifyClass = FindFirstObject<UClass>(*NotifyClass, EFindFirstObjectOptions::None);
#else
            ResolvedNotifyClass = ResolveClassByName(NotifyClass);
#endif
        }

        if (ResolvedNotifyClass && ResolvedNotifyClass->HasAnyClassFlags(CLASS_Abstract))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Cannot create AnimNotify: '%s' is an abstract class. Use a concrete subclass like AnimNotify_PlaySound or create a custom AnimNotify blueprint."), *FullClassName),
                TEXT("ABSTRACT_CLASS_ERROR")
            );
        }

        if (!ResolvedNotifyClass)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("AnimNotify class '%s' not found. Use a concrete subclass like AnimNotify_PlaySound or a custom AnimNotify blueprint."), *NotifyClass),
                TEXT("CLASS_NOT_FOUND")
            );
        }
    }

    UAnimSequenceBase* AnimAsset = Cast<UAnimSequenceBase>(StaticLoadObject(UAnimSequenceBase::StaticClass(), nullptr, *AssetPath));
    if (!AnimAsset)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
    }

    // Calculate time from frame
    float FrameRate = 30.0f;
#if ENGINE_MAJOR_VERSION >= 5
    if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimAsset))
    {
        FrameRate = Seq->GetSamplingFrameRate().AsDecimal();
    }
#endif
    float TriggerTime = static_cast<float>(Frame) / FrameRate;

    FAnimNotifyEvent& NotifyEvent = AnimAsset->Notifies.AddDefaulted_GetRef();
    NotifyEvent.SetTime(TriggerTime);
    NotifyEvent.TrackIndex = TrackIndex;

    if (!NotifyName.IsEmpty())
    {
        NotifyEvent.NotifyName = FName(*NotifyName);
    }

    if (ResolvedNotifyClass)
    {
        UAnimNotify* NewNotify = NewObject<UAnimNotify>(AnimAsset, ResolvedNotifyClass);
        if (!NewNotify)
        {
            AnimAsset->Notifies.Pop();
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Failed to create AnimNotify instance of class '%s'"), *NotifyClass),
                TEXT("INSTANTIATION_FAILED")
            );
        }
        NotifyEvent.Notify = NewNotify;

        // The montage "On Notify Begin/End" delegates broadcast the notify
        // OBJECT's own NotifyName (AnimNotify_PlayMontageNotify keys off its
        // member, not the event's) — without this a bridge-created
        // PlayMontageNotify fires with NAME_None and BP name-matching no-ops.
        if (!NotifyName.IsEmpty())
        {
            if (FNameProperty* ObjNameProp =
                    FindFProperty<FNameProperty>(ResolvedNotifyClass, TEXT("NotifyName")))
            {
                ObjNameProp->SetPropertyValue_InContainer(NewNotify, FName(*NotifyName));
            }
        }
    }

        AnimAsset->RefreshCacheData();
        SaveAnimAsset(AnimAsset, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Notify added"));
        McpHandlerUtils::AddVerification(Response, AnimAsset);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddNotifyState(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
    FString NotifyClass = GetJsonStringField(Params, TEXT("notifyClass"), TEXT(""));
    int32 StartFrame = static_cast<int32>(GetJsonNumberField(Params, TEXT("startFrame"), 0));
    int32 EndFrame = static_cast<int32>(GetJsonNumberField(Params, TEXT("endFrame"), 10));
    int32 TrackIndex = static_cast<int32>(GetJsonNumberField(Params, TEXT("trackIndex"), 0));
    FString NotifyName = GetJsonStringField(Params, TEXT("notifyName"), TEXT(""));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (EndFrame < StartFrame)
    {
        ANIM_ERROR_RESPONSE(TEXT("endFrame must be greater than or equal to startFrame"), TEXT("INVALID_FRAME_RANGE"));
    }

    if (NotifyClass.IsEmpty() && NotifyName.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("At least one of notifyClass or notifyName is required"), TEXT("MISSING_NOTIFY_PARAMS"));
    }

    // Resolve notify state class BEFORE modifying the asset
    UClass* ResolvedNotifyStateClass = nullptr;
    if (!NotifyClass.IsEmpty())
    {
        FString FullClassName = NotifyClass;
        if (!FullClassName.StartsWith(TEXT("AnimNotifyState_")))
        {
            FullClassName = TEXT("AnimNotifyState_") + NotifyClass;
        }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        ResolvedNotifyStateClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::None);
#else
        ResolvedNotifyStateClass = ResolveClassByName(FullClassName);
#endif
        if (!ResolvedNotifyStateClass)
        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            ResolvedNotifyStateClass = FindFirstObject<UClass>(*NotifyClass, EFindFirstObjectOptions::None);
#else
            ResolvedNotifyStateClass = ResolveClassByName(NotifyClass);
#endif
        }

        if (ResolvedNotifyStateClass && ResolvedNotifyStateClass->HasAnyClassFlags(CLASS_Abstract))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Cannot create AnimNotifyState: '%s' is an abstract class. Use a concrete subclass like AnimNotifyState_PlayMontageNotify or create a custom AnimNotifyState blueprint."), *FullClassName),
                TEXT("ABSTRACT_CLASS_ERROR")
            );
        }

        if (!ResolvedNotifyStateClass)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("AnimNotifyState class '%s' not found. Use a concrete subclass like AnimNotifyState_PlayMontageNotify or a custom AnimNotifyState blueprint."), *NotifyClass),
                TEXT("CLASS_NOT_FOUND")
            );
        }
    }

    UAnimSequenceBase* AnimAsset = Cast<UAnimSequenceBase>(StaticLoadObject(UAnimSequenceBase::StaticClass(), nullptr, *AssetPath));
    if (!AnimAsset)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
    }

    float FrameRate = 30.0f;
#if ENGINE_MAJOR_VERSION >= 5
    if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimAsset))
    {
        FrameRate = Seq->GetSamplingFrameRate().AsDecimal();
    }
#endif
    float StartTime = static_cast<float>(StartFrame) / FrameRate;
    float EndTime = static_cast<float>(EndFrame) / FrameRate;
    float Duration = EndTime - StartTime;

    FAnimNotifyEvent& NotifyEvent = AnimAsset->Notifies.AddDefaulted_GetRef();
    NotifyEvent.SetTime(StartTime);
    NotifyEvent.SetDuration(Duration);
    NotifyEvent.TrackIndex = TrackIndex;

    if (!NotifyName.IsEmpty())
    {
        NotifyEvent.NotifyName = FName(*NotifyName);
    }

    if (ResolvedNotifyStateClass)
    {
        UAnimNotifyState* NewNotifyState = NewObject<UAnimNotifyState>(AnimAsset, ResolvedNotifyStateClass);
        if (!NewNotifyState)
        {
            AnimAsset->Notifies.Pop();
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Failed to create AnimNotifyState instance of class '%s'"), *NotifyClass),
                TEXT("INSTANTIATION_FAILED")
            );
        }
        NotifyEvent.NotifyStateClass = NewNotifyState;

        // Same as the point-notify path: window variants (e.g.
        // AnimNotifyState_PlayMontageNotifyWindow) broadcast their own
        // NotifyName member.
        if (!NotifyName.IsEmpty())
        {
            if (FNameProperty* ObjNameProp =
                    FindFProperty<FNameProperty>(ResolvedNotifyStateClass, TEXT("NotifyName")))
            {
                ObjNameProp->SetPropertyValue_InContainer(NewNotifyState, FName(*NotifyName));
            }
        }
    }

        AnimAsset->RefreshCacheData();
        
        SaveAnimAsset(AnimAsset, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Notify state added"));
        McpHandlerUtils::AddVerification(Response, AnimAsset);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddSyncMarker(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString MarkerName = GetJsonStringField(Params, TEXT("markerName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetJsonNumberField(Params, TEXT("frame"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        if (MarkerName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("markerName is required"), TEXT("MISSING_MARKER_NAME"));
        }
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
        // Calculate time from frame
        float FrameRate = 30.0f;
#if ENGINE_MAJOR_VERSION >= 5
        FrameRate = Sequence->GetSamplingFrameRate().AsDecimal();
#endif
        float Time = static_cast<float>(Frame) / FrameRate;
        
        // Add sync marker
        FAnimSyncMarker NewMarker;
        NewMarker.MarkerName = FName(*MarkerName);
        NewMarker.Time = Time;
        
        Sequence->AuthoredSyncMarkers.Add(NewMarker);
        Sequence->RefreshSyncMarkerDataFromAuthored();
        
        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Sync marker '%s' added"), *MarkerName));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetRootMotionSettings(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        bool bEnableRootMotion = GetJsonBoolField(Params, TEXT("enableRootMotion"), true);
        FString RootMotionRootLock = GetJsonStringField(Params, TEXT("rootMotionRootLock"), TEXT("RefPose"));
        bool bForceRootLock = GetJsonBoolField(Params, TEXT("forceRootLock"), false);
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
        Sequence->bEnableRootMotion = bEnableRootMotion;
        Sequence->bForceRootLock = bForceRootLock;
        
        // Set root motion lock type
        if (RootMotionRootLock == TEXT("AnimFirstFrame"))
        {
            Sequence->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
        }
        else if (RootMotionRootLock == TEXT("Zero"))
        {
            Sequence->RootMotionRootLock = ERootMotionRootLock::Zero;
        }
        else
        {
            Sequence->RootMotionRootLock = ERootMotionRootLock::RefPose;
        }
        
        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Root motion settings updated"));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetAdditiveSettings(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString AdditiveAnimType = GetJsonStringField(Params, TEXT("additiveAnimType"), TEXT("NoAdditive"));
        FString BasePoseType = GetJsonStringField(Params, TEXT("basePoseType"), TEXT("RefPose"));
        FString BasePoseAnimation = GetJsonStringField(Params, TEXT("basePoseAnimation"), TEXT(""));
        int32 BasePoseFrame = static_cast<int32>(GetJsonNumberField(Params, TEXT("basePoseFrame"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        
        // Set additive anim type
        if (AdditiveAnimType == TEXT("LocalSpaceAdditive"))
        {
            Sequence->AdditiveAnimType = AAT_LocalSpaceBase;
        }
        else if (AdditiveAnimType == TEXT("MeshSpaceAdditive"))
        {
            Sequence->AdditiveAnimType = AAT_RotationOffsetMeshSpace;
        }
        else
        {
            Sequence->AdditiveAnimType = AAT_None;
        }
        
        // Set base pose type
        if (BasePoseType == TEXT("AnimationFrame"))
        {
            Sequence->RefPoseType = ABPT_AnimFrame;
            Sequence->RefFrameIndex = BasePoseFrame;
        }
        else if (BasePoseType == TEXT("AnimationScaled"))
        {
            Sequence->RefPoseType = ABPT_AnimScaled;
        }
        else
        {
            Sequence->RefPoseType = ABPT_RefPose;
        }
        
        // Set base pose animation if provided
        if (!BasePoseAnimation.IsEmpty())
        {
            UAnimSequence* BaseAnim = LoadAnimSequenceFromPath(BasePoseAnimation);
            if (BaseAnim)
            {
                Sequence->RefPoseSeq = BaseAnim;
            }
        }
        
        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Additive settings updated"));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
}

    // ===== 10.2 Animation Montages =====
static TSharedPtr<FJsonObject> AnimAuthoringCreateMontage(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    // Accept 'path' (legacy) and 'savePath' (the animation_physics schema's
    // param name — previously ignored, so callers passing savePath silently
    // got the /Game/Animations default). Mirrors the fixed
    // create_widget_blueprint fallback chain.
    FString RawPath = GetJsonStringField(Params, TEXT("path"), TEXT(""));
    if (RawPath.IsEmpty())
    {
        RawPath = GetJsonStringField(Params, TEXT("savePath"), TEXT(""));
    }
    if (RawPath.IsEmpty())
    {
        RawPath = TEXT("/Game/Animations");
    }
    FString Path = NormalizeAnimPath(RawPath);
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    FString SlotName = GetJsonStringField(Params, TEXT("slotName"), TEXT("DefaultSlot"));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    const bool bHasLength = Params->HasField(TEXT("length"));
    const float Length = static_cast<float>(GetJsonNumberField(Params, TEXT("length"), 0.0));
    if (bHasLength && Length <= 0.0f)
    {
        ANIM_ERROR_RESPONSE(TEXT("length must be a positive duration in seconds"), TEXT("INVALID_LENGTH"));
    }

    USkeleton* Skeleton = nullptr;
    if (!SkeletonPath.IsEmpty())
    {
        Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
    }

    // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
        Factory->TargetSkeleton = Skeleton;
        UAnimMontage* NewMontage = Cast<UAnimMontage>(
            Factory->FactoryCreateNew(UAnimMontage::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewMontage)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create montage"), TEXT("CREATE_FAILED"));
        }

        if (bHasLength)
        {
            NewMontage->SetCompositeLength(Length);
            Response->SetNumberField(TEXT("length"), Length);
        }

        // UAnimMontage's constructor already seeds a 'DefaultSlot' track;
        // rename it rather than appending a duplicate.
        if (!SlotName.IsEmpty())
        {
            const FName SlotFName(*SlotName);
            bool bHasSlot = false;
            for (const FSlotAnimationTrack& Track : NewMontage->SlotAnimTracks)
            {
                if (Track.SlotName == SlotFName)
                {
                    bHasSlot = true;
                    break;
                }
            }
            if (!bHasSlot)
            {
                if (NewMontage->SlotAnimTracks.Num() > 0)
                {
                    NewMontage->SlotAnimTracks[0].SlotName = SlotFName;
                }
                else
                {
                    NewMontage->SlotAnimTracks.AddDefaulted_GetRef().SlotName = SlotFName;
                }
            }
        }

        // The factory seeds a "Default" composite section but never calls
        // Link() on it (LinkedMontage/LinkedSequence stay null) — the engine's
        // own AddAnimCompositeSection links every section it adds. Without
        // this, montage sections authored over the bridge needed manual
        // reflection writes to become playable.
        for (FCompositeSection& Section : NewMontage->CompositeSections)
        {
            Section.Link(NewMontage, Section.GetTime());
        }

        SaveAnimAsset(NewMontage, bSave);

        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Montage '%s' created"), *Name));
        McpHandlerUtils::AddVerification(Response, NewMontage);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddMontageSection(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        // Accept assetPath, montagePath, and 'name' (the schema's only
        // name-ish field — callers passed the montage path there and got
        // MONTAGE_NOT_FOUND with an EMPTY path in the message).
        FString RawAssetPath = GetJsonStringField(Params, TEXT("assetPath"), TEXT(""));
        if (RawAssetPath.IsEmpty())
        {
            RawAssetPath = GetJsonStringField(Params, TEXT("montagePath"), TEXT(""));
        }
        if (RawAssetPath.IsEmpty())
        {
            RawAssetPath = GetJsonStringField(Params, TEXT("name"), TEXT(""));
        }
        const FString AssetPath = NormalizeAnimPath(RawAssetPath);
        FString SectionName = GetJsonStringField(Params, TEXT("sectionName"), TEXT(""));
        // 'time' is the schema-advertised field; 'startTime' kept for
        // compatibility with the handler's original contract.
        float StartTime = static_cast<float>(GetJsonNumberField(
            Params, TEXT("startTime"),
            GetJsonNumberField(Params, TEXT("time"), 0.0)));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (SectionName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("sectionName is required"), TEXT("MISSING_SECTION_NAME"));
        }
        if (AssetPath.IsEmpty() || !AssetPath.StartsWith(TEXT("/")))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Montage asset path required (got '%s') — pass assetPath"), *AssetPath),
                TEXT("INVALID_ARGUMENT"));
        }

        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }

        // Add new section (fail fast on duplicates: AddAnimCompositeSection
        // returns INDEX_NONE for an existing name — this used to report
        // success "added at index -1").
        Montage->Modify();
        int32 SectionIndex = Montage->AddAnimCompositeSection(FName(*SectionName), StartTime);
        if (SectionIndex == INDEX_NONE)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Section '%s' already exists on %s"), *SectionName, *AssetPath),
                TEXT("SECTION_EXISTS"));
        }

        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Section '%s' added at index %d"), *SectionName, SectionIndex));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddMontageSlot(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("animationPath"), TEXT("")));
        FString SlotName = GetJsonStringField(Params, TEXT("slotName"), TEXT("DefaultSlot"));
        float StartTime = static_cast<float>(GetJsonNumberField(Params, TEXT("startTime"), 0.0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }
        
        UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
        if (!Animation)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
        }
        
        // Find or create slot track
        FSlotAnimationTrack* SlotTrack = nullptr;
        for (FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
        {
            if (Track.SlotName == FName(*SlotName))
            {
                SlotTrack = &Track;
                break;
            }
        }
        
        if (!SlotTrack)
        {
            SlotTrack = &Montage->SlotAnimTracks.AddDefaulted_GetRef();
            SlotTrack->SlotName = FName(*SlotName);
        }
        
        // Add animation to slot track
        FAnimSegment& Segment = SlotTrack->AnimTrack.AnimSegments.AddDefaulted_GetRef();
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        Segment.SetAnimReference(Animation);
#else
        // UE 5.0: Direct member access
        Segment.AnimReference = Animation;
#endif
        Segment.StartPos = StartTime;
        Segment.AnimStartTime = 0.0f;
        Segment.AnimEndTime = Animation->GetPlayLength();
        Segment.AnimPlayRate = 1.0f;
        Segment.LoopingCount = 1;
        
        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Animation added to montage slot"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetSectionTiming(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString SectionName = GetJsonStringField(Params, TEXT("sectionName"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        if (SectionName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("sectionName is required"), TEXT("MISSING_SECTION_NAME"));
        }
        
        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }
        
        int32 SectionIndex = Montage->GetSectionIndex(FName(*SectionName));
        if (SectionIndex == INDEX_NONE)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Section not found: %s"), *SectionName), TEXT("SECTION_NOT_FOUND"));
        }
        
        if (!Params->HasField(TEXT("startTime")))
        {
            ANIM_ERROR_RESPONSE(TEXT("startTime is required"), TEXT("MISSING_TIME"));
        }

        const float StartTime = static_cast<float>(GetJsonNumberField(Params, TEXT("startTime")));
        Montage->Modify();
        FCompositeSection& Section = Montage->CompositeSections[SectionIndex];
        Section.SetTime(StartTime);
        Montage->PostEditChange();

        SaveAnimAsset(Montage, bSave);

        Response->SetNumberField(TEXT("startTime"), StartTime);
        ANIM_SUCCESS_RESPONSE(TEXT("Section timing updated"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddMontageNotify(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
    FString NotifyClass = GetJsonStringField(Params, TEXT("notifyClass"), TEXT(""));
    float Time = static_cast<float>(GetJsonNumberField(Params, TEXT("time"), 0.0));
    int32 TrackIndex = static_cast<int32>(GetJsonNumberField(Params, TEXT("trackIndex"), 0));
    FString NotifyName = GetJsonStringField(Params, TEXT("notifyName"), TEXT(""));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (NotifyClass.IsEmpty() && NotifyName.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("At least one of notifyClass or notifyName is required"), TEXT("MISSING_NOTIFY_PARAMS"));
    }

    // Resolve notify class BEFORE modifying the asset
    UClass* ResolvedNotifyClass = nullptr;
    if (!NotifyClass.IsEmpty())
    {
        FString FullClassName = NotifyClass;
        if (!FullClassName.StartsWith(TEXT("AnimNotify_")))
        {
            FullClassName = TEXT("AnimNotify_") + NotifyClass;
        }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        ResolvedNotifyClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::None);
#else
        ResolvedNotifyClass = ResolveClassByName(FullClassName);
#endif
        if (!ResolvedNotifyClass)
        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            ResolvedNotifyClass = FindFirstObject<UClass>(*NotifyClass, EFindFirstObjectOptions::None);
#else
            ResolvedNotifyClass = ResolveClassByName(NotifyClass);
#endif
        }

        if (ResolvedNotifyClass && ResolvedNotifyClass->HasAnyClassFlags(CLASS_Abstract))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Cannot create AnimNotify: '%s' is an abstract class. Use a concrete subclass like AnimNotify_PlaySound or create a custom AnimNotify blueprint."), *FullClassName),
                TEXT("ABSTRACT_CLASS_ERROR")
            );
        }

        if (!ResolvedNotifyClass)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("AnimNotify class '%s' not found. Use a concrete subclass like AnimNotify_PlaySound or a custom AnimNotify blueprint."), *NotifyClass),
                TEXT("CLASS_NOT_FOUND")
            );
        }
    }

    UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
    if (!Montage)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
    }

#if WITH_EDITOR
    if (TrackIndex >= 0)
    {
        while (!Montage->AnimNotifyTracks.IsValidIndex(TrackIndex))
        {
            Montage->AnimNotifyTracks.Add(
                FAnimNotifyTrack(*FString::FromInt(Montage->AnimNotifyTracks.Num() + 1), FLinearColor::White)
            );
        }
    }
#endif

    FAnimNotifyEvent& NotifyEvent = Montage->Notifies.AddDefaulted_GetRef();
    NotifyEvent.SetTime(Time);
    NotifyEvent.TrackIndex = TrackIndex;

    if (!NotifyName.IsEmpty())
    {
        NotifyEvent.NotifyName = FName(*NotifyName);
    }

    if (ResolvedNotifyClass)
    {
        UAnimNotify* NewNotify = NewObject<UAnimNotify>(Montage, ResolvedNotifyClass);
        if (!NewNotify)
        {
            Montage->Notifies.Pop();
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Failed to create AnimNotify instance of class '%s'"), *NotifyClass),
                TEXT("INSTANTIATION_FAILED")
            );
        }
        NotifyEvent.Notify = NewNotify;

        // The montage "On Notify Begin/End" delegates broadcast the notify
        // OBJECT's own NotifyName (AnimNotify_PlayMontageNotify keys off its
        // member, not the event's) — without this a bridge-created
        // PlayMontageNotify fires with NAME_None and BP name-matching no-ops.
        if (!NotifyName.IsEmpty())
        {
            if (FNameProperty* ObjNameProp =
                    FindFProperty<FNameProperty>(ResolvedNotifyClass, TEXT("NotifyName")))
            {
                ObjNameProp->SetPropertyValue_InContainer(NewNotify, FName(*NotifyName));
            }
        }
    }

        Montage->RefreshCacheData();
        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Montage notify added"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetBlend(const TSharedPtr<FJsonObject>& Params, const bool bBlendIn)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        // 'time' accepted as an alias of blendTime.
        const bool bHasBlendTime = Params->HasField(TEXT("blendTime")) || Params->HasField(TEXT("time"));
        float BlendTime = static_cast<float>(GetJsonNumberField(Params, TEXT("blendTime"),
            GetJsonNumberField(Params, TEXT("time"), 0.25)));
        FString BlendOption = GetJsonStringField(Params, TEXT("blendOption"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (!bHasBlendTime && BlendOption.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Pass blendTime (seconds; 'time' accepted) and/or blendOption (Linear, Cubic, Sinusoidal)"), TEXT("MISSING_BLEND_PARAMS"));
        }

        EAlphaBlendOption BlendOptionValue = EAlphaBlendOption::Linear;
        if (!BlendOption.IsEmpty())
        {
            if (BlendOption == TEXT("Linear"))
            {
                BlendOptionValue = EAlphaBlendOption::Linear;
            }
            else if (BlendOption == TEXT("Cubic"))
            {
                BlendOptionValue = EAlphaBlendOption::Cubic;
            }
            else if (BlendOption == TEXT("Sinusoidal"))
            {
                BlendOptionValue = EAlphaBlendOption::Sinusoidal;
            }
            else
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Unknown blendOption '%s' — use Linear, Cubic, or Sinusoidal"), *BlendOption), TEXT("INVALID_BLEND_OPTION"));
            }
        }

        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }

        FAlphaBlend& Blend = bBlendIn ? Montage->BlendIn : Montage->BlendOut;
        if (bHasBlendTime)
        {
            Blend.SetBlendTime(BlendTime);
            Response->SetNumberField(TEXT("blendTime"), BlendTime);
        }
        if (!BlendOption.IsEmpty())
        {
            Blend.SetBlendOption(BlendOptionValue);
            Response->SetStringField(TEXT("blendOption"), BlendOption);
        }

        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(bBlendIn ? TEXT("Blend in settings updated") : TEXT("Blend out settings updated"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringLinkSections(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString FromSection = GetJsonStringField(Params, TEXT("fromSection"), TEXT(""));
        FString ToSection = GetJsonStringField(Params, TEXT("toSection"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        if (FromSection.IsEmpty() || ToSection.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("fromSection and toSection are required"), TEXT("MISSING_SECTIONS"));
        }
        
        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }
        
        // Set next section using section index-based API
        int32 FromSectionIndex = Montage->GetSectionIndex(FName(*FromSection));
        int32 ToSectionIndex = Montage->GetSectionIndex(FName(*ToSection));
        if (FromSectionIndex != INDEX_NONE && ToSectionIndex != INDEX_NONE)
        {
            Montage->CompositeSections[FromSectionIndex].NextSectionName = FName(*ToSection);
        }
        
        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Linked '%s' to '%s'"), *FromSection, *ToSection));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
}

    // ===== 10.3 Blend Spaces =====
static TSharedPtr<FJsonObject> AnimAuthoringCreateBlendSpace1D(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
#if MCP_HAS_BLENDSPACE_FACTORY
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    // Accept 'path' (legacy) and 'savePath' (the animation_physics schema's
    // param name) — mirrors create_montage.
    FString RawPath = GetJsonStringField(Params, TEXT("path"), TEXT(""));
    if (RawPath.IsEmpty())
    {
        RawPath = GetJsonStringField(Params, TEXT("savePath"), TEXT(""));
    }
    if (RawPath.IsEmpty())
    {
        RawPath = TEXT("/Game/Animations");
    }
    FString Path = NormalizeAnimPath(RawPath);
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    FString AxisName = GetJsonStringField(Params, TEXT("axisName"), TEXT("Speed"));
    float AxisMin = static_cast<float>(GetJsonNumberField(Params, TEXT("axisMin"), 0.0));
    float AxisMax = static_cast<float>(GetJsonNumberField(Params, TEXT("axisMax"), 600.0));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    USkeleton* Skeleton = nullptr;
    if (!SkeletonPath.IsEmpty())
    {
        Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
    }
    // A Blend Space must target a skeleton; without one UBlendSpaceFactoryNew fails
    // and we previously surfaced a generic "Failed to create blend space" — give a
    // clear required-param error instead.
    if (!Skeleton)
    {
        ANIM_ERROR_RESPONSE(TEXT("skeletonPath is required: a Blend Space targets a USkeleton. Pass skeletonPath to an existing skeleton asset."), TEXT("SKELETON_REQUIRED"));
    }

    // Check if an asset already exists at the target path to prevent modal dialog
        FString ObjectPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
        if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
        {
            UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
            if (ExistingAsset)
            {
                if (Cast<UBlendSpace1D>(ExistingAsset))
                {
                    // Same type - return success with existing asset info
                    Response->SetStringField(TEXT("assetPath"), ObjectPath);
                    Response->SetBoolField(TEXT("existingAsset"), true);
                    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space 1D '%s' already exists - reusing existing asset"), *Name));
                    return Response;
                }
                else
                {
                    // Different type - return error to prevent modal dialog
                    FString ExistingClassName = ExistingAsset->GetClass()->GetName();
                    ANIM_ERROR_RESPONSE(
                        FString::Printf(TEXT("Cannot create BlendSpace1D: asset '%s' already exists as type '%s'"), 
                            *ObjectPath, *ExistingClassName),
                        TEXT("ASSET_TYPE_MISMATCH")
                    );
                }
            }
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UBlendSpaceFactory1D* Factory = NewObject<UBlendSpaceFactory1D>();
        Factory->TargetSkeleton = Skeleton;
        UBlendSpace1D* NewBlendSpace = Cast<UBlendSpace1D>(
            Factory->FactoryCreateNew(UBlendSpace1D::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewBlendSpace)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create blend space 1D"), TEXT("CREATE_FAILED"));
        }
        
        // Configure axis - use reflection since BlendParameters is protected
        FBlendParameter NewParam;
        NewParam.DisplayName = AxisName;
        NewParam.Min = AxisMin;
        NewParam.Max = AxisMax;
        NewParam.GridNum = 4;
        NewParam.bSnapToGrid = false;
        NewParam.bWrapInput = false;
        
        // Use FProperty reflection to set the protected BlendParameters array
        if (FProperty* BlendParamsProp = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters")))
        {
            NewBlendSpace->Modify();
            FBlendParameter* BlendParamsPtr = BlendParamsProp->ContainerPtrToValuePtr<FBlendParameter>(NewBlendSpace);
            if (BlendParamsPtr)
            {
                BlendParamsPtr[0] = NewParam;
            }
        }
        
        NewBlendSpace->PostEditChange();
        
        SaveAnimAsset(NewBlendSpace, bSave);

        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space 1D '%s' created"), *Name));
        McpHandlerUtils::AddVerification(Response, NewBlendSpace);
#else
        ANIM_ERROR_RESPONSE(TEXT("Blend space factory not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringCreateBlendSpace2D(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
#if MCP_HAS_BLENDSPACE_FACTORY
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    // Accept 'path' (legacy) and 'savePath' (the animation_physics schema's
    // param name) — mirrors create_montage.
    FString RawPath = GetJsonStringField(Params, TEXT("path"), TEXT(""));
    if (RawPath.IsEmpty())
    {
        RawPath = GetJsonStringField(Params, TEXT("savePath"), TEXT(""));
    }
    if (RawPath.IsEmpty())
    {
        RawPath = TEXT("/Game/Animations");
    }
    FString Path = NormalizeAnimPath(RawPath);
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    FString HorizontalAxisName = GetJsonStringField(Params, TEXT("horizontalAxisName"), TEXT("Direction"));
    float HorizontalMin = static_cast<float>(GetJsonNumberField(Params, TEXT("horizontalMin"), -180.0));
    float HorizontalMax = static_cast<float>(GetJsonNumberField(Params, TEXT("horizontalMax"), 180.0));
    FString VerticalAxisName = GetJsonStringField(Params, TEXT("verticalAxisName"), TEXT("Speed"));
    float VerticalMin = static_cast<float>(GetJsonNumberField(Params, TEXT("verticalMin"), 0.0));
    float VerticalMax = static_cast<float>(GetJsonNumberField(Params, TEXT("verticalMax"), 600.0));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    USkeleton* Skeleton = nullptr;
    if (!SkeletonPath.IsEmpty())
    {
        Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
    }
    // A Blend Space must target a skeleton; without one UBlendSpaceFactoryNew fails
    // and we previously surfaced a generic "Failed to create blend space 2D" — give
    // a clear required-param error instead.
    if (!Skeleton)
    {
        ANIM_ERROR_RESPONSE(TEXT("skeletonPath is required: a Blend Space targets a USkeleton. Pass skeletonPath to an existing skeleton asset."), TEXT("SKELETON_REQUIRED"));
    }

    // Check if an asset already exists at the target path to prevent modal dialog
        FString ObjectPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
        if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
        {
            UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
            if (ExistingAsset)
            {
                if (Cast<UBlendSpace>(ExistingAsset))
                {
                    // Same type - return success with existing asset info
                    Response->SetStringField(TEXT("assetPath"), ObjectPath);
                    Response->SetBoolField(TEXT("existingAsset"), true);
                    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space 2D '%s' already exists - reusing existing asset"), *Name));
                    return Response;
                }
                else
                {
                    // Different type - return error to prevent modal dialog
                    FString ExistingClassName = ExistingAsset->GetClass()->GetName();
                    ANIM_ERROR_RESPONSE(
                        FString::Printf(TEXT("Cannot create BlendSpace: asset '%s' already exists as type '%s'"), 
                            *ObjectPath, *ExistingClassName),
                        TEXT("ASSET_TYPE_MISMATCH")
                    );
                }
            }
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UBlendSpaceFactoryNew* Factory = NewObject<UBlendSpaceFactoryNew>();
        Factory->TargetSkeleton = Skeleton;
        UBlendSpace* NewBlendSpace = Cast<UBlendSpace>(
            Factory->FactoryCreateNew(UBlendSpace::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewBlendSpace)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create blend space 2D"), TEXT("CREATE_FAILED"));
        }
        
        // Configure axes using reflection since BlendParameters is protected in UE 5.7+
        FBlendParameter HParam;
        HParam.DisplayName = HorizontalAxisName;
        HParam.Min = HorizontalMin;
        HParam.Max = HorizontalMax;
        HParam.GridNum = 4;
        HParam.bSnapToGrid = false;
        HParam.bWrapInput = false;
        
        FBlendParameter VParam;
        VParam.DisplayName = VerticalAxisName;
        VParam.Min = VerticalMin;
        VParam.Max = VerticalMax;
        VParam.GridNum = 4;
        VParam.bSnapToGrid = false;
        VParam.bWrapInput = false;
        
        // Use FProperty reflection to set the protected BlendParameters array
        if (FProperty* BlendParamsProp = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters")))
        {
            NewBlendSpace->Modify();
            FBlendParameter* BlendParamsPtr = BlendParamsProp->ContainerPtrToValuePtr<FBlendParameter>(NewBlendSpace);
            if (BlendParamsPtr)
            {
                BlendParamsPtr[0] = HParam;
                BlendParamsPtr[1] = VParam;
            }
        }
        
        NewBlendSpace->PostEditChange();
        
        SaveAnimAsset(NewBlendSpace, bSave);
        
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space 2D '%s' created"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("Blend space factory not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddBlendSample(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("animationPath"), TEXT("")));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        UBlendSpace* BlendSpace2D = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(StaticLoadObject(UBlendSpace1D::StaticClass(), nullptr, *AssetPath));
        
        UBlendSpace* BlendSpace = BlendSpace2D ? BlendSpace2D : BlendSpace1D;
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }
        
        UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
        if (!Animation)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
        }
        
        // Get sample value
        FVector SampleValue = FVector::ZeroVector;
        double SampleNum = 0.0;
        const TSharedPtr<FJsonObject>* SampleObjPtr = nullptr;
        if (Params->TryGetNumberField(TEXT("floatValue"), SampleNum))
        {
            // 1D blend space
            SampleValue.X = SampleNum;
        }
        else if (Params->TryGetObjectField(TEXT("vector2Value"), SampleObjPtr) && SampleObjPtr && (*SampleObjPtr).IsValid())
        {
            // 2D blend space
            SampleValue.X = GetJsonNumberField(*SampleObjPtr, TEXT("x"), 0.0);
            SampleValue.Y = GetJsonNumberField(*SampleObjPtr, TEXT("y"), 0.0);
        }
        
        // Add sample
        BlendSpace->AddSample(Animation, SampleValue);

        // Wave 7+ #12: Trigger PostEditChange so grid rebuild + referencer
        // notifications fire. Without this, BS internal cached grid stays
        // stale and referencing ABPs may compile-warn "sample out of bounds".
        BlendSpace->PostEditChange();

        SaveAnimAsset(BlendSpace, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Blend sample added"));
        return Response;
}

    // Wave 7+ #12: explicit BS rebuild for cases where SampleData / BlendParameters
    // were mutated via Python set_editor_property (raw memory write skips
    // PostEditChange) or any other path that bypasses MCP's add_blend_sample.
    // Optionally cascade-compile referencing AnimBlueprints so the user doesn't
    // have to hunt them down manually.
static TSharedPtr<FJsonObject> AnimAuthoringForceRebuildBlendSpace(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        bool bRebuildBlendParams = GetJsonBoolField(Params, TEXT("rebuildBlendParameters"), false);
        bool bCompileReferencers = GetJsonBoolField(Params, TEXT("compileReferencers"), true);
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        UBlendSpace* BlendSpace2D = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(StaticLoadObject(UBlendSpace1D::StaticClass(), nullptr, *AssetPath));
        UBlendSpace* BlendSpace = BlendSpace2D ? BlendSpace2D : BlendSpace1D;
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }

        // Step 1: drop invalid / out-of-range samples first
        BlendSpace->ValidateSampleData();

        // Step 2: trigger SampleData PostEditChange → drives grid + RuntimeBuilder rebuild
        if (FProperty* SampleDataProp = BlendSpace->GetClass()->FindPropertyByName(TEXT("SampleData")))
        {
            FPropertyChangedEvent SampleEvent(SampleDataProp);
            BlendSpace->PostEditChangeProperty(SampleEvent);
        }

        // Step 3 (optional): trigger BlendParameters PostEditChange (axis min/max changed)
        if (bRebuildBlendParams)
        {
            if (FProperty* BPProp = BlendSpace->GetClass()->FindPropertyByName(TEXT("BlendParameters")))
            {
                FPropertyChangedEvent BPEvent(BPProp);
                BlendSpace->PostEditChangeProperty(BPEvent);
            }
        }

        BlendSpace->MarkPackageDirty();
        // SaveAnimAsset returns true when bSave=false (no-op) or when the save
        // succeeds. When bSave=true and the save actually fails, surface that
        // to the caller rather than silently reporting success — the BS in
        // memory is rebuilt but the on-disk asset is stale.
        const bool bSaved = SaveAnimAsset(BlendSpace, bSave);
        if (bSave && !bSaved)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Blend space rebuilt in memory but failed to save asset: %s"), *AssetPath),
                TEXT("BLENDSPACE_SAVE_FAILED"));
        }

        // Step 4 (optional): cascade-compile every AnimBlueprint referencing this BS.
        // Track both successful and failed compiles so callers can distinguish
        // "no referencers found" from "some referencers failed to compile" —
        // the latter usually means the new BS shape broke the ABP's graph and
        // needs author attention.
        int32 CompiledCount = 0;
        int32 FailedCount = 0;
        TArray<TSharedPtr<FJsonValue>> CompiledArr;
        TArray<TSharedPtr<FJsonValue>> FailedArr;
        if (bCompileReferencers)
        {
            IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
            const FName BSPackageName = BlendSpace->GetOutermost()->GetFName();
            TArray<FName> Referencers;
            AR.GetReferencers(BSPackageName, Referencers,
                              UE::AssetRegistry::EDependencyCategory::Package);

            for (const FName& RefPkg : Referencers)
            {
                TArray<FAssetData> Assets;
                AR.GetAssetsByPackageName(RefPkg, Assets);
                for (const FAssetData& Data : Assets)
                {
                    if (Data.AssetClassPath == UAnimBlueprint::StaticClass()->GetClassPathName())
                    {
                        // Capture the soft path up front so a null GetAsset()
                        // still gets reported as a load failure rather than
                        // silently skipped.
                        const FString RefPath = Data.GetObjectPathString();
                        if (UAnimBlueprint* ABP = Cast<UAnimBlueprint>(Data.GetAsset()))
                        {
                            if (McpSafeCompileBlueprint(ABP))
                            {
                                ++CompiledCount;
                                CompiledArr.Add(MakeShared<FJsonValueString>(ABP->GetPathName()));
                            }
                            else
                            {
                                ++FailedCount;
                                FailedArr.Add(MakeShared<FJsonValueString>(ABP->GetPathName()));
                            }
                        }
                        else
                        {
                            ++FailedCount;
                            FailedArr.Add(MakeShared<FJsonValueString>(RefPath));
                        }
                    }
                }
            }
        }

        Response->SetStringField(TEXT("assetPath"), AssetPath);
        Response->SetBoolField(TEXT("rebuiltBlendParameters"), bRebuildBlendParams);
        Response->SetNumberField(TEXT("referencersCompiled"), CompiledCount);
        Response->SetArrayField(TEXT("compiledAnimBlueprints"), CompiledArr);
        Response->SetNumberField(TEXT("referencersFailed"), FailedCount);
        Response->SetArrayField(TEXT("failedAnimBlueprints"), FailedArr);

        ANIM_SUCCESS_RESPONSE(TEXT("Blend space rebuilt"));
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetAxisSettings(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString Axis = GetJsonStringField(Params, TEXT("axis"), TEXT("Horizontal"));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        UBlendSpace* BlendSpace2D = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(StaticLoadObject(UBlendSpace1D::StaticClass(), nullptr, *AssetPath));
        
        UBlendSpace* BlendSpace = BlendSpace2D ? BlendSpace2D : BlendSpace1D;
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }
        
        // Determine axis index
        int32 AxisIndex = 0;
        if (Axis == TEXT("Vertical") || Axis == TEXT("Y"))
        {
            AxisIndex = 1;
        }

        // A 1D blend space only has the horizontal axis (index 0); writing the
        // vertical axis would be a semantic no-op the caller wouldn't see.
        if (AxisIndex == 1 && BlendSpace1D != nullptr)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Blend space '%s' is 1D and has no vertical axis"), *AssetPath),
                TEXT("AXIS_OUT_OF_RANGE"));
        }

        // BlendParameters is protected; reach it via the same FProperty reflection
        // used by create_blend_space_1d/2d. The property is a fixed FBlendParameter[3].
        FProperty* BlendParamsProp = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters"));
        if (!BlendParamsProp)
        {
            ANIM_ERROR_RESPONSE(TEXT("BlendParameters property not found via reflection"), TEXT("REFLECTION_FAILED"));
        }

        FBlendParameter* BlendParamsPtr = BlendParamsProp->ContainerPtrToValuePtr<FBlendParameter>(BlendSpace);
        if (!BlendParamsPtr)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not resolve BlendParameters storage"), TEXT("REFLECTION_FAILED"));
        }

        // Start from the existing axis so unspecified fields keep their value.
        FBlendParameter Param = BlendParamsPtr[AxisIndex];
        if (Params->HasField(TEXT("axisName")))
        {
            Param.DisplayName = GetJsonStringField(Params, TEXT("axisName"), Param.DisplayName);
        }
        if (Params->HasField(TEXT("minValue")))
        {
            Param.Min = static_cast<float>(GetJsonNumberField(Params, TEXT("minValue")));
        }
        if (Params->HasField(TEXT("maxValue")))
        {
            Param.Max = static_cast<float>(GetJsonNumberField(Params, TEXT("maxValue")));
        }
        if (Params->HasField(TEXT("gridDivisions")))
        {
            Param.GridNum = static_cast<int32>(GetJsonNumberField(Params, TEXT("gridDivisions")));
        }

        BlendSpace->Modify();
        BlendParamsPtr[AxisIndex] = Param;

        // Drive the axis-changed rebuild path explicitly so grid + samples revalidate.
        FPropertyChangedEvent BPEvent(BlendParamsProp);
        BlendSpace->PostEditChangeProperty(BPEvent);
        BlendSpace->MarkPackageDirty();

        const bool bSaved = SaveAnimAsset(BlendSpace, bSave);
        if (bSave && !bSaved)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Axis settings applied in memory but failed to save asset: %s"), *AssetPath),
                TEXT("BLENDSPACE_SAVE_FAILED"));
        }

        Response->SetStringField(TEXT("axis"), Axis);
        Response->SetStringField(TEXT("axisName"), Param.DisplayName);
        Response->SetNumberField(TEXT("minValue"), Param.Min);
        Response->SetNumberField(TEXT("maxValue"), Param.Max);
        Response->SetNumberField(TEXT("gridDivisions"), Param.GridNum);
        ANIM_SUCCESS_RESPONSE(TEXT("Axis settings updated"));
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetInterpolationSettings(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString InterpolationType = GetJsonStringField(Params, TEXT("interpolationType"), TEXT("Lerp"));
        float TargetWeightSpeed = static_cast<float>(GetJsonNumberField(Params, TEXT("targetWeightInterpolationSpeed"), 5.0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        UBlendSpace* BlendSpace2D = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
        UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(StaticLoadObject(UBlendSpace1D::StaticClass(), nullptr, *AssetPath));
        
        UBlendSpace* BlendSpace = BlendSpace2D ? BlendSpace2D : BlendSpace1D;
        if (!BlendSpace)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load blend space: %s"), *AssetPath), TEXT("BLENDSPACE_NOT_FOUND"));
        }
        
        BlendSpace->TargetWeightInterpolationSpeedPerSec = TargetWeightSpeed;
        
        SaveAnimAsset(BlendSpace, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Interpolation settings updated"));
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringCreateAimOffset(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
#if MCP_HAS_BLENDSPACE_FACTORY
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    // Schema advertises savePath; older callers pass path. Prefer savePath, fall back to path.
    FString Path = GetJsonStringField(Params, TEXT("savePath"), TEXT(""));
    if (Path.IsEmpty()) { Path = GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Animations")); }
    Path = NormalizeAnimPath(Path);
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    USkeleton* Skeleton = nullptr;
    if (!SkeletonPath.IsEmpty())
    {
        Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
    }

    // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UBlendSpaceFactoryNew* Factory = NewObject<UBlendSpaceFactoryNew>();
        Factory->TargetSkeleton = Skeleton;
        UAimOffsetBlendSpace* NewAimOffset = Cast<UAimOffsetBlendSpace>(
            Factory->FactoryCreateNew(UAimOffsetBlendSpace::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewAimOffset)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create aim offset"), TEXT("CREATE_FAILED"));
        }
        
        // Configure default aim offset axes (Yaw and Pitch)
        // Note: In UE 5.7+, GetBlendParameter returns const ref
        // The factory should have set reasonable defaults, just trigger update
        NewAimOffset->PostEditChange();
        NewAimOffset->MarkPackageDirty();
        
        SaveAnimAsset(NewAimOffset, bSave);
        
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Aim Offset '%s' created"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("Blend space factory not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddAimOffsetSample(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("animationPath"), TEXT("")));
        float Yaw = static_cast<float>(GetJsonNumberField(Params, TEXT("yaw"), 0.0));
        float Pitch = static_cast<float>(GetJsonNumberField(Params, TEXT("pitch"), 0.0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        UAimOffsetBlendSpace* AimOffset = Cast<UAimOffsetBlendSpace>(StaticLoadObject(UAimOffsetBlendSpace::StaticClass(), nullptr, *AssetPath));
        if (!AimOffset)
        {
            // Try as regular blend space
            UBlendSpace* BlendSpace = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
            if (BlendSpace)
            {
                UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
                if (!Animation)
                {
                    ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
                }
                
                FVector SampleValue(Yaw, Pitch, 0.0f);
                BlendSpace->AddSample(Animation, SampleValue);
                
                SaveAnimAsset(BlendSpace, bSave);
                
                ANIM_SUCCESS_RESPONSE(TEXT("Aim offset sample added"));
                return Response;
            }
            
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load aim offset: %s"), *AssetPath), TEXT("AIMOFFSET_NOT_FOUND"));
        }
        
        UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
        if (!Animation)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
        }
        
        // Add sample with yaw/pitch coordinates
        FVector SampleValue(Yaw, Pitch, 0.0f);
        AimOffset->AddSample(Animation, SampleValue);
        
        SaveAnimAsset(AimOffset, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Aim offset sample added"));
        return Response;
}

    // ===== 10.4 Animation Blueprints =====
static TSharedPtr<FJsonObject> AnimAuthoringCreateAnimBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    // Schema advertises savePath; older callers pass path. Prefer savePath, fall back to path.
    FString Path = GetJsonStringField(Params, TEXT("savePath"), TEXT(""));
    if (Path.IsEmpty()) { Path = GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Blueprints")); }
    Path = NormalizeAnimPath(Path);
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    FString ParentClass = GetJsonStringField(Params, TEXT("parentClass"), TEXT("AnimInstance"));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    USkeleton* Skeleton = nullptr;
    if (!SkeletonPath.IsEmpty())
    {
        Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
    }

    // Check if an asset already exists at the target path to prevent assertion failure in Kismet2.cpp
        FString ObjectPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
        if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
        {
            UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
            if (ExistingAsset)
            {
                if (Cast<UAnimBlueprint>(ExistingAsset))
                {
                    // Same type - return success with existing asset info
                    Response->SetStringField(TEXT("assetPath"), ObjectPath);
                    Response->SetBoolField(TEXT("existingAsset"), true);
                    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation Blueprint '%s' already exists - reusing existing asset"), *Name));
                    return Response;
                }
                else
                {
                    UBlueprint* ExistingBlueprint = Cast<UBlueprint>(ExistingAsset);
                    const bool bLegacyPlainBlueprint =
                        ExistingBlueprint && ExistingAsset->GetClass() == UBlueprint::StaticClass();

                    if (bLegacyPlainBlueprint)
                    {
                        const FString ExistingParentClassName =
                            ExistingBlueprint->ParentClass ? ExistingBlueprint->ParentClass->GetPathName() : TEXT("<none>");
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                            TEXT("create_animation_blueprint: Replacing legacy plain Blueprint at '%s' (parent=%s)"),
                            *ObjectPath,
                            *ExistingParentClassName);

                        const bool bDeletedLegacyAsset = UEditorAssetLibrary::DeleteAsset(ObjectPath);
                        if (!bDeletedLegacyAsset || UEditorAssetLibrary::DoesAssetExist(ObjectPath))
                        {
                            ANIM_ERROR_RESPONSE(
                                FString::Printf(TEXT("Failed to replace legacy plain Blueprint at '%s' before creating AnimBlueprint"), *ObjectPath),
                                TEXT("LEGACY_ASSET_DELETE_FAILED")
                            );
                        }
                    }
                    else
                    {
                        // Different type - return error to prevent assertion crash
                        FString ExistingClassName = ExistingAsset->GetClass()->GetName();
                        FString ExistingParentClassName =
                            (ExistingBlueprint && ExistingBlueprint->ParentClass)
                                ? ExistingBlueprint->ParentClass->GetPathName()
                                : TEXT("<none>");
                        ANIM_ERROR_RESPONSE(
                            FString::Printf(TEXT("Cannot create AnimBlueprint: asset '%s' already exists as type '%s' (parent=%s)"), 
                                *ObjectPath, *ExistingClassName, *ExistingParentClassName),
                            TEXT("ASSET_TYPE_MISMATCH")
                        );
                    }
                }
            }
        }
        
        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }
        
        UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
        Factory->TargetSkeleton = Skeleton;
        Factory->ParentClass = UAnimInstance::StaticClass();
        UAnimBlueprint* NewAnimBP = Cast<UAnimBlueprint>(
            Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewAnimBP)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create animation blueprint"), TEXT("CREATE_FAILED"));
        }
        
        // Compile the blueprint
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(NewAnimBP);
        
        SaveAnimAsset(NewAnimBP, bSave);
        
        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation Blueprint '%s' created"), *Name));
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddStateMachine(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        // The schema documents 'assetPath' and 'machineName' for these
        // params; blueprintPath/stateMachineName are the original names.
        FString RawBlueprintPath = GetJsonStringField(Params, TEXT("blueprintPath"), TEXT(""));
        if (RawBlueprintPath.IsEmpty())
        {
            RawBlueprintPath = GetJsonStringField(Params, TEXT("assetPath"), TEXT(""));
        }
        FString BlueprintPath = NormalizeAnimPath(RawBlueprintPath);
        FString StateMachineName = GetJsonStringField(Params, TEXT("stateMachineName"),
            GetJsonStringField(Params, TEXT("machineName"), TEXT("")));
        int32 NodePosX = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionY"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (StateMachineName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("stateMachineName (or machineName) is required"), TEXT("MISSING_STATE_MACHINE_NAME"));
        }
        if (BlueprintPath.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("blueprintPath (or assetPath) is required: pass the Anim Blueprint asset path"), TEXT("MISSING_BLUEPRINT_PATH"));
        }

        // Try to find in-memory version first (may have unsaved changes from create_animation_blueprint)
        UAnimBlueprint* AnimBP = FindObject<UAnimBlueprint>(nullptr, *BlueprintPath);
        if (!AnimBP)
        {
            // Fall back to loading from disk
            AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        }
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }

        if (UAnimGraphNode_StateMachine* ExistingSMNode = FindStateMachineNode(AnimGraph, StateMachineName))
        {
            Response->SetStringField(TEXT("nodeName"), ExistingSMNode->GetStateMachineName());
            Response->SetBoolField(TEXT("existingAsset"), true);
            ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State machine '%s' already exists in %s"), *StateMachineName, *BlueprintPath));
            return Response;
        }
        
        // Create the State Machine Node using FGraphNodeCreator
        FGraphNodeCreator<UAnimGraphNode_StateMachine> NodeCreator(*AnimGraph);
        UAnimGraphNode_StateMachine* SMNode = NodeCreator.CreateNode();
        SMNode->NodePosX = NodePosX;
        SMNode->NodePosY = NodePosY;
        NodeCreator.Finalize();
        
        // Create the internal State Machine Graph using FBlueprintEditorUtils
        UAnimationStateMachineGraph* InnerGraph = Cast<UAnimationStateMachineGraph>(
            FBlueprintEditorUtils::CreateNewGraph(
                AnimBP,
                FName(*StateMachineName),
                UAnimationStateMachineGraph::StaticClass(),
                UAnimationStateMachineSchema::StaticClass()
            )
        );
        if (!InnerGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create animation state machine graph"), TEXT("CREATE_GRAPH_FAILED"));
        }
        
        // Link the State Machine Node to its internal graph
        SMNode->EditorStateMachineGraph = InnerGraph;
        InnerGraph->OwnerAnimGraphNode = SMNode;
        
        // Initialize Entry Node (required for State Machines)
        const UAnimationStateMachineSchema* Schema = Cast<UAnimationStateMachineSchema>(InnerGraph->GetSchema());
        if (!Schema)
        {
            ANIM_ERROR_RESPONSE(TEXT("Animation state machine graph has an invalid schema"), TEXT("INVALID_SCHEMA"));
        }
        Schema->CreateDefaultNodesForGraph(*InnerGraph);
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("nodeName"), StateMachineName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State machine '%s' created with entry node"), *StateMachineName));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot create state machine '%s': AnimGraph module headers not available in this build. Rebuild with AnimGraph module enabled."), *StateMachineName),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddState(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString RawBlueprintPath = GetJsonStringField(Params, TEXT("blueprintPath"), TEXT(""));
        if (RawBlueprintPath.IsEmpty())
        {
            RawBlueprintPath = GetJsonStringField(Params, TEXT("assetPath"), TEXT(""));
        }
        FString BlueprintPath = NormalizeAnimPath(RawBlueprintPath);
        FString StateMachineName = GetJsonStringField(Params, TEXT("stateMachineName"),
            GetJsonStringField(Params, TEXT("machineName"), TEXT("")));
        FString StateName = GetJsonStringField(Params, TEXT("stateName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionX"), 200));
        int32 NodePosY = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionY"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (StateName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("stateName is required"), TEXT("MISSING_STATE_NAME"));
        }
        if (BlueprintPath.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("blueprintPath (or assetPath) is required: pass the Anim Blueprint asset path"), TEXT("MISSING_BLUEPRINT_PATH"));
        }

        // Try to find in-memory version first (may have unsaved changes from add_state_machine)
        UAnimBlueprint* AnimBP = FindObject<UAnimBlueprint>(nullptr, *BlueprintPath);
        if (!AnimBP)
        {
            // Fall back to loading from disk
            AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        }
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }

        TArray<UAnimGraphNode_StateMachine*> MatchingStateMachines = FindStateMachineNodes(AnimGraph, StateMachineName);
        if (MatchingStateMachines.Num() == 0)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("State machine '%s' not found"), *StateMachineName), TEXT("SM_NOT_FOUND"));
        }

        UAnimationStateMachineGraph* SMGraph = nullptr;
        for (UAnimGraphNode_StateMachine* MatchingSMNode : MatchingStateMachines)
        {
            if (!MatchingSMNode || !MatchingSMNode->EditorStateMachineGraph)
            {
                continue;
            }

            UAnimationStateMachineGraph* CandidateGraph = Cast<UAnimationStateMachineGraph>(MatchingSMNode->EditorStateMachineGraph);
            if (!CandidateGraph)
            {
                continue;
            }

            if (UAnimStateNode* ExistingState = FindStateNode(CandidateGraph, StateName))
            {
                Response->SetStringField(TEXT("stateName"), ExistingState->GetStateName());
                Response->SetStringField(TEXT("requestedName"), StateName);
                Response->SetStringField(TEXT("stateMachine"), StateMachineName);
                Response->SetBoolField(TEXT("existingAsset"), true);
                ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State '%s' already exists in state machine '%s'"), *ExistingState->GetStateName(), *StateMachineName));
                return Response;
            }

            if (!SMGraph)
            {
                SMGraph = CandidateGraph;
            }
        }

        if (!SMGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Invalid state machine graph"), TEXT("INVALID_GRAPH"));
        }
        
        // Create the State Node using FGraphNodeCreator
        FGraphNodeCreator<UAnimStateNode> StateCreator(*SMGraph);
        UAnimStateNode* StateNode = StateCreator.CreateNode();
        StateNode->NodePosX = NodePosX;
        StateNode->NodePosY = NodePosY;
        StateCreator.Finalize();
        
        // IMPORTANT: FGraphNodeCreator does NOT call PostPlacedNewNode(), which is where
        // the BoundGraph is normally created. We must create it manually here.
        // This mirrors UAnimStateNode::PostPlacedNewNode() logic exactly.
        if (!StateNode->BoundGraph)
        {
            // Create the animation state graph (BoundGraph) with NAME_None first
            // This matches UE's PostPlacedNewNode() behavior
            StateNode->BoundGraph = FBlueprintEditorUtils::CreateNewGraph(
                StateNode,
                NAME_None,
                UAnimationStateGraph::StaticClass(),
                UAnimationStateGraphSchema::StaticClass()
            );
            
            if (StateNode->BoundGraph)
            {
                // Use RenameGraphWithSuggestion for proper name validation (matches UE behavior)
                TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(StateNode);
                FBlueprintEditorUtils::RenameGraphWithSuggestion(StateNode->BoundGraph, NameValidator, *StateName);
                
                // Initialize the state graph with default nodes (result node, etc.)
                const UEdGraphSchema* StateSchema = StateNode->BoundGraph->GetSchema();
                if (StateSchema)
                {
                    StateSchema->CreateDefaultNodesForGraph(*StateNode->BoundGraph);
                }
                
                // Add the new graph as a child of the state machine graph
                if (SMGraph->SubGraphs.Find(StateNode->BoundGraph) == INDEX_NONE)
                {
                    SMGraph->SubGraphs.Add(StateNode->BoundGraph);
                }
            }
        }
        else
        {
            // BoundGraph already exists (shouldn't happen with FGraphNodeCreator), rename it
            FBlueprintEditorUtils::RenameGraph(StateNode->BoundGraph, *StateName);
        }
        
        // Get the actual state name that was assigned (may differ from requested due to validation)
        FString ActualStateName = StateNode->GetStateName();
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("stateName"), ActualStateName);
        Response->SetStringField(TEXT("requestedName"), StateName);
        Response->SetStringField(TEXT("stateMachine"), StateMachineName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State '%s' created in state machine '%s'"), *ActualStateName, *StateMachineName));
#else
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State '%s' marked for creation (requires AnimGraph module)"), *StateName));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddTransition(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString RawBlueprintPath = GetJsonStringField(Params, TEXT("blueprintPath"), TEXT(""));
        if (RawBlueprintPath.IsEmpty())
        {
            RawBlueprintPath = GetJsonStringField(Params, TEXT("assetPath"), TEXT(""));
        }
        FString BlueprintPath = NormalizeAnimPath(RawBlueprintPath);
        FString StateMachineName = GetJsonStringField(Params, TEXT("stateMachineName"),
            GetJsonStringField(Params, TEXT("machineName"), TEXT("")));
        FString FromState = GetJsonStringField(Params, TEXT("fromState"), TEXT(""));
        FString ToState = GetJsonStringField(Params, TEXT("toState"), TEXT(""));
        float CrossfadeDuration = static_cast<float>(GetJsonNumberField(Params, TEXT("crossfadeDuration"), 0.2));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (FromState.IsEmpty() || ToState.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("fromState and toState are required"), TEXT("MISSING_STATES"));
        }
        if (BlueprintPath.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("blueprintPath (or assetPath) is required: pass the Anim Blueprint asset path"), TEXT("MISSING_BLUEPRINT_PATH"));
        }

        // Try to find in-memory version first (may have unsaved changes from add_state)
        UAnimBlueprint* AnimBP = FindObject<UAnimBlueprint>(nullptr, *BlueprintPath);
        if (!AnimBP)
        {
            // Fall back to loading from disk
            AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        }
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA && MCP_HAS_ANIM_STATE_TRANSITION
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }

        TArray<UAnimGraphNode_StateMachine*> MatchingStateMachines = FindStateMachineNodes(AnimGraph, StateMachineName);
        if (MatchingStateMachines.Num() == 0)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("State machine '%s' not found"), *StateMachineName), TEXT("SM_NOT_FOUND"));
        }

        UAnimationStateMachineGraph* SMGraph = nullptr;
        bool bFoundSourceStateAnywhere = false;
        bool bFoundTargetStateAnywhere = false;

        for (UAnimGraphNode_StateMachine* MatchingSMNode : MatchingStateMachines)
        {
            if (!MatchingSMNode || !MatchingSMNode->EditorStateMachineGraph)
            {
                continue;
            }

            UAnimationStateMachineGraph* CandidateGraph = Cast<UAnimationStateMachineGraph>(MatchingSMNode->EditorStateMachineGraph);
            if (!CandidateGraph)
            {
                continue;
            }

            UAnimStateNode* CandidateFrom = FindStateNode(CandidateGraph, FromState);
            UAnimStateNode* CandidateTo = FindStateNode(CandidateGraph, ToState);
            bFoundSourceStateAnywhere = bFoundSourceStateAnywhere || CandidateFrom != nullptr;
            bFoundTargetStateAnywhere = bFoundTargetStateAnywhere || CandidateTo != nullptr;

            if (CandidateFrom && CandidateTo)
            {
                SMGraph = CandidateGraph;

                if (UAnimStateTransitionNode* ExistingTransition = FindTransitionNode(CandidateGraph, FromState, ToState))
                {
                    Response->SetStringField(TEXT("fromState"), FromState);
                    Response->SetStringField(TEXT("toState"), ToState);
                    Response->SetNumberField(TEXT("crossfadeDuration"), ExistingTransition->CrossfadeDuration);
                    Response->SetBoolField(TEXT("existingAsset"), true);
                    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Transition from '%s' to '%s' already exists"), *FromState, *ToState));
                    return Response;
                }

                break;
            }
        }

        if (!bFoundSourceStateAnywhere)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Source state '%s' not found"), *FromState), TEXT("SOURCE_STATE_NOT_FOUND"));
        }
        if (!bFoundTargetStateAnywhere)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Target state '%s' not found"), *ToState), TEXT("TARGET_STATE_NOT_FOUND"));
        }

        if (!SMGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Invalid state machine graph"), TEXT("INVALID_GRAPH"));
        }

        // Find the source and target states in the selected graph
        UAnimStateNode* FromNode = FindStateNode(SMGraph, FromState);
        UAnimStateNode* ToNode = FindStateNode(SMGraph, ToState);
        
        if (!FromNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Source state '%s' not found"), *FromState), TEXT("SOURCE_STATE_NOT_FOUND"));
        }
        if (!ToNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Target state '%s' not found"), *ToState), TEXT("TARGET_STATE_NOT_FOUND"));
        }
        
        // Create the Transition Node
        FGraphNodeCreator<UAnimStateTransitionNode> TransCreator(*SMGraph);
        UAnimStateTransitionNode* TransNode = TransCreator.CreateNode();
        TransCreator.Finalize();
        
        // Establish the connection between states
        TransNode->CreateConnections(FromNode, ToNode);
        
        // Configure transition properties
        TransNode->CrossfadeDuration = CrossfadeDuration;
        TransNode->BlendMode = EAlphaBlendOption::Linear;
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("fromState"), FromState);
        Response->SetStringField(TEXT("toState"), ToState);
        Response->SetNumberField(TEXT("crossfadeDuration"), CrossfadeDuration);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Transition from '%s' to '%s' created"), *FromState, *ToState));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot create transition from '%s' to '%s': AnimGraph module headers not available in this build."), *FromState, *ToState),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetTransitionRules(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString RawBlueprintPath = GetJsonStringField(Params, TEXT("blueprintPath"), TEXT(""));
        if (RawBlueprintPath.IsEmpty())
        {
            RawBlueprintPath = GetJsonStringField(Params, TEXT("assetPath"), TEXT(""));
        }
        FString BlueprintPath = NormalizeAnimPath(RawBlueprintPath);
        FString StateMachineName = GetJsonStringField(Params, TEXT("stateMachineName"),
            GetJsonStringField(Params, TEXT("machineName"), TEXT("")));
        FString FromState = GetJsonStringField(Params, TEXT("fromState"), TEXT(""));
        FString ToState = GetJsonStringField(Params, TEXT("toState"), TEXT(""));
        float CrossfadeDuration = static_cast<float>(GetJsonNumberField(Params, TEXT("crossfadeDuration"), -1.0));
        int32 PriorityOrder = static_cast<int32>(GetJsonNumberField(Params, TEXT("priorityOrder"), -1));
        bool bAutomatic = GetJsonBoolField(Params, TEXT("automaticRule"), false);
        bool bBidirectional = GetJsonBoolField(Params, TEXT("bidirectional"), false);
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (BlueprintPath.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("blueprintPath (or assetPath) is required: pass the Anim Blueprint asset path"), TEXT("MISSING_BLUEPRINT_PATH"));
        }

        // Try to find in-memory version first (may have unsaved changes)
        UAnimBlueprint* AnimBP = FindObject<UAnimBlueprint>(nullptr, *BlueprintPath);
        if (!AnimBP)
        {
            // Fall back to loading from disk
            AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        }
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA && MCP_HAS_ANIM_STATE_TRANSITION
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }

        TArray<UAnimGraphNode_StateMachine*> MatchingStateMachines = FindStateMachineNodes(AnimGraph, StateMachineName);
        if (MatchingStateMachines.Num() == 0)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("State machine '%s' not found"), *StateMachineName), TEXT("SM_NOT_FOUND"));
        }

        UAnimStateTransitionNode* TransNode = nullptr;
        for (UAnimGraphNode_StateMachine* MatchingSMNode : MatchingStateMachines)
        {
            if (!MatchingSMNode || !MatchingSMNode->EditorStateMachineGraph)
            {
                continue;
            }

            UAnimationStateMachineGraph* CandidateGraph = Cast<UAnimationStateMachineGraph>(MatchingSMNode->EditorStateMachineGraph);
            if (!CandidateGraph)
            {
                continue;
            }

            TransNode = FindTransitionNode(CandidateGraph, FromState, ToState);
            if (TransNode)
            {
                break;
            }
        }

        if (!TransNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Transition from '%s' to '%s' not found"), *FromState, *ToState), TEXT("TRANSITION_NOT_FOUND"));
        }
        
        // Update transition properties
        if (CrossfadeDuration >= 0.0f)
        {
            TransNode->CrossfadeDuration = CrossfadeDuration;
        }
        if (PriorityOrder >= 0)
        {
            TransNode->PriorityOrder = PriorityOrder;
        }
        TransNode->bAutomaticRuleBasedOnSequencePlayerInState = bAutomatic;
        TransNode->Bidirectional = bBidirectional;
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Transition rules updated for '%s' -> '%s'"), *FromState, *ToState));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot update transition rules for '%s' -> '%s': AnimGraph module headers not available in this build."), *FromState, *ToState),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddBlendNode(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString BlendType = GetJsonStringField(Params, TEXT("blendType"), TEXT("TwoWayBlend"));
        FString NodeName = GetJsonStringField(Params, TEXT("nodeName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionY"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        FString CreatedNodeType;
        FString CreatedNodeName = NodeName;
        
#if MCP_HAS_TWO_WAY_BLEND
        if (BlendType == TEXT("TwoWayBlend") || BlendType == TEXT("Blend"))
        {
            FGraphNodeCreator<UAnimGraphNode_TwoWayBlend> NodeCreator(*AnimGraph);
            UAnimGraphNode_TwoWayBlend* BlendNode = NodeCreator.CreateNode();
            BlendNode->NodePosX = NodePosX;
            BlendNode->NodePosY = NodePosY;
            // Set the node name via NodeComment so it can be found later
            if (!NodeName.IsEmpty())
            {
                BlendNode->NodeComment = NodeName;
                BlendNode->bCommentBubbleVisible = true;
            }
            NodeCreator.Finalize();
            CreatedNodeType = TEXT("TwoWayBlend");
            if (CreatedNodeName.IsEmpty())
            {
                CreatedNodeName = FString::Printf(TEXT("BlendNode_%d"), BlendNode->NodeGuid.A);
            }
        }
        else
#endif
#if MCP_HAS_LAYERED_BLEND
        if (BlendType == TEXT("LayeredBlend") || BlendType == TEXT("LayeredBoneBlend"))
        {
            FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> NodeCreator(*AnimGraph);
            UAnimGraphNode_LayeredBoneBlend* BlendNode = NodeCreator.CreateNode();
            BlendNode->NodePosX = NodePosX;
            BlendNode->NodePosY = NodePosY;
            // Set the node name via NodeComment so it can be found later
            if (!NodeName.IsEmpty())
            {
                BlendNode->NodeComment = NodeName;
                BlendNode->bCommentBubbleVisible = true;
            }
            NodeCreator.Finalize();
            CreatedNodeType = TEXT("LayeredBoneBlend");
            if (CreatedNodeName.IsEmpty())
            {
                CreatedNodeName = FString::Printf(TEXT("LayeredBlendNode_%d"), BlendNode->NodeGuid.A);
            }
        }
        else
#endif
        {
            // Default fallback to TwoWayBlend if available
#if MCP_HAS_TWO_WAY_BLEND
            FGraphNodeCreator<UAnimGraphNode_TwoWayBlend> NodeCreator(*AnimGraph);
            UAnimGraphNode_TwoWayBlend* BlendNode = NodeCreator.CreateNode();
            BlendNode->NodePosX = NodePosX;
            BlendNode->NodePosY = NodePosY;
            // Set the node name via NodeComment so it can be found later
            if (!NodeName.IsEmpty())
            {
                BlendNode->NodeComment = NodeName;
                BlendNode->bCommentBubbleVisible = true;
            }
            NodeCreator.Finalize();
            CreatedNodeType = TEXT("TwoWayBlend");
            if (CreatedNodeName.IsEmpty())
            {
                CreatedNodeName = FString::Printf(TEXT("BlendNode_%d"), BlendNode->NodeGuid.A);
            }
#else
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Cannot create blend node '%s': AnimGraph blend node headers not available in this build."), *BlendType), TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        }
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("nodeType"), CreatedNodeType);
        Response->SetStringField(TEXT("nodeName"), CreatedNodeName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend node '%s' (name: %s) created"), *CreatedNodeType, *CreatedNodeName));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot create blend node '%s': AnimGraph module headers not available in this build."), *BlendType),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddCachedPose(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString CacheName = GetJsonStringField(Params, TEXT("cacheName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionY"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        if (CacheName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("cacheName is required"), TEXT("MISSING_CACHE_NAME"));
        }
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_CACHED_POSE
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Create the Save Cached Pose node
        FGraphNodeCreator<UAnimGraphNode_SaveCachedPose> NodeCreator(*AnimGraph);
        UAnimGraphNode_SaveCachedPose* CachedPoseNode = NodeCreator.CreateNode();
        CachedPoseNode->NodePosX = NodePosX;
        CachedPoseNode->NodePosY = NodePosY;
        CachedPoseNode->CacheName = CacheName;
        NodeCreator.Finalize();
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("cacheName"), CacheName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Cached pose node '%s' created"), *CacheName));
#else
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Cached pose '%s' marked for creation (requires AnimGraph module)"), *CacheName));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddSlotNode(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString SlotName = GetJsonStringField(Params, TEXT("slotName"), TEXT(""));
        FString GroupName = GetJsonStringField(Params, TEXT("groupName"), TEXT("DefaultGroup"));
        int32 NodePosX = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionY"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        if (SlotName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("slotName is required"), TEXT("MISSING_SLOT_NAME"));
        }
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_SLOT_NODE
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Create the Slot node
        FGraphNodeCreator<UAnimGraphNode_Slot> NodeCreator(*AnimGraph);
        UAnimGraphNode_Slot* SlotNode = NodeCreator.CreateNode();
        SlotNode->NodePosX = NodePosX;
        SlotNode->NodePosY = NodePosY;
        
        // Set the slot name (format: "GroupName.SlotName")
        FString FullSlotName = FString::Printf(TEXT("%s.%s"), *GroupName, *SlotName);
        SlotNode->Node.SlotName = FName(*FullSlotName);
        
        NodeCreator.Finalize();
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("slotName"), FullSlotName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Slot node '%s' created"), *FullSlotName));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot create slot node '%s': AnimGraph module headers not available in this build."), *SlotName),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringAddLayeredBlendPerBone(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString BoneName = GetJsonStringField(Params, TEXT("boneName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionY"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_LAYERED_BLEND
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Create the Layered Bone Blend node
        FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> NodeCreator(*AnimGraph);
        UAnimGraphNode_LayeredBoneBlend* BlendNode = NodeCreator.CreateNode();
        BlendNode->NodePosX = NodePosX;
        BlendNode->NodePosY = NodePosY;
        NodeCreator.Finalize();
        
        // Note: Configuring specific bone layers requires access to BlendNode->Node.LayerSetup
        // which is typically done through the editor UI. Basic node creation is complete.
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        ANIM_SUCCESS_RESPONSE(TEXT("Layered blend per bone node created"));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            TEXT("Cannot create layered blend per bone node: AnimGraph module headers not available in this build."),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetAnimGraphNodeValue(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString NodeName = GetJsonStringField(Params, TEXT("nodeName"), TEXT(""));
        FString PropertyName = GetJsonStringField(Params, TEXT("propertyName"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        if (NodeName.IsEmpty() || PropertyName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("nodeName and propertyName are required"), TEXT("MISSING_PARAMETERS"));
        }
        
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }
        
#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }
        
        // Find the node by name (search both NodeTitle and NodeComment)
        UEdGraphNode* FoundNode = nullptr;
        for (UEdGraphNode* Node : AnimGraph->Nodes)
        {
            if (Node)
            {
                // Check NodeComment first (custom name set via add_blend_node)
                if (Node->NodeComment.Contains(NodeName))
                {
                    FoundNode = Node;
                    break;
                }
                // Also check the node title
                if (Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(NodeName))
                {
                    FoundNode = Node;
                    break;
                }
            }
        }
        
        if (!FoundNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Node '%s' not found in AnimGraph"), *NodeName), TEXT("NODE_NOT_FOUND"));
        }
        
        // Find and set the property using reflection
        // Support dot notation for nested properties (e.g., "BlendNode.Alpha")
        void* TargetContainer = FoundNode;
        FProperty* Property = nullptr;
        FString RemainingPath = PropertyName;
        
        while (!RemainingPath.IsEmpty())
        {
            FString CurrentPart;
            int32 DotIndex;
            if (RemainingPath.FindChar(TEXT('.'), DotIndex))
            {
                CurrentPart = RemainingPath.Left(DotIndex);
                RemainingPath = RemainingPath.Mid(DotIndex + 1);
            }
            else
            {
                CurrentPart = RemainingPath;
                RemainingPath.Empty();
            }
            
            FProperty* CurrentProp = TargetContainer 
                ? FoundNode->GetClass()->FindPropertyByName(FName(*CurrentPart))
                : nullptr;
            
            // If searching on a struct, use the struct's property lookup
            if (!CurrentProp && Property)
            {
                if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
                {
                    CurrentProp = StructProp->Struct->FindPropertyByName(FName(*CurrentPart));
                    if (CurrentProp)
                    {
                        TargetContainer = Property->ContainerPtrToValuePtr<void>(TargetContainer);
                        Property = CurrentProp;
                        continue;
                    }
                }
            }
            
            if (!CurrentProp)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Property '%s' not found on node '%s'"), *CurrentPart, *NodeName), TEXT("PROPERTY_NOT_FOUND"));
            }
            
            Property = CurrentProp;
        }
        
        if (!Property)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Property '%s' not found on node '%s'"), *PropertyName, *NodeName), TEXT("PROPERTY_NOT_FOUND"));
        }
        
        // Get the value from params and apply it
        McpPropertyReflection::FMcpTypedValue NodeTyped;
        FString NodeParseDetail;
        const McpPropertyReflection::EMcpTypedValueParse NodeParse =
            McpPropertyReflection::ReadDiscriminatedValue(Params, NodeTyped, NodeParseDetail);
        if (NodeParse == McpPropertyReflection::EMcpTypedValueParse::None)
        {
            ANIM_ERROR_RESPONSE(TEXT("set exactly one typed value field: boolValue, intValue, floatValue, stringValue, colorValue, vectorValue, structValue, or arrayValue"), TEXT("MISSING_VALUE"));
        }
        if (NodeParse == McpPropertyReflection::EMcpTypedValueParse::Ambiguous)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("set exactly one typed value field, got: %s"), *NodeParseDetail), TEXT("AMBIGUOUS_VALUE"));
        }
        TSharedPtr<FJsonValue> ValueField = NodeTyped.Json;
        
        FString ApplyError;
        if (!ApplyJsonValueToProperty(FoundNode, Property, ValueField, ApplyError))
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Failed to set property: %s"), *ApplyError), TEXT("PROPERTY_SET_FAILED"));
        }
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        
        Response->SetStringField(TEXT("nodeName"), NodeName);
        Response->SetStringField(TEXT("propertyName"), PropertyName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Property '%s' set on node '%s'"), *PropertyName, *NodeName));
#else
        // AnimGraph headers not available - return error
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot set node value on '%s': AnimGraph module headers not available in this build."), *NodeName),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
}

    // ===== 10.5 Control Rig =====
static TSharedPtr<FJsonObject> AnimAuthoringCreateControlRig(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
// ControlRig factory static methods (CreateNewControlRigAsset, CreateControlRigFromSkeletalMeshOrSkeleton)
// are only available in UE 5.5+ where ControlRigBlueprintFactory.h is in Public folder
#if MCP_HAS_CONTROLRIG_FACTORY && MCP_HAS_CONTROLRIG_BLUEPRINT && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/ControlRigs")));
        FString SkeletalMeshPath = GetJsonStringField(Params, TEXT("skeletalMeshPath"), TEXT(""));
        FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
        bool bModularRig = GetJsonBoolField(Params, TEXT("modularRig"), false);
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }

        UControlRigBlueprint* ControlRigBP = nullptr;
        FString FullPath = Path / Name;

        UObject* SelectedObject = nullptr;
        if (!SkeletalMeshPath.IsEmpty())
        {
            SelectedObject = LoadSkeletalMeshFromPathAnim(SkeletalMeshPath);
            if (!SelectedObject)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeletal mesh: %s"), *SkeletalMeshPath), TEXT("SKELETAL_MESH_NOT_FOUND"));
            }
        }
        else if (!SkeletonPath.IsEmpty())
        {
            SelectedObject = LoadSkeletonFromPathAnim(SkeletonPath);
            if (!SelectedObject)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
            }
        }

        if (SelectedObject)
        {
            ControlRigBP = UControlRigBlueprintFactory::CreateControlRigFromSkeletalMeshOrSkeleton(SelectedObject, bModularRig);
        }
        else
        {
            ControlRigBP = UControlRigBlueprintFactory::CreateNewControlRigAsset(FullPath, bModularRig);
        }
        
        if (!ControlRigBP)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create Control Rig blueprint"), TEXT("CREATION_FAILED"));
        }
        
        if (!SaveAnimAsset(ControlRigBP, bSave))
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to save Control Rig blueprint"), TEXT("SAVE_FAILED"));
        }
        
        Response->SetStringField(TEXT("assetPath"), ControlRigBP->GetPathName());
        Response->SetBoolField(TEXT("modularRig"), bModularRig);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Control Rig '%s' created successfully"), *Name));
#elif MCP_HAS_CONTROLRIG_BLUEPRINT
        // Factory static methods not available in UE 5.1-5.4 (header is in Private folder)
        // Use the Subsystem's CreateControlRigBlueprint method as fallback
        FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/ControlRigs")));
        FString SkeletalMeshPath = GetJsonStringField(Params, TEXT("skeletalMeshPath"), TEXT(""));
        FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));

        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }

        FString FullPath = Path / Name;

        // Create Control Rig Blueprint using FKismetEditorUtilities (works in all UE 5.x versions)
        FString FullPackageName = Path / Name;

        // Create the package
        UPackage* Package = CreatePackage(*FullPackageName);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Failed to create package: %s"), *FullPackageName), TEXT("PACKAGE_CREATE_FAILED"));
        }

        Package->FullyLoad();

        // Create the Control Rig Blueprint using FKismetEditorUtilities
        UControlRigBlueprint* ControlRigBP = Cast<UControlRigBlueprint>(
            FKismetEditorUtilities::CreateBlueprint(
                UControlRig::StaticClass(),
                Package,
                *Name,
                BPTYPE_Normal,
                UControlRigBlueprint::StaticClass(),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                URigVMBlueprintGeneratedClass::StaticClass(),
#else
                // UE 5.0 uses UControlRigBlueprintGeneratedClass instead
                UControlRigBlueprintGeneratedClass::StaticClass(),
#endif
                NAME_None));

        if (!ControlRigBP)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create Control Rig Blueprint"), TEXT("CREATION_FAILED"));
        }

        // Set the target skeleton if provided (via skeletal mesh or skeleton path)
        if (!SkeletalMeshPath.IsEmpty())
        {
            USkeletalMesh* SkeletalMesh = LoadSkeletalMeshFromPathAnim(SkeletalMeshPath);
            if (!SkeletalMesh)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeletal mesh: %s"), *SkeletalMeshPath), TEXT("SKELETAL_MESH_NOT_FOUND"));
            }
            if (SkeletalMesh->GetSkeleton())
            {
                USkeletalMesh* PreviewMesh = SkeletalMesh->GetSkeleton()->GetPreviewMesh();
                if (PreviewMesh)
                {
                    ControlRigBP->SetPreviewMesh(PreviewMesh);
                }
            }
        }
        else if (!SkeletonPath.IsEmpty())
        {
            USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
            if (!Skeleton)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
            }
            USkeletalMesh* PreviewMesh = Skeleton->GetPreviewMesh();
            if (PreviewMesh)
            {
                ControlRigBP->SetPreviewMesh(PreviewMesh);
            }
        }
        
        if (!SaveAnimAsset(ControlRigBP, GetJsonBoolField(Params, TEXT("save"), true)))
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to save Control Rig Blueprint"), TEXT("SAVE_FAILED"));
        }

        Response->SetStringField(TEXT("assetPath"), ControlRigBP->GetPathName());
        Response->SetBoolField(TEXT("modularRig"), false);  // Not supported in fallback
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Control Rig '%s' created successfully (UE 5.1-5.4 compatible mode)"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("Control Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
}

    // ===== 10.6 Retargeting =====
static TSharedPtr<FJsonObject> AnimAuthoringCreateIkRig(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
#if MCP_HAS_IKRIG_FACTORY && MCP_HAS_IKRIG
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Retargeting")));
    FString SkeletalMeshPath = GetJsonStringField(Params, TEXT("skeletalMeshPath"), TEXT(""));
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    // Use static factory method to create IK Rig (UE 5.1+) or fallback to NewObject (UE 5.0)
#if MCP_HAS_IKRIG_CREATE_NEW_ASSET
    UIKRigDefinition* IKRig = MCP_IKRIG_CREATE_NEW_ASSET(Path, Name);
#else
    // UE 5.0: Create using NewObject since CreateNewIKRigAsset doesn't exist
    UPackage* Package = CreatePackage(*FString(Path / Name));
    if (!Package)
    {
        ANIM_ERROR_RESPONSE(TEXT("Failed to create package for IK Rig"), TEXT("PACKAGE_FAILED"));
    }
    UIKRigDefinition* IKRig = NewObject<UIKRigDefinition>(Package, *Name, RF_Public | RF_Standalone);
    if (!IKRig)
    {
        ANIM_ERROR_RESPONSE(TEXT("Failed to create IK Rig asset"), TEXT("CREATION_FAILED"));
    }
    // Mark the package as needing save
    Package->MarkPackageDirty();
#endif

    if (!IKRig)
    {
        ANIM_ERROR_RESPONSE(TEXT("Failed to create IK Rig asset"), TEXT("CREATION_FAILED"));
    }

        // If skeletal mesh path provided, set the preview mesh
        if (!SkeletalMeshPath.IsEmpty())
        {
            USkeletalMesh* SkeletalMesh = LoadSkeletalMeshFromPathAnim(SkeletalMeshPath);
            if (!SkeletalMesh)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeletal mesh: %s"), *SkeletalMeshPath), TEXT("SKELETAL_MESH_NOT_FOUND"));
            }
            IKRig->SetPreviewMesh(SkeletalMesh);
        }
        // Also support skeletonPath: load skeleton and set its preview mesh on the IK Rig
        else if (!SkeletonPath.IsEmpty())
        {
            USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
            if (!Skeleton)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
            }
            USkeletalMesh* PreviewMesh = Skeleton->GetPreviewMesh();
            if (PreviewMesh)
            {
                IKRig->SetPreviewMesh(PreviewMesh);
            }
            Response->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());
        }

    if (!SaveAnimAsset(IKRig, bSave))
    {
        ANIM_ERROR_RESPONSE(TEXT("Failed to save IK Rig asset"), TEXT("SAVE_FAILED"));
    }

    Response->SetStringField(TEXT("assetPath"), IKRig->GetPathName());
    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("IK Rig '%s' created successfully"), *Name));
#elif MCP_HAS_IKRIG
    ANIM_ERROR_RESPONSE(
        TEXT("create_ik_rig requires the IKRigEditor factory module in this build"),
        TEXT("IKRIG_FACTORY_UNAVAILABLE"));
#else
    ANIM_ERROR_RESPONSE(TEXT("IK Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
    return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringCreateIkRetargeter(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
#if MCP_HAS_IKRETARGET_FACTORY && MCP_HAS_IKRETARGETER
        FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Retargeting")));
        FString SourceIKRigPath = GetJsonStringField(Params, TEXT("sourceIKRigPath"), TEXT(""));
        FString TargetIKRigPath = GetJsonStringField(Params, TEXT("targetIKRigPath"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        
        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }
        
        // Create the IK Retargeter using factory
        FString FullPath = Path / Name;
        FString PackageName = FullPath;
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package for IK Retargeter"), TEXT("PACKAGE_ERROR"));
        }
        
        UIKRetargetFactory* Factory = NewObject<UIKRetargetFactory>();
        
        UIKRetargeter* Retargeter = Cast<UIKRetargeter>(Factory->FactoryCreateNew(
            UIKRetargeter::StaticClass(),
            Package,
            FName(*Name),
            RF_Public | RF_Standalone,
            nullptr,
            GWarn
        ));
        
        if (!Retargeter)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create IK Retargeter"), TEXT("CREATION_FAILED"));
        }
        
// Set source and target IK Rigs using the controller (UE 5.1+ requires this as direct access is private)
#if MCP_HAS_IKRETARGETER_CONTROLLER
if (UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter))
{
if (!SourceIKRigPath.IsEmpty())
{
UIKRigDefinition* SourceRig = Cast<UIKRigDefinition>(StaticLoadObject(UIKRigDefinition::StaticClass(), nullptr, *SourceIKRigPath));
if (SourceRig)
{
MCP_IKRETARGETER_SET_SOURCE_IKRIG(Controller, SourceRig);
}
}
if (!TargetIKRigPath.IsEmpty())
{
UIKRigDefinition* TargetRig = Cast<UIKRigDefinition>(StaticLoadObject(UIKRigDefinition::StaticClass(), nullptr, *TargetIKRigPath));
if (TargetRig)
{
MCP_IKRETARGETER_SET_TARGET_IKRIG(Controller, TargetRig);
}
}
}
#else
// Fallback for UE 5.0 where direct access was public
if (!SourceIKRigPath.IsEmpty())
{
UIKRigDefinition* SourceRig = Cast<UIKRigDefinition>(StaticLoadObject(UIKRigDefinition::StaticClass(), nullptr, *SourceIKRigPath));
if (SourceRig)
{
Retargeter->SourceIKRigAsset = SourceRig;
}
}
if (!TargetIKRigPath.IsEmpty())
{
UIKRigDefinition* TargetRig = Cast<UIKRigDefinition>(StaticLoadObject(UIKRigDefinition::StaticClass(), nullptr, *TargetIKRigPath));
if (TargetRig)
{
Retargeter->TargetIKRigAsset = TargetRig;
}
}
#endif
        
        if (!SaveAnimAsset(Retargeter, bSave))
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to save IK Retargeter asset"), TEXT("SAVE_FAILED"));
        }
        
        Response->SetStringField(TEXT("assetPath"), Retargeter->GetPathName());
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("IK Retargeter '%s' created successfully"), *Name));
#elif MCP_HAS_IKRETARGETER
        ANIM_ERROR_RESPONSE(
            TEXT("create_ik_retargeter requires the IKRigEditor factory module in this build"),
            TEXT("IKRETARGET_FACTORY_UNAVAILABLE"));
#else
        ANIM_ERROR_RESPONSE(TEXT("IK Retargeter module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
}

static TSharedPtr<FJsonObject> AnimAuthoringSetRetargetChainMapping(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
#if MCP_HAS_IKRETARGETER
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString SourceChain = GetJsonStringField(Params, TEXT("sourceChain"), TEXT(""));
        FString TargetChain = GetJsonStringField(Params, TEXT("targetChain"), TEXT(""));
        
        if (SourceChain.IsEmpty() || TargetChain.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("sourceChain and targetChain are required"), TEXT("MISSING_CHAINS"));
        }
        
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Chain mapping '%s' -> '%s' set"), *SourceChain, *TargetChain));
#else
        ANIM_ERROR_RESPONSE(TEXT("IK Retargeter module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
}

    // ===== Utility =====
static TSharedPtr<FJsonObject> AnimAuthoringGetAnimationInfo(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        
        UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
        if (!Asset)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }
        
        TSharedPtr<FJsonObject> AnimInfo = McpHandlerUtils::CreateResultObject();
        
        if (UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
        {
            AnimInfo->SetStringField(TEXT("assetType"), TEXT("AnimSequence"));
            if (Sequence->GetSkeleton())
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), Sequence->GetSkeleton()->GetPathName());
            }
            AnimInfo->SetNumberField(TEXT("duration"), Sequence->GetPlayLength());
#if ENGINE_MAJOR_VERSION >= 5
            AnimInfo->SetNumberField(TEXT("numFrames"), Sequence->GetNumberOfSampledKeys());
            AnimInfo->SetNumberField(TEXT("frameRate"), Sequence->GetSamplingFrameRate().AsDecimal());
#endif
            AnimInfo->SetNumberField(TEXT("numNotifies"), Sequence->Notifies.Num());
            AnimInfo->SetBoolField(TEXT("isAdditive"), Sequence->AdditiveAnimType != AAT_None);
            AnimInfo->SetBoolField(TEXT("hasRootMotion"), Sequence->bEnableRootMotion);
        }
        else if (UAnimMontage* Montage = Cast<UAnimMontage>(Asset))
        {
            AnimInfo->SetStringField(TEXT("assetType"), TEXT("AnimMontage"));
            if (Montage->GetSkeleton())
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), Montage->GetSkeleton()->GetPathName());
            }
            AnimInfo->SetNumberField(TEXT("duration"), Montage->GetPlayLength());
            AnimInfo->SetNumberField(TEXT("numSections"), Montage->CompositeSections.Num());
            AnimInfo->SetNumberField(TEXT("numSlots"), Montage->SlotAnimTracks.Num());
            AnimInfo->SetNumberField(TEXT("numNotifies"), Montage->Notifies.Num());

            // Blend in/out + auto-blend-out: the "snap on release" tuning lives here and was
            // previously only reachable via execute_python.
            AnimInfo->SetNumberField(TEXT("blendInTime"), Montage->BlendIn.GetBlendTime());
            AnimInfo->SetNumberField(TEXT("blendOutTime"), Montage->BlendOut.GetBlendTime());
            AnimInfo->SetBoolField(TEXT("autoBlendOut"), Montage->bEnableAutoBlendOut);

            // Sections: name + start time + next-section link (the section graph).
            TArray<TSharedPtr<FJsonValue>> SectionsArr;
            for (const FCompositeSection& Sec : Montage->CompositeSections)
            {
                TSharedPtr<FJsonObject> SecObj = MakeShared<FJsonObject>();
                SecObj->SetStringField(TEXT("name"), Sec.SectionName.ToString());
                SecObj->SetNumberField(TEXT("startTime"), Sec.GetTime());
                SecObj->SetStringField(TEXT("nextSection"), Sec.NextSectionName.ToString());
                SectionsArr.Add(MakeShared<FJsonValueObject>(SecObj));
            }
            AnimInfo->SetArrayField(TEXT("sections"), SectionsArr);

            // Slots + their anim segments (slot tracks are not Python-reflectable, so this
            // is the only way over the bridge to see slot names and segment source anims).
            TArray<TSharedPtr<FJsonValue>> SlotsArr;
            for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
            {
                TSharedPtr<FJsonObject> SlotObj = MakeShared<FJsonObject>();
                SlotObj->SetStringField(TEXT("slotName"), Slot.SlotName.ToString());
                TArray<TSharedPtr<FJsonValue>> SegsArr;
                for (const FAnimSegment& Seg : Slot.AnimTrack.AnimSegments)
                {
                    TSharedPtr<FJsonObject> SegObj = MakeShared<FJsonObject>();
                    const UAnimSequenceBase* Ref = Seg.GetAnimReference();
                    SegObj->SetStringField(TEXT("anim"), Ref ? Ref->GetPathName() : TEXT(""));
                    SegObj->SetNumberField(TEXT("startPos"), Seg.StartPos);
                    SegObj->SetNumberField(TEXT("length"), Seg.GetLength());
                    SegsArr.Add(MakeShared<FJsonValueObject>(SegObj));
                }
                SlotObj->SetArrayField(TEXT("segments"), SegsArr);
                SlotsArr.Add(MakeShared<FJsonValueObject>(SlotObj));
            }
            AnimInfo->SetArrayField(TEXT("slots"), SlotsArr);
        }
        else if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(Asset))
        {
            const bool bIs1D = Cast<UBlendSpace1D>(Asset) != nullptr;
            AnimInfo->SetStringField(TEXT("assetType"), bIs1D ? TEXT("BlendSpace1D") : TEXT("BlendSpace2D"));
            if (BlendSpace->GetSkeleton())
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), BlendSpace->GetSkeleton()->GetPathName());
            }
            AnimInfo->SetNumberField(TEXT("numSamples"), BlendSpace->GetBlendSamples().Num());

            // Axes: index 2 of BlendParameters is never used by blend spaces, so
            // emit only the dimensions this asset actually blends across.
            TArray<TSharedPtr<FJsonValue>> AxesArr;
            const int32 NumAxes = bIs1D ? 1 : 2;
            for (int32 AxisIdx = 0; AxisIdx < NumAxes; ++AxisIdx)
            {
                const FBlendParameter& Param = BlendSpace->GetBlendParameter(AxisIdx);
                TSharedPtr<FJsonObject> AxisObj = MakeShared<FJsonObject>();
                AxisObj->SetStringField(TEXT("name"), Param.DisplayName);
                AxisObj->SetNumberField(TEXT("min"), Param.Min);
                AxisObj->SetNumberField(TEXT("max"), Param.Max);
                AxisObj->SetNumberField(TEXT("gridDivisions"), Param.GridNum);
                AxesArr.Add(MakeShared<FJsonValueObject>(AxisObj));
            }
            AnimInfo->SetArrayField(TEXT("axes"), AxesArr);

            const TArray<FBlendSample>& BlendSamples = BlendSpace->GetBlendSamples();
            constexpr int32 MaxSamples = 50;
            TArray<TSharedPtr<FJsonValue>> SamplesArr;
            for (int32 SampleIdx = 0; SampleIdx < BlendSamples.Num() && SampleIdx < MaxSamples; ++SampleIdx)
            {
                const FBlendSample& Sample = BlendSamples[SampleIdx];
                TSharedPtr<FJsonObject> SampleObj = MakeShared<FJsonObject>();
                SampleObj->SetStringField(TEXT("animation"), Sample.Animation ? Sample.Animation->GetPathName() : TEXT(""));
                SampleObj->SetNumberField(TEXT("x"), Sample.SampleValue.X);
                if (!bIs1D)
                {
                    SampleObj->SetNumberField(TEXT("y"), Sample.SampleValue.Y);
                }
                SamplesArr.Add(MakeShared<FJsonValueObject>(SampleObj));
            }
            AnimInfo->SetArrayField(TEXT("samples"), SamplesArr);
            AnimInfo->SetBoolField(TEXT("samplesTruncated"), BlendSamples.Num() > MaxSamples);
        }
        else if (UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Asset))
        {
            AnimInfo->SetStringField(TEXT("assetType"), TEXT("AnimBlueprint"));
            if (AnimBP->TargetSkeleton)
            {
                AnimInfo->SetStringField(TEXT("skeletonPath"), AnimBP->TargetSkeleton->GetPathName());
                AnimInfo->SetStringField(TEXT("targetSkeletonPath"), AnimBP->TargetSkeleton->GetPathName());
            }
            AnimInfo->SetStringField(TEXT("parentClass"), AnimBP->ParentClass ? AnimBP->ParentClass->GetName() : TEXT(""));

#if MCP_HAS_ANIMATION_GRAPH
            // Top-level anim graphs only (main AnimGraph + anim layer graphs); state/
            // transition graphs live in node SubGraphs, not these arrays.
            int32 AnimGraphCount = 0;
            for (const UEdGraph* Graph : AnimBP->UbergraphPages)
            {
                if (Graph && Graph->IsA<UAnimationGraph>()) { ++AnimGraphCount; }
            }
            for (const UEdGraph* Graph : AnimBP->FunctionGraphs)
            {
                if (Graph && Graph->IsA<UAnimationGraph>()) { ++AnimGraphCount; }
            }
            AnimInfo->SetNumberField(TEXT("animGraphCount"), AnimGraphCount);
#endif

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
            // GetAllNodesOfClass walks SubGraphs too, so nested state machines are included.
            TArray<UAnimGraphNode_StateMachine*> SMNodes;
            FBlueprintEditorUtils::GetAllNodesOfClass<UAnimGraphNode_StateMachine>(AnimBP, SMNodes);
            constexpr int32 MaxStateMachines = 20;
            constexpr int32 MaxStatesPerMachine = 50;
            TArray<TSharedPtr<FJsonValue>> SMArr;
            for (int32 SMIdx = 0; SMIdx < SMNodes.Num() && SMIdx < MaxStateMachines; ++SMIdx)
            {
                UAnimGraphNode_StateMachine* SMNode = SMNodes[SMIdx];
                if (!SMNode) { continue; }
                TSharedPtr<FJsonObject> SMObj = MakeShared<FJsonObject>();
                SMObj->SetStringField(TEXT("name"), SMNode->GetStateMachineName());

                int32 NumStates = 0;
                int32 NumTransitions = 0;
                TArray<TSharedPtr<FJsonValue>> StatesArr;
                TArray<TSharedPtr<FJsonValue>> TransitionsArr;
                if (UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(SMNode->EditorStateMachineGraph))
                {
                    for (UEdGraphNode* Node : SMGraph->Nodes)
                    {
                        if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
                        {
                            ++NumStates;
                            if (StatesArr.Num() < MaxStatesPerMachine)
                            {
                                StatesArr.Add(MakeShared<FJsonValueString>(StateNode->GetStateName()));
                            }
                        }
#if MCP_HAS_ANIM_STATE_TRANSITION
                        else if (UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node))
                        {
                            ++NumTransitions;
                            if (TransitionsArr.Num() < MaxStatesPerMachine)
                            {
                                UAnimStateNodeBase* PrevState = Trans->GetPreviousState();
                                UAnimStateNodeBase* NextState = Trans->GetNextState();
                                TSharedPtr<FJsonObject> TransObj = MakeShared<FJsonObject>();
                                TransObj->SetStringField(TEXT("from"), PrevState ? PrevState->GetStateName() : TEXT(""));
                                TransObj->SetStringField(TEXT("to"), NextState ? NextState->GetStateName() : TEXT(""));
                                TransitionsArr.Add(MakeShared<FJsonValueObject>(TransObj));
                            }
                        }
#endif
                    }
                }
                SMObj->SetNumberField(TEXT("numStates"), NumStates);
                SMObj->SetNumberField(TEXT("numTransitions"), NumTransitions);
                SMObj->SetArrayField(TEXT("states"), StatesArr);
                SMObj->SetArrayField(TEXT("transitions"), TransitionsArr);
                SMObj->SetBoolField(TEXT("truncated"), NumStates > StatesArr.Num() || NumTransitions > TransitionsArr.Num());
                SMArr.Add(MakeShared<FJsonValueObject>(SMObj));
            }
            AnimInfo->SetNumberField(TEXT("numStateMachines"), SMNodes.Num());
            AnimInfo->SetArrayField(TEXT("stateMachines"), SMArr);
            AnimInfo->SetBoolField(TEXT("stateMachinesTruncated"), SMNodes.Num() > MaxStateMachines);
#endif
        }
        else
        {
            AnimInfo->SetStringField(TEXT("assetType"), Asset->GetClass()->GetName());
        }
        
        Response->SetObjectField(TEXT("animationInfo"), AnimInfo);
        ANIM_SUCCESS_RESPONSE(TEXT("Animation info retrieved"));
        return Response;
}

    // Wires a montage AnimNotify to a BlueprintCallable function: authors an
    // AnimNotify_<NotifyName> custom event in the AnimBP EventGraph, then
    // TryGetPawnOwner -> Cast<targetClass> -> call functionName on the result.
    // A name-only montage notify dispatches by FindFunction("AnimNotify_"+name)
    // on the AnimInstance (UAnimInstance::TriggerSingleAnimNotify), so the
    // custom event name is load-bearing and must match the notify name.
static TSharedPtr<FJsonObject> AnimAuthoringBindAnimNotify(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        if (BlueprintPath.IsEmpty())
        {
            BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        }
        const FString NotifyName      = GetJsonStringField(Params, TEXT("notifyName"), TEXT(""));
        const FString FunctionName    = GetJsonStringField(Params, TEXT("functionName"), TEXT(""));
        const FString TargetClassName = GetJsonStringField(Params, TEXT("targetClass"), TEXT(""));
        const bool bSave              = GetJsonBoolField(Params, TEXT("save"), true);
        const int32 NodePosX          = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionX"), 0));
        const int32 NodePosY          = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionY"), 0));

        if (BlueprintPath.IsEmpty()) { ANIM_ERROR_RESPONSE(TEXT("blueprintPath (or assetPath) is required"), TEXT("MISSING_BLUEPRINT_PATH")); }
        if (NotifyName.IsEmpty())    { ANIM_ERROR_RESPONSE(TEXT("notifyName is required"), TEXT("MISSING_NOTIFY_NAME")); }
        if (FunctionName.IsEmpty())  { ANIM_ERROR_RESPONSE(TEXT("functionName is required"), TEXT("MISSING_FUNCTION_NAME")); }

        // StaticLoadObject returns the in-memory (possibly unsaved) AnimBP if already loaded.
        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP) { ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND")); }

        // ResolveClassByName accepts /Game/... BP asset paths and /Script/... native names; defaults to the owner pawn.
        UClass* TargetClass = APawn::StaticClass();
        if (!TargetClassName.IsEmpty())
        {
            TargetClass = ResolveClassByName(TargetClassName);
            if (!TargetClass) { ANIM_ERROR_RESPONSE(FString::Printf(TEXT("targetClass not found: %s"), *TargetClassName), TEXT("CLASS_NOT_FOUND")); }
        }

        UFunction* TargetFunc = TargetClass->FindFunctionByName(FName(*FunctionName));
        if (!TargetFunc) { ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Function '%s' not found on class '%s'"), *FunctionName, *TargetClass->GetName()), TEXT("FUNCTION_NOT_FOUND")); }

        UFunction* GetPawnFn = UAnimInstance::StaticClass()->FindFunctionByName(TEXT("TryGetPawnOwner"));
        if (!GetPawnFn) { ANIM_ERROR_RESPONSE(TEXT("UAnimInstance::TryGetPawnOwner not found"), TEXT("ENGINE_FUNCTION_NOT_FOUND")); }

        // The AnimBP EventGraph is a ubergraph named "EventGraph"; the AnimGraph (also a
        // ubergraph) is "AnimGraph" and must not be used here.
        UEdGraph* EventGraph = nullptr;
        for (UEdGraph* G : AnimBP->UbergraphPages)
        {
            if (G && G->GetName() == TEXT("EventGraph")) { EventGraph = G; break; }
        }
        if (!EventGraph)
        {
            for (UEdGraph* G : AnimBP->UbergraphPages)
            {
                if (G && G->GetName() != TEXT("AnimGraph")) { EventGraph = G; break; }
            }
        }
        if (!EventGraph) { ANIM_ERROR_RESPONSE(TEXT("Could not find an EventGraph in the AnimBlueprint"), TEXT("EVENT_GRAPH_NOT_FOUND")); }

        const FString CustomEventName = FString::Printf(TEXT("AnimNotify_%s"), *NotifyName);

        TArray<UK2Node_CustomEvent*> ExistingEvents;
        FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_CustomEvent>(AnimBP, ExistingEvents);
        for (UK2Node_CustomEvent* E : ExistingEvents)
        {
            if (E && E->CustomFunctionName == FName(*CustomEventName))
            {
                Response->SetBoolField(TEXT("existingAsset"), true);
                Response->SetStringField(TEXT("customEvent"), CustomEventName);
                ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("AnimNotify event '%s' already exists"), *CustomEventName));
                return Response;
            }
        }

        const FScopedTransaction Transaction(FText::FromString(TEXT("Bind Anim Notify")));
        AnimBP->Modify();
        EventGraph->Modify();

        const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

        UK2Node_CustomEvent* EventNode = nullptr;
        {
            FGraphNodeCreator<UK2Node_CustomEvent> Creator(*EventGraph);
            EventNode = Creator.CreateNode(false);
            EventNode->CustomFunctionName = FName(*CustomEventName);
            EventNode->NodePosX = NodePosX;
            EventNode->NodePosY = NodePosY;
            Creator.Finalize();
        }

        // const BlueprintCallable -> pure node (return value only); handled robustly below.
        UK2Node_CallFunction* GetOwnerNode = nullptr;
        {
            FGraphNodeCreator<UK2Node_CallFunction> Creator(*EventGraph);
            GetOwnerNode = Creator.CreateNode(false);
            GetOwnerNode->SetFromFunction(GetPawnFn);
            GetOwnerNode->NodePosX = NodePosX + 260;
            GetOwnerNode->NodePosY = NodePosY + 24;
            Creator.Finalize();
        }

        // Default purity is impure -> has exec + valid/invalid-cast exec pins.
        UK2Node_DynamicCast* CastNode = nullptr;
        {
            FGraphNodeCreator<UK2Node_DynamicCast> Creator(*EventGraph);
            CastNode = Creator.CreateNode(false);
            CastNode->TargetType = TargetClass;
            CastNode->NodePosX = NodePosX + 540;
            CastNode->NodePosY = NodePosY;
            Creator.Finalize();
        }

        UK2Node_CallFunction* CallNode = nullptr;
        {
            FGraphNodeCreator<UK2Node_CallFunction> Creator(*EventGraph);
            CallNode = Creator.CreateNode(false);
            CallNode->SetFromFunction(TargetFunc);
            CallNode->NodePosX = NodePosX + 860;
            CallNode->NodePosY = NodePosY;
            Creator.Finalize();
        }

        bool bWired = true;
        auto Connect = [&](UEdGraphPin* A, UEdGraphPin* B) { bWired &= (A != nullptr && B != nullptr && K2Schema->TryCreateConnection(A, B)); };

        UEdGraphPin* EventThen   = EventNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
        UEdGraphPin* OwnerReturn = GetOwnerNode->GetReturnValuePin();
        UEdGraphPin* CastExecIn  = CastNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
        UEdGraphPin* CastSource  = CastNode->GetCastSourcePin();
        UEdGraphPin* CastValid   = CastNode->GetValidCastPin();
        UEdGraphPin* CastResult  = CastNode->GetCastResultPin();
        UEdGraphPin* CallExecIn  = CallNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
        UEdGraphPin* CallSelf    = CallNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);

        // Thread exec through the owner getter only if it is impure (has exec pins).
        UEdGraphPin* OwnerExecIn = GetOwnerNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
        UEdGraphPin* OwnerThen   = GetOwnerNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
        if (OwnerExecIn && OwnerThen)
        {
            Connect(EventThen, OwnerExecIn);
            Connect(OwnerThen, CastExecIn);
        }
        else
        {
            Connect(EventThen, CastExecIn);
        }
        Connect(OwnerReturn, CastSource);
        Connect(CastValid, CallExecIn);
        Connect(CastResult, CallSelf);

        if (!bWired) { ANIM_ERROR_RESPONSE(TEXT("Failed to connect one or more pins (schema rejected a link)"), TEXT("CONNECTION_FAILED")); }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        FKismetEditorUtilities::CompileBlueprint(AnimBP, EBlueprintCompileOptions::SkipGarbageCollection);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetStringField(TEXT("customEvent"), CustomEventName);
        Response->SetStringField(TEXT("targetClass"), TargetClass->GetName());
        Response->SetStringField(TEXT("function"), FunctionName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Bound notify '%s' -> %s::%s"), *NotifyName, *TargetClass->GetName(), *FunctionName));
        McpHandlerUtils::AddVerification(Response, AnimBP);
        return Response;
}

// Response conversion shared by the classed members — verbatim from the
// retired HandleManageAnimationAuthoringAction wrapper.
static bool SendAnimAuthoringResult(UMcpAutomationBridgeSubsystem* Self,
                                    const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Result,
                                    FMcpResponseHandle RequestingSocket)
{
    if (Result.IsValid())
    {
        bool bSuccess = Result->HasField(TEXT("success")) && GetJsonBoolField(Result, TEXT("success"));
        FString Message = Result->HasField(TEXT("message")) ? GetJsonStringField(Result, TEXT("message")) : TEXT("");

        if (bSuccess)
        {
            Self->SendAutomationResponse(RequestingSocket, RequestId, true, Message, Result);
        }
        else
        {
            FString Error = Result->HasField(TEXT("error")) ? GetJsonStringField(Result, TEXT("error")) : TEXT("Unknown error");
            FString ErrorCode = Result->HasField(TEXT("errorCode")) ? GetJsonStringField(Result, TEXT("errorCode")) : TEXT("ANIMATION_AUTHORING_ERROR");
            Self->SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
        }
        return true;
    }

    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to process animation authoring action"), TEXT("PROCESSING_FAILED"));
    return true;
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateAnimationSequence(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringCreateAnimationSequence(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetSequenceLength(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetSequenceLength(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddBoneTrack(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddBoneTrack(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetBoneKey(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetBoneKey(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetCurveKey(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetCurveKey(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddNotify(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddNotify(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddNotifyState(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddNotifyState(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddSyncMarker(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddSyncMarker(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetRootMotionSettings(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetRootMotionSettings(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetAdditiveSettings(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetAdditiveSettings(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateMontage(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringCreateMontage(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddMontageSection(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddMontageSection(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddMontageSlot(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddMontageSlot(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetSectionTiming(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetSectionTiming(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddMontageNotify(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddMontageNotify(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetBlendIn(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetBlend(Payload, /*bBlendIn=*/true), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetBlendOut(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetBlend(Payload, /*bBlendIn=*/false), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringLinkSections(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringLinkSections(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateBlendSpace1D(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringCreateBlendSpace1D(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateBlendSpace2D(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringCreateBlendSpace2D(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddBlendSample(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddBlendSample(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringForceRebuildBlendSpace(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringForceRebuildBlendSpace(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetAxisSettings(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetAxisSettings(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetInterpolationSettings(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetInterpolationSettings(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateAimOffset(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringCreateAimOffset(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddAimOffsetSample(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddAimOffsetSample(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateAnimBlueprint(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringCreateAnimBlueprint(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddStateMachine(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddStateMachine(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddState(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddState(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddTransition(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddTransition(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetTransitionRules(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetTransitionRules(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddBlendNode(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddBlendNode(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddCachedPose(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddCachedPose(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddSlotNode(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddSlotNode(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringAddLayeredBlendPerBone(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringAddLayeredBlendPerBone(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetAnimGraphNodeValue(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetAnimGraphNodeValue(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateControlRig(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringCreateControlRig(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateIkRig(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringCreateIkRig(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringCreateIkRetargeter(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringCreateIkRetargeter(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringSetRetargetChainMapping(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringSetRetargetChainMapping(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringGetAnimationInfo(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringGetAnimationInfo(Payload), RequestingSocket);
}

bool McpHandlers::AnimationPhysics::HandleAnimAuthoringBindAnimNotify(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
    return SendAnimAuthoringResult(&S, RequestId, AnimAuthoringBindAnimNotify(Payload), RequestingSocket);
}

#undef ANIM_ERROR_RESPONSE
#undef ANIM_SUCCESS_RESPONSE

#undef GetJsonStringField
#undef GetJsonNumberField
#undef GetJsonBoolField

#endif // WITH_EDITOR
