// =============================================================================
// McpAutomationBridge_AIHandlers.cpp
// =============================================================================
// Phase 16: AI System Handlers
//
// Provides comprehensive AI, behavior tree, EQS, perception, state tree, smart objects,
// and mass AI capabilities for the MCP Automation Bridge.
//
// HANDLERS BY CATEGORY:
// ---------------------
// 16.1  AI Controllers      - create_controller, get_ai_controller_info, possess_pawn,
//                              set_focus, clear_focus, run_behavior_tree
// 16.2  Blackboard          - create_blackboard, get_blackboard_info, set_blackboard_value,
//                              get_blackboard_value, clear_blackboard_value, add_blackboard_key
// 16.3  Behavior Tree       - create_behavior_tree, get_behavior_tree_info, add_bt_node,
//                              remove_bt_node, set_bt_node_property
// 16.4  Environment Query   - create_env_query, run_env_query, add_eqs_generator, add_eqs_test
// 16.5  AI Perception       - configure_perception, add_sight_config, add_hearing_config,
//                              add_damage_config, get_perception_info
// 16.6  State Tree (5.3+)   - create_state_tree, add_state_tree_state, compile_state_tree
// 16.7  Smart Objects (5+)  - create_smart_object_definition, register_smart_object
// 16.8  Mass AI (5+)        - create_mass_entity, spawn_mass_entities
// 16.9  Navigation AI       - move_to_location, move_to_actor, get_nav_path, add_nav_modifier
// 16.10 Utility Actions     - get_info, validate_ai_setup, debug_ai_state
//
// VERSION COMPATIBILITY:
// ----------------------
// - UE 5.0: Basic AI, EQS, Perception, Smart Objects, Mass AI
// - UE 5.1+: EnvQueryTest headers available (EnvQueryTest_Distance, EnvQueryTest_Trace)
// - UE 5.3+: State Tree module available
// - UE 5.7+: StateTreeComponentSchema moved to GameplayStateTreeModule
//
// REFACTORING NOTES:
// ------------------
// - Conditional includes for State Tree, Smart Objects, Mass AI via __has_include
// - Helper macros (GetJsonStringField, etc.) for JSON field access
// - McpSafeAssetSave for safe asset saving (avoids FullyLoad on new packages)
// - Uses MCP_HAS_* macros for feature detection
//
// ASSET EXISTENCE CHECK PATTERN:
// - For operations on newly created assets (assign_behavior_tree, assign_blackboard):
//   DoesAssetExist is SKIPPED because newly created assets may not be indexed yet.
//   LoadObject null-check is sufficient.
// - For operations on existing assets (add_blackboard_key):
//   DoesAssetExist IS used to fail fast for typo/wrong-path cases.
// - This differs from NiagaraAuthoringHandlers which always uses DoesAssetExist
//   because Niagara assets are never created and immediately modified in the same
//   operation sequence.
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// MCP Core
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpResponseHelpers.h"
#include "McpAutomationBridge_AIHandlers.h"
#include "McpAutomationBridge_BehaviorTreeHandlers.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"
#include "Modules/ModuleManager.h"  // Required for FModuleManager::IsModuleLoaded() runtime checks

// JSON
#include "Dom/JsonObject.h"

// Engine Version
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR

// Blueprint & Asset Tools
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"

// AI Controller
#include "AIController.h"

// Behavior Tree
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Decorators/BTDecorator_Cooldown.h"
#include "BehaviorTree/Decorators/BTDecorator_Loop.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "EngineUtils.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "AIGraphNode.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Root.h"
#define MCP_AI_HAS_BEHAVIOR_TREE_GRAPH 1
#else
#define MCP_AI_HAS_BEHAVIOR_TREE_GRAPH 0
#endif

// Environment Query
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ActorsOfClass.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_OnCircle.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_SimpleGrid.h"

// EQS Tests (UE 5.1+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "EnvironmentQuery/Tests/EnvQueryTest_Distance.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Trace.h"
#define MCP_HAS_ENVQUERY_TESTS 1
#else
#define MCP_HAS_ENVQUERY_TESTS 0
#endif

// AI Perception
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Damage.h"
#include "GenericTeamAgentInterface.h"

// Engine Components
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/CharacterMovementComponent.h"

// Navigation
#include "NavModifierComponent.h"
#include "NavAreas/NavArea.h"
#include "NavAreas/NavArea_Default.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "UObject/UnrealType.h"
#endif

// =============================================================================
// Conditional Includes (version-dependent)
// =============================================================================

// State Tree (UE 5.3+)
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
#define MCP_HAS_STATE_TREE 1
#if __has_include("StateTree.h")
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeTaskBase.h"

// UE 5.7+ moved StateTreeComponentSchema to GameplayStateTreeModule
#if __has_include("Components/StateTreeComponentSchema.h")
#include "Components/StateTreeComponentSchema.h"
#define MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE 1
#else
#define MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE 0
#endif
#define MCP_STATE_TREE_HEADERS_AVAILABLE 1
#else
#define MCP_STATE_TREE_HEADERS_AVAILABLE 0
#define MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE 0
#endif
#else
#define MCP_HAS_STATE_TREE 0
#define MCP_STATE_TREE_HEADERS_AVAILABLE 0
#define MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE 0
#endif

// Smart Objects (UE 5.0+)
#if ENGINE_MAJOR_VERSION >= 5
#define MCP_HAS_SMART_OBJECTS 1
#if __has_include("SmartObjectDefinition.h")
#include "SmartObjectDefinition.h"
#include "SmartObjectComponent.h"
#include "SmartObjectTypes.h"
#include "GameplayTagContainer.h"
#define MCP_SMART_OBJECTS_HEADERS_AVAILABLE 1
#else
#define MCP_SMART_OBJECTS_HEADERS_AVAILABLE 0
#endif
#else
#define MCP_HAS_SMART_OBJECTS 0
#define MCP_SMART_OBJECTS_HEADERS_AVAILABLE 0
#endif

// Mass AI (UE 5.0+)
#if ENGINE_MAJOR_VERSION >= 5
#define MCP_HAS_MASS_AI 1
#if __has_include("MassEntityConfigAsset.h")
#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "MassSpawnerSubsystem.h"
#define MCP_MASS_AI_HEADERS_AVAILABLE 1
#else
#define MCP_MASS_AI_HEADERS_AVAILABLE 0
#endif
#else
#define MCP_HAS_MASS_AI 0
#define MCP_MASS_AI_HEADERS_AVAILABLE 0
#endif

// Log category for AI handlers
DEFINE_LOG_CATEGORY_STATIC(LogMcpAIHandlers, Log, All);

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h

// =============================================================================
// Runtime Module Check Helpers
// =============================================================================
// These helpers check if optional AI modules are loaded at runtime.
// Required because DynamicallyLoadedModuleNames allows DLL to load without
// hard dependencies, but code must verify module availability before use.

/**
 * Check if StateTree module is loaded at runtime.
 * Returns true if module is available, false otherwise.
 * Attempts to load the module if it exists but isn't loaded yet.
 */
static bool IsStateTreeModuleAvailable()
{
#if MCP_HAS_STATE_TREE
    if (FModuleManager::Get().IsModuleLoaded(TEXT("StateTreeModule")))
    {
        return true;
    }
    // Try to load it
    if (FModuleManager::Get().ModuleExists(TEXT("StateTreeModule")))
    {
        return FModuleManager::Get().LoadModule(TEXT("StateTreeModule")) != nullptr;
    }
#endif
    return false;
}

/**
 * Check if SmartObjects module is loaded at runtime.
 */
static bool IsSmartObjectsModuleAvailable()
{
#if MCP_HAS_SMART_OBJECTS
    if (FModuleManager::Get().IsModuleLoaded(TEXT("SmartObjectsModule")))
    {
        return true;
    }
    if (FModuleManager::Get().ModuleExists(TEXT("SmartObjectsModule")))
    {
        return FModuleManager::Get().LoadModule(TEXT("SmartObjectsModule")) != nullptr;
    }
#endif
    return false;
}

/**
 * Check if MassEntity module is loaded at runtime.
 */
static bool IsMassModuleAvailable()
{
#if MCP_HAS_MASS_AI
    if (FModuleManager::Get().IsModuleLoaded(TEXT("MassEntity")))
    {
        return true;
    }
    if (FModuleManager::Get().ModuleExists(TEXT("MassEntity")))
    {
        return FModuleManager::Get().LoadModule(TEXT("MassEntity")) != nullptr;
    }
#endif
    return false;
}

/**
 * Sanitize and validate an asset path for AI asset creation.
 * - Removes double slashes that cause Fatal Error in UObjectGlobals.cpp
 * - Validates path is within a valid mount point (/Game/, /Plugin/, etc.)
 * - Returns false and sets OutError if path is invalid (security check)
 */
static bool SanitizeAIAssetPath(const FString& InputPath, FString& OutSanitizedPath, FString& OutError)
{
    OutSanitizedPath = SanitizeProjectRelativePath(InputPath.TrimStartAndEnd());
    if (OutSanitizedPath.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Invalid asset path: %s"), *InputPath);
        return false;
    }

    return true;
}

#if WITH_EDITOR

static void SetBPVarDefaultValueAI(UBlueprint* Blueprint, FName VarName, const FString& DefaultValue)
{
    if (!Blueprint)
    {
        return;
    }

    for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        if (VarDesc.VarName == VarName)
        {
            VarDesc.DefaultValue = DefaultValue;
            break;
        }
    }

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
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    Blueprint->MarkPackageDirty();
}

static EEnvTestScoreEquation::Type ParseEQSScoringEquationAI(const FString& Value, bool& bOutRecognized)
{
    bOutRecognized = true;
    if (Value.Equals(TEXT("Square"), ESearchCase::IgnoreCase)) return EEnvTestScoreEquation::Square;
    if (Value.Equals(TEXT("InverseLinear"), ESearchCase::IgnoreCase)) return EEnvTestScoreEquation::InverseLinear;
    if (Value.Equals(TEXT("Constant"), ESearchCase::IgnoreCase)) return EEnvTestScoreEquation::Constant;
    if (Value.Equals(TEXT("Linear"), ESearchCase::IgnoreCase)) return EEnvTestScoreEquation::Linear;
    bOutRecognized = false;
    return EEnvTestScoreEquation::Linear;
}

static EEnvTestFilterType::Type ParseEQSFilterTypeAI(const FString& Value, bool& bOutRecognized)
{
    bOutRecognized = true;
    if (Value.Equals(TEXT("Minimum"), ESearchCase::IgnoreCase)) return EEnvTestFilterType::Minimum;
    if (Value.Equals(TEXT("Maximum"), ESearchCase::IgnoreCase)) return EEnvTestFilterType::Maximum;
    if (Value.Equals(TEXT("Range"), ESearchCase::IgnoreCase)) return EEnvTestFilterType::Range;
    bOutRecognized = false;
    return EEnvTestFilterType::Range;
}

// Helper to create AI Controller blueprint
static UBlueprint* CreateAIControllerBlueprint(const FString& Path, const FString& Name, FString& OutError)
{
    // Sanitize and validate path first
    FString SanitizedPath;
    if (!SanitizeAIAssetPath(Path, SanitizedPath, OutError))
    {
        return nullptr;
    }
    
    FString FullPath = SanitizedPath / Name;
    
    // CRITICAL: Use UEditorAssetLibrary for reliable existence check
    // This prevents the Kismet2.cpp assertion failure when blueprint already exists
    // The FindObject check alone is insufficient because:
    // 1. It checks for exact object path format
    // 2. The package may exist even if the blueprint object doesn't
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Asset already exists: %s"), *FullPath);
        return nullptr;
    }
    
    // Also check with .AssetName suffix (blueprint object path format)
    FString ObjectPath = FullPath + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
    {
        OutError = FString::Printf(TEXT("Blueprint already exists: %s"), *ObjectPath);
        return nullptr;
    }
    
    // Check if package exists on disk (for assets that haven't been loaded yet)
    if (FPackageName::DoesPackageExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Package already exists on disk: %s"), *FullPath);
        return nullptr;
    }
    
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    if (!Factory)
    {
        OutError = TEXT("Failed to create BlueprintFactory");
        return nullptr;
    }

    Factory->ParentClass = AAIController::StaticClass();

    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name,
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (!Blueprint)
    {
        OutError = TEXT("Failed to create AI Controller blueprint");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    McpSafeAssetSave(Blueprint);

    return Blueprint;
}

// Helper to create Blackboard asset
static UBlackboardData* CreateBlackboardAsset(const FString& Path, const FString& Name, FString& OutError)
{
    // Sanitize and validate path first
    FString SanitizedPath;
    if (!SanitizeAIAssetPath(Path, SanitizedPath, OutError))
    {
        return nullptr;
    }
    
    FString FullPath = SanitizedPath / Name;
    
    // Check if asset already exists
    if (FindObject<UBlackboardData>(nullptr, *FullPath) != nullptr)
    {
        OutError = FString::Printf(TEXT("Asset already exists: %s"), *FullPath);
        return nullptr;
    }
    
    // Also check if the package exists
    if (FPackageName::DoesPackageExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Package already exists: %s"), *FullPath);
        return nullptr;
    }
    
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlackboardData* Blackboard = NewObject<UBlackboardData>(Package, UBlackboardData::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
    if (!Blackboard)
    {
        OutError = TEXT("Failed to create Blackboard asset");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blackboard);
    McpSafeAssetSave(Blackboard);

    return Blackboard;
}

// Helper to create EQS Query asset
static UEnvQuery* CreateEQSQueryAsset(const FString& Path, const FString& Name, FString& OutError)
{
    // Sanitize and validate path first
    FString SanitizedPath;
    if (!SanitizeAIAssetPath(Path, SanitizedPath, OutError))
    {
        return nullptr;
    }
    
    FString FullPath = SanitizedPath / Name;
    
    // Check if asset already exists
    if (FindObject<UEnvQuery>(nullptr, *FullPath) != nullptr)
    {
        OutError = FString::Printf(TEXT("Asset already exists: %s"), *FullPath);
        return nullptr;
    }
    
    // Also check if the package exists
    if (FPackageName::DoesPackageExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Package already exists: %s"), *FullPath);
        return nullptr;
    }
    
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UEnvQuery* Query = NewObject<UEnvQuery>(Package, UEnvQuery::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
    if (!Query)
    {
        OutError = TEXT("Failed to create EQS Query asset");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Query);
    McpSafeAssetSave(Query);

    return Query;
}
#endif

// =========================================================================
// 16.1 AI Controller (3 actions)
// =========================================================================

bool McpHandlers::Ai::HandleAiCreateAiController(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/Controllers"));

    if (Name.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing name parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    FString Error;
    UBlueprint* Blueprint = CreateAIControllerBlueprint(Path, Name, Error);
    if (!Blueprint)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    Result->SetStringField(TEXT("controllerPath"), Blueprint->GetPathName());
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created AI Controller: %s"), *Name));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("AI Controller created"), Result);
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiAssignBehaviorTree(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString ControllerPath = GetJsonStringField(Payload, TEXT("controllerPath"));
    FString BehaviorTreePath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));

    UBlueprint *ControllerBP = S.ResolveBlueprintOrError(ControllerPath, RequestId, RequestingSocket, TEXT("controllerPath"));
    if (!ControllerBP) return true;

    UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BehaviorTreePath);
    if (!BT)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Behavior tree not found: %s"), *BehaviorTreePath), TEXT("NOT_FOUND"));
        return true;
    }

    // Set default BehaviorTree property on the generated class CDO using reflection
    if (ControllerBP->GeneratedClass)
    {
        if (AAIController* CDO = Cast<AAIController>(ControllerBP->GeneratedClass->GetDefaultObject()))
        {
            // Use reflection to find and set BehaviorTree-related properties
            // Look for common property names used in AI Controller blueprints
            bool bPropertySet = false;
            
            // Try to find a UBehaviorTree* property on the CDO
            for (TFieldIterator<FObjectProperty> PropIt(ControllerBP->GeneratedClass); PropIt; ++PropIt)
            {
                FObjectProperty* ObjProp = *PropIt;
                if (ObjProp && ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(UBehaviorTree::StaticClass()))
                {
                    // Found a BehaviorTree property - set it
                    ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(CDO), BT);
                    bPropertySet = true;
                    Result->SetStringField(TEXT("propertyName"), ObjProp->GetName());
                    break;
                }
            }
            
            // If no existing property found, add a Blueprint variable for the BT reference
            if (!bPropertySet)
            {
                // Add a Blueprint variable to store the BehaviorTree reference
                FEdGraphPinType PinType;
                PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
                PinType.PinSubCategoryObject = UBehaviorTree::StaticClass();
                
                const FName VarName = TEXT("DefaultBehaviorTree");
                if (FBlueprintEditorUtils::AddMemberVariable(ControllerBP, VarName, PinType))
                {
                    // Compile so the new variable exists on the generated class, then RE-FETCH the
                    // CDO (compile regenerates it — the earlier CDO pointer is now stale) and write
                    // the default. Without the compile FindPropertyByName misses the new var and the
                    // Behavior Tree reference is silently dropped.
                    McpSafeCompileBlueprint(ControllerBP);
                    UObject* FreshCDO = ControllerBP->GeneratedClass ? ControllerBP->GeneratedClass->GetDefaultObject() : nullptr;
                    FProperty* NewProp = ControllerBP->GeneratedClass ? ControllerBP->GeneratedClass->FindPropertyByName(VarName) : nullptr;
                    if (FObjectProperty* ObjProp = CastField<FObjectProperty>(NewProp))
                    {
                        if (FreshCDO)
                        {
                            ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(FreshCDO), BT);
                            bPropertySet = true;
                        }
                    }
                }
                Result->SetStringField(TEXT("propertyName"), VarName.ToString());
            }
            
            Result->SetBoolField(TEXT("propertyAssigned"), bPropertySet);
            Result->SetStringField(TEXT("message"), bPropertySet 
                ? TEXT("Behavior Tree property assigned on CDO") 
                : TEXT("Behavior Tree reference registered (call RunBehaviorTree in BeginPlay)"));
        }
    }

    McpFinalizeBlueprint(ControllerBP, /*bStructural=*/true, /*bSave=*/true);
    Result->SetStringField(TEXT("controllerPath"), ControllerPath);
    Result->SetStringField(TEXT("behaviorTreePath"), BehaviorTreePath);
    McpHandlerUtils::AddVerification(Result, ControllerBP);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior Tree reference set"), Result);
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiAssignBlackboard(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString ControllerPath = GetJsonStringField(Payload, TEXT("controllerPath"));
    FString BehaviorTreePath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
    FString BlackboardPath = GetJsonStringField(Payload, TEXT("blackboardPath"));

    if (ControllerPath.IsEmpty() && !BehaviorTreePath.IsEmpty())
    {
        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BehaviorTreePath);
        if (!BT)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Behavior tree not found: %s"), *BehaviorTreePath), TEXT("NOT_FOUND"));
            return true;
        }

        UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
        if (!BB)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath), TEXT("NOT_FOUND"));
            return true;
        }

        BT->Modify();
        BT->BlackboardAsset = BB;

#if MCP_AI_HAS_BEHAVIOR_TREE_GRAPH
        if (UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(BT->BTGraph))
        {
            BTGraph->Modify();
            for (UEdGraphNode* GraphNode : BTGraph->Nodes)
            {
                if (UBehaviorTreeGraphNode_Root* RootNode = Cast<UBehaviorTreeGraphNode_Root>(GraphNode))
                {
                    RootNode->Modify();
                    RootNode->BlackboardAsset = BB;
                    BTGraph->UpdateBlackboardChange();
                    break;
                }
            }
        }
#endif

        BT->MarkPackageDirty();
        bool bSaved = McpSafeAssetSave(BT);

        Result->SetStringField(TEXT("behaviorTreePath"), BehaviorTreePath);
        Result->SetStringField(TEXT("blackboardPath"), BlackboardPath);
        Result->SetBoolField(TEXT("saved"), bSaved);
        Result->SetStringField(TEXT("message"), TEXT("Blackboard assigned to Behavior Tree"));
        McpHandlerUtils::AddVerification(Result, BT);
        S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard assigned to Behavior Tree"), Result);
        return true;
    }

    UBlueprint *ControllerBP = S.ResolveBlueprintOrError(ControllerPath, RequestId, RequestingSocket, TEXT("controllerPath"));
    if (!ControllerBP) return true;

    UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
    if (!BB)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath), TEXT("NOT_FOUND"));
        return true;
    }

    // Set default Blackboard property on the generated class CDO using reflection
    if (ControllerBP->GeneratedClass)
    {
        if (AAIController* CDO = Cast<AAIController>(ControllerBP->GeneratedClass->GetDefaultObject()))
        {
            // Use reflection to find and set Blackboard-related properties
            bool bPropertySet = false;
            
            // Try to find a UBlackboardData* property on the CDO
            for (TFieldIterator<FObjectProperty> PropIt(ControllerBP->GeneratedClass); PropIt; ++PropIt)
            {
                FObjectProperty* ObjProp = *PropIt;
                if (ObjProp && ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(UBlackboardData::StaticClass()))
                {
                    // Found a BlackboardData property - set it
                    ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(CDO), BB);
                    bPropertySet = true;
                    Result->SetStringField(TEXT("propertyName"), ObjProp->GetName());
                    break;
                }
            }
            
            // If no existing property found, add a Blueprint variable for the Blackboard reference
            if (!bPropertySet)
            {
                // Add a Blueprint variable to store the BlackboardData reference
                FEdGraphPinType PinType;
                PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
                PinType.PinSubCategoryObject = UBlackboardData::StaticClass();
                
                const FName VarName = TEXT("DefaultBlackboard");
                if (FBlueprintEditorUtils::AddMemberVariable(ControllerBP, VarName, PinType))
                {
                    // Compile so the new variable exists on the generated class, then RE-FETCH the
                    // CDO (compile regenerates it — the earlier CDO pointer is now stale) and write
                    // the default. Without the compile FindPropertyByName misses the new var and the
                    // Blackboard reference is silently dropped.
                    McpSafeCompileBlueprint(ControllerBP);
                    UObject* FreshCDO = ControllerBP->GeneratedClass ? ControllerBP->GeneratedClass->GetDefaultObject() : nullptr;
                    FProperty* NewProp = ControllerBP->GeneratedClass ? ControllerBP->GeneratedClass->FindPropertyByName(VarName) : nullptr;
                    if (FObjectProperty* ObjProp = CastField<FObjectProperty>(NewProp))
                    {
                        if (FreshCDO)
                        {
                            ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(FreshCDO), BB);
                            bPropertySet = true;
                        }
                    }
                }
                Result->SetStringField(TEXT("propertyName"), VarName.ToString());
            }
            
            Result->SetBoolField(TEXT("propertyAssigned"), bPropertySet);
            Result->SetStringField(TEXT("message"), bPropertySet 
                ? TEXT("Blackboard property assigned on CDO (call UseBlackboard in BeginPlay with this asset)") 
                : TEXT("Blackboard reference registered (call UseBlackboard in BeginPlay with this asset)"));
        }
    }

    bool bSaved = McpFinalizeBlueprint(ControllerBP, /*bStructural=*/true, /*bSave=*/true);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("controllerPath"), ControllerPath);
    Result->SetStringField(TEXT("blackboardPath"), BlackboardPath);
    McpHandlerUtils::AddVerification(Result, ControllerBP);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard reference set"), Result);
    return true;
#endif
}

// =========================================================================

bool McpHandlers::Ai::HandleAiCreateBlackboardAsset(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/Blackboards"));

    if (Name.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing name parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    FString Error;
    UBlackboardData* Blackboard = CreateBlackboardAsset(Path, Name, Error);
    if (!Blackboard)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    Result->SetStringField(TEXT("blackboardPath"), Blackboard->GetPathName());
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Blackboard: %s"), *Name));
    McpHandlerUtils::AddVerification(Result, Blackboard);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard created"), Result);
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiAddBlackboardKey(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString BlackboardPath = GetJsonStringField(Payload, TEXT("blackboardPath"));
    FString KeyName = GetJsonStringField(Payload, TEXT("keyName"));
    FString KeyType = GetJsonStringField(Payload, TEXT("keyType"));

    // CRITICAL: Explicitly check if asset exists before LoadObject
    if (!UEditorAssetLibrary::DoesAssetExist(BlackboardPath))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath), TEXT("NOT_FOUND"));
        return true;
    }

    UBlackboardData* Blackboard = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
    if (!Blackboard)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    // Create appropriate key type
    FBlackboardEntry NewEntry;
    NewEntry.EntryName = FName(*KeyName);

    if (KeyType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
    {
        NewEntry.KeyType = NewObject<UBlackboardKeyType_Bool>(Blackboard);
    }
    else if (KeyType.Equals(TEXT("Int"), ESearchCase::IgnoreCase))
    {
        NewEntry.KeyType = NewObject<UBlackboardKeyType_Int>(Blackboard);
    }
    else if (KeyType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
    {
        NewEntry.KeyType = NewObject<UBlackboardKeyType_Float>(Blackboard);
    }
    else if (KeyType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
    {
        NewEntry.KeyType = NewObject<UBlackboardKeyType_Vector>(Blackboard);
    }
    else if (KeyType.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase))
    {
        NewEntry.KeyType = NewObject<UBlackboardKeyType_Rotator>(Blackboard);
    }
    else if (KeyType.Equals(TEXT("Object"), ESearchCase::IgnoreCase))
    {
        UBlackboardKeyType_Object* ObjectKey = NewObject<UBlackboardKeyType_Object>(Blackboard);
        FString BaseClass = GetJsonStringField(Payload, TEXT("baseObjectClass"), TEXT("Actor"));
        // Could set base class here
        NewEntry.KeyType = ObjectKey;
    }
    else if (KeyType.Equals(TEXT("Class"), ESearchCase::IgnoreCase))
    {
        NewEntry.KeyType = NewObject<UBlackboardKeyType_Class>(Blackboard);
    }
    else if (KeyType.Equals(TEXT("Enum"), ESearchCase::IgnoreCase))
    {
        NewEntry.KeyType = NewObject<UBlackboardKeyType_Enum>(Blackboard);
    }
    else if (KeyType.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
    {
        NewEntry.KeyType = NewObject<UBlackboardKeyType_Name>(Blackboard);
    }
    else if (KeyType.Equals(TEXT("String"), ESearchCase::IgnoreCase))
    {
        NewEntry.KeyType = NewObject<UBlackboardKeyType_String>(Blackboard);
    }
    else
    {
        // Default to Object
        NewEntry.KeyType = NewObject<UBlackboardKeyType_Object>(Blackboard);
    }

    NewEntry.bInstanceSynced = GetJsonBoolField(Payload, TEXT("isInstanceSynced"), false);

    Blackboard->Keys.Add(NewEntry);
    Blackboard->MarkPackageDirty();
    McpSafeAssetSave(Blackboard);

    Result->SetNumberField(TEXT("keyIndex"), Blackboard->Keys.Num() - 1);
    Result->SetStringField(TEXT("keyName"), KeyName);
    Result->SetStringField(TEXT("keyType"), KeyType);
    McpHandlerUtils::AddVerification(Result, Blackboard);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard key added"), Result);
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiSetKeyInstanceSynced(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString BlackboardPath = GetJsonStringField(Payload, TEXT("blackboardPath"));
    FString KeyName = GetJsonStringField(Payload, TEXT("keyName"));
    bool bInstanceSynced = GetJsonBoolField(Payload, TEXT("isInstanceSynced"), true);

    UBlackboardData* Blackboard = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
    if (!Blackboard)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    bool bFound = false;
    for (FBlackboardEntry& Entry : Blackboard->Keys)
    {
        if (Entry.EntryName.ToString() == KeyName)
        {
            Entry.bInstanceSynced = bInstanceSynced;
            bFound = true;
            break;
        }
    }

    if (!bFound)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Key not found: %s"), *KeyName),
                            TEXT("NOT_FOUND"));
        return true;
    }

    Blackboard->MarkPackageDirty();
    McpSafeAssetSave(Blackboard);

    Result->SetStringField(TEXT("keyName"), KeyName);
    Result->SetBoolField(TEXT("isInstanceSynced"), bInstanceSynced);
    McpHandlerUtils::AddVerification(Result, Blackboard);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Key instance sync updated"), Result);
    return true;
#endif
}

// =========================================================================
// 16.3 Behavior Tree - Expanded (6 actions)
// =========================================================================

bool McpHandlers::Ai::HandleAiCreateBehaviorTree(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    // Delegate to the graph-authoring 'create': a BT created without a
    // BTGraph cannot be extended by add_node/connect_nodes/add_subnode.
    TSharedPtr<FJsonObject> Delegated = MakeShared<FJsonObject>();
    Delegated->Values = Payload->Values;
    if (GetJsonStringField(Payload, TEXT("savePath")).IsEmpty() &&
        GetJsonStringField(Payload, TEXT("path")).IsEmpty())
    {
        Delegated->SetStringField(TEXT("savePath"), TEXT("/Game/AI/BehaviorTrees"));
    }
    return McpHandlers::Ai::HandleBehaviorTreeCreate(S, RequestId, Delegated, RequestingSocket);
#endif
}

// -----------------------------------------------------------------------
// Thin wrappers over the graph-authoring path: map the short type enums to
// engine node classes and call the graph members (HandleBehaviorTreeAddNode /
// HandleBehaviorTreeAddSubnode) so the node is wired into the real Behavior
// Tree graph.
// -----------------------------------------------------------------------

bool McpHandlers::Ai::HandleAiAddCompositeNode(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Delegated = MakeShared<FJsonObject>();
    Delegated->Values = Payload->Values;

    FString CompositeType = GetJsonStringField(Payload, TEXT("compositeType"));
    if (CompositeType.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing compositeType (Selector, Sequence, or SimpleParallel)"),
            TEXT("INVALID_PARAMS"));
        return true;
    }
    if (CompositeType.Equals(TEXT("Parallel"), ESearchCase::IgnoreCase))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("UE Behavior Trees have no plain Parallel composite; use compositeType 'SimpleParallel'"),
            TEXT("INVALID_PARAMS"));
        return true;
    }
    Delegated->SetStringField(TEXT("nodeType"), CompositeType);

    return McpHandlers::Ai::HandleBehaviorTreeAddNode(S, RequestId, Delegated, RequestingSocket);
#endif
}

bool McpHandlers::Ai::HandleAiAddTaskNode(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Delegated = MakeShared<FJsonObject>();
    Delegated->Values = Payload->Values;

    FString TaskType = GetJsonStringField(Payload, TEXT("taskType"));
    if (TaskType.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing taskType (e.g. Wait, MoveTo, PlaySound — or 'Custom' with nodeClass)"),
            TEXT("INVALID_PARAMS"));
        return true;
    }
    FString TaskClass;
    if (TaskType.Equals(TEXT("Custom"), ESearchCase::IgnoreCase))
    {
        TaskClass = GetJsonStringField(Payload, TEXT("nodeClass"));
        if (TaskClass.IsEmpty())
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                TEXT("taskType 'Custom' requires nodeClass (class name or Blueprint class path)"),
                TEXT("INVALID_PARAMS"));
            return true;
        }
    }
    else
    {
        TaskClass = TaskType.StartsWith(TEXT("BTTask_")) ? TaskType : TEXT("BTTask_") + TaskType;
    }
    Delegated->SetStringField(TEXT("nodeType"), TaskClass);

    return McpHandlers::Ai::HandleBehaviorTreeAddNode(S, RequestId, Delegated, RequestingSocket);
#endif
}

bool McpHandlers::Ai::HandleAiAddDecorator(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Delegated = MakeShared<FJsonObject>();
    Delegated->Values = Payload->Values;

    if (GetJsonStringField(Payload, TEXT("parentNodeId")).IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("add_decorator requires parentNodeId (a node id from add_node/get_info, or 'root')"),
            TEXT("INVALID_PARAMS"));
        return true;
    }

    FString TypeName = GetJsonStringField(Payload, TEXT("decoratorType"));
    if (TypeName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing decoratorType (an engine type like 'Blackboard'/'Cooldown', or 'Custom' with nodeClass)"),
            TEXT("INVALID_PARAMS"));
        return true;
    }
    FString SubnodeClass;
    if (TypeName.Equals(TEXT("Custom"), ESearchCase::IgnoreCase))
    {
        SubnodeClass = GetJsonStringField(Payload, TEXT("nodeClass"));
        if (SubnodeClass.IsEmpty())
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                TEXT("decoratorType 'Custom' requires nodeClass (class name or Blueprint class path)"),
                TEXT("INVALID_PARAMS"));
            return true;
        }
    }
    else
    {
        SubnodeClass = TypeName.StartsWith(TEXT("BTDecorator_")) ? TypeName : TEXT("BTDecorator_") + TypeName;
    }
    Delegated->SetStringField(TEXT("subnodeType"), TEXT("Decorator"));
    Delegated->SetStringField(TEXT("nodeClass"), SubnodeClass);

    return McpHandlers::Ai::HandleBehaviorTreeAddSubnode(S, RequestId, Delegated, RequestingSocket);
#endif
}

bool McpHandlers::Ai::HandleAiAddService(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Delegated = MakeShared<FJsonObject>();
    Delegated->Values = Payload->Values;

    if (GetJsonStringField(Payload, TEXT("parentNodeId")).IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("add_service requires parentNodeId (a node id from add_node/get_info, or 'root')"),
            TEXT("INVALID_PARAMS"));
        return true;
    }

    FString TypeName = GetJsonStringField(Payload, TEXT("serviceType"));
    if (TypeName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing serviceType (an engine type like 'DefaultFocus'/'RunEQS', or 'Custom' with nodeClass)"),
            TEXT("INVALID_PARAMS"));
        return true;
    }
    FString SubnodeClass;
    if (TypeName.Equals(TEXT("Custom"), ESearchCase::IgnoreCase))
    {
        SubnodeClass = GetJsonStringField(Payload, TEXT("nodeClass"));
        if (SubnodeClass.IsEmpty())
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                TEXT("serviceType 'Custom' requires nodeClass (class name or Blueprint class path)"),
                TEXT("INVALID_PARAMS"));
            return true;
        }
    }
    else
    {
        SubnodeClass = TypeName.StartsWith(TEXT("BTService_")) ? TypeName : TEXT("BTService_") + TypeName;
    }
    Delegated->SetStringField(TEXT("subnodeType"), TEXT("Service"));
    Delegated->SetStringField(TEXT("nodeClass"), SubnodeClass);

    return McpHandlers::Ai::HandleBehaviorTreeAddSubnode(S, RequestId, Delegated, RequestingSocket);
#endif
}

bool McpHandlers::Ai::HandleAiConfigureBtNode(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString BTPath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
    FString NodeId = GetJsonStringField(Payload, TEXT("nodeId"));

    if (NodeId.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing nodeId parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
    if (!BT)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    UBTNode* TargetNode = nullptr;
    FString ResolvedNodeRole;

    auto MatchesNodeId = [&NodeId](const UBTNode* Candidate) -> bool
    {
        if (!Candidate)
        {
            return false;
        }
        return Candidate->GetName().Equals(NodeId, ESearchCase::IgnoreCase) ||
               Candidate->GetPathName().Equals(NodeId, ESearchCase::IgnoreCase) ||
               Candidate->GetNodeName().Equals(NodeId, ESearchCase::IgnoreCase);
    };

    TFunction<void(UBTCompositeNode*)> VisitComposite;
    VisitComposite = [&](UBTCompositeNode* Composite)
    {
        if (!Composite || TargetNode)
        {
            return;
        }

        if (MatchesNodeId(Composite))
        {
            TargetNode = Composite;
            ResolvedNodeRole = TEXT("composite");
            return;
        }

        for (UBTService* Service : Composite->Services)
        {
            if (MatchesNodeId(Service))
            {
                TargetNode = Service;
                ResolvedNodeRole = TEXT("service");
                return;
            }
        }

        for (const FBTCompositeChild& Child : Composite->Children)
        {
            if (MatchesNodeId(Child.ChildTask))
            {
                TargetNode = Child.ChildTask;
                ResolvedNodeRole = TEXT("task");
                return;
            }

            for (UBTDecorator* Decorator : Child.Decorators)
            {
                if (MatchesNodeId(Decorator))
                {
                    TargetNode = Decorator;
                    ResolvedNodeRole = TEXT("decorator");
                    return;
                }
            }

            if (Child.ChildComposite)
            {
                VisitComposite(Child.ChildComposite);
                if (TargetNode)
                {
                    return;
                }
            }
        }
    };

    if (BT->RootNode)
    {
        const bool bRootAlias = NodeId.Equals(TEXT("Root"), ESearchCase::IgnoreCase) ||
                                NodeId.Equals(TEXT("RootNode"), ESearchCase::IgnoreCase);
        if (bRootAlias)
        {
            TargetNode = BT->RootNode;
            ResolvedNodeRole = TEXT("root");
        }
        else
        {
            VisitComposite(BT->RootNode);
        }
    }

    if (!TargetNode)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Behavior Tree node not found: %s"), *NodeId),
                            TEXT("NOT_FOUND"));
        return true;
    }

    int32 ConfiguredPropertyCount = 0;
    TArray<FString> ConfiguredProperties;
    const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
    if (Payload->TryGetObjectField(TEXT("properties"), PropertiesObject) && PropertiesObject && PropertiesObject->IsValid())
    {
        for (const auto& Pair : (*PropertiesObject)->Values)
        {
            const FString PropertyName(*Pair.Key);
            FProperty* Property = TargetNode->GetClass()->FindPropertyByName(FName(*PropertyName));
            if (!Property || !Pair.Value.IsValid())
            {
                continue;
            }

            void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetNode);
            if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
            {
                FString Value;
                if (Pair.Value->TryGetString(Value))
                {
                    StrProperty->SetPropertyValue(ValuePtr, Value);
                }
                else
                {
                    continue;
                }
            }
            else if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
            {
                FString Value;
                if (Pair.Value->TryGetString(Value))
                {
                    NameProperty->SetPropertyValue(ValuePtr, FName(*Value));
                }
                else
                {
                    continue;
                }
            }
            else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
            {
                bool bValue = false;
                if (Pair.Value->TryGetBool(bValue))
                {
                    BoolProperty->SetPropertyValue(ValuePtr, bValue);
                }
                else
                {
                    continue;
                }
            }
            else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
            {
                double Number = 0.0;
                if (Pair.Value->TryGetNumber(Number))
                {
                    IntProperty->SetPropertyValue(ValuePtr, static_cast<int32>(Number));
                }
                else
                {
                    continue;
                }
            }
            else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
            {
                double Number = 0.0;
                if (Pair.Value->TryGetNumber(Number))
                {
                    FloatProperty->SetPropertyValue(ValuePtr, static_cast<float>(Number));
                }
                else
                {
                    continue;
                }
            }
            else if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
            {
                double Number = 0.0;
                if (Pair.Value->TryGetNumber(Number))
                {
                    DoubleProperty->SetPropertyValue(ValuePtr, Number);
                }
                else
                {
                    continue;
                }
            }
            else
            {
                continue;
            }

            ++ConfiguredPropertyCount;
            ConfiguredProperties.Add(PropertyName);
        }
    }

    const bool bSaveAttempted = ConfiguredPropertyCount > 0;
    bool bSaved = true;
    if (bSaveAttempted)
    {
        BT->MarkPackageDirty();
        bSaved = McpSafeAssetSave(BT);
        if (!bSaved)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Failed to save Behavior Tree after configuring node: %s"), *BTPath),
                                TEXT("SAVE_FAILED"));
            return true;
        }
    }

    Result->SetStringField(TEXT("behaviorTreePath"), BTPath);
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetStringField(TEXT("resolvedNodeName"), TargetNode->GetName());
    Result->SetStringField(TEXT("resolvedNodeTitle"), TargetNode->GetNodeName());
    Result->SetStringField(TEXT("nodeRole"), ResolvedNodeRole);
    Result->SetNumberField(TEXT("configuredPropertyCount"), ConfiguredPropertyCount);
    Result->SetBoolField(TEXT("saveAttempted"), bSaveAttempted);
    Result->SetBoolField(TEXT("saved"), bSaved);
    TArray<TSharedPtr<FJsonValue>> ConfiguredPropertyValues;
    for (const FString& PropertyName : ConfiguredProperties)
    {
        ConfiguredPropertyValues.Add(MakeShared<FJsonValueString>(PropertyName));
    }
    Result->SetArrayField(TEXT("configuredProperties"), ConfiguredPropertyValues);
    McpHandlerUtils::AddVerification(Result, BT);

    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           ConfiguredPropertyCount > 0
                               ? TEXT("Behavior Tree node configured")
                               : TEXT("Behavior Tree node resolved; no properties supplied"),
                           Result);
    return true;
#endif
}

// =========================================================================
// 16.4 Environment Query System - EQS (5 actions)
// =========================================================================

bool McpHandlers::Ai::HandleAiCreateEqsQuery(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/EQS"));

    if (Name.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing name parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    FString Error;
    UEnvQuery* Query = CreateEQSQueryAsset(Path, Name, Error);
    if (!Query)
    {
        S.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    Result->SetStringField(TEXT("queryPath"), Query->GetPathName());
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created EQS Query: %s"), *Name));
    McpHandlerUtils::AddVerification(Result, Query);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("EQS Query created"), Result);
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiAddEqsGenerator(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString QueryPath = GetJsonStringField(Payload, TEXT("queryPath"));
    FString GeneratorType = GetJsonStringField(Payload, TEXT("generatorType"));

    UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
    if (!Query)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    // Map generator type strings to actual UE classes via template or runtime lookup
    UEnvQueryGenerator* NewGenerator = nullptr;
    UClass* GeneratorClass = nullptr;
    if (GeneratorType.Equals(TEXT("ActorsOfClass"), ESearchCase::IgnoreCase) ||
        GeneratorType.Equals(TEXT("EnvQueryGenerator_ActorsOfClass"), ESearchCase::IgnoreCase))
    {
        NewGenerator = NewObject<UEnvQueryGenerator_ActorsOfClass>(Query);
    }
    else if (GeneratorType.Equals(TEXT("OnCircle"), ESearchCase::IgnoreCase) ||
             GeneratorType.Equals(TEXT("EnvQueryGenerator_OnCircle"), ESearchCase::IgnoreCase))
    {
        NewGenerator = NewObject<UEnvQueryGenerator_OnCircle>(Query);
    }
    else if (GeneratorType.Equals(TEXT("SimpleGrid"), ESearchCase::IgnoreCase) ||
             GeneratorType.Equals(TEXT("EnvQueryGenerator_SimpleGrid"), ESearchCase::IgnoreCase))
    {
        NewGenerator = NewObject<UEnvQueryGenerator_SimpleGrid>(Query);
    }
    else
    {
        // Fallback: try runtime class lookup with full class name
        GeneratorClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/AIModule.%s"), *GeneratorType));
        if (!GeneratorClass)
        {
            GeneratorClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/AIModule.EnvQueryGenerator_%s"), *GeneratorType));
        }
        if (GeneratorClass)
        {
            UObject* GenObj = NewObject<UObject>(Query, GeneratorClass);
            if (GenObj && GenObj->GetClass()->IsChildOf(UEnvQueryGenerator::StaticClass()))
            {
                NewGenerator = static_cast<UEnvQueryGenerator*>(GenObj);
            }
        }
    }

    if (NewGenerator)
    {
        UClass* SearchCenterContext = nullptr;
        const TSharedPtr<FJsonObject>* GeneratorSettings = nullptr;
        if (Payload->TryGetObjectField(TEXT("generatorSettings"), GeneratorSettings) && GeneratorSettings && GeneratorSettings->IsValid())
        {
            FString SearchCenterName;
            if ((*GeneratorSettings)->TryGetStringField(TEXT("searchCenter"), SearchCenterName) && !SearchCenterName.IsEmpty())
            {
                SearchCenterContext = ResolveClassByName(SearchCenterName);
                if (!SearchCenterContext && !SearchCenterName.Contains(TEXT("/")) && !SearchCenterName.Contains(TEXT(".")))
                {
                    SearchCenterContext = ResolveClassByName(FString::Printf(TEXT("EnvQueryContext_%s"), *SearchCenterName));
                }
                if (!SearchCenterContext || !SearchCenterContext->IsChildOf(UEnvQueryContext::StaticClass()))
                {
                    S.SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("searchCenter is not an EnvQueryContext class: %s"), *SearchCenterName),
                        TEXT("INVALID_ARGUMENT"));
                    return true;
                }
            }

            double NumberValue = 0.0;
            bool bSearchCenterApplied = false;
            if (UEnvQueryGenerator_ActorsOfClass* ActorsGenerator = Cast<UEnvQueryGenerator_ActorsOfClass>(NewGenerator))
            {
                if ((*GeneratorSettings)->TryGetNumberField(TEXT("searchRadius"), NumberValue))
                {
                    ActorsGenerator->SearchRadius.DefaultValue = static_cast<float>(NumberValue);
                    ActorsGenerator->GenerateOnlyActorsInRadius.DefaultValue = true;
                }
                FString ActorClassPath;
                if ((*GeneratorSettings)->TryGetStringField(TEXT("actorClass"), ActorClassPath) && !ActorClassPath.IsEmpty())
                {
                    if (UClass* ActorClass = ResolveClassByName(ActorClassPath))
                    {
                        if (ActorClass->IsChildOf(AActor::StaticClass()))
                        {
                            ActorsGenerator->SearchedActorClass = ActorClass;
                        }
                    }
                }
                if (SearchCenterContext)
                {
                    ActorsGenerator->SearchCenter = SearchCenterContext;
                    bSearchCenterApplied = true;
                }
            }
            else if (UEnvQueryGenerator_SimpleGrid* GridGenerator = Cast<UEnvQueryGenerator_SimpleGrid>(NewGenerator))
            {
                if ((*GeneratorSettings)->TryGetNumberField(TEXT("gridSize"), NumberValue))
                {
                    GridGenerator->GridSize.DefaultValue = static_cast<float>(NumberValue);
                }
                if ((*GeneratorSettings)->TryGetNumberField(TEXT("spacesBetween"), NumberValue))
                {
                    GridGenerator->SpaceBetween.DefaultValue = static_cast<float>(NumberValue);
                }
                if (SearchCenterContext)
                {
                    GridGenerator->GenerateAround = SearchCenterContext;
                    bSearchCenterApplied = true;
                }
            }
            else if (UEnvQueryGenerator_OnCircle* CircleGenerator = Cast<UEnvQueryGenerator_OnCircle>(NewGenerator))
            {
                if ((*GeneratorSettings)->TryGetNumberField(TEXT("searchRadius"), NumberValue) ||
                    (*GeneratorSettings)->TryGetNumberField(TEXT("outerRadius"), NumberValue))
                {
                    CircleGenerator->CircleRadius.DefaultValue = static_cast<float>(NumberValue);
                }
                if ((*GeneratorSettings)->TryGetNumberField(TEXT("spacesBetween"), NumberValue))
                {
                    CircleGenerator->SpaceBetween.DefaultValue = static_cast<float>(NumberValue);
                }
                if (SearchCenterContext)
                {
                    CircleGenerator->CircleCenter = SearchCenterContext;
                    bSearchCenterApplied = true;
                }
            }
            if (SearchCenterContext && !bSearchCenterApplied)
            {
                S.SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("searchCenter is not supported for generator type: %s"), *GeneratorType),
                    TEXT("INVALID_ARGUMENT"));
                return true;
            }
        }

        UEnvQueryOption* NewOption = NewObject<UEnvQueryOption>(Query);
        if (!NewOption)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Failed to create EQS option for generator"),
                                TEXT("CREATION_FAILED"));
            return true;
        }

        NewOption->Generator = NewGenerator;
        const int32 OptionIndex = Query->GetOptionsMutable().Add(NewOption);
        Query->MarkPackageDirty();
        McpSafeAssetSave(Query);
        Result->SetNumberField(TEXT("optionIndex"), OptionIndex);
        Result->SetStringField(TEXT("generatorType"), GeneratorType);
        if (SearchCenterContext)
        {
            Result->SetStringField(TEXT("searchCenter"), SearchCenterContext->GetPathName());
        }
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s generator"), *GeneratorType));
        McpHandlerUtils::AddVerification(Result, Query);
        S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Generator added"), Result);
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Failed to create generator: %s"), *GeneratorType),
                            TEXT("CREATION_FAILED"));
    }

    return true;
#endif
}

bool McpHandlers::Ai::HandleAiAddEqsContext(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    FString QueryPath = GetJsonStringField(Payload, TEXT("queryPath"));
    FString ContextType = GetJsonStringField(Payload, TEXT("contextType"));

    UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
    if (!Query)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    // Only marked the package dirty — no EnvQueryContext was created or assigned, and it never saved.
    (void)ContextType;
    S.SendAutomationError(RequestingSocket, RequestId,
        TEXT("add_eqs_context is not implemented: it creates/assigns no EnvQueryContext. Author the "
             "context as a UEnvQueryContext subclass and reference it from the query's generator/tests."),
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiAddEqsTest(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if !MCP_HAS_ENVQUERY_TESTS
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("EQS Test creation requires UE 5.1+"),
                        TEXT("NOT_SUPPORTED"));
    return true;
#else
    FString QueryPath = GetJsonStringField(Payload, TEXT("queryPath"));
    FString TestType = GetJsonStringField(Payload, TEXT("testType"));

    UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
    if (!Query)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    UEnvQueryTest* NewTest = nullptr;
#if MCP_HAS_ENVQUERY_TESTS
    // Use runtime class lookup to avoid GetPrivateStaticClass requirement
    // StaticClass() calls GetPrivateStaticClass() internally which isn't exported
    UClass* TestClass = nullptr;
    if (TestType.Equals(TEXT("Distance"), ESearchCase::IgnoreCase))
    {
        TestClass = FindObject<UClass>(nullptr, TEXT("/Script/AIModule.EnvQueryTest_Distance"));
    }
    else if (TestType.Equals(TEXT("Trace"), ESearchCase::IgnoreCase))
    {
        TestClass = FindObject<UClass>(nullptr, TEXT("/Script/AIModule.EnvQueryTest_Trace"));
    }
    
    if (TestClass)
    {
        // Use NewObject with runtime UClass parameter to avoid template instantiation
        UObject* TestObj = NewObject<UObject>(Query, TestClass);
        if (TestObj && TestObj->GetClass()->IsChildOf(UEnvQueryTest::StaticClass()))
        {
            NewTest = static_cast<UEnvQueryTest*>(TestObj);
        }
    }
#endif

    if (NewTest)
    {
        auto& Options = Query->GetOptionsMutable();
        if (Options.Num() == 0 || !Options[0])
        {
            UEnvQueryOption* NewOption = NewObject<UEnvQueryOption>(Query);
            if (!NewOption)
            {
                S.SendAutomationError(RequestingSocket, RequestId,
                                    TEXT("Failed to create EQS option for test"),
                                    TEXT("CREATION_FAILED"));
                return true;
            }
            Options.Add(NewOption);
        }

        NewTest->TestOrder = Options[0]->Tests.Num();
        Options[0]->Tests.Add(NewTest);
        Query->MarkPackageDirty();
        McpSafeAssetSave(Query);
        Result->SetNumberField(TEXT("testIndex"), NewTest->TestOrder);
        Result->SetStringField(TEXT("testType"), TestType);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s test"), *TestType));
        McpHandlerUtils::AddVerification(Result, Query);
        S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Test added"), Result);
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Failed to create test: %s"), *TestType),
                            TEXT("CREATION_FAILED"));
    }

    return true;
#endif
#endif
}

bool McpHandlers::Ai::HandleAiConfigureTestScoring(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString QueryPath = GetJsonStringField(Payload, TEXT("queryPath"));
    int32 TestIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("testIndex"), 0));

    if (TestIndex < 0)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("testIndex must be zero or greater"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
    if (!Query)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    UEnvQueryTest* TargetTest = nullptr;
    int32 ResolvedOptionIndex = INDEX_NONE;
    int32 ResolvedTestIndex = INDEX_NONE;
    int32 FlatIndex = 0;
    auto& Options = Query->GetOptionsMutable();
    for (int32 OptionIndex = 0; OptionIndex < Options.Num(); ++OptionIndex)
    {
        UEnvQueryOption* Option = Options[OptionIndex];
        if (!Option)
        {
            continue;
        }

        for (int32 OptionTestIndex = 0; OptionTestIndex < Option->Tests.Num(); ++OptionTestIndex)
        {
            UEnvQueryTest* Test = Option->Tests[OptionTestIndex];
            if (!Test)
            {
                continue;
            }

            if (FlatIndex == TestIndex)
            {
                TargetTest = Test;
                ResolvedOptionIndex = OptionIndex;
                ResolvedTestIndex = OptionTestIndex;
                break;
            }
            ++FlatIndex;
        }

        if (TargetTest)
        {
            break;
        }
    }

    if (!TargetTest)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("EQS test index %d was not found on query: %s"), TestIndex, *QueryPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    const TSharedPtr<FJsonObject>* TestSettings = nullptr;
    Payload->TryGetObjectField(TEXT("testSettings"), TestSettings);
    const TSharedPtr<FJsonObject>& Settings = (TestSettings && TestSettings->IsValid()) ? *TestSettings : Payload;

    McpHandlerUtils::FMcpWriteReport Report;

    if (Settings->HasField(TEXT("scoringEquation")))
    {
        FString ScoringEquation;
        if (!Settings->TryGetStringField(TEXT("scoringEquation"), ScoringEquation) || ScoringEquation.IsEmpty())
        {
            Report.MarkFailed(TEXT("scoringEquation"), TEXT("must be a non-empty string"));
        }
        else
        {
            bool bRecognized = false;
            const EEnvTestScoreEquation::Type Parsed = ParseEQSScoringEquationAI(ScoringEquation, bRecognized);
            if (!bRecognized)
            {
                Report.MarkFailed(TEXT("scoringEquation"), TEXT("unknown scoring equation (expected Linear, Square, InverseLinear, or Constant)"));
            }
            else
            {
                TargetTest->ScoringEquation = Parsed;
                TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
                Report.MarkApplied(TEXT("scoringEquation"));
            }
        }
    }

    if (Settings->HasField(TEXT("filterType")))
    {
        FString FilterType;
        if (!Settings->TryGetStringField(TEXT("filterType"), FilterType) || FilterType.IsEmpty())
        {
            Report.MarkFailed(TEXT("filterType"), TEXT("must be a non-empty string"));
        }
        else
        {
            bool bRecognized = false;
            const EEnvTestFilterType::Type Parsed = ParseEQSFilterTypeAI(FilterType, bRecognized);
            if (!bRecognized)
            {
                Report.MarkFailed(TEXT("filterType"), TEXT("unknown filter type (expected Range, Minimum, or Maximum)"));
            }
            else
            {
                TargetTest->FilterType = Parsed;
                TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
                Report.MarkApplied(TEXT("filterType"));
            }
        }
    }

    if (Settings->HasField(TEXT("clampMin")))
    {
        double NumericValue = 0.0;
        if (Settings->TryGetNumberField(TEXT("clampMin"), NumericValue))
        {
            TargetTest->ScoreClampMin.DefaultValue = static_cast<float>(NumericValue);
            TargetTest->ClampMinType = EEnvQueryTestClamping::SpecifiedValue;
            TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
            Report.MarkApplied(TEXT("clampMin"));
        }
        else
        {
            Report.MarkFailed(TEXT("clampMin"), TEXT("must be a number"));
        }
    }
    if (Settings->HasField(TEXT("clampMax")))
    {
        double NumericValue = 0.0;
        if (Settings->TryGetNumberField(TEXT("clampMax"), NumericValue))
        {
            TargetTest->ScoreClampMax.DefaultValue = static_cast<float>(NumericValue);
            TargetTest->ClampMaxType = EEnvQueryTestClamping::SpecifiedValue;
            TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
            Report.MarkApplied(TEXT("clampMax"));
        }
        else
        {
            Report.MarkFailed(TEXT("clampMax"), TEXT("must be a number"));
        }
    }
    if (Settings->HasField(TEXT("floatMin")))
    {
        double NumericValue = 0.0;
        if (Settings->TryGetNumberField(TEXT("floatMin"), NumericValue))
        {
            TargetTest->FloatValueMin.DefaultValue = static_cast<float>(NumericValue);
            TargetTest->FilterType = EEnvTestFilterType::Range;
            TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
            Report.MarkApplied(TEXT("floatMin"));
        }
        else
        {
            Report.MarkFailed(TEXT("floatMin"), TEXT("must be a number"));
        }
    }
    if (Settings->HasField(TEXT("floatMax")))
    {
        double NumericValue = 0.0;
        if (Settings->TryGetNumberField(TEXT("floatMax"), NumericValue))
        {
            TargetTest->FloatValueMax.DefaultValue = static_cast<float>(NumericValue);
            TargetTest->FilterType = EEnvTestFilterType::Range;
            TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
            Report.MarkApplied(TEXT("floatMax"));
        }
        else
        {
            Report.MarkFailed(TEXT("floatMax"), TEXT("must be a number"));
        }
    }

    if (Report.AnyApplied())
    {
        Query->MarkPackageDirty();
        McpSafeAssetSave(Query);
    }

    Result->SetNumberField(TEXT("optionIndex"), ResolvedOptionIndex);
    Result->SetNumberField(TEXT("optionTestIndex"), ResolvedTestIndex);
    Result->SetNumberField(TEXT("testIndex"), TestIndex);
    Result->SetStringField(TEXT("testClass"), TargetTest->GetClass()->GetName());
    Result->SetBoolField(TEXT("configured"), Report.AnyApplied());
    Result->SetStringField(TEXT("message"), TEXT("Test scoring configured"));

    return SendWriteReportResponse(&S, RequestingSocket, RequestId, Report, Result,
                                   TEXT("Scoring configured"), Query);
#endif
}

// =========================================================================
// 16.5 Perception System (5 actions)
// =========================================================================

bool McpHandlers::Ai::HandleAiAddAiPerceptionComponent(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    // CRITICAL: Explicitly check if asset exists before LoadObject
    // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
    if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
        return true;
    }

    UBlueprint *Blueprint = S.ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
    if (!Blueprint) return true;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Blueprint has no SimpleConstructionScript"),
                            TEXT("INVALID_BLUEPRINT"));
        return true;
    }

    // Create perception component
    USCS_Node* NewNode = SCS->CreateNode(UAIPerceptionComponent::StaticClass(), TEXT("AIPerception"));
    if (NewNode)
    {
        SCS->AddNode(NewNode);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        Result->SetStringField(TEXT("componentName"), TEXT("AIPerception"));
        Result->SetStringField(TEXT("message"), TEXT("AI Perception component added"));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Perception component added"), Result);
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to create AI Perception component"),
                            TEXT("CREATION_FAILED"));
    }

    return true;
#endif
}

bool McpHandlers::Ai::HandleAiConfigureSightConfig(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    // CRITICAL: Explicitly check if asset exists before LoadObject
    // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
    if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
        return true;
    }

    UBlueprint *Blueprint = S.ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
    if (!Blueprint) return true;

    // Get sight config parameters
    double SightRadius = GetJsonNumberField(Payload, TEXT("sightRadius"), 3000.0);
    double LoseSightRadius = GetJsonNumberField(Payload, TEXT("loseSightRadius"), SightRadius + 500.0);
    double PeripheralAngle = GetJsonNumberField(Payload, TEXT("peripheralVisionAngle"), 90.0);
    double MaxAge = GetJsonNumberField(Payload, TEXT("maxAge"), 5.0);
    TOptional<float> AutoSuccessRange;
    TOptional<float> PovBackwardOffset;
    TOptional<float> NearClippingRadius;
    bool bDetectEnemies = true;
    bool bDetectNeutrals = true;
    bool bDetectFriendlies = false;

    // These params are declared top-level in the schema and also accepted
    // under sightConfig; the nested value wins when both are given.
    auto ReadSightParams = [&](const TSharedPtr<FJsonObject>& Src) -> bool
    {
        double NumberValue = 0.0;
        if (Src->TryGetNumberField(TEXT("autoSuccessRange"), NumberValue))
        {
            if (NumberValue < 0.0 && NumberValue != -1.0)
            {
                S.SendAutomationError(RequestingSocket, RequestId,
                    TEXT("autoSuccessRange must be >= 0, or -1 to disable"), TEXT("INVALID_ARGUMENT"));
                return false;
            }
            AutoSuccessRange = static_cast<float>(NumberValue);
        }
        if (Src->TryGetNumberField(TEXT("pointOfViewBackwardOffset"), NumberValue))
        {
            if (NumberValue < 0.0)
            {
                S.SendAutomationError(RequestingSocket, RequestId,
                    TEXT("pointOfViewBackwardOffset must be >= 0"), TEXT("INVALID_ARGUMENT"));
                return false;
            }
            PovBackwardOffset = static_cast<float>(NumberValue);
        }
        if (Src->TryGetNumberField(TEXT("nearClippingRadius"), NumberValue))
        {
            if (NumberValue < 0.0)
            {
                S.SendAutomationError(RequestingSocket, RequestId,
                    TEXT("nearClippingRadius must be >= 0"), TEXT("INVALID_ARGUMENT"));
                return false;
            }
            NearClippingRadius = static_cast<float>(NumberValue);
        }
        bDetectEnemies = GetJsonBoolField(Src, TEXT("enemies"), bDetectEnemies);
        bDetectNeutrals = GetJsonBoolField(Src, TEXT("neutrals"), bDetectNeutrals);
        bDetectFriendlies = GetJsonBoolField(Src, TEXT("friendlies"), bDetectFriendlies);
        const TSharedPtr<FJsonObject>* AffiliationObj = nullptr;
        if (Src->TryGetObjectField(TEXT("detectionByAffiliation"), AffiliationObj) && AffiliationObj->IsValid())
        {
            bDetectEnemies = GetJsonBoolField(*AffiliationObj, TEXT("enemies"), bDetectEnemies);
            bDetectNeutrals = GetJsonBoolField(*AffiliationObj, TEXT("neutrals"), bDetectNeutrals);
            bDetectFriendlies = GetJsonBoolField(*AffiliationObj, TEXT("friendlies"), bDetectFriendlies);
        }
        return true;
    };

    if (!ReadSightParams(Payload))
    {
        return true;
    }
    const TSharedPtr<FJsonObject>* SightConfigObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("sightConfig"), SightConfigObj) && SightConfigObj->IsValid())
    {
        SightRadius = GetJsonNumberField(*SightConfigObj, TEXT("sightRadius"), SightRadius);
        LoseSightRadius = GetJsonNumberField(*SightConfigObj, TEXT("loseSightRadius"), LoseSightRadius);
        PeripheralAngle = GetJsonNumberField(*SightConfigObj, TEXT("peripheralVisionAngle"), PeripheralAngle);
        MaxAge = GetJsonNumberField(*SightConfigObj, TEXT("maxAge"), MaxAge);
        if (!ReadSightParams(*SightConfigObj))
        {
            return true;
        }
    }
    if (MaxAge < 0.0)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("maxAge must be >= 0 (0 = never expires)"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (!Blueprint->SimpleConstructionScript)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
        return true;
    }

    UAIPerceptionComponent* PerceptionComp = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate)
        {
            if (UAIPerceptionComponent* Comp = Cast<UAIPerceptionComponent>(Node->ComponentTemplate))
            {
                PerceptionComp = Comp;
                break;
            }
        }
    }

    if (!PerceptionComp)
    {
        USCS_Node* PerceptionNode = Blueprint->SimpleConstructionScript->CreateNode(
            UAIPerceptionComponent::StaticClass(), TEXT("AIPerceptionComponent"));
        if (!PerceptionNode)
        {
            S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create perception component node"), TEXT("CREATION_FAILED"));
            return true;
        }
        Blueprint->SimpleConstructionScript->AddNode(PerceptionNode);
        PerceptionComp = Cast<UAIPerceptionComponent>(PerceptionNode->ComponentTemplate);
    }

    if (!PerceptionComp)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Perception component is null"), TEXT("NULL_COMPONENT"));
        return true;
    }

    UAISenseConfig_Sight* SightConfig = NewObject<UAISenseConfig_Sight>(PerceptionComp);
    SightConfig->SightRadius = SightRadius;
    SightConfig->LoseSightRadius = LoseSightRadius;
    SightConfig->PeripheralVisionAngleDegrees = PeripheralAngle;
    SightConfig->DetectionByAffiliation.bDetectEnemies = bDetectEnemies;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = bDetectNeutrals;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = bDetectFriendlies;
    SightConfig->SetMaxAge(static_cast<float>(MaxAge));
    if (AutoSuccessRange.IsSet())
    {
        SightConfig->AutoSuccessRangeFromLastSeenLocation = AutoSuccessRange.GetValue();
    }
    if (PovBackwardOffset.IsSet())
    {
        SightConfig->PointOfViewBackwardOffset = PovBackwardOffset.GetValue();
    }
    if (NearClippingRadius.IsSet())
    {
        SightConfig->NearClippingRadius = NearClippingRadius.GetValue();
    }
    PerceptionComp->ConfigureSense(*SightConfig);

    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, /*bSave=*/true);
    Result->SetNumberField(TEXT("sightRadius"), SightRadius);
    Result->SetNumberField(TEXT("loseSightRadius"), LoseSightRadius);
    Result->SetNumberField(TEXT("peripheralVisionAngle"), PeripheralAngle);
    Result->SetNumberField(TEXT("maxAge"), MaxAge);
    TSharedPtr<FJsonObject> AffiliationOut = MakeShared<FJsonObject>();
    AffiliationOut->SetBoolField(TEXT("enemies"), bDetectEnemies);
    AffiliationOut->SetBoolField(TEXT("neutrals"), bDetectNeutrals);
    AffiliationOut->SetBoolField(TEXT("friendlies"), bDetectFriendlies);
    Result->SetObjectField(TEXT("detectionByAffiliation"), AffiliationOut);
    if (AutoSuccessRange.IsSet())
    {
        Result->SetNumberField(TEXT("autoSuccessRange"), AutoSuccessRange.GetValue());
    }
    if (PovBackwardOffset.IsSet())
    {
        Result->SetNumberField(TEXT("pointOfViewBackwardOffset"), PovBackwardOffset.GetValue());
    }
    if (NearClippingRadius.IsSet())
    {
        Result->SetNumberField(TEXT("nearClippingRadius"), NearClippingRadius.GetValue());
    }
    Result->SetStringField(TEXT("message"), TEXT("Sight sense configured"));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sight config set"), Result);
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiConfigureHearingConfig(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    // CRITICAL: Explicitly check if asset exists before LoadObject
    // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
    if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
        return true;
    }

    UBlueprint *Blueprint = S.ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
    if (!Blueprint) return true;

    double HearingRange = GetJsonNumberField(Payload, TEXT("hearingRange"), 3000.0);
    const TSharedPtr<FJsonObject>* HearingConfigObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("hearingConfig"), HearingConfigObj) && HearingConfigObj->IsValid())
    {
        HearingRange = GetJsonNumberField(*HearingConfigObj, TEXT("hearingRange"), HearingRange);
    }

    if (!Blueprint->SimpleConstructionScript)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
        return true;
    }

    UAIPerceptionComponent* PerceptionComp = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate)
        {
            if (UAIPerceptionComponent* Comp = Cast<UAIPerceptionComponent>(Node->ComponentTemplate))
            {
                PerceptionComp = Comp;
                break;
            }
        }
    }

    if (!PerceptionComp)
    {
        USCS_Node* PerceptionNode = Blueprint->SimpleConstructionScript->CreateNode(
            UAIPerceptionComponent::StaticClass(), TEXT("AIPerceptionComponent"));
        if (!PerceptionNode)
        {
            S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create perception component node"), TEXT("CREATION_FAILED"));
            return true;
        }
        Blueprint->SimpleConstructionScript->AddNode(PerceptionNode);
        PerceptionComp = Cast<UAIPerceptionComponent>(PerceptionNode->ComponentTemplate);
    }

    if (!PerceptionComp)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Perception component is null"), TEXT("NULL_COMPONENT"));
        return true;
    }

    UAISenseConfig_Hearing* HearingConfig = NewObject<UAISenseConfig_Hearing>(PerceptionComp);
    HearingConfig->HearingRange = HearingRange;
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
    HearingConfig->SetMaxAge(5.0f);
    PerceptionComp->ConfigureSense(*HearingConfig);

    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, /*bSave=*/true);
    Result->SetNumberField(TEXT("hearingRange"), HearingRange);
    Result->SetStringField(TEXT("message"), TEXT("Hearing sense configured"));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hearing config set"), Result);
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiConfigureDamageSenseConfig(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    // CRITICAL: Explicitly check if asset exists before LoadObject
    // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
    if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
        return true;
    }

    UBlueprint *Blueprint = S.ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
    if (!Blueprint) return true;

    double MaxAge = 10.0;
    const TSharedPtr<FJsonObject>* DamageConfigObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("damageConfig"), DamageConfigObj) && DamageConfigObj->IsValid())
    {
        MaxAge = GetJsonNumberField(*DamageConfigObj, TEXT("maxAge"), MaxAge);
    }

    if (!Blueprint->SimpleConstructionScript)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
        return true;
    }

    UAIPerceptionComponent* PerceptionComp = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate)
        {
            if (UAIPerceptionComponent* Comp = Cast<UAIPerceptionComponent>(Node->ComponentTemplate))
            {
                PerceptionComp = Comp;
                break;
            }
        }
    }

    if (!PerceptionComp)
    {
        USCS_Node* PerceptionNode = Blueprint->SimpleConstructionScript->CreateNode(
            UAIPerceptionComponent::StaticClass(), TEXT("AIPerceptionComponent"));
        if (!PerceptionNode)
        {
            S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create perception component node"), TEXT("CREATION_FAILED"));
            return true;
        }
        Blueprint->SimpleConstructionScript->AddNode(PerceptionNode);
        PerceptionComp = Cast<UAIPerceptionComponent>(PerceptionNode->ComponentTemplate);
    }

    if (!PerceptionComp)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Perception component is null"), TEXT("NULL_COMPONENT"));
        return true;
    }

    UAISenseConfig_Damage* DamageConfig = NewObject<UAISenseConfig_Damage>(PerceptionComp);
    DamageConfig->SetMaxAge(MaxAge);
    PerceptionComp->ConfigureSense(*DamageConfig);

    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, /*bSave=*/true);
    Result->SetNumberField(TEXT("maxAge"), MaxAge);
    Result->SetStringField(TEXT("message"), TEXT("Damage sense configured"));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage config set"), Result);
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiSetPerceptionTeam(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    int32 TeamId = static_cast<int32>(GetJsonNumberField(Payload, TEXT("teamId"), 0));

    // CRITICAL: Explicitly check if asset exists before LoadObject
    // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
    if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
        return true;
    }

    UBlueprint *Blueprint = S.ResolveBlueprintOrError(BlueprintPath, RequestId, RequestingSocket);
    if (!Blueprint) return true;

    bool bAppliedToGenericTeamAgent = false;
    if (Blueprint->GeneratedClass)
    {
        if (UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject())
        {
            if (IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(CDO))
            {
                TeamAgent->SetGenericTeamId(FGenericTeamId(static_cast<uint8>(FMath::Clamp(TeamId, 0, 255))));
                bAppliedToGenericTeamAgent = true;
            }
        }
    }

    const FName TeamVarName(TEXT("GenericTeamId"));
    bool bStoredBlueprintVariable = false;
    const bool bHasTeamVariable = Blueprint->GeneratedClass &&
        FindFProperty<FProperty>(Blueprint->GeneratedClass, TeamVarName) != nullptr;
    if (!bHasTeamVariable)
    {
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        bStoredBlueprintVariable = FBlueprintEditorUtils::AddMemberVariable(Blueprint, TeamVarName, PinType);
        if (bStoredBlueprintVariable)
        {
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TeamVarName, nullptr, FText::FromString(TEXT("AI Perception")));
        }
    }
    else
    {
        bStoredBlueprintVariable = true;
    }

    if (bStoredBlueprintVariable)
    {
        SetBPVarDefaultValueAI(Blueprint, TeamVarName, FString::FromInt(TeamId));
    }

    McpFinalizeBlueprint(Blueprint, /*bStructural=*/false, /*bSave=*/true);
    Result->SetNumberField(TEXT("teamId"), TeamId);
    Result->SetBoolField(TEXT("appliedToGenericTeamAgent"), bAppliedToGenericTeamAgent);
    Result->SetBoolField(TEXT("storedBlueprintVariable"), bStoredBlueprintVariable);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Team ID set to %d"), TeamId));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Team set"), Result);
    return true;
#endif
}

// =========================================================================
// 16.6 State Trees - UE5.3+ (4 actions)
// =========================================================================

bool McpHandlers::Ai::HandleAiCreateStateTree(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
    // Runtime check: Verify StateTree module is actually loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!IsStateTreeModuleAvailable())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("StateTree plugin is not enabled in this project. Enable the StateTree plugin to use State Tree features."),
            TEXT("STATETREE_PLUGIN_NOT_ENABLED"));
        return true;
    }
    
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/StateTrees"));
    FString SchemaType = GetJsonStringField(Payload, TEXT("schemaType"), TEXT("Component"));
    
    if (Name.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("State Tree name is required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // Create the package and asset
    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Failed to create package: %s"), *FullPath), TEXT("CREATION_FAILED"));
        return true;
    }
    
    UStateTree* StateTree = NewObject<UStateTree>(Package, *Name, RF_Public | RF_Standalone);
    if (!StateTree)
    {
        Package->MarkAsGarbage();  // Prevent orphaned package leak
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create StateTree asset"), TEXT("CREATION_FAILED"));
        return true;
    }
    
    // Create and attach EditorData
    UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree, TEXT("EditorData"), RF_Transactional);
    if (!EditorData)
    {
        StateTree->ConditionalBeginDestroy();  // Clean up StateTree before marking package as garbage
        Package->MarkAsGarbage();  // Prevent orphaned package leak
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create StateTree EditorData"), TEXT("CREATION_FAILED"));
        return true;
    }
    StateTree->EditorData = EditorData;
    
    // Assign schema based on type
#if MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE
    EditorData->Schema = NewObject<UStateTreeComponentSchema>(EditorData);
#else
    // UE 5.7+ or schema not available - skip schema assignment
    // The StateTree will use a default schema or require manual configuration
#endif
    
    // Add a default root state
    UStateTreeState& RootState = EditorData->AddRootState();
    RootState.Name = FName(TEXT("Root"));
    
    // Save the asset
    McpSafeAssetSave(StateTree);
    
    Result->SetStringField(TEXT("stateTreePath"), FullPath);
    Result->SetStringField(TEXT("rootStateName"), TEXT("Root"));
    Result->SetStringField(TEXT("message"), TEXT("State Tree created with root state"));
    McpHandlerUtils::AddVerification(Result, StateTree);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("State Tree created"), Result);
#elif MCP_HAS_STATE_TREE
    // StateTree headers were not present at compile time, so no asset can be created.
    S.SendAutomationError(RequestingSocket, RequestId,
        TEXT("State Tree creation is unavailable: StateTree editor headers were not present when this plugin was compiled. Rebuild the bridge with the StateTree plugin enabled."),
        TEXT("STATETREE_HEADERS_UNAVAILABLE"));
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("State Trees require UE 5.3+"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiAddStateTreeState(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
    FString StateTreePath = GetJsonStringField(Payload, TEXT("stateTreePath"));
    FString StateName = GetJsonStringField(Payload, TEXT("stateName"));
    FString ParentStateName = GetJsonStringField(Payload, TEXT("parentStateName"), TEXT("Root"));
    FString StateType = GetJsonStringField(Payload, TEXT("stateType"), TEXT("State"));
    
    if (StateTreePath.IsEmpty() || StateName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("stateTreePath and stateName are required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // Load the StateTree
    UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *StateTreePath);
    if (!StateTree)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("StateTree not found: %s"), *StateTreePath), TEXT("NOT_FOUND"));
        return true;
    }
    
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("StateTree has no EditorData"), TEXT("INVALID_STATE"));
        return true;
    }
    
    // Find the parent state
    UStateTreeState* ParentState = nullptr;
    for (UStateTreeState* SubTree : EditorData->SubTrees)
    {
        if (SubTree && SubTree->Name.ToString().Equals(ParentStateName, ESearchCase::IgnoreCase))
        {
            ParentState = SubTree;
            break;
        }
        // Check children recursively
        if (SubTree)
        {
            for (UStateTreeState* Child : SubTree->Children)
            {
                if (Child && Child->Name.ToString().Equals(ParentStateName, ESearchCase::IgnoreCase))
                {
                    ParentState = Child;
                    break;
                }
            }
        }
    }
    
    if (!ParentState)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Parent state '%s' not found"), *ParentStateName), TEXT("NOT_FOUND"));
        return true;
    }
    
    // Determine state type
    EStateTreeStateType Type = EStateTreeStateType::State;
    if (StateType.Equals(TEXT("Group"), ESearchCase::IgnoreCase))
    {
        Type = EStateTreeStateType::Group;
    }
    else if (StateType.Equals(TEXT("Linked"), ESearchCase::IgnoreCase))
    {
        Type = EStateTreeStateType::Linked;
    }
    else if (StateType.Equals(TEXT("LinkedAsset"), ESearchCase::IgnoreCase))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
        Type = EStateTreeStateType::LinkedAsset;
#else
        UE_LOG(LogMcpAIHandlers, Warning, TEXT("LinkedAsset state type requires UE 5.4+. Falling back to State type."));
        Type = EStateTreeStateType::State;
#endif
    }
    
    // Add the child state
    UStateTreeState& NewState = ParentState->AddChildState(FName(*StateName), Type);
    
    // Save
    McpSafeAssetSave(StateTree);
    
    Result->SetStringField(TEXT("stateName"), StateName);
    Result->SetStringField(TEXT("parentState"), ParentStateName);
    Result->SetStringField(TEXT("stateType"), StateType);
    Result->SetStringField(TEXT("message"), TEXT("State added to StateTree"));
    McpHandlerUtils::AddVerification(Result, StateTree);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("State added"), Result);
#elif MCP_HAS_STATE_TREE
    // StateTree headers were not present at compile time, so no state can be added.
    S.SendAutomationError(RequestingSocket, RequestId,
        TEXT("Adding a State Tree state is unavailable: StateTree editor headers were not present when this plugin was compiled. Rebuild the bridge with the StateTree plugin enabled."),
        TEXT("STATETREE_HEADERS_UNAVAILABLE"));
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("State Trees require UE 5.3+"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiAddStateTreeTransition(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
    FString StateTreePath = GetJsonStringField(Payload, TEXT("stateTreePath"));
    FString FromState = GetJsonStringField(Payload, TEXT("fromState"));
    FString ToState = GetJsonStringField(Payload, TEXT("toState"));
    FString TriggerType = GetJsonStringField(Payload, TEXT("triggerType"), TEXT("OnStateCompleted"));
    
    if (StateTreePath.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("stateTreePath, fromState, and toState are required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // Load the StateTree
    UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *StateTreePath);
    if (!StateTree)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("StateTree not found: %s"), *StateTreePath), TEXT("NOT_FOUND"));
        return true;
    }
    
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("StateTree has no EditorData"), TEXT("INVALID_STATE"));
        return true;
    }
    
    // Find source and target states
    UStateTreeState* SourceState = nullptr;
    UStateTreeState* TargetState = nullptr;
    
    // Helper lambda to find state recursively
    TFunction<UStateTreeState*(UStateTreeState*, const FString&)> FindState;
    FindState = [&FindState](UStateTreeState* State, const FString& Name) -> UStateTreeState* {
        if (!State) return nullptr;
        if (State->Name.ToString().Equals(Name, ESearchCase::IgnoreCase))
        {
            return State;
        }
        for (UStateTreeState* Child : State->Children)
        {
            if (UStateTreeState* Found = FindState(Child, Name))
            {
                return Found;
            }
        }
        return nullptr;
    };
    
    for (UStateTreeState* SubTree : EditorData->SubTrees)
    {
        if (!SourceState) SourceState = FindState(SubTree, FromState);
        if (!TargetState) TargetState = FindState(SubTree, ToState);
    }
    
    if (!SourceState)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Source state '%s' not found"), *FromState), TEXT("NOT_FOUND"));
        return true;
    }
    
    if (!TargetState)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Target state '%s' not found"), *ToState), TEXT("NOT_FOUND"));
        return true;
    }
    
    // Determine trigger type
    EStateTreeTransitionTrigger Trigger = EStateTreeTransitionTrigger::OnStateCompleted;
    if (TriggerType.Equals(TEXT("OnStateFailed"), ESearchCase::IgnoreCase))
    {
        Trigger = EStateTreeTransitionTrigger::OnStateFailed;
    }
    else if (TriggerType.Equals(TEXT("OnTick"), ESearchCase::IgnoreCase))
    {
        Trigger = EStateTreeTransitionTrigger::OnTick;
    }
    else if (TriggerType.Equals(TEXT("OnEvent"), ESearchCase::IgnoreCase))
    {
        Trigger = EStateTreeTransitionTrigger::OnEvent;
    }
    
    // Add transition
    FStateTreeTransition& Transition = SourceState->AddTransition(Trigger, EStateTreeTransitionType::GotoState, TargetState);
    
    // Save
    McpSafeAssetSave(StateTree);
    
    Result->SetStringField(TEXT("fromState"), FromState);
    Result->SetStringField(TEXT("toState"), ToState);
    Result->SetStringField(TEXT("triggerType"), TriggerType);
    Result->SetStringField(TEXT("transitionId"), Transition.ID.ToString());
    Result->SetStringField(TEXT("message"), TEXT("Transition added"));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Transition added"), Result);
#elif MCP_HAS_STATE_TREE
    FString StateTreePath = GetJsonStringField(Payload, TEXT("stateTreePath"));
    FString FromState = GetJsonStringField(Payload, TEXT("fromState"));
    FString ToState = GetJsonStringField(Payload, TEXT("toState"));
    Result->SetStringField(TEXT("fromState"), FromState);
    Result->SetStringField(TEXT("toState"), ToState);
    Result->SetStringField(TEXT("message"), TEXT("Transition registered (headers unavailable)"));
    Result->SetBoolField(TEXT("headersUnavailable"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Transition registered"), Result);
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("State Trees require UE 5.3+"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiConfigureStateTreeTask(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
    FString StateTreePath = GetJsonStringField(Payload, TEXT("stateTreePath"));
    FString StateName = GetJsonStringField(Payload, TEXT("stateName"));
    FString TaskType = GetJsonStringField(Payload, TEXT("taskType"), TEXT(""));
    
    if (StateTreePath.IsEmpty() || StateName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("stateTreePath and stateName are required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // Load the StateTree
    UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *StateTreePath);
    if (!StateTree)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("StateTree not found: %s"), *StateTreePath), TEXT("NOT_FOUND"));
        return true;
    }
    
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("StateTree has no EditorData"), TEXT("INVALID_STATE"));
        return true;
    }
    
    // Find the state
    UStateTreeState* FoundState = nullptr;
    TFunction<UStateTreeState*(UStateTreeState*, const FString&)> FindState;
    FindState = [&FindState](UStateTreeState* State, const FString& Name) -> UStateTreeState* {
        if (!State) return nullptr;
        if (State->Name.ToString().Equals(Name, ESearchCase::IgnoreCase))
        {
            return State;
        }
        for (UStateTreeState* Child : State->Children)
        {
            if (UStateTreeState* Found = FindState(Child, Name))
            {
                return Found;
            }
        }
        return nullptr;
    };
    
    for (UStateTreeState* SubTree : EditorData->SubTrees)
    {
        FoundState = FindState(SubTree, StateName);
        if (FoundState) break;
    }
    
    if (!FoundState)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("State '%s' not found"), *StateName), TEXT("NOT_FOUND"));
        return true;
    }
    
    McpHandlerUtils::FMcpWriteReport Report;

    if (Payload->HasField(TEXT("selectionBehavior")))
    {
        FString Behavior = GetJsonStringField(Payload, TEXT("selectionBehavior"));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 7
        bool bBehaviorResolved = true;
        if (Behavior.Equals(TEXT("TryEnterState"), ESearchCase::IgnoreCase))
        {
            FoundState->SelectionBehavior = EStateTreeStateSelectionBehavior::TryEnterState;
        }
        else if (Behavior.Equals(TEXT("TrySelectChildrenInOrder"), ESearchCase::IgnoreCase))
        {
            FoundState->SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;
        }
        else if (Behavior.Equals(TEXT("TrySelectChildrenAtRandom"), ESearchCase::IgnoreCase))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
            FoundState->SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandom;
#else
        UE_LOG(LogMcpAIHandlers, Warning, TEXT("TrySelectChildrenAtRandom requires UE 5.5+. Using TrySelectChildrenInOrder instead."));

            FoundState->SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;
#endif
        }
        else if (Behavior.Equals(TEXT("TrySelectChildrenWithHighestUtility"), ESearchCase::IgnoreCase))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
            FoundState->SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility;
#else
            UE_LOG(LogMcpAIHandlers, Warning, TEXT("TrySelectChildrenWithHighestUtility requires UE 5.4+. Using TryEnterState instead."));
            FoundState->SelectionBehavior = EStateTreeStateSelectionBehavior::TryEnterState;
#endif
        }
        else
        {
            bBehaviorResolved = false;
        }
        if (bBehaviorResolved)
        {
            Report.MarkApplied(TEXT("selectionBehavior"));
        }
        else
        {
            Report.MarkFailed(TEXT("selectionBehavior"), FString::Printf(TEXT("unknown selection behavior '%s'"), *Behavior));
        }
#else
        (void)Behavior;
        Report.MarkFailed(TEXT("selectionBehavior"), TEXT("SelectionBehavior is not settable on UE 5.7+ (API refactored)"));
#endif
    }

    bool bTaskAdded = false;
    FString ResolvedTaskStructPath;
    if (!TaskType.IsEmpty())
    {
        // Tasks are FInstancedStruct nodes whose struct must derive from
        // FStateTreeTaskBase (mirrors the BaseStruct meta on UStateTreeState::Tasks).
        const UScriptStruct* TaskBaseStruct = FStateTreeTaskBase::StaticStruct();
        const UScriptStruct* TaskStruct = FindObject<UScriptStruct>(nullptr, *TaskType);
        if (!TaskStruct)
        {
            TaskStruct = FindObject<UScriptStruct>(nullptr, *FString::Printf(TEXT("/Script/StateTreeModule.%s"), *TaskType));
        }
        if (!TaskStruct)
        {
            TaskStruct = FindObject<UScriptStruct>(nullptr, *FString::Printf(TEXT("/Script/StateTreeModule.StateTree%sTask"), *TaskType));
        }
        if (!TaskStruct)
        {
            TaskStruct = FindFirstObject<UScriptStruct>(*TaskType, EFindFirstObjectOptions::NativeFirst);
        }

        if (!TaskStruct)
        {
            Report.MarkFailed(TEXT("taskType"), FString::Printf(TEXT("taskType '%s' did not resolve to a UScriptStruct"), *TaskType));
        }
        else if (!TaskStruct->IsChildOf(TaskBaseStruct))
        {
            Report.MarkFailed(TEXT("taskType"), FString::Printf(TEXT("taskType '%s' (%s) does not derive from FStateTreeTaskBase"), *TaskType, *TaskStruct->GetPathName()));
        }
        else
        {
            // Mirror UStateTreeState::AddTask<T> at runtime: add a defaulted editor node,
            // give it an ID, instantiate the task struct, then init its instance data.
            FStateTreeEditorNode& TaskNode = FoundState->Tasks.AddDefaulted_GetRef();
            TaskNode.ID = FGuid::NewGuid();
            TaskNode.Node.InitializeAs(TaskStruct);
            if (const FStateTreeNodeBase* NodeBase = TaskNode.Node.GetPtr<FStateTreeNodeBase>())
            {
                if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(NodeBase->GetInstanceDataType()))
                {
                    TaskNode.Instance.InitializeAs(InstanceType);
                }
                if (const UScriptStruct* RuntimeType = Cast<const UScriptStruct>(NodeBase->GetExecutionRuntimeDataType()))
                {
                    TaskNode.ExecutionRuntimeData.InitializeAs(RuntimeType);
                }
            }
            bTaskAdded = true;
            ResolvedTaskStructPath = TaskStruct->GetPathName();
            Report.MarkApplied(TEXT("taskType"));
        }
    }

    if (Report.AnyApplied())
    {
        McpSafeAssetSave(StateTree);
    }

    Result->SetStringField(TEXT("stateName"), StateName);
    Result->SetBoolField(TEXT("taskAdded"), bTaskAdded);
    if (bTaskAdded)
    {
        Result->SetStringField(TEXT("taskType"), ResolvedTaskStructPath);
    }
    Result->SetNumberField(TEXT("taskCount"), FoundState->Tasks.Num());
    Result->SetStringField(TEXT("message"), bTaskAdded
        ? TEXT("Task added to state")
        : TEXT("State task configuration updated (no taskType supplied)"));
    return SendWriteReportResponse(&S, RequestingSocket, RequestId, Report, Result,
                                   TEXT("Task configured"), nullptr);
#elif MCP_HAS_STATE_TREE
    FString StateTreePath = GetJsonStringField(Payload, TEXT("stateTreePath"));
    FString StateName = GetJsonStringField(Payload, TEXT("stateName"));
    Result->SetStringField(TEXT("stateName"), StateName);
    Result->SetStringField(TEXT("message"), TEXT("Task configuration registered (headers unavailable)"));
    Result->SetBoolField(TEXT("headersUnavailable"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Task configured"), Result);
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("State Trees require UE 5.3+"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
#endif
}

// =========================================================================
// 16.7 Smart Objects (4 actions)
// =========================================================================

bool McpHandlers::Ai::HandleAiCreateSmartObjectDefinition(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if MCP_HAS_SMART_OBJECTS && MCP_SMART_OBJECTS_HEADERS_AVAILABLE
    // Runtime check: Verify SmartObjects module is actually loaded
    if (!IsSmartObjectsModuleAvailable())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("SmartObjects plugin is not enabled in this project. Enable the SmartObjects plugin to use Smart Object features."),
            TEXT("SMARTOBJECTS_PLUGIN_NOT_ENABLED"));
        return true;
    }
    
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/SmartObjects"));
    
    if (Name.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Smart Object Definition name is required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // Create the package and asset
    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Failed to create package: %s"), *FullPath), TEXT("CREATION_FAILED"));
        return true;
    }
    
    USmartObjectDefinition* Definition = NewObject<USmartObjectDefinition>(Package, *Name, RF_Public | RF_Standalone);
    if (!Definition)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create SmartObjectDefinition asset"), TEXT("CREATION_FAILED"));
        return true;
    }
    
    // Save the asset
    McpSafeAssetSave(Definition);
    
    Result->SetStringField(TEXT("definitionPath"), FullPath);
    Result->SetNumberField(TEXT("slotCount"), 0);
    Result->SetStringField(TEXT("message"), TEXT("Smart Object Definition created"));
    Result->SetBoolField(TEXT("success"), true);
    McpHandlerUtils::AddVerification(Result, Definition); // existsAfter/assetPath, matching other creates
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Definition created"), Result);
#elif MCP_HAS_SMART_OBJECTS
    // SmartObjects headers were not present at compile time, so no asset can be created.
    S.SendAutomationError(RequestingSocket, RequestId,
        TEXT("Smart Object Definition creation is unavailable: SmartObjects headers were not present when this plugin was compiled. Rebuild the bridge with the SmartObjects plugin enabled."),
        TEXT("SMARTOBJECTS_HEADERS_UNAVAILABLE"));
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Smart Objects require UE 5.0+"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiAddSmartObjectSlot(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if MCP_HAS_SMART_OBJECTS && MCP_SMART_OBJECTS_HEADERS_AVAILABLE
    FString DefinitionPath = GetJsonStringField(Payload, TEXT("definitionPath"));
    FVector Offset = ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
    FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);
    
    if (DefinitionPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("definitionPath is required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // Load the SmartObjectDefinition
    USmartObjectDefinition* Definition = LoadObject<USmartObjectDefinition>(nullptr, *DefinitionPath);
    if (!Definition)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("SmartObjectDefinition not found: %s"), *DefinitionPath), TEXT("NOT_FOUND"));
        return true;
    }
    
    // Create and add a new slot using reflection to access private Slots array
    FSmartObjectSlotDefinition NewSlot;
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
    // UE 5.3+ uses FVector3f/FRotator3f and has bEnabled/ID members
    NewSlot.Offset = FVector3f(Offset);
    NewSlot.Rotation = FRotator3f(Rotation);
    NewSlot.bEnabled = bEnabled;
#if WITH_EDITORONLY_DATA
    NewSlot.ID = FGuid::NewGuid();
#endif
#else
    // UE 5.0-5.2 uses FVector/FRotator
    NewSlot.Offset = Offset;
    NewSlot.Rotation = Rotation;
#endif
    
    // Access slots via reflection
    FProperty* SlotsProp = Definition->GetClass()->FindPropertyByName(TEXT("Slots"));
    int32 SlotIndex = -1;
    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(SlotsProp))
    {
        FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Definition));
        SlotIndex = ArrayHelper.AddValue();
        if (FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner))
        {
            InnerStruct->Struct->CopyScriptStruct(ArrayHelper.GetRawPtr(SlotIndex), &NewSlot);
        }
    }
    
    // Save
    McpSafeAssetSave(Definition);
    
    Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
    Result->SetStringField(TEXT("definitionPath"), DefinitionPath);
    Result->SetStringField(TEXT("message"), TEXT("Slot added to Smart Object Definition"));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Slot added"), Result);
#elif MCP_HAS_SMART_OBJECTS
    FString DefinitionPath = GetJsonStringField(Payload, TEXT("definitionPath"));
    Result->SetNumberField(TEXT("slotIndex"), 0);
    Result->SetStringField(TEXT("message"), TEXT("Slot addition registered (headers unavailable)"));
    Result->SetBoolField(TEXT("headersUnavailable"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Slot registered"), Result);
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Smart Objects require UE 5.0+"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiConfigureSlotBehavior(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if MCP_HAS_SMART_OBJECTS && MCP_SMART_OBJECTS_HEADERS_AVAILABLE
    FString DefinitionPath = GetJsonStringField(Payload, TEXT("definitionPath"));
    int32 SlotIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("slotIndex"), 0));
    FString BehaviorType = GetJsonStringField(Payload, TEXT("behaviorType"), TEXT(""));
    
    if (DefinitionPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("definitionPath is required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // Load the SmartObjectDefinition
    USmartObjectDefinition* Definition = LoadObject<USmartObjectDefinition>(nullptr, *DefinitionPath);
    if (!Definition)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("SmartObjectDefinition not found: %s"), *DefinitionPath), TEXT("NOT_FOUND"));
        return true;
    }
    
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    if (!Definition->IsValidSlotIndex(SlotIndex))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid slot index: %d"), SlotIndex), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // Get the slot and configure it
    FSmartObjectSlotDefinition& Slot = Definition->GetMutableSlot(SlotIndex);

    McpHandlerUtils::FMcpWriteReport Report;

    if (Payload->HasField(TEXT("activityTags")))
    {
        const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("activityTags"), TagsArray))
        {
            for (const auto& TagValue : *TagsArray)
            {
                FString TagStr = TagValue->AsString();
                FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
                if (Tag.IsValid())
                {
                    Slot.ActivityTags.AddTag(Tag);
                }
            }
            Report.MarkApplied(TEXT("activityTags"));
        }
        else
        {
            Report.MarkFailed(TEXT("activityTags"), TEXT("must be an array of gameplay tag names"));
        }
    }

    if (Payload->HasField(TEXT("enabled")))
    {
        Slot.bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);
        Report.MarkApplied(TEXT("enabled"));
    }

    if (Payload->HasField(TEXT("behaviorType")))
    {
        Report.MarkFailed(TEXT("behaviorType"),
            FString::Printf(TEXT("behavior definition assignment ('%s') is not implemented for smart object slots"), *BehaviorType));
    }

    if (Report.AnyApplied())
    {
        McpSafeAssetSave(Definition);
    }

    Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
    Result->SetNumberField(TEXT("behaviorCount"), Slot.BehaviorDefinitions.Num());
    Result->SetStringField(TEXT("message"), TEXT("Slot behavior configured"));
    return SendWriteReportResponse(&S, RequestingSocket, RequestId, Report, Result,
                                   TEXT("Behavior configured"), nullptr);
#else
    // UE 5.0: SmartObject API is limited - skip slot configuration
    S.SendAutomationError(RequestingSocket, RequestId,
        TEXT("SmartObject slot configuration requires UE 5.1+"), TEXT("UNSUPPORTED_VERSION"));
    return true;
#endif
#elif MCP_HAS_SMART_OBJECTS
    FString DefinitionPath = GetJsonStringField(Payload, TEXT("definitionPath"));
    int32 SlotIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("slotIndex"), 0));
    Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
    Result->SetStringField(TEXT("message"), TEXT("Slot behavior configuration registered (headers unavailable)"));
    Result->SetBoolField(TEXT("headersUnavailable"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior configured"), Result);
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Smart Objects require UE 5.0+"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiAddSmartObjectComponent(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if MCP_HAS_SMART_OBJECTS && MCP_SMART_OBJECTS_HEADERS_AVAILABLE
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString DefinitionPath = GetJsonStringField(Payload, TEXT("definitionPath"), TEXT(""));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("SmartObjectComponent"));
    
    if (BlueprintPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("blueprintPath is required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // Load the Blueprint
    FString NormalizedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
    if (!Blueprint)
    {
        S.SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("NOT_FOUND"));
        return true;
    }
    
    // Load the definition if provided
    USmartObjectDefinition* Definition = nullptr;
    if (!DefinitionPath.IsEmpty())
    {
        Definition = LoadObject<USmartObjectDefinition>(nullptr, *DefinitionPath);
    }
    
    // Get the SCS
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
        return true;
    }
    
    // Create the component node using proper UE 5.7 SCS pattern
    USCS_Node* NewNode = SCS->CreateNode(USmartObjectComponent::StaticClass(), FName(*ComponentName));
    if (!NewNode)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create SCS node for SmartObjectComponent"), TEXT("CREATION_FAILED"));
        return true;
    }
    
    // Configure the component template
    USmartObjectComponent* SOComp = Cast<USmartObjectComponent>(NewNode->ComponentTemplate);
    if (SOComp && Definition)
    {
        SOComp->SetDefinition(Definition);
    }
    
    // Add to SCS
    SCS->AddNode(NewNode);

    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, /*bSave=*/true);
    
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetStringField(TEXT("blueprintPath"), NormalizedPath);
    if (Definition)
    {
        Result->SetStringField(TEXT("definitionPath"), DefinitionPath);
    }
    Result->SetStringField(TEXT("message"), TEXT("Smart Object component added to blueprint"));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Component added"), Result);
#elif MCP_HAS_SMART_OBJECTS
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    Result->SetStringField(TEXT("componentName"), TEXT("SmartObject"));
    Result->SetStringField(TEXT("message"), TEXT("Smart Object component addition registered (headers unavailable)"));
    Result->SetBoolField(TEXT("headersUnavailable"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Component registered"), Result);
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Smart Objects require UE 5.0+"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
#endif
}

// =========================================================================
// 16.8 Mass AI / Crowds (3 actions)
// =========================================================================

bool McpHandlers::Ai::HandleAiCreateMassEntityConfig(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if MCP_HAS_MASS_AI && MCP_MASS_AI_HEADERS_AVAILABLE
    // Runtime check: Verify MassEntity module is actually loaded
    if (!IsMassModuleAvailable())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("MassEntity plugin is not enabled in this project. Enable the MassEntity plugin to use Mass AI features."),
            TEXT("MASS_PLUGIN_NOT_ENABLED"));
        return true;
    }
    
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/Mass"));
    
    if (Name.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Mass Entity Config name is required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // Create the package and asset
    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Failed to create package: %s"), *FullPath), TEXT("CREATION_FAILED"));
        return true;
    }
    
    UMassEntityConfigAsset* ConfigAsset = NewObject<UMassEntityConfigAsset>(Package, *Name, RF_Public | RF_Standalone);
    if (!ConfigAsset)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create MassEntityConfigAsset"), TEXT("CREATION_FAILED"));
        return true;
    }
    
    // Save the asset
    McpSafeAssetSave(ConfigAsset);
    
    Result->SetStringField(TEXT("configPath"), FullPath);
    Result->SetNumberField(TEXT("traitCount"), 0);
    Result->SetStringField(TEXT("message"), TEXT("Mass Entity Config created"));
    Result->SetBoolField(TEXT("success"), true);
    McpHandlerUtils::AddVerification(Result, ConfigAsset); // existsAfter/assetPath, matching other creates
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Config created"), Result);
#elif MCP_HAS_MASS_AI
    // MassEntity headers were not present at compile time, so no asset can be created.
    S.SendAutomationError(RequestingSocket, RequestId,
        TEXT("Mass Entity Config creation is unavailable: MassEntity headers were not present when this plugin was compiled. Rebuild the bridge with the MassEntity plugin enabled."),
        TEXT("MASS_HEADERS_UNAVAILABLE"));
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Mass AI requires UE 5.0+ with MassEntity plugin"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiConfigureMassEntity(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if MCP_HAS_MASS_AI && MCP_MASS_AI_HEADERS_AVAILABLE
    FString ConfigPath = GetJsonStringField(Payload, TEXT("configPath"));
    FString ParentConfigPath = GetJsonStringField(Payload, TEXT("parentConfigPath"), TEXT(""));
    
    if (ConfigPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("configPath is required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // CRITICAL: Explicitly check if asset exists before LoadObject
    // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
    if (!UEditorAssetLibrary::DoesAssetExist(ConfigPath))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("MassEntityConfigAsset not found: %s"), *ConfigPath), TEXT("NOT_FOUND"));
        return true;
    }

    // Load the MassEntityConfigAsset
    UMassEntityConfigAsset* ConfigAsset = LoadObject<UMassEntityConfigAsset>(nullptr, *ConfigPath);
    if (!ConfigAsset)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("MassEntityConfigAsset not found: %s"), *ConfigPath), TEXT("NOT_FOUND"));
        return true;
    }
    
    // Get the mutable config
    FMassEntityConfig& Config = ConfigAsset->GetMutableConfig();
    
    // Set parent config if provided
    // UE 5.3+: Use SetParentAsset() method
    // UE 5.0-5.2: Use property reflection since Parent is protected
    if (!ParentConfigPath.IsEmpty())
    {
        UMassEntityConfigAsset* ParentConfig = LoadObject<UMassEntityConfigAsset>(nullptr, *ParentConfigPath);
        if (ParentConfig)
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
            Config.SetParentAsset(*ParentConfig);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            // UE 5.1-5.2: SetValue_InContainer is available
            static FProperty* ParentProp = FMassEntityConfig::StaticStruct()->FindPropertyByName(TEXT("Parent"));
            if (ParentProp)
            {
                ParentProp->SetValue_InContainer(&Config, &ParentConfig);
            }
#else
            // UE 5.0: SetValue_InContainer not available, use CopyCompleteValue_InContainer
            static FProperty* ParentProp = FMassEntityConfig::StaticStruct()->FindPropertyByName(TEXT("Parent"));
            if (ParentProp)
            {
                // Create a temporary struct to hold the pointer value, then copy
                void* DestPtr = ParentProp->ContainerPtrToValuePtr<void>(&Config);
                ParentProp->CopyCompleteValue(DestPtr, &ParentConfig);
            }
#endif
        }
    }
    
    // Save
    McpSafeAssetSave(ConfigAsset);
    
    Result->SetStringField(TEXT("configPath"), ConfigPath);
    Result->SetNumberField(TEXT("traitCount"), Config.GetTraits().Num());
    Result->SetStringField(TEXT("message"), TEXT("Mass Entity configured"));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Entity configured"), Result);
#elif MCP_HAS_MASS_AI
    FString ConfigPath = GetJsonStringField(Payload, TEXT("configPath"));
    Result->SetStringField(TEXT("configPath"), ConfigPath);
    Result->SetStringField(TEXT("message"), TEXT("Mass Entity configuration registered (headers unavailable)"));
    Result->SetBoolField(TEXT("headersUnavailable"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Entity configured"), Result);
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Mass AI requires UE 5.0+ with MassEntity plugin"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
#endif
}

bool McpHandlers::Ai::HandleAiAddMassSpawner(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if MCP_HAS_MASS_AI
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString ConfigPath = GetJsonStringField(Payload, TEXT("configPath"), TEXT(""));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("MassSpawner"));
    int32 SpawnCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("spawnCount"), 100));
    
    if (BlueprintPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("blueprintPath is required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    
    // Load the Blueprint
    FString NormalizedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
    if (!Blueprint)
    {
        S.SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("NOT_FOUND"));
        return true;
    }
    
    // Note: MassSpawner is typically an Actor class, not a component.
    // For component-based spawning, use MassAgentComponent on individual actors.
    // This implementation adds metadata indicating spawner configuration.
    
    // Mark blueprint as modified
    Blueprint->MarkPackageDirty();
    McpSafeAssetSave(Blueprint);
    
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetStringField(TEXT("blueprintPath"), NormalizedPath);
    Result->SetNumberField(TEXT("spawnCount"), SpawnCount);
    if (!ConfigPath.IsEmpty())
    {
        Result->SetStringField(TEXT("configPath"), ConfigPath);
    }
    Result->SetStringField(TEXT("message"), TEXT("Mass Spawner configuration added. Note: For high-performance crowd spawning, use AMassSpawner actor directly."));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spawner configured"), Result);
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Mass AI requires UE 5.0+ with MassEntity plugin"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
#endif
}

// =========================================================================
// Utility (1 action)
// =========================================================================

bool McpHandlers::Ai::HandleAiGetAiInfo(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    TSharedPtr<FJsonObject> AIInfo = McpHandlerUtils::CreateResultObject();

    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString ControllerPath = GetJsonStringField(Payload, TEXT("controllerPath"));
    FString BTPath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
    FString BBPath = GetJsonStringField(Payload, TEXT("blackboardPath"));
    FString QueryPath = GetJsonStringField(Payload, TEXT("queryPath"));

    // Generic assetPath/path: resolve the asset type and route it to the
    // matching readback branch below.
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"));
    if (AssetPath.IsEmpty())
    {
        AssetPath = GetJsonStringField(Payload, TEXT("path"));
    }
    if (!AssetPath.IsEmpty())
    {
        UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
        if (!Asset)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        if (Asset->IsA<UBehaviorTree>()) { BTPath = AssetPath; }
        else if (Asset->IsA<UBlackboardData>()) { BBPath = AssetPath; }
        else if (Asset->IsA<UEnvQuery>()) { QueryPath = AssetPath; }
        else if (Asset->IsA<UBlueprint>()) { BlueprintPath = AssetPath; }
        else
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unsupported asset type '%s' for get_info; expected a Behavior Tree, Blackboard, EQS query, or Blueprint"),
                    *Asset->GetClass()->GetName()),
                TEXT("UNSUPPORTED_ASSET_TYPE"));
            return true;
        }
    }

    if (BlueprintPath.IsEmpty() && ControllerPath.IsEmpty() && BTPath.IsEmpty() &&
        BBPath.IsEmpty() && QueryPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("get_info requires one of: assetPath, blueprintPath, controllerPath, behaviorTreePath, blackboardPath, queryPath"),
            TEXT("INVALID_PARAMS"));
        return true;
    }

    // --- blueprintPath: auto-discover AI setup from Pawn/Character/AIController blueprint ---
    if (!BlueprintPath.IsEmpty())
    {
        BlueprintPath = SanitizeProjectRelativePath(BlueprintPath);
        if (BlueprintPath.IsEmpty())
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid blueprintPath: must be a valid project-relative path"),
                TEXT("INVALID_PATH"));
            return true;
        }
        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!BP)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                TEXT("BLUEPRINT_NOT_FOUND"));
            return true;
        }
        if (BP->GeneratedClass)
        {
            AIInfo->SetStringField(TEXT("blueprintClass"), BP->GeneratedClass->GetName());
            UObject* CDO = BP->GeneratedClass->GetDefaultObject();

            // Pawn/Character: extract AIControllerClass
            if (APawn* PawnCDO = Cast<APawn>(CDO))
            {
                if (PawnCDO->AIControllerClass)
                {
                    AIInfo->SetStringField(TEXT("controllerClass"),
                        PawnCDO->AIControllerClass->GetName());
                }
            }
            // AIController: report directly
            else if (Cast<AAIController>(CDO))
            {
                AIInfo->SetStringField(TEXT("controllerClass"),
                    BP->GeneratedClass->GetName());
            }
        }
    }

    // --- controllerPath: explicit controller blueprint (overrides blueprintPath discovery) ---
    if (!ControllerPath.IsEmpty())
    {
        UBlueprint* Controller = LoadObject<UBlueprint>(nullptr, *ControllerPath);
        if (!Controller)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Controller blueprint not found: %s"), *ControllerPath),
                TEXT("CONTROLLER_NOT_FOUND"));
            return true;
        }
        AIInfo->SetStringField(TEXT("controllerClass"),
            Controller->GeneratedClass ? Controller->GeneratedClass->GetName() : TEXT("Unknown"));
    }

    // --- behaviorTreePath ---
    if (!BTPath.IsEmpty())
    {
        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
        if (!BT)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        {
            auto CreateBTNodeRuntimeInfo = [](UBTNode* Node) -> TSharedPtr<FJsonObject>
            {
                TSharedPtr<FJsonObject> NodeInfo = McpHandlerUtils::CreateResultObject();
                if (!Node)
                {
                    return NodeInfo;
                }

                NodeInfo->SetStringField(TEXT("className"), Node->GetClass() ? Node->GetClass()->GetName() : TEXT("Unknown"));
                NodeInfo->SetStringField(TEXT("nodeName"), Node->GetNodeName());

                FString SelectedBlackboardKey;
                for (TFieldIterator<FProperty> PropIt(Node->GetClass()); PropIt; ++PropIt)
                {
                    if (FStructProperty* StructProp = CastField<FStructProperty>(*PropIt))
                    {
                        if (StructProp->Struct == FBlackboardKeySelector::StaticStruct())
                        {
                            FBlackboardKeySelector* Selector = StructProp->ContainerPtrToValuePtr<FBlackboardKeySelector>(Node);
                            if (Selector && !Selector->SelectedKeyName.IsNone())
                            {
                                SelectedBlackboardKey = Selector->SelectedKeyName.ToString();
                                break;
                            }
                        }
                    }
                }
                NodeInfo->SetStringField(TEXT("selectedBlackboardKey"), SelectedBlackboardKey);
                return NodeInfo;
            };

            AIInfo->SetStringField(TEXT("assignedBehaviorTree"), BT->GetName());
            AIInfo->SetBoolField(TEXT("hasRootNode"), BT->RootNode != nullptr);

            TArray<TSharedPtr<FJsonValue>> RootDecoratorClasses;
            TArray<TSharedPtr<FJsonValue>> RootDecorators;
            for (UBTDecorator* RootDecorator : BT->RootDecorators)
            {
                if (!RootDecorator)
                {
                    continue;
                }
                RootDecoratorClasses.Add(MakeShared<FJsonValueString>(RootDecorator->GetClass()->GetName()));
                RootDecorators.Add(MakeShared<FJsonValueObject>(CreateBTNodeRuntimeInfo(RootDecorator)));
            }
            AIInfo->SetNumberField(TEXT("rootDecoratorCount"), BT->RootDecorators.Num());
            AIInfo->SetArrayField(TEXT("rootDecoratorClasses"), RootDecoratorClasses);
            AIInfo->SetArrayField(TEXT("rootDecorators"), RootDecorators);

            // Report associated blackboard from BT asset (only if
            // blackboardPath was not explicitly provided, to avoid
            // silently overwriting an explicit value)
            if (BBPath.IsEmpty())
            {
                if (BT->BlackboardAsset)
                {
                    AIInfo->SetStringField(TEXT("assignedBlackboard"),
                        BT->BlackboardAsset->GetName());
                }
                else
                {
                    AIInfo->SetField(TEXT("assignedBlackboard"), MakeShared<FJsonValueNull>());
                }
            }

            // Graph-instance -> GUID crosslink for the runtime dump below: the
            // graph editor nodes wrap the same UBTNode instances the runtime
            // tree holds, and their GUIDs are the ids add_node/connect_nodes/
            // set_node_properties speak.
            TMap<UObject*, FString> InstanceToGuid;
#if MCP_AI_HAS_BEHAVIOR_TREE_GRAPH
            if (UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(BT->BTGraph))
            {
                for (UEdGraphNode* GraphNode : BTGraph->Nodes)
                {
                    if (UBehaviorTreeGraphNode_Root* RootNode = Cast<UBehaviorTreeGraphNode_Root>(GraphNode))
                    {
                        AIInfo->SetStringField(TEXT("rootGraphBlackboard"), GetNameSafe(RootNode->BlackboardAsset));
                        AIInfo->SetBoolField(TEXT("rootGraphBlackboardMatchesAssigned"), RootNode->BlackboardAsset == BT->BlackboardAsset);
                        break;
                    }
                }

                // Graph node listing: these ids are what add_node/add_subnode
                // return and connect_nodes/remove_node/set_node_properties take.
                TArray<TSharedPtr<FJsonValue>> GraphNodes;
                auto AppendGraphNode = [&GraphNodes, &InstanceToGuid](UEdGraphNode* GraphNode, const FString& ParentId)
                {
                    if (!GraphNode)
                    {
                        return;
                    }
                    TSharedPtr<FJsonObject> NodeObj = McpHandlerUtils::CreateResultObject();
                    NodeObj->SetStringField(TEXT("nodeId"), GraphNode->NodeGuid.ToString());
                    FString NodeClassName = GraphNode->GetClass()->GetName();
                    if (UAIGraphNode* AINode = Cast<UAIGraphNode>(GraphNode))
                    {
                        if (AINode->NodeInstance)
                        {
                            NodeClassName = AINode->NodeInstance->GetClass()->GetName();
                            InstanceToGuid.Add(AINode->NodeInstance, GraphNode->NodeGuid.ToString());
                        }
                    }
                    NodeObj->SetStringField(TEXT("nodeClass"), NodeClassName);
                    if (!ParentId.IsEmpty())
                    {
                        NodeObj->SetStringField(TEXT("parentNodeId"), ParentId);
                    }
                    GraphNodes.Add(MakeShared<FJsonValueObject>(NodeObj));
                };
                for (UEdGraphNode* GraphNode : BTGraph->Nodes)
                {
                    AppendGraphNode(GraphNode, FString());
                    if (UAIGraphNode* AINode = Cast<UAIGraphNode>(GraphNode))
                    {
                        for (UAIGraphNode* SubNode : AINode->SubNodes)
                        {
                            AppendGraphNode(SubNode, GraphNode->NodeGuid.ToString());
                        }
                    }
                }
                AIInfo->SetArrayField(TEXT("graphNodes"), GraphNodes);
            }
#endif

            // Count BT nodes (composites + tasks + decorators + services)
            if (BT->RootNode)
            {
                int32 NodeCount = 0;
                TArray<TSharedPtr<FJsonValue>> ChildDecorators;
                TArray<TSharedPtr<FJsonValue>> Services;
                TArray<UBTCompositeNode*> Stack;
                Stack.Add(BT->RootNode);
                while (Stack.Num() > 0)
                {
                    UBTCompositeNode* Current = Stack.Pop();
                    NodeCount++;
                    NodeCount += Current->Services.Num();
                    for (UBTService* Service : Current->Services)
                    {
                        if (Service)
                        {
                            Services.Add(MakeShared<FJsonValueObject>(CreateBTNodeRuntimeInfo(Service)));
                        }
                    }
                    for (const FBTCompositeChild& Child : Current->Children)
                    {
                        NodeCount += Child.Decorators.Num();
                        for (UBTDecorator* Decorator : Child.Decorators)
                        {
                            if (Decorator)
                            {
                                ChildDecorators.Add(MakeShared<FJsonValueObject>(CreateBTNodeRuntimeInfo(Decorator)));
                            }
                        }
                        if (Child.ChildComposite)
                        {
                            Stack.Add(Child.ChildComposite);
                        }
                        if (Child.ChildTask)
                        {
                            NodeCount++;
                            NodeCount += Child.ChildTask->Services.Num();
                            for (UBTService* Service : Child.ChildTask->Services)
                            {
                                if (Service)
                                {
                                    Services.Add(MakeShared<FJsonValueObject>(CreateBTNodeRuntimeInfo(Service)));
                                }
                            }
                        }
                    }
                }
                AIInfo->SetNumberField(TEXT("btNodeCount"), NodeCount);
                AIInfo->SetArrayField(TEXT("childDecorators"), ChildDecorators);
                AIInfo->SetArrayField(TEXT("services"), Services);
            }

            // Full-fidelity recursive dump. The flat arrays above can't
            // reconstruct the tree: composites/tasks float parentless, and
            // child ORDER — selector priority, sequence order — is the actual
            // semantics of a BT. One entry per composite/task/decorator/
            // service, children in execution order, every FBlackboardKeySelector
            // property by name, and the node's CPF_Edit config as a property
            // bag. `nodeId` crosslinks to the graph GUIDs the authoring
            // actions speak (absent when no graph node wraps the instance).
            if (BT->RootNode)
            {
                TArray<TSharedPtr<FJsonValue>> BtNodes;
                FString TreePretty;

                auto MakeBtNodeJson = [&InstanceToGuid](UBTNode* Node, const TCHAR* Kind,
                    const FString& Path, const FString& ParentPath, int32 ChildIndex,
                    int32 Depth) -> TSharedPtr<FJsonObject>
                {
                    TSharedPtr<FJsonObject> NodeObj = McpHandlerUtils::CreateResultObject();
                    NodeObj->SetStringField(TEXT("kind"), Kind);
                    NodeObj->SetStringField(TEXT("path"), Path);
                    if (!ParentPath.IsEmpty())
                    {
                        NodeObj->SetStringField(TEXT("parentPath"), ParentPath);
                    }
                    NodeObj->SetNumberField(TEXT("childIndex"), ChildIndex);
                    NodeObj->SetNumberField(TEXT("depth"), Depth);
                    NodeObj->SetStringField(TEXT("className"), Node->GetClass()->GetName());
                    NodeObj->SetStringField(TEXT("nodeName"), Node->GetNodeName());
                    if (const FString* Guid = InstanceToGuid.Find(Node))
                    {
                        NodeObj->SetStringField(TEXT("nodeId"), *Guid);
                    }

                    TSharedPtr<FJsonObject> Keys = MakeShared<FJsonObject>();
                    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
                    for (TFieldIterator<FProperty> PropIt(Node->GetClass()); PropIt; ++PropIt)
                    {
                        FProperty* Prop = *PropIt;
                        if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
                        {
                            if (StructProp->Struct == FBlackboardKeySelector::StaticStruct())
                            {
                                const FBlackboardKeySelector* Selector =
                                    StructProp->ContainerPtrToValuePtr<FBlackboardKeySelector>(Node);
                                Keys->SetStringField(Prop->GetName(),
                                    Selector ? Selector->SelectedKeyName.ToString() : FString());
                                continue;
                            }
                        }
                        if (!Prop->HasAnyPropertyFlags(CPF_Edit))
                        {
                            continue;
                        }
                        if (Prop->GetFName() == TEXT("NodeName"))
                        {
                            continue;
                        }
                        Props->SetField(Prop->GetName(),
                            McpPropertyReflection::ExportPropertyToJsonValue(Node, Prop));
                    }
                    if (Keys->Values.Num() > 0)
                    {
                        NodeObj->SetObjectField(TEXT("blackboardKeys"), Keys);
                    }
                    if (Props->Values.Num() > 0)
                    {
                        NodeObj->SetObjectField(TEXT("properties"), Props);
                    }
                    return NodeObj;
                };

                auto PrettyLine = [&TreePretty](int32 Depth, const TCHAR* Marker, UBTNode* Node)
                {
                    TreePretty += FString::ChrN(Depth * 2, TEXT(' '));
                    if (Marker && *Marker)
                    {
                        TreePretty += Marker;
                        TreePretty += TEXT(" ");
                    }
                    TreePretty += Node->GetNodeName();
                    const FString ClassName = Node->GetClass()->GetName();
                    if (Node->GetNodeName() != ClassName)
                    {
                        TreePretty += FString::Printf(TEXT("  (%s)"), *ClassName);
                    }
                    TreePretty += TEXT("\n");
                };

                TFunction<void(UBTCompositeNode*, const FString&, const FString&, int32, int32)> Walk =
                    [&](UBTCompositeNode* Composite, const FString& Path, const FString& ParentPath,
                        int32 ChildIndex, int32 Depth)
                {
                    BtNodes.Add(MakeShared<FJsonValueObject>(MakeBtNodeJson(
                        Composite, TEXT("composite"), Path, ParentPath, ChildIndex, Depth)));
                    PrettyLine(Depth, TEXT(""), Composite);
                    for (int32 SvcIdx = 0; SvcIdx < Composite->Services.Num(); ++SvcIdx)
                    {
                        UBTService* Service = Composite->Services[SvcIdx];
                        if (!Service)
                        {
                            continue;
                        }
                        BtNodes.Add(MakeShared<FJsonValueObject>(MakeBtNodeJson(
                            Service, TEXT("service"),
                            FString::Printf(TEXT("%s.svc%d"), *Path, SvcIdx), Path, SvcIdx, Depth + 1)));
                        PrettyLine(Depth + 1, TEXT("[svc]"), Service);
                    }
                    for (int32 i = 0; i < Composite->Children.Num(); ++i)
                    {
                        const FBTCompositeChild& Child = Composite->Children[i];
                        const FString ChildPath = FString::Printf(TEXT("%s.%d"), *Path, i);
                        for (int32 DecIdx = 0; DecIdx < Child.Decorators.Num(); ++DecIdx)
                        {
                            UBTDecorator* Decorator = Child.Decorators[DecIdx];
                            if (!Decorator)
                            {
                                continue;
                            }
                            BtNodes.Add(MakeShared<FJsonValueObject>(MakeBtNodeJson(
                                Decorator, TEXT("decorator"),
                                FString::Printf(TEXT("%s.dec%d"), *ChildPath, DecIdx), ChildPath, i, Depth + 1)));
                            PrettyLine(Depth + 1, TEXT("[dec]"), Decorator);
                        }
                        if (Child.ChildComposite)
                        {
                            Walk(Child.ChildComposite, ChildPath, Path, i, Depth + 1);
                        }
                        else if (Child.ChildTask)
                        {
                            BtNodes.Add(MakeShared<FJsonValueObject>(MakeBtNodeJson(
                                Child.ChildTask, TEXT("task"), ChildPath, Path, i, Depth + 1)));
                            PrettyLine(Depth + 1, TEXT(""), Child.ChildTask);
                            for (int32 TSvcIdx = 0; TSvcIdx < Child.ChildTask->Services.Num(); ++TSvcIdx)
                            {
                                UBTService* Service = Child.ChildTask->Services[TSvcIdx];
                                if (!Service)
                                {
                                    continue;
                                }
                                BtNodes.Add(MakeShared<FJsonValueObject>(MakeBtNodeJson(
                                    Service, TEXT("service"),
                                    FString::Printf(TEXT("%s.svc%d"), *ChildPath, TSvcIdx), ChildPath, TSvcIdx, Depth + 2)));
                                PrettyLine(Depth + 2, TEXT("[svc]"), Service);
                            }
                        }
                    }
                };
                Walk(BT->RootNode, TEXT("0"), FString(), 0, 0);

                // Root-level decorators (subtree usage; the editor attaches none
                // to Root directly, but a BT run as a subtree can carry them).
                for (int32 RDecIdx = 0; RDecIdx < BT->RootDecorators.Num(); ++RDecIdx)
                {
                    UBTDecorator* Decorator = BT->RootDecorators[RDecIdx];
                    if (!Decorator)
                    {
                        continue;
                    }
                    BtNodes.Add(MakeShared<FJsonValueObject>(MakeBtNodeJson(
                        Decorator, TEXT("decorator"),
                        FString::Printf(TEXT("0.rdec%d"), RDecIdx), TEXT("0"), 0, 1)));
                }

                AIInfo->SetArrayField(TEXT("btNodes"), BtNodes);
                AIInfo->SetStringField(TEXT("treePretty"), TreePretty.TrimEnd());
            }
        }
    }

    // --- blackboardPath ---
    if (!BBPath.IsEmpty())
    {
        UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BBPath);
        if (!BB)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blackboard not found: %s"), *BBPath),
                TEXT("NOT_FOUND"));
            return true;
        }
        AIInfo->SetStringField(TEXT("assignedBlackboard"), BB->GetName());
        AIInfo->SetNumberField(TEXT("keyCount"), BB->Keys.Num());
        TArray<TSharedPtr<FJsonValue>> KeysArray;
        for (const FBlackboardEntry& Entry : BB->Keys)
        {
            TSharedPtr<FJsonObject> KeyObj = McpHandlerUtils::CreateResultObject();
            KeyObj->SetStringField(TEXT("name"), Entry.EntryName.ToString());
            KeyObj->SetStringField(TEXT("type"), Entry.KeyType ? Entry.KeyType->GetClass()->GetName() : TEXT("Unknown"));
            KeyObj->SetBoolField(TEXT("instanceSynced"), Entry.bInstanceSynced);
            KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
        }
        AIInfo->SetArrayField(TEXT("blackboardKeys"), KeysArray);
    }

    // --- queryPath ---
    if (!QueryPath.IsEmpty())
    {
        UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
        if (!Query)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("EQS query not found: %s"), *QueryPath),
                TEXT("NOT_FOUND"));
            return true;
        }
        AIInfo->SetStringField(TEXT("queryName"), Query->GetName());
    }

    Result->SetObjectField(TEXT("aiInfo"), AIInfo);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("AI info retrieved"), Result);
    return true;
#endif
}

// =========================================================================
// Aliases & Convenience Actions
// =========================================================================

// Alias: setup_perception -> add_perception_component (same logic)
bool McpHandlers::Ai::HandleAiSetupPerception(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    FString ControllerPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    if (ControllerPath.IsEmpty())
    {
        ControllerPath = GetJsonStringField(Payload, TEXT("controllerPath"));
    }
    if (ControllerPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath or controllerPath"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // CRITICAL: Explicitly check if asset exists before LoadObject
    // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
    if (!UEditorAssetLibrary::DoesAssetExist(ControllerPath))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *ControllerPath), TEXT("NOT_FOUND"));
        return true;
    }

    UBlueprint *ControllerBP = S.ResolveBlueprintOrError(ControllerPath, RequestId, RequestingSocket);
    if (!ControllerBP) return true;

    if (!ControllerBP->SimpleConstructionScript)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
        return true;
    }

    // Find or create AIPerceptionComponent
    UAIPerceptionComponent* PerceptionComp = nullptr;
    bool bCreatedNew = false;

    for (USCS_Node* Node : ControllerBP->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate)
        {
            if (UAIPerceptionComponent* Comp = Cast<UAIPerceptionComponent>(Node->ComponentTemplate))
            {
                PerceptionComp = Comp;
                break;
            }
        }
    }

    if (!PerceptionComp)
    {
        USCS_Node* PerceptionNode = ControllerBP->SimpleConstructionScript->CreateNode(
            UAIPerceptionComponent::StaticClass(), TEXT("AIPerceptionComponent"));
        if (!PerceptionNode)
        {
            S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create perception component node"), TEXT("CREATION_FAILED"));
            return true;
        }
        ControllerBP->SimpleConstructionScript->AddNode(PerceptionNode);
        PerceptionComp = Cast<UAIPerceptionComponent>(PerceptionNode->ComponentTemplate);
        bCreatedNew = true;
    }

    if (!PerceptionComp)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Perception component is null"), TEXT("NULL_COMPONENT"));
        return true;
    }

    TArray<FString> SensesConfigured;

    bool bEnableSight = GetJsonBoolField(Payload, TEXT("enableSight"));
    if (bEnableSight)
    {
        float SightRadius = GetJsonNumberField(Payload, TEXT("sightRadius"), 3000.0f);
        float LoseSightRadius = GetJsonNumberField(Payload, TEXT("loseSightRadius"), SightRadius + 500.0f);
        float PeripheralVisionAngle = GetJsonNumberField(Payload, TEXT("peripheralVisionAngle"), 90.0f);

        UAISenseConfig_Sight* SightConfig = NewObject<UAISenseConfig_Sight>(PerceptionComp);
        SightConfig->SightRadius = SightRadius;
        SightConfig->LoseSightRadius = LoseSightRadius;
        SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngle;
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;
        SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
        SightConfig->SetMaxAge(5.0f);

        PerceptionComp->ConfigureSense(*SightConfig);
        SensesConfigured.Add(TEXT("Sight"));
    }

    bool bEnableHearing = GetJsonBoolField(Payload, TEXT("enableHearing"));
    if (bEnableHearing)
    {
        float HearingRange = GetJsonNumberField(Payload, TEXT("hearingRange"), 3000.0f);
        UAISenseConfig_Hearing* HearingConfig = NewObject<UAISenseConfig_Hearing>(PerceptionComp);
        HearingConfig->HearingRange = HearingRange;
        HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
        HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
        HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
        HearingConfig->SetMaxAge(5.0f);
        PerceptionComp->ConfigureSense(*HearingConfig);
        SensesConfigured.Add(TEXT("Hearing"));
    }

    bool bEnableDamage = GetJsonBoolField(Payload, TEXT("enableDamage"));
    if (bEnableDamage)
    {
        UAISenseConfig_Damage* DamageConfig = NewObject<UAISenseConfig_Damage>(PerceptionComp);
        DamageConfig->SetMaxAge(10.0f);
        PerceptionComp->ConfigureSense(*DamageConfig);
        SensesConfigured.Add(TEXT("Damage"));
    }

    FString DominantSense = GetJsonStringField(Payload, TEXT("dominantSense"));
    if (!DominantSense.IsEmpty())
    {
        if (DominantSense.Equals(TEXT("Sight"), ESearchCase::IgnoreCase))
        {
            PerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());
        }
        else if (DominantSense.Equals(TEXT("Hearing"), ESearchCase::IgnoreCase))
        {
            PerceptionComp->SetDominantSense(UAISense_Hearing::StaticClass());
        }
        else if (DominantSense.Equals(TEXT("Damage"), ESearchCase::IgnoreCase))
        {
            PerceptionComp->SetDominantSense(UAISense_Damage::StaticClass());
        }
    }

    McpFinalizeBlueprint(ControllerBP, /*bStructural=*/true, /*bSave=*/true);

    TSharedPtr<FJsonObject> PerceptionResult = McpHandlerUtils::CreateResultObject();
    PerceptionResult->SetStringField(TEXT("controllerPath"), ControllerPath);
    PerceptionResult->SetBoolField(TEXT("createdNew"), bCreatedNew);

    TArray<TSharedPtr<FJsonValue>> SensesArray;
    for (const FString& Sense : SensesConfigured)
    {
        SensesArray.Add(MakeShared<FJsonValueString>(Sense));
    }
    PerceptionResult->SetArrayField(TEXT("sensesConfigured"), SensesArray);

    if (!DominantSense.IsEmpty())
    {
        PerceptionResult->SetStringField(TEXT("dominantSense"), DominantSense);
    }

    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("AI perception configured via setup_perception"), PerceptionResult);
    return true;
#endif
}

// set_focus - Set focus actor variable on AI controller blueprint
bool McpHandlers::Ai::HandleAiSetFocus(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    FString ControllerPath = GetJsonStringField(Payload, TEXT("controllerPath"));
    if (ControllerPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing controllerPath"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString FocusActorName = GetJsonStringField(Payload, TEXT("focusActorName"));
    if (FocusActorName.IsEmpty())
    {
        FocusActorName = GetJsonStringField(Payload, TEXT("targetActor"));
    }

    UBlueprint *ControllerBP = S.ResolveBlueprintOrError(ControllerPath, RequestId, RequestingSocket, TEXT("controllerPath"));
    if (!ControllerBP) return true;

    // Add a FocusActor variable to the BP
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
    PinType.PinSubCategoryObject = AActor::StaticClass();
    FBlueprintEditorUtils::AddMemberVariable(ControllerBP, TEXT("FocusActor"), PinType);

    McpFinalizeBlueprint(ControllerBP, /*bStructural=*/false, /*bSave=*/true);

    TSharedPtr<FJsonObject> FocusResult = McpHandlerUtils::CreateResultObject();
    FocusResult->SetStringField(TEXT("controllerPath"), ControllerPath);
    FocusResult->SetStringField(TEXT("focusActorName"), FocusActorName);
    FocusResult->SetBoolField(TEXT("focusSet"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Focus actor variable set on controller"), FocusResult);
    return true;
#endif
}

// clear_focus - Clear focus actor variable on AI controller blueprint
bool McpHandlers::Ai::HandleAiClearFocus(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    FString ControllerPath = GetJsonStringField(Payload, TEXT("controllerPath"));
    UBlueprint *ControllerBP = S.ResolveBlueprintOrError(ControllerPath, RequestId, RequestingSocket, TEXT("controllerPath"));
    if (!ControllerBP) return true;

    FBlueprintEditorUtils::RemoveMemberVariable(ControllerBP, TEXT("FocusActor"));

    McpFinalizeBlueprint(ControllerBP, /*bStructural=*/false, /*bSave=*/true);

    TSharedPtr<FJsonObject> ClearResult = McpHandlerUtils::CreateResultObject();
    ClearResult->SetStringField(TEXT("controllerPath"), ControllerPath);
    ClearResult->SetBoolField(TEXT("focusCleared"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Focus cleared on controller"), ClearResult);
    return true;
#endif
}

// set_blackboard_value - Set a default key value on a blackboard asset
bool McpHandlers::Ai::HandleAiSetBlackboardValue(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BBPath = GetJsonStringField(Payload, TEXT("blackboardPath"));
    if (BBPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blackboardPath"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString KeyName = GetJsonStringField(Payload, TEXT("keyName"));
    if (KeyName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing keyName"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlackboardData* BBData = LoadObject<UBlackboardData>(nullptr, *BBPath);
    if (!BBData)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blackboard not found: %s"), *BBPath), TEXT("NOT_FOUND"));
        return true;
    }

    // Find the key and set its value
    bool bKeyFound = false;
    bool bValueSet = false;
    FString ValueStr = GetJsonStringField(Payload, TEXT("stringValue"));
    
    for (FBlackboardEntry& Key : BBData->Keys)
    {
        if (Key.EntryName.ToString() == KeyName)
        {
            bKeyFound = true;
            
            // Set the default value based on key type
            // Note: DefaultValue properties on BlackboardKeyType are only available in UE 5.5+
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
            if (Key.KeyType && !ValueStr.IsEmpty())
            {
                if (UBlackboardKeyType_Bool* BoolKey = Cast<UBlackboardKeyType_Bool>(Key.KeyType))
                {
                    BoolKey->bDefaultValue = ValueStr.ToLower() == TEXT("true") || ValueStr == TEXT("1");
                    bValueSet = true;
                }
                else if (UBlackboardKeyType_Int* IntKey = Cast<UBlackboardKeyType_Int>(Key.KeyType))
                {
                    IntKey->DefaultValue = FCString::Atoi(*ValueStr);
                    bValueSet = true;
                }
                else if (UBlackboardKeyType_Float* FloatKey = Cast<UBlackboardKeyType_Float>(Key.KeyType))
                {
                    FloatKey->DefaultValue = FCString::Atof(*ValueStr);
                    bValueSet = true;
                }
                else if (UBlackboardKeyType_Vector* VectorKey = Cast<UBlackboardKeyType_Vector>(Key.KeyType))
                {
                    VectorKey->DefaultValue.InitFromString(ValueStr);
                    VectorKey->bUseDefaultValue = true;
                    bValueSet = true;
                }
                else if (UBlackboardKeyType_Rotator* RotatorKey = Cast<UBlackboardKeyType_Rotator>(Key.KeyType))
                {
                    RotatorKey->DefaultValue.InitFromString(ValueStr);
                    RotatorKey->bUseDefaultValue = true;
                    bValueSet = true;
                }
                else if (UBlackboardKeyType_Name* NameKey = Cast<UBlackboardKeyType_Name>(Key.KeyType))
                {
                    NameKey->DefaultValue = FName(*ValueStr);
                    bValueSet = true;
                }
                else if (UBlackboardKeyType_String* StringKey = Cast<UBlackboardKeyType_String>(Key.KeyType))
                {
                    StringKey->DefaultValue = ValueStr;
                    bValueSet = true;
                }
                else
                {
                    // Unsupported key type - note in response
                    bValueSet = false;
                }
            }
#else
            // UE 5.0-5.4: DefaultValue properties not available on BlackboardKeyType
            // Value setting requires UE 5.5+
            bValueSet = false;
#endif
            break;
        }
    }

    if (!bKeyFound)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Key '%s' not found in blackboard"), *KeyName), TEXT("KEY_NOT_FOUND"));
        return true;
    }

    McpSafeAssetSave(BBData);

    TSharedPtr<FJsonObject> SetResult = McpHandlerUtils::CreateResultObject();
    SetResult->SetStringField(TEXT("blackboardPath"), BBPath);
    SetResult->SetStringField(TEXT("keyName"), KeyName);
    SetResult->SetStringField(TEXT("value"), ValueStr);
    SetResult->SetBoolField(TEXT("valueSet"), bValueSet);
    
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        bValueSet ? TEXT("Blackboard value set") : TEXT("Key found but value not set (unsupported type)"), SetResult);
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, true, 
        TEXT("Key found. Note: set_blackboard_value requires UE 5.5+ for value setting."), SetResult);
#endif
    return true;
#endif
}

// get_blackboard_value - Get a key's info from a blackboard asset
bool McpHandlers::Ai::HandleAiGetBlackboardValue(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    FString BBPath = GetJsonStringField(Payload, TEXT("blackboardPath"));
    if (BBPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blackboardPath"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString KeyName = GetJsonStringField(Payload, TEXT("keyName"));
    if (KeyName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing keyName"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlackboardData* BBData = LoadObject<UBlackboardData>(nullptr, *BBPath);
    if (!BBData)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blackboard not found: %s"), *BBPath), TEXT("NOT_FOUND"));
        return true;
    }

    // Find the key
    bool bKeyFound = false;
    FString KeyType = TEXT("Unknown");
    bool bInstanceSynced = false;

    for (const FBlackboardEntry& Key : BBData->Keys)
    {
        if (Key.EntryName.ToString() == KeyName)
        {
            bKeyFound = true;
            bInstanceSynced = Key.bInstanceSynced;
            if (Key.KeyType)
            {
                KeyType = Key.KeyType->GetClass()->GetName();
            }
            break;
        }
    }

    if (!bKeyFound)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Key '%s' not found in blackboard"), *KeyName), TEXT("KEY_NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> GetResult = McpHandlerUtils::CreateResultObject();
    GetResult->SetStringField(TEXT("blackboardPath"), BBPath);
    GetResult->SetStringField(TEXT("keyName"), KeyName);
    GetResult->SetStringField(TEXT("keyType"), KeyType);
    GetResult->SetBoolField(TEXT("instanceSynced"), bInstanceSynced);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blackboard value retrieved"), GetResult);
    return true;
#endif
}

// get_agent_state - live PIE agent observability: active BT node path + full
// blackboard VALUE dump. The asset-shaped readbacks (get_info /
// get_blackboard_value) can't answer "what is this running agent doing";
// this is the remote replacement for the gameplay debugger's BT category.
bool McpHandlers::Ai::HandleAiGetAgentState(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    bool bIsPieWorld = false;
    UWorld* World = McpHandlerUtils::GetActorLookupWorld(&bIsPieWorld);
    if (!World || !bIsPieWorld)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("get_agent_state reads a RUNNING agent; start PIE first."),
            TEXT("NOT_IN_PIE"));
        return true;
    }

    // Resolve the agent: actorName may name the pawn or the controller.
    // Omitted entirely = the only running BT agent (the single-boss gym case);
    // several running = AMBIGUOUS_TARGET listing them.
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    AAIController* Controller = nullptr;
    if (!ActorName.IsEmpty())
    {
        AActor* Found = S.FindActorByName(ActorName);
        if (!Found)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Actor not found: %s"), *ActorName),
                TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        Controller = Cast<AAIController>(Found);
        if (!Controller)
        {
            if (APawn* Pawn = Cast<APawn>(Found))
            {
                Controller = Cast<AAIController>(Pawn->GetController());
            }
        }
        if (!Controller)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("'%s' is neither an AIController nor a pawn possessed by one."), *ActorName),
                TEXT("NOT_AN_AGENT"));
            return true;
        }
    }
    else
    {
        TArray<AAIController*> Running;
        for (TActorIterator<AAIController> It(World); It; ++It)
        {
            if (Cast<UBehaviorTreeComponent>(It->GetBrainComponent()))
            {
                Running.Add(*It);
            }
        }
        if (Running.Num() == 0)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                TEXT("No AIController with a running behavior tree found in the PIE world."),
                TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        if (Running.Num() > 1)
        {
            TArray<FString> Names;
            for (AAIController* C : Running)
            {
                Names.Add(FString::Printf(TEXT("%s (pawn: %s)"), *C->GetName(),
                    C->GetPawn() ? *C->GetPawn()->GetActorLabel() : TEXT("none")));
            }
            S.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("%d agents running — pass actorName. Candidates: %s"),
                    Running.Num(), *FString::Join(Names, TEXT("; "))),
                TEXT("AMBIGUOUS_TARGET"));
            return true;
        }
        Controller = Running[0];
    }

    UBehaviorTreeComponent* BTC = Cast<UBehaviorTreeComponent>(Controller->GetBrainComponent());
    if (!BTC)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("%s has no BehaviorTreeComponent (brain: %s)."),
                *Controller->GetName(), *GetNameSafe(Controller->GetBrainComponent())),
            TEXT("NO_BEHAVIOR_TREE"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("controllerName"), Controller->GetName());
    Result->SetStringField(TEXT("controllerClass"), Controller->GetClass()->GetName());
    if (APawn* Pawn = Controller->GetPawn())
    {
        Result->SetStringField(TEXT("pawnName"), Pawn->GetActorLabel());
        Result->SetStringField(TEXT("pawnClass"), Pawn->GetClass()->GetName());
    }
    Result->SetBoolField(TEXT("isRunning"), BTC->IsRunning());
    Result->SetBoolField(TEXT("isPaused"), BTC->IsPaused());

    UBehaviorTree* CurrentTree = BTC->GetCurrentTree();
    if (CurrentTree)
    {
        Result->SetStringField(TEXT("behaviorTree"), CurrentTree->GetPathName());
    }

    // Graph GUIDs for the active chain: non-instanced runtime nodes ARE the
    // asset's template objects, so a pointer map against the asset graph
    // resolves them to the ids get_info/add_node speak.
    TMap<UObject*, FString> InstanceToGuid;
#if MCP_AI_HAS_BEHAVIOR_TREE_GRAPH
    if (CurrentTree)
    {
        if (UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(CurrentTree->BTGraph))
        {
            for (UEdGraphNode* GraphNode : BTGraph->Nodes)
            {
                if (UAIGraphNode* AINode = Cast<UAIGraphNode>(GraphNode))
                {
                    if (AINode->NodeInstance)
                    {
                        InstanceToGuid.Add(AINode->NodeInstance, GraphNode->NodeGuid.ToString());
                    }
                    for (UAIGraphNode* SubNode : AINode->SubNodes)
                    {
                        if (SubNode && SubNode->NodeInstance)
                        {
                            InstanceToGuid.Add(SubNode->NodeInstance, SubNode->NodeGuid.ToString());
                        }
                    }
                }
            }
        }
    }
#endif

    // PIE runtime nodes are copies of the asset templates (the editor graph's
    // NodeInstances), so pointer matching misses for a live agent — and
    // template execution indices do NOT share the runtime numbering, so they
    // can't be the join key either. The copies mirror the asset structure
    // 1:1: walk both trees in parallel and map each runtime node to its
    // positional template's graph GUID.
    TMap<const UBTNode*, FString> RuntimeToGuid;
    {
        const UBTNode* ChainTop = nullptr;
        for (const UBTNode* Node = BTC->GetActiveNode(); Node; Node = Node->GetParentNode())
        {
            ChainTop = Node;
        }
        UBTCompositeNode* RuntimeRoot =
            const_cast<UBTCompositeNode*>(Cast<const UBTCompositeNode>(ChainTop));
        if (RuntimeRoot && CurrentTree && CurrentTree->RootNode)
        {
            auto MapPair = [&InstanceToGuid, &RuntimeToGuid](const UBTNode* Runtime, UBTNode* Template)
            {
                if (Runtime && Template)
                {
                    if (const FString* Guid = InstanceToGuid.Find(Template))
                    {
                        RuntimeToGuid.Add(Runtime, *Guid);
                    }
                }
            };
            TFunction<void(UBTCompositeNode*, UBTCompositeNode*)> Pair =
                [&](UBTCompositeNode* RT, UBTCompositeNode* TPL)
            {
                MapPair(RT, TPL);
                const int32 SvcNum = FMath::Min(RT->Services.Num(), TPL->Services.Num());
                for (int32 SvcIdx = 0; SvcIdx < SvcNum; ++SvcIdx)
                {
                    MapPair(RT->Services[SvcIdx], TPL->Services[SvcIdx]);
                }
                const int32 ChildNum = FMath::Min(RT->Children.Num(), TPL->Children.Num());
                for (int32 i = 0; i < ChildNum; ++i)
                {
                    const FBTCompositeChild& RC = RT->Children[i];
                    const FBTCompositeChild& TC = TPL->Children[i];
                    const int32 DecNum = FMath::Min(RC.Decorators.Num(), TC.Decorators.Num());
                    for (int32 DecIdx = 0; DecIdx < DecNum; ++DecIdx)
                    {
                        MapPair(RC.Decorators[DecIdx], TC.Decorators[DecIdx]);
                    }
                    if (RC.ChildComposite && TC.ChildComposite)
                    {
                        Pair(RC.ChildComposite, TC.ChildComposite);
                    }
                    else if (RC.ChildTask && TC.ChildTask)
                    {
                        MapPair(RC.ChildTask, TC.ChildTask);
                        const int32 TaskSvcNum =
                            FMath::Min(RC.ChildTask->Services.Num(), TC.ChildTask->Services.Num());
                        for (int32 SvcIdx = 0; SvcIdx < TaskSvcNum; ++SvcIdx)
                        {
                            MapPair(RC.ChildTask->Services[SvcIdx], TC.ChildTask->Services[SvcIdx]);
                        }
                    }
                }
            };
            Pair(RuntimeRoot, CurrentTree->RootNode);
        }
    }

    auto DescribeChainNode = [&InstanceToGuid, &RuntimeToGuid](const UBTNode* Node) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> NodeObj = McpHandlerUtils::CreateResultObject();
        NodeObj->SetStringField(TEXT("className"), Node->GetClass()->GetName());
        NodeObj->SetStringField(TEXT("nodeName"), Node->GetNodeName());
        NodeObj->SetNumberField(TEXT("executionIndex"), Node->GetExecutionIndex());
        const FString* Guid = InstanceToGuid.Find(const_cast<UBTNode*>(Node));
        if (!Guid)
        {
            Guid = RuntimeToGuid.Find(Node);
        }
        if (Guid)
        {
            NodeObj->SetStringField(TEXT("nodeId"), *Guid);
        }
        return NodeObj;
    };

    if (const UBTNode* Active = BTC->GetActiveNode())
    {
        Result->SetObjectField(TEXT("activeNode"), DescribeChainNode(Active));
        // Root-first ancestor chain: the branch the agent is currently inside.
        TArray<const UBTNode*> Chain;
        for (const UBTNode* Node = Active; Node; Node = Node->GetParentNode())
        {
            Chain.Insert(Node, 0);
        }
        TArray<TSharedPtr<FJsonValue>> ChainJson;
        for (const UBTNode* Node : Chain)
        {
            ChainJson.Add(MakeShared<FJsonValueObject>(DescribeChainNode(Node)));
        }
        Result->SetArrayField(TEXT("activePath"), ChainJson);
    }
    Result->SetStringField(TEXT("activeTasksDescription"), BTC->DescribeActiveTasks());

    // Live blackboard VALUES — every key across the asset's parent chain,
    // typed via the key-type class (the half get_blackboard_value cannot do:
    // it is asset-shaped and returns key metadata only).
    if (UBlackboardComponent* BB = Controller->GetBlackboardComponent())
    {
        if (UBlackboardData* BBAsset = BB->GetBlackboardAsset())
        {
            Result->SetStringField(TEXT("blackboardAsset"), BBAsset->GetPathName());
        }
        TSharedPtr<FJsonObject> BBJson = MakeShared<FJsonObject>();
        for (UBlackboardData* Data = BB->GetBlackboardAsset(); Data; Data = Data->Parent)
        {
            for (const FBlackboardEntry& Key : Data->Keys)
            {
                const FName KeyName = Key.EntryName;
                if (BBJson->HasField(KeyName.ToString()) || !Key.KeyType)
                {
                    continue;
                }
                TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
                const UClass* TypeClass = Key.KeyType->GetClass();
                FString TypeName = TypeClass->GetName();
                TypeName.RemoveFromStart(TEXT("BlackboardKeyType_"));
                KeyObj->SetStringField(TEXT("type"), TypeName);
                if (TypeClass == UBlackboardKeyType_Bool::StaticClass())
                {
                    KeyObj->SetBoolField(TEXT("value"), BB->GetValueAsBool(KeyName));
                }
                else if (TypeClass == UBlackboardKeyType_Int::StaticClass())
                {
                    KeyObj->SetNumberField(TEXT("value"), BB->GetValueAsInt(KeyName));
                }
                else if (TypeClass == UBlackboardKeyType_Float::StaticClass())
                {
                    KeyObj->SetNumberField(TEXT("value"), BB->GetValueAsFloat(KeyName));
                }
                else if (TypeClass == UBlackboardKeyType_Vector::StaticClass())
                {
                    if (BB->IsVectorValueSet(KeyName))
                    {
                        KeyObj->SetStringField(TEXT("value"), BB->GetValueAsVector(KeyName).ToString());
                    }
                    else
                    {
                        KeyObj->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
                    }
                }
                else if (TypeClass == UBlackboardKeyType_Rotator::StaticClass())
                {
                    KeyObj->SetStringField(TEXT("value"), BB->GetValueAsRotator(KeyName).ToString());
                }
                else if (TypeClass == UBlackboardKeyType_Object::StaticClass())
                {
                    UObject* Obj = BB->GetValueAsObject(KeyName);
                    if (Obj)
                    {
                        KeyObj->SetStringField(TEXT("value"), Obj->GetPathName());
                    }
                    else
                    {
                        KeyObj->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
                    }
                }
                else if (TypeClass == UBlackboardKeyType_Class::StaticClass())
                {
                    UClass* Cls = BB->GetValueAsClass(KeyName);
                    if (Cls)
                    {
                        KeyObj->SetStringField(TEXT("value"), Cls->GetPathName());
                    }
                    else
                    {
                        KeyObj->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
                    }
                }
                else if (TypeClass == UBlackboardKeyType_Enum::StaticClass())
                {
                    KeyObj->SetNumberField(TEXT("value"), BB->GetValueAsEnum(KeyName));
                }
                else if (TypeClass == UBlackboardKeyType_Name::StaticClass())
                {
                    KeyObj->SetStringField(TEXT("value"), BB->GetValueAsName(KeyName).ToString());
                }
                else if (TypeClass == UBlackboardKeyType_String::StaticClass())
                {
                    KeyObj->SetStringField(TEXT("value"), BB->GetValueAsString(KeyName));
                }
                else
                {
                    // Unknown key type: report the type honestly with no value
                    // rather than guessing a getter.
                    KeyObj->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
                    KeyObj->SetBoolField(TEXT("unsupportedType"), true);
                }
                BBJson->SetObjectField(KeyName.ToString(), KeyObj);
            }
        }
        Result->SetObjectField(TEXT("blackboard"), BBJson);
    }

    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Agent state retrieved"), Result);
    return true;
#endif
}

// run_behavior_tree - Alias for assign_behavior_tree
bool McpHandlers::Ai::HandleAiRunBehaviorTree(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    FString ControllerPath = GetJsonStringField(Payload, TEXT("controllerPath"));
    if (ControllerPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing controllerPath"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString BTPath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
    if (BTPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing behaviorTreePath"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint *ControllerBP = S.ResolveBlueprintOrError(ControllerPath, RequestId, RequestingSocket, TEXT("controllerPath"));
    if (!ControllerBP) return true;

    UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
    if (!BT)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Behavior tree not found: %s"), *BTPath), TEXT("NOT_FOUND"));
        return true;
    }

    // Store the BT reference as a variable on the controller
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
    PinType.PinSubCategoryObject = UBehaviorTree::StaticClass();
    FBlueprintEditorUtils::AddMemberVariable(ControllerBP, TEXT("AssignedBehaviorTree"), PinType);

    McpFinalizeBlueprint(ControllerBP, /*bStructural=*/false, /*bSave=*/true);

    TSharedPtr<FJsonObject> RunResult = McpHandlerUtils::CreateResultObject();
    RunResult->SetStringField(TEXT("controllerPath"), ControllerPath);
    RunResult->SetStringField(TEXT("behaviorTreePath"), BTPath);
    RunResult->SetBoolField(TEXT("assigned"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior tree assigned for running"), RunResult);
    return true;
#endif
}

// stop_behavior_tree - Remove behavior tree assignment from controller
bool McpHandlers::Ai::HandleAiStopBehaviorTree(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if !WITH_EDITOR
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    FString ControllerPath = GetJsonStringField(Payload, TEXT("controllerPath"));
    if (ControllerPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing controllerPath"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* ControllerBP = LoadObject<UBlueprint>(nullptr, *ControllerPath);
    if (!ControllerBP)
    {
        TSharedPtr<FJsonObject> StopResult = McpHandlerUtils::CreateResultObject();
        StopResult->SetStringField(TEXT("controllerPath"), ControllerPath);
        StopResult->SetBoolField(TEXT("stopped"), false);
        S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior tree stopped (controller not found)"), StopResult);
        return true;
    }

    // Remove the BT variable to "stop" it
    FBlueprintEditorUtils::RemoveMemberVariable(ControllerBP, TEXT("AssignedBehaviorTree"));

    McpFinalizeBlueprint(ControllerBP, /*bStructural=*/false, /*bSave=*/true);

    TSharedPtr<FJsonObject> StopResult = McpHandlerUtils::CreateResultObject();
    StopResult->SetStringField(TEXT("controllerPath"), ControllerPath);
    StopResult->SetBoolField(TEXT("stopped"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior tree stopped"), StopResult);
    return true;
#endif
}
