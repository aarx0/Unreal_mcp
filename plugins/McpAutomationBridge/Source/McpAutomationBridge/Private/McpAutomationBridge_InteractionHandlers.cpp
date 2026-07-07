// =============================================================================
// McpAutomationBridge_InteractionHandlers.cpp
// =============================================================================
// Phase 18: Interaction System Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// --------------------
// Section 1: Interaction Component (18.1)
//   - create_interaction_component       : Add sphere collision interaction component to BP
//   - configure_interaction_trace        : Configure trace type/distance on BP components
//   - configure_interaction_widget       : Set up widget display variables on BP
//   - add_interaction_events             : Add event dispatcher variables to BP
//
// Section 2: Interactables (18.2)
//   - create_interactable_interface      : Create Blueprint Interface for interactables
//   - create_door_actor                  : Create door BP with pivot, mesh, collision nodes
//   - configure_door_properties          : Add/configure door variables (angle, time, lock)
//   - create_switch_actor                : Create switch BP with mesh and trigger nodes
//   - configure_switch_properties        : Add/configure switch variables (type, toggle, reset)
//   - create_chest_actor                 : Create chest BP with base, lid, trigger nodes
//   - configure_chest_properties         : Add/configure chest variables (lock, angle, loot)
//   - create_lever_actor                 : Create lever BP with base, pivot, handle, trigger
//
// Section 3: Destructibles (18.3)
//   - setup_destructible_mesh            : Configure destructible mesh on existing actor
//   - add_destruction_component          : Add destruction SceneComponent with health vars
//
// Section 4: Trigger System (18.4)
//   - create_trigger_actor               : Create trigger BP with shape (box/sphere/capsule)
//   - configure_trigger_events           : Configure trigger event setup
//
// Section 5: Utility
//   - get_info               : Query interaction info for BP or actor
//
// PAYLOAD/RESPONSE FORMATS:
// -------------------------
// create_interaction_component (Blueprint):
//   Payload: { "blueprintPath": string, "componentName"?: string, "traceDistance"?: number }
//   Response: { "componentAdded": bool, "componentName": string, assetVerification... }
//
// configure_interaction_trace:
//   Payload: { "blueprintPath": string, "traceType"?: string, "traceDistance"?: number,
//              "traceRadius"?: number }
//   Response: { "traceType"?: string, "traceDistance"?: number, "traceRadius"?: number,
//              "componentsUpdated": number, "variablesAdded": string[],
//              "appliedProperties": string[], "configured": bool, assetVerification... }
//
// configure_interaction_widget:
//   Payload: { "blueprintPath": string, "widgetClass"?: string, "showOnHover"?: bool,
//              "showPromptText"?: bool, "promptTextFormat"?: string }
//   Response: { "widgetClass"?: string, "showOnHover"?: bool, "showPromptText"?: bool,
//              "promptTextFormat"?: string, "variablesAdded": string[],
//              "appliedProperties": string[], "configured": bool, "blueprintPath": string }
//
// add_interaction_events:
//   Payload: { "blueprintPath": string }
//   Response: { "eventsAdded": string[], "blueprintPath": string, "eventCount": number }
//
// create_interactable_interface:
//   Payload: { "name": string, "folder"?: string }
//   Response: { "interfacePath": string, "interfaceName": string, "created": bool,
//              "recommendedFunctions": string[], "note": string }
//
// create_door_actor (Blueprint):
//   Payload: { "name": string, "folder"?: string, "openAngle"?: number, "openTime"?: number,
//              "autoClose"?: bool, "autoCloseDelay"?: number, "requiresKey"?: bool }
//   Response: { "openAngle": number, "openTime": number, "autoClose": bool,
//              "autoCloseDelay": number, "requiresKey": bool,
//              "appliedProperties": string[], assetVerification... }
//
// configure_door_properties:
//   Payload: { "doorPath": string, "openAngle"?: number, "openTime"?: number, "locked"?: bool,
//              "autoClose"?: bool, "autoCloseDelay"?: number, "requiresKey"?: bool }
//   Response: { applied params echoed, "variablesAdded": string[],
//              "appliedProperties": string[], "configured": bool }
//
// create_switch_actor:
//   Payload: { "name": string, "folder"?: string, "switchType"?: string }
//   Response: { "switchPath": string, "blueprintPath": string, "switchType": string,
//              "appliedProperties": string[] }
//
// configure_switch_properties:
//   Payload: { "switchPath": string, "switchType"?: string, "canToggle"?: bool, "resetTime"?: number }
//   Response: { applied params echoed, "variablesAdded": string[],
//              "appliedProperties": string[], "configured": bool }
//
// create_chest_actor:
//   Payload: { "name": string, "folder"?: string, "locked"?: bool }
//   Response: { "chestPath": string, "blueprintPath": string, "locked": bool,
//              "appliedProperties": string[] }
//
// configure_chest_properties:
//   Payload: { "chestPath": string, "locked"?: bool, "openAngle"?: number, "openTime"?: number,
//              "lootTablePath"?: string }
//   Response: { applied params echoed, "variablesAdded": string[],
//              "appliedProperties": string[], "configured": bool }
//
// create_lever_actor:
//   Payload: { "name": string, "folder"?: string }
//   Response: { "leverPath": string, "blueprintPath": string }
//
// setup_destructible_mesh:
//   Payload: { "actorName": string }
//   Response: { "actorName": string, "configured": bool }
//
// add_destruction_component:
//   Payload: { "blueprintPath": string, "componentName"?: string }
//   Response: { "componentAdded": bool, "componentName": string, "blueprintPath": string,
//              "variablesAdded": string[] }
//
// create_trigger_actor:
//   Payload: { "name": string, "folder"?: string, "triggerShape"?: string }
//   Response: { "triggerPath": string, "blueprintPath": string, "triggerShape": string }
//
// configure_trigger_events:
//   Payload: { "triggerPath": string }
//   Response: { "configured": bool }
//
// get_info:
//   Payload: { "blueprintPath"?: string, "actorName"?: string, "doorPath"?: string,
//              "switchPath"?: string, "chestPath"?: string, "triggerPath"?: string }
//   Response: { "assetType": string, "name"?: string, "variables"?: object,
//              "variableCount"?: number, "components"?: object[],
//              "actorName"?: string, "actorClass"?: string }
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0: BPTYPE_Interface may not be available on BlueprintFactory
// UE 5.1+: BlueprintFactory->BlueprintType = BPTYPE_Interface supported
// UE 5.7+: Use McpSafeAssetSave() instead of UPackage::SavePackage()
// UE 5.7+: SCS component templates owned by SCS_Node (CreateNode + AddNode pattern)
//
// REFACTORING NOTES:
// ------------------
// - All asset saves use McpSafeAssetSave() for UE 5.7+ safety
// - Blueprint SCS operations follow CreateNode -> Configure -> AddNode -> SetParent pattern
// - Path validation uses ValidateAssetCreationPath() and SanitizeAssetName()
// - Security: All paths validated before CreatePackage()
// - Editor-only operations guarded by #if WITH_EDITOR
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"
#include "McpHandlerUtils.h"

// =============================================================================
// Core Includes
// =============================================================================
#include "McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"

// =============================================================================
// Editor-Only Includes
// =============================================================================
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

// =============================================================================
// Component Includes
// =============================================================================
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TimelineComponent.h"

// =============================================================================
// Actor & Asset Includes
// =============================================================================
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "Factories/BlueprintFactory.h"
#include "UObject/Interface.h"
#include "EditorAssetLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#endif // WITH_EDITOR

// =============================================================================
// Logging Category
// =============================================================================
DEFINE_LOG_CATEGORY_STATIC(LogMcpInteractionHandlers, Log, All);

// =============================================================================
// Section 0: Blueprint Variable Helpers
// =============================================================================

#if WITH_EDITOR
// Adds the member variable when missing. Returns true only when newly added.
static bool McpInteractionEnsureVar(UBlueprint* Blueprint, FName VarName, const FEdGraphPinType& Type) {
  for (const FBPVariableDescription& Var : Blueprint->NewVariables) {
    if (Var.VarName == VarName) {
      return false;
    }
  }
  return FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, Type);
}

// Persist a variable default on an already-compiled Blueprint. The value is
// imported into the generated-class CDO (immediate readback; serialized with
// the asset) and recorded in the variable description, which the compiler
// re-imports whenever the class is regenerated. Both writes are required: a
// bare CDO write does not survive a recompile.
static bool McpInteractionApplyVarDefault(UBlueprint* Blueprint, FName VarName,
                                          const FString& Value, FString& OutError) {
  UClass* GeneratedClass = Blueprint->GeneratedClass;
  UObject* CDO = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
  FProperty* Property = GeneratedClass ? FindFProperty<FProperty>(GeneratedClass, VarName) : nullptr;
  if (!CDO || !Property) {
    OutError = FString::Printf(TEXT("Variable '%s' does not exist on the compiled class"), *VarName.ToString());
    return false;
  }
  if (!FBlueprintEditorUtils::PropertyValueFromString(Property, Value, reinterpret_cast<uint8*>(CDO), CDO)) {
    OutError = FString::Printf(TEXT("Value '%s' could not be imported into variable '%s' (%s)"),
                               *Value, *VarName.ToString(), *Property->GetClass()->GetName());
    return false;
  }
  for (FBPVariableDescription& VarDesc : Blueprint->NewVariables) {
    if (VarDesc.VarName == VarName) {
      VarDesc.DefaultValue = Value;
      break;
    }
  }
  return true;
}

// Reads every Blueprint member variable's current value into Result.variables
// (from the CDO, or from Container when inspecting a placed actor instance).
static void McpInteractionAddVariableReadback(const TSharedPtr<FJsonObject>& Result,
                                              UBlueprint* Blueprint,
                                              UObject* Container = nullptr) {
  if (!Container && Blueprint->GeneratedClass) {
    Container = Blueprint->GeneratedClass->GetDefaultObject();
  }
  TSharedPtr<FJsonObject> Variables = MakeShared<FJsonObject>();
  for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables) {
    const FString VarName = VarDesc.VarName.ToString();
    FProperty* Property = Container ? FindFProperty<FProperty>(Container->GetClass(), VarDesc.VarName) : nullptr;
    if (!Property) {
      Variables->SetField(VarName, MakeShared<FJsonValueNull>());
      continue;
    }
    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property)) {
      Variables->SetBoolField(VarName, BoolProp->GetPropertyValue_InContainer(Container));
    } else if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property)) {
      const void* ValuePtr = NumProp->ContainerPtrToValuePtr<void>(Container);
      Variables->SetNumberField(VarName,
                                NumProp->IsFloatingPoint()
                                    ? NumProp->GetFloatingPointPropertyValue(ValuePtr)
                                    : static_cast<double>(NumProp->GetSignedIntPropertyValue(ValuePtr)));
    } else {
      FString ValueString;
      FBlueprintEditorUtils::PropertyValueToString(Property, reinterpret_cast<const uint8*>(Container), ValueString, Container);
      Variables->SetStringField(VarName, ValueString);
    }
  }
  Result->SetObjectField(TEXT("variables"), Variables);
  Result->SetNumberField(TEXT("variableCount"), Blueprint->NewVariables.Num());
}

// Lists the Blueprint's SCS components into Result.components.
static void McpInteractionAddComponentReadback(const TSharedPtr<FJsonObject>& Result, UBlueprint* Blueprint) {
  TArray<TSharedPtr<FJsonValue>> Components;
  if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript) {
    for (USCS_Node* Node : SCS->GetAllNodes()) {
      if (!Node) continue;
      TSharedPtr<FJsonObject> Component = MakeShared<FJsonObject>();
      Component->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
      Component->SetStringField(TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : FString());
      Components.Add(MakeShared<FJsonValueObject>(Component));
    }
  }
  Result->SetArrayField(TEXT("components"), Components);
}
#endif // WITH_EDITOR

// =============================================================================
// Interaction Member Handlers
// =============================================================================
// Dispatch lives in the manage_interaction FMcpCall classes
// (Private/MCP/Calls/McpCalls_ManageInteraction.cpp); each HandleInteraction*
// member here implements one advertised action.

// ===========================================================================
// 18.1 Interaction Component
// ===========================================================================

/**
 * create_interaction_component
 * ----------------------------
 * Adds a sphere collision component to a Blueprint via SCS for interaction detection.
 *
 * Payload: { "blueprintPath": string, "componentName"?: string, "traceDistance"?: number }
 * Response: { "componentAdded": bool, "componentName": string, assetVerification... }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionCreateComponent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
  FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("InteractionComponent"));

  if (BlueprintPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: blueprintPath"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  FString ResolvedPath, LoadError;
  UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
  if (!Blueprint) {
    SendAutomationError(Socket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  if (!Blueprint->SimpleConstructionScript) {
    SendAutomationError(Socket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_BP"));
    return true;
  }

  USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(USphereComponent::StaticClass(), *ComponentName);
  if (Node) {
    USphereComponent* Template = Cast<USphereComponent>(Node->ComponentTemplate);
    if (Template) {
      float TraceDistance = static_cast<float>(GetJsonNumberField(Payload, TEXT("traceDistance"), 200.0));
      Template->SetSphereRadius(TraceDistance);
      Template->SetCollisionProfileName(TEXT("OverlapAll"));
      Template->SetGenerateOverlapEvents(true);
    }
    Blueprint->SimpleConstructionScript->AddNode(Node);
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/false, /*bSave=*/true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("componentAdded"), true);
    Result->SetStringField(TEXT("componentName"), ComponentName);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Interaction component added"), Result);
  } else {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create interaction component"), TEXT("COMPONENT_CREATE_FAILED"));
  }
#else
  SendAutomationError(Socket, RequestId, TEXT("create_interaction_component is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

/**
 * configure_interaction_trace
 * ---------------------------
 * Configures interaction trace parameters on Blueprint shape components and
 * writes TraceDistance/TraceType variable defaults. Only parameters present
 * in the payload are applied; everything else is left untouched.
 *
 * Payload: { "blueprintPath": string, "traceType"?: string, "traceDistance"?: number,
 *            "traceRadius"?: number }
 * Response: { "traceType"?: string, "traceDistance"?: number, "traceRadius"?: number,
 *            "componentsUpdated": number, "variablesAdded": string[],
 *            "appliedProperties": string[], "configured": bool, assetVerification... }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionConfigureTrace(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
  FString TraceType;
  const bool bHasTraceType = Payload->TryGetStringField(TEXT("traceType"), TraceType);
  double TraceDistance = 0.0;
  const bool bHasTraceDistance = Payload->TryGetNumberField(TEXT("traceDistance"), TraceDistance);
  double TraceRadius = 0.0;
  const bool bHasTraceRadius = Payload->TryGetNumberField(TEXT("traceRadius"), TraceRadius);

  if (BlueprintPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: blueprintPath"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  FString ResolvedPath, LoadError;
  UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
  if (!Blueprint) {
    SendAutomationError(Socket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  int32 ComponentsUpdated = 0;

  // Apply requested trace values to existing interaction shape components
  USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
  if (SCS && (bHasTraceDistance || bHasTraceRadius)) {
    for (USCS_Node* Node : SCS->GetAllNodes()) {
      if (!Node || !Node->ComponentClass) continue;

      if (Node->ComponentClass->IsChildOf(USphereComponent::StaticClass())) {
        USphereComponent* SphereComp = Cast<USphereComponent>(Node->ComponentTemplate);
        if (SphereComp && bHasTraceDistance) {
          SphereComp->SetSphereRadius(static_cast<float>(TraceDistance));
          SphereComp->SetCollisionProfileName(TEXT("OverlapAll"));
          SphereComp->SetGenerateOverlapEvents(true);
          ++ComponentsUpdated;
        }
      }
      else if (Node->ComponentClass->IsChildOf(UBoxComponent::StaticClass())) {
        UBoxComponent* BoxComp = Cast<UBoxComponent>(Node->ComponentTemplate);
        if (BoxComp) {
          const FVector CurrentExtent = BoxComp->GetUnscaledBoxExtent();
          const double ExtentX = bHasTraceDistance ? TraceDistance : CurrentExtent.X;
          const double ExtentYZ = bHasTraceRadius ? TraceRadius : CurrentExtent.Y;
          BoxComp->SetBoxExtent(FVector(static_cast<float>(ExtentX), static_cast<float>(ExtentYZ), static_cast<float>(ExtentYZ)));
          BoxComp->SetCollisionProfileName(TEXT("OverlapAll"));
          BoxComp->SetGenerateOverlapEvents(true);
          ++ComponentsUpdated;
        }
      }
    }
  }

  FEdGraphPinType FloatType;
  FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
  FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

  FEdGraphPinType NameType;
  NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

  TArray<TSharedPtr<FJsonValue>> VariablesAdded;
  if (McpInteractionEnsureVar(Blueprint, TEXT("TraceDistance"), FloatType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("TraceDistance")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("TraceType"), NameType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("TraceType")));
  }

  if (!McpSafeCompileBlueprint(Blueprint)) {
    SendAutomationError(Socket, RequestId, TEXT("Blueprint failed to compile; trace values were not written"), TEXT("COMPILE_FAILED"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> AppliedProperties;
  auto ApplyVar = [&](const TCHAR* ParamName, const FName VarName, const FString& Value) -> bool {
    FString ApplyError;
    if (!McpInteractionApplyVarDefault(Blueprint, VarName, Value, ApplyError)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Failed to apply '%s': %s"), ParamName, *ApplyError),
                          TEXT("PROPERTY_WRITE_FAILED"));
      return false;
    }
    AppliedProperties.Add(MakeShared<FJsonValueString>(VarName.ToString()));
    return true;
  };

  if (bHasTraceDistance && !ApplyVar(TEXT("traceDistance"), TEXT("TraceDistance"), FString::SanitizeFloat(TraceDistance))) {
    return true;
  }
  if (bHasTraceType && !ApplyVar(TEXT("traceType"), TEXT("TraceType"), TraceType)) {
    return true;
  }

  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
  SaveLoadedAssetThrottled(Blueprint);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  if (bHasTraceType) {
    Result->SetStringField(TEXT("traceType"), TraceType);
  }
  if (bHasTraceDistance) {
    Result->SetNumberField(TEXT("traceDistance"), TraceDistance);
  }
  if (bHasTraceRadius) {
    Result->SetNumberField(TEXT("traceRadius"), TraceRadius);
  }
  Result->SetNumberField(TEXT("componentsUpdated"), ComponentsUpdated);
  Result->SetArrayField(TEXT("variablesAdded"), VariablesAdded);
  Result->SetArrayField(TEXT("appliedProperties"), AppliedProperties);
  Result->SetBoolField(TEXT("configured"), true);
  McpHandlerUtils::AddVerification(Result, Blueprint);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Interaction trace configured"), Result);
#else
  SendAutomationError(Socket, RequestId, TEXT("configure_interaction_trace is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

/**
 * configure_interaction_widget
 * ----------------------------
 * Configures interaction widget display variables on a Blueprint including
 * hover behavior, prompt text, and widget class references. Only parameters
 * present in the payload are applied; everything else is left untouched.
 *
 * Payload: { "blueprintPath": string, "widgetClass"?: string, "showOnHover"?: bool,
 *            "showPromptText"?: bool, "promptTextFormat"?: string }
 * Response: { "widgetClass"?: string, "showOnHover"?: bool, "showPromptText"?: bool,
 *            "promptTextFormat"?: string, "variablesAdded": string[],
 *            "appliedProperties": string[], "configured": bool, "blueprintPath": string }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionConfigureWidget(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
  FString WidgetClass;
  const bool bHasWidgetClass = Payload->TryGetStringField(TEXT("widgetClass"), WidgetClass) && !WidgetClass.IsEmpty();
  bool ShowOnHover = false;
  const bool bHasShowOnHover = Payload->TryGetBoolField(TEXT("showOnHover"), ShowOnHover);
  bool ShowPromptText = false;
  const bool bHasShowPromptText = Payload->TryGetBoolField(TEXT("showPromptText"), ShowPromptText);
  FString PromptTextFormat;
  const bool bHasPromptTextFormat = Payload->TryGetStringField(TEXT("promptTextFormat"), PromptTextFormat);

  if (BlueprintPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: blueprintPath"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  FString ResolvedPath, LoadError;
  UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
  if (!Blueprint) {
    SendAutomationError(Socket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  // Normalize the widget class to a loadable class object path and verify it
  FString WidgetClassPath = WidgetClass;
  if (bHasWidgetClass && !WidgetClassPath.StartsWith(TEXT("/Script/"))) {
    if (!WidgetClassPath.Contains(TEXT("."))) {
      WidgetClassPath += TEXT(".") + FPaths::GetBaseFilename(WidgetClassPath) + TEXT("_C");
    }
    if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(WidgetClassPath))) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("widgetClass '%s' does not exist"), *WidgetClass),
                          TEXT("WIDGET_CLASS_NOT_FOUND"));
      return true;
    }
  }

  FEdGraphPinType BoolType;
  BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

  FEdGraphPinType StringType;
  StringType.PinCategory = UEdGraphSchema_K2::PC_String;

  FEdGraphPinType SoftClassType;
  SoftClassType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;

  TArray<TSharedPtr<FJsonValue>> VariablesAdded;
  if (McpInteractionEnsureVar(Blueprint, TEXT("bShowOnHover"), BoolType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("bShowOnHover")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("bShowPromptText"), BoolType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("bShowPromptText")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("PromptTextFormat"), StringType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("PromptTextFormat")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("InteractionWidgetClass"), SoftClassType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("InteractionWidgetClass")));
  }

  if (!McpSafeCompileBlueprint(Blueprint)) {
    SendAutomationError(Socket, RequestId, TEXT("Blueprint failed to compile; widget values were not written"), TEXT("COMPILE_FAILED"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> AppliedProperties;
  auto ApplyVar = [&](const TCHAR* ParamName, const FName VarName, const FString& Value) -> bool {
    FString ApplyError;
    if (!McpInteractionApplyVarDefault(Blueprint, VarName, Value, ApplyError)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Failed to apply '%s': %s"), ParamName, *ApplyError),
                          TEXT("PROPERTY_WRITE_FAILED"));
      return false;
    }
    AppliedProperties.Add(MakeShared<FJsonValueString>(VarName.ToString()));
    return true;
  };

  if (bHasShowOnHover && !ApplyVar(TEXT("showOnHover"), TEXT("bShowOnHover"), ShowOnHover ? TEXT("true") : TEXT("false"))) {
    return true;
  }
  if (bHasShowPromptText && !ApplyVar(TEXT("showPromptText"), TEXT("bShowPromptText"), ShowPromptText ? TEXT("true") : TEXT("false"))) {
    return true;
  }
  if (bHasPromptTextFormat && !ApplyVar(TEXT("promptTextFormat"), TEXT("PromptTextFormat"), PromptTextFormat)) {
    return true;
  }
  if (bHasWidgetClass && !ApplyVar(TEXT("widgetClass"), TEXT("InteractionWidgetClass"), WidgetClassPath)) {
    return true;
  }

  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
  SaveLoadedAssetThrottled(Blueprint);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  if (bHasWidgetClass) {
    Result->SetStringField(TEXT("widgetClass"), WidgetClassPath);
  }
  if (bHasShowOnHover) {
    Result->SetBoolField(TEXT("showOnHover"), ShowOnHover);
  }
  if (bHasShowPromptText) {
    Result->SetBoolField(TEXT("showPromptText"), ShowPromptText);
  }
  if (bHasPromptTextFormat) {
    Result->SetStringField(TEXT("promptTextFormat"), PromptTextFormat);
  }
  Result->SetArrayField(TEXT("variablesAdded"), VariablesAdded);
  Result->SetArrayField(TEXT("appliedProperties"), AppliedProperties);
  Result->SetBoolField(TEXT("configured"), true);
  Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
  McpHandlerUtils::AddVerification(Result, Blueprint);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Interaction widget configured"), Result);
#else
  SendAutomationError(Socket, RequestId, TEXT("configure_interaction_widget is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

/**
 * add_interaction_events
 * -----------------------
 * Adds standard interaction event dispatcher variables to a Blueprint:
 * OnInteractionStart, OnInteractionEnd, OnInteractableFound, OnInteractableLost.
 *
 * Payload: { "blueprintPath": string }
 * Response: { "eventsAdded": string[], "blueprintPath": string, "eventCount": number }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionAddEvents(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

#if WITH_EDITOR
  FString ResolvedPath, LoadError;
  UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
  if (!Blueprint) {
    SendAutomationError(Socket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  // Define event dispatchers to add
  TArray<FString> EventNames = { 
    TEXT("OnInteractionStart"), 
    TEXT("OnInteractionEnd"), 
    TEXT("OnInteractableFound"), 
    TEXT("OnInteractableLost") 
  };

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> AddedEvents;

  // Add event dispatcher variables for each event
  FEdGraphPinType DelegateType;
  DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

  for (const FString& EventName : EventNames) {
    // Check if variable already exists
    bool bExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName.ToString() == EventName) {
        bExists = true;
        break;
      }
    }

    if (!bExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*EventName), DelegateType);
      AddedEvents.Add(MakeShared<FJsonValueString>(EventName));
    } else {
      AddedEvents.Add(MakeShared<FJsonValueString>(EventName + TEXT(" (exists)")));
    }
  }

  Result->SetArrayField(TEXT("eventsAdded"), AddedEvents);
  Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
  Result->SetNumberField(TEXT("eventCount"), EventNames.Num());

  McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, /*bSave=*/true);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Interaction events added"), Result);
#else
  SendAutomationError(Socket, RequestId, TEXT("add_interaction_events is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

// ===========================================================================
// 18.2 Interactables
// ===========================================================================

/**
 * create_interactable_interface
 * -----------------------------
 * Creates a Blueprint Interface asset with recommended interaction functions.
 * The interface is created as BPTYPE_Interface with UInterface parent class.
 *
 * NOTE: UE 5.0 may not support BlueprintFactory->BlueprintType = BPTYPE_Interface.
 *       Guarded with ENGINE_MINOR_VERSION >= 1 check.
 *
 * Payload: { "name": string, "folder"?: string }
 * Response: { "interfacePath": string, "interfaceName": string, "created": bool,
 *            "recommendedFunctions": string[], "note": string }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionCreateInteractableInterface(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetJsonStringField(Payload, TEXT("name"));
  FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interfaces"));

  if (Name.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  // Normalize the path
  FString PackagePath = Folder.IsEmpty() ? TEXT("/Game/Interfaces") : Folder;
  if (!PackagePath.StartsWith(TEXT("/"))) { 
    PackagePath = TEXT("/Game/") + PackagePath; 
  }
  FString PackageName = PackagePath / Name;

  // Create the package
  UPackage* Package = CreatePackage(*PackageName);
  if (!Package) {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  // Create a Blueprint Interface
  UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Factory->BlueprintType = BPTYPE_Interface;
#endif
  Factory->ParentClass = UInterface::StaticClass();

  UBlueprint* InterfaceBP = Cast<UBlueprint>(
      Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name), 
                                RF_Public | RF_Standalone, nullptr, GWarn));

  if (InterfaceBP) {
    // Mark as interface type
    InterfaceBP->BlueprintType = BPTYPE_Interface;

    TArray<TSharedPtr<FJsonValue>> FunctionsAdded;
    const TArray<FString> FunctionNames = {
      TEXT("Interact"),
      TEXT("CanInteract"),
      TEXT("GetInteractionPrompt")
    };

    for (const FString& FunctionName : FunctionNames) {
      UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
          InterfaceBP,
          FName(*FunctionName),
          UEdGraph::StaticClass(),
          UEdGraphSchema_K2::StaticClass());
      if (NewGraph) {
        FBlueprintEditorUtils::AddFunctionGraph<UFunction>(InterfaceBP, NewGraph, false, static_cast<UFunction*>(nullptr));
        FunctionsAdded.Add(MakeShared<FJsonValueString>(FunctionName));
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InterfaceBP);
    FAssetRegistryModule::AssetCreated(InterfaceBP);
    McpSafeAssetSave(InterfaceBP);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("interfacePath"), InterfaceBP->GetPathName());
    Result->SetStringField(TEXT("interfaceName"), Name);
    Result->SetBoolField(TEXT("created"), true);

    Result->SetArrayField(TEXT("functionsAdded"), FunctionsAdded);

    SendAutomationResponse(Socket, RequestId, true, TEXT("Interactable interface created"), Result);
  } else {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create interface blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
  }
#else
  SendAutomationError(Socket, RequestId, TEXT("create_interactable_interface is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

/**
 * create_door_actor
 * ------------------
 * Creates a door Blueprint actor with SCS component hierarchy:
 *   Root (SceneComponent)
 *     ├── DoorPivot (SceneComponent)
 *     │   └── DoorMesh (StaticMeshComponent)
 *     └── InteractionTrigger (BoxComponent, OverlapAll)
 *
 * Scaffolds the door variables (OpenAngle, OpenTime, bIsLocked, bIsOpen,
 * bAutoClose, AutoCloseDelay, bRequiresKey) and seeds them with the requested
 * (or default) values. Uses ValidateAssetCreationPath() for path security.
 *
 * Payload: { "name": string, "folder"?: string, "openAngle"?: number,
 *            "openTime"?: number, "autoClose"?: bool, "autoCloseDelay"?: number,
 *            "requiresKey"?: bool }
 * Response: { "openAngle": number, "openTime": number, "autoClose": bool,
 *            "autoCloseDelay": number, "requiresKey": bool,
 *            "appliedProperties": string[], assetVerification... }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionCreateDoorActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetJsonStringField(Payload, TEXT("name"));
  FString Folder = GetJsonStringField(Payload, TEXT("folder"),
      GetJsonStringField(Payload, TEXT("path"),
          GetJsonStringField(Payload, TEXT("savePath"), TEXT("/Game/Interactables"))));
  double OpenAngle = GetJsonNumberField(Payload, TEXT("openAngle"), 90.0);
  double OpenTime = GetJsonNumberField(Payload, TEXT("openTime"), 0.5);
  bool AutoClose = GetJsonBoolField(Payload, TEXT("autoClose"), false);
  double AutoCloseDelay = GetJsonNumberField(Payload, TEXT("autoCloseDelay"), 3.0);
  bool RequiresKey = GetJsonBoolField(Payload, TEXT("requiresKey"), false);

  if (Name.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  // Validate and sanitize the asset creation path
  FString PackageName;
  FString PathError;
  if (!ValidateAssetCreationPath(Folder, Name, PackageName, PathError)) {
    SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
    return true;
  }

  const FString SanitizedName = SanitizeAssetName(Name);
  const FString ObjectPath = PackageName + TEXT(".") + SanitizedName;
  if (UBlueprint* ExistingDoorBP = LoadObject<UBlueprint>(nullptr, *ObjectPath)) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("alreadyExisted"), true);
    TSharedPtr<FJsonObject> IgnoredParams = MakeShared<FJsonObject>();
    for (const TCHAR* ParamName : {TEXT("openAngle"), TEXT("openTime"), TEXT("autoClose"), TEXT("autoCloseDelay"), TEXT("requiresKey")}) {
      if (Payload->HasField(ParamName)) {
        IgnoredParams->SetStringField(ParamName, TEXT("Door already exists; use configure_door_properties to change it."));
      }
    }
    if (IgnoredParams->Values.Num() > 0) {
      Result->SetObjectField(TEXT("ignoredParams"), IgnoredParams);
    }
    McpHandlerUtils::AddVerification(Result, ExistingDoorBP);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Door actor already exists"), Result);
    return true;
  }

  UPackage* Package = CreatePackage(*PackageName);
  if (!Package) {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  if (FindObject<UBlueprint>(Package, *SanitizedName)) {
    SendAutomationError(Socket, RequestId, TEXT("Door blueprint already exists in package but could not be loaded"), TEXT("ASSET_ALREADY_EXISTS"));
    return true;
  }

  UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
  Factory->ParentClass = AActor::StaticClass();
  UBlueprint* DoorBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *SanitizedName, RF_Public | RF_Standalone, nullptr, GWarn));

  if (DoorBP) {
    USimpleConstructionScript* SCS = DoorBP->SimpleConstructionScript;
    
    // Step 1: Create all nodes
    USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
    USCS_Node* PivotNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("DoorPivot"));
    USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("DoorMesh"));
    USCS_Node* CollisionNode = SCS->CreateNode(UBoxComponent::StaticClass(), TEXT("InteractionTrigger"));

    // Step 2: Configure component templates
    if (UBoxComponent* CollisionTemplate = Cast<UBoxComponent>(CollisionNode->ComponentTemplate)) {
      CollisionTemplate->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
      CollisionTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
      CollisionTemplate->SetGenerateOverlapEvents(true);
    }

    // Step 3: Add nodes - Root First, Then Children
    SCS->AddNode(RootNode);

    SCS->AddNode(PivotNode);
    PivotNode->SetParent(RootNode);

    SCS->AddNode(MeshNode);
    MeshNode->SetParent(PivotNode);

    SCS->AddNode(CollisionNode);
    CollisionNode->SetParent(RootNode);

    // Scaffold the door variables and seed the requested (or default) values
    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    McpInteractionEnsureVar(DoorBP, TEXT("OpenAngle"), FloatType);
    McpInteractionEnsureVar(DoorBP, TEXT("OpenTime"), FloatType);
    McpInteractionEnsureVar(DoorBP, TEXT("bIsLocked"), BoolType);
    McpInteractionEnsureVar(DoorBP, TEXT("bIsOpen"), BoolType);
    McpInteractionEnsureVar(DoorBP, TEXT("bAutoClose"), BoolType);
    McpInteractionEnsureVar(DoorBP, TEXT("AutoCloseDelay"), FloatType);
    McpInteractionEnsureVar(DoorBP, TEXT("bRequiresKey"), BoolType);

    if (!McpSafeCompileBlueprint(DoorBP)) {
      SendAutomationError(Socket, RequestId, TEXT("Door blueprint was created but failed to compile; door properties were not written"), TEXT("COMPILE_FAILED"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> AppliedProperties;
    auto ApplyVar = [&](const TCHAR* ParamName, const FName VarName, const FString& Value) -> bool {
      FString ApplyError;
      if (!McpInteractionApplyVarDefault(DoorBP, VarName, Value, ApplyError)) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Failed to apply '%s': %s"), ParamName, *ApplyError),
                            TEXT("PROPERTY_WRITE_FAILED"));
        return false;
      }
      AppliedProperties.Add(MakeShared<FJsonValueString>(VarName.ToString()));
      return true;
    };

    if (!ApplyVar(TEXT("openAngle"), TEXT("OpenAngle"), FString::SanitizeFloat(OpenAngle))) {
      return true;
    }
    if (!ApplyVar(TEXT("openTime"), TEXT("OpenTime"), FString::SanitizeFloat(OpenTime))) {
      return true;
    }
    if (!ApplyVar(TEXT("autoClose"), TEXT("bAutoClose"), AutoClose ? TEXT("true") : TEXT("false"))) {
      return true;
    }
    if (!ApplyVar(TEXT("autoCloseDelay"), TEXT("AutoCloseDelay"), FString::SanitizeFloat(AutoCloseDelay))) {
      return true;
    }
    if (!ApplyVar(TEXT("requiresKey"), TEXT("bRequiresKey"), RequiresKey ? TEXT("true") : TEXT("false"))) {
      return true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(DoorBP);
    SaveLoadedAssetThrottled(DoorBP);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("openAngle"), OpenAngle);
    Result->SetNumberField(TEXT("openTime"), OpenTime);
    Result->SetBoolField(TEXT("autoClose"), AutoClose);
    Result->SetNumberField(TEXT("autoCloseDelay"), AutoCloseDelay);
    Result->SetBoolField(TEXT("requiresKey"), RequiresKey);
    Result->SetArrayField(TEXT("appliedProperties"), AppliedProperties);
    McpHandlerUtils::AddVerification(Result, DoorBP);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Door actor created"), Result);
  } else {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create door blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
  }
#else
  SendAutomationError(Socket, RequestId, TEXT("create_door_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

/**
 * configure_door_properties
 * --------------------------
 * Ensures the door property variables (OpenAngle, OpenTime, bIsLocked, bIsOpen,
 * bAutoClose, AutoCloseDelay, bRequiresKey) exist and writes defaults for the
 * parameters present in the payload; everything else is left untouched.
 *
 * Payload: { "doorPath": string, "openAngle"?: number, "openTime"?: number,
 *            "locked"?: bool, "autoClose"?: bool, "autoCloseDelay"?: number,
 *            "requiresKey"?: bool }
 * Response: { "openAngle"?: number, "openTime"?: number, "locked"?: bool,
 *            "variablesAdded": string[], "appliedProperties": string[],
 *            "configured": bool, "doorPath": string, assetVerification... }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionConfigureDoorProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString DoorPath = GetJsonStringField(Payload, TEXT("doorPath"));
  if (DoorPath.IsEmpty()) {
    DoorPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
  }
  double OpenAngle = 0.0;
  const bool bHasOpenAngle = Payload->TryGetNumberField(TEXT("openAngle"), OpenAngle);
  double OpenTime = 0.0;
  const bool bHasOpenTime = Payload->TryGetNumberField(TEXT("openTime"), OpenTime);
  bool Locked = false;
  const bool bHasLocked = Payload->TryGetBoolField(TEXT("locked"), Locked);
  bool AutoClose = false;
  const bool bHasAutoClose = Payload->TryGetBoolField(TEXT("autoClose"), AutoClose);
  double AutoCloseDelay = 0.0;
  const bool bHasAutoCloseDelay = Payload->TryGetNumberField(TEXT("autoCloseDelay"), AutoCloseDelay);
  bool RequiresKey = false;
  const bool bHasRequiresKey = Payload->TryGetBoolField(TEXT("requiresKey"), RequiresKey);

  if (DoorPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: doorPath"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  FString ResolvedPath, LoadError;
  UBlueprint* Blueprint = LoadBlueprintAsset(DoorPath, ResolvedPath, LoadError);
  if (!Blueprint) {
    SendAutomationError(Socket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  FEdGraphPinType FloatType;
  FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
  FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

  FEdGraphPinType BoolType;
  BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

  TArray<TSharedPtr<FJsonValue>> VariablesAdded;
  if (McpInteractionEnsureVar(Blueprint, TEXT("OpenAngle"), FloatType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("OpenAngle")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("OpenTime"), FloatType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("OpenTime")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("bIsLocked"), BoolType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("bIsLocked")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("bIsOpen"), BoolType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("bIsOpen")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("bAutoClose"), BoolType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("bAutoClose")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("AutoCloseDelay"), FloatType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("AutoCloseDelay")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("bRequiresKey"), BoolType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("bRequiresKey")));
  }

  if (!McpSafeCompileBlueprint(Blueprint)) {
    SendAutomationError(Socket, RequestId, TEXT("Blueprint failed to compile; door properties were not written"), TEXT("COMPILE_FAILED"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> AppliedProperties;
  auto ApplyVar = [&](const TCHAR* ParamName, const FName VarName, const FString& Value) -> bool {
    FString ApplyError;
    if (!McpInteractionApplyVarDefault(Blueprint, VarName, Value, ApplyError)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Failed to apply '%s': %s"), ParamName, *ApplyError),
                          TEXT("PROPERTY_WRITE_FAILED"));
      return false;
    }
    AppliedProperties.Add(MakeShared<FJsonValueString>(VarName.ToString()));
    return true;
  };

  if (bHasOpenAngle && !ApplyVar(TEXT("openAngle"), TEXT("OpenAngle"), FString::SanitizeFloat(OpenAngle))) {
    return true;
  }
  if (bHasOpenTime && !ApplyVar(TEXT("openTime"), TEXT("OpenTime"), FString::SanitizeFloat(OpenTime))) {
    return true;
  }
  if (bHasLocked && !ApplyVar(TEXT("locked"), TEXT("bIsLocked"), Locked ? TEXT("true") : TEXT("false"))) {
    return true;
  }
  if (bHasAutoClose && !ApplyVar(TEXT("autoClose"), TEXT("bAutoClose"), AutoClose ? TEXT("true") : TEXT("false"))) {
    return true;
  }
  if (bHasAutoCloseDelay && !ApplyVar(TEXT("autoCloseDelay"), TEXT("AutoCloseDelay"), FString::SanitizeFloat(AutoCloseDelay))) {
    return true;
  }
  if (bHasRequiresKey && !ApplyVar(TEXT("requiresKey"), TEXT("bRequiresKey"), RequiresKey ? TEXT("true") : TEXT("false"))) {
    return true;
  }

  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
  SaveLoadedAssetThrottled(Blueprint);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  if (bHasOpenAngle) {
    Result->SetNumberField(TEXT("openAngle"), OpenAngle);
  }
  if (bHasOpenTime) {
    Result->SetNumberField(TEXT("openTime"), OpenTime);
  }
  if (bHasLocked) {
    Result->SetBoolField(TEXT("locked"), Locked);
  }
  if (bHasAutoClose) {
    Result->SetBoolField(TEXT("autoClose"), AutoClose);
  }
  if (bHasAutoCloseDelay) {
    Result->SetNumberField(TEXT("autoCloseDelay"), AutoCloseDelay);
  }
  if (bHasRequiresKey) {
    Result->SetBoolField(TEXT("requiresKey"), RequiresKey);
  }
  Result->SetArrayField(TEXT("variablesAdded"), VariablesAdded);
  Result->SetArrayField(TEXT("appliedProperties"), AppliedProperties);
  Result->SetBoolField(TEXT("configured"), true);
  Result->SetStringField(TEXT("doorPath"), DoorPath);
  McpHandlerUtils::AddVerification(Result, Blueprint);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Door properties configured"), Result);
#else
  SendAutomationError(Socket, RequestId, TEXT("configure_door_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

/**
 * create_switch_actor
 * --------------------
 * Creates a switch Blueprint actor with SCS component hierarchy:
 *   Root (SceneComponent)
 *     ├── SwitchMesh (StaticMeshComponent)
 *     └── InteractionTrigger (SphereComponent, radius=100, OverlapAll)
 *
 * Scaffolds the switch variables (SwitchType, bCanToggle, bIsActivated,
 * ResetTime) and seeds SwitchType/bCanToggle. Uses ValidateAssetCreationPath()
 * for path security validation.
 *
 * Payload: { "name": string, "folder"?: string, "switchType"?: string }
 * Response: { "switchPath": string, "blueprintPath": string, "switchType": string,
 *            "appliedProperties": string[], assetVerification... }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionCreateSwitchActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetJsonStringField(Payload, TEXT("name"));
  FString Folder = GetJsonStringField(Payload, TEXT("folder"),
      GetJsonStringField(Payload, TEXT("path"),
          GetJsonStringField(Payload, TEXT("savePath"), TEXT("/Game/Interactables"))));
  FString SwitchType = GetJsonStringField(Payload, TEXT("switchType"), TEXT("button"));

  if (Name.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  // Validate and sanitize the asset creation path
  FString PackageName;
  FString PathError;
  if (!ValidateAssetCreationPath(Folder, Name, PackageName, PathError)) {
    SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
    return true;
  }

  UPackage* Package = CreatePackage(*PackageName);
  if (!Package) {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
  Factory->ParentClass = AActor::StaticClass();
  FString SanitizedName = SanitizeAssetName(Name);
  UBlueprint* SwitchBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *SanitizedName, RF_Public | RF_Standalone, nullptr, GWarn));

  if (SwitchBP) {
    USimpleConstructionScript* SCS = SwitchBP->SimpleConstructionScript;
    
    // Step 1: Create all nodes
    USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
    USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("SwitchMesh"));
    USCS_Node* TriggerNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionTrigger"));

    // Step 2: Configure component templates
    if (USphereComponent* TriggerTemplate = Cast<USphereComponent>(TriggerNode->ComponentTemplate)) {
      TriggerTemplate->SetSphereRadius(100.0f);
      TriggerTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
      TriggerTemplate->SetGenerateOverlapEvents(true);
    }

    // Step 3: Add nodes - Root First
    SCS->AddNode(RootNode);

    SCS->AddNode(MeshNode);
    MeshNode->SetParent(RootNode);

    SCS->AddNode(TriggerNode);
    TriggerNode->SetParent(RootNode);

    // Scaffold the switch variables and seed the requested (or default) values
    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    McpInteractionEnsureVar(SwitchBP, TEXT("SwitchType"), NameType);
    McpInteractionEnsureVar(SwitchBP, TEXT("bCanToggle"), BoolType);
    McpInteractionEnsureVar(SwitchBP, TEXT("bIsActivated"), BoolType);
    McpInteractionEnsureVar(SwitchBP, TEXT("ResetTime"), FloatType);

    if (!McpSafeCompileBlueprint(SwitchBP)) {
      SendAutomationError(Socket, RequestId, TEXT("Switch blueprint was created but failed to compile; switch properties were not written"), TEXT("COMPILE_FAILED"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> AppliedProperties;
    auto ApplyVar = [&](const TCHAR* ParamName, const FName VarName, const FString& Value) -> bool {
      FString ApplyError;
      if (!McpInteractionApplyVarDefault(SwitchBP, VarName, Value, ApplyError)) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Failed to apply '%s': %s"), ParamName, *ApplyError),
                            TEXT("PROPERTY_WRITE_FAILED"));
        return false;
      }
      AppliedProperties.Add(MakeShared<FJsonValueString>(VarName.ToString()));
      return true;
    };

    if (!ApplyVar(TEXT("switchType"), TEXT("SwitchType"), SwitchType)) {
      return true;
    }
    if (!ApplyVar(TEXT("canToggle"), TEXT("bCanToggle"), TEXT("true"))) {
      return true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(SwitchBP);
    SaveLoadedAssetThrottled(SwitchBP);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("switchPath"), SwitchBP->GetPathName());
    Result->SetStringField(TEXT("blueprintPath"), SwitchBP->GetPathName());
    Result->SetStringField(TEXT("switchType"), SwitchType);
    Result->SetArrayField(TEXT("appliedProperties"), AppliedProperties);
    McpHandlerUtils::AddVerification(Result, SwitchBP);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Switch actor created"), Result);
  } else {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create switch blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
  }
#else
  SendAutomationError(Socket, RequestId, TEXT("create_switch_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

/**
 * configure_switch_properties
 * ----------------------------
 * Ensures the switch property variables (SwitchType, bCanToggle, bIsActivated,
 * ResetTime) exist and writes defaults for the parameters present in the
 * payload; everything else is left untouched.
 *
 * Payload: { "switchPath": string, "switchType"?: string, "canToggle"?: bool,
 *            "resetTime"?: number }
 * Response: { "switchType"?: string, "canToggle"?: bool, "resetTime"?: number,
 *            "variablesAdded": string[], "appliedProperties": string[],
 *            "configured": bool, "switchPath": string, assetVerification... }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionConfigureSwitchProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString SwitchPath = GetJsonStringField(Payload, TEXT("switchPath"));
  if (SwitchPath.IsEmpty()) {
    SwitchPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
  }
  FString SwitchType;
  const bool bHasSwitchType = Payload->TryGetStringField(TEXT("switchType"), SwitchType);
  bool CanToggle = false;
  const bool bHasCanToggle = Payload->TryGetBoolField(TEXT("canToggle"), CanToggle);
  double ResetTime = 0.0;
  const bool bHasResetTime = Payload->TryGetNumberField(TEXT("resetTime"), ResetTime);

  if (SwitchPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: switchPath"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  FString ResolvedPath, LoadError;
  UBlueprint* Blueprint = LoadBlueprintAsset(SwitchPath, ResolvedPath, LoadError);
  if (!Blueprint) {
    SendAutomationError(Socket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  FEdGraphPinType NameType;
  NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

  FEdGraphPinType BoolType;
  BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

  FEdGraphPinType FloatType;
  FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
  FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

  TArray<TSharedPtr<FJsonValue>> VariablesAdded;
  if (McpInteractionEnsureVar(Blueprint, TEXT("SwitchType"), NameType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("SwitchType")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("bCanToggle"), BoolType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("bCanToggle")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("bIsActivated"), BoolType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("bIsActivated")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("ResetTime"), FloatType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("ResetTime")));
  }

  if (!McpSafeCompileBlueprint(Blueprint)) {
    SendAutomationError(Socket, RequestId, TEXT("Blueprint failed to compile; switch properties were not written"), TEXT("COMPILE_FAILED"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> AppliedProperties;
  auto ApplyVar = [&](const TCHAR* ParamName, const FName VarName, const FString& Value) -> bool {
    FString ApplyError;
    if (!McpInteractionApplyVarDefault(Blueprint, VarName, Value, ApplyError)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Failed to apply '%s': %s"), ParamName, *ApplyError),
                          TEXT("PROPERTY_WRITE_FAILED"));
      return false;
    }
    AppliedProperties.Add(MakeShared<FJsonValueString>(VarName.ToString()));
    return true;
  };

  if (bHasSwitchType && !ApplyVar(TEXT("switchType"), TEXT("SwitchType"), SwitchType)) {
    return true;
  }
  if (bHasCanToggle && !ApplyVar(TEXT("canToggle"), TEXT("bCanToggle"), CanToggle ? TEXT("true") : TEXT("false"))) {
    return true;
  }
  if (bHasResetTime && !ApplyVar(TEXT("resetTime"), TEXT("ResetTime"), FString::SanitizeFloat(ResetTime))) {
    return true;
  }

  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
  SaveLoadedAssetThrottled(Blueprint);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  if (bHasSwitchType) {
    Result->SetStringField(TEXT("switchType"), SwitchType);
  }
  if (bHasCanToggle) {
    Result->SetBoolField(TEXT("canToggle"), CanToggle);
  }
  if (bHasResetTime) {
    Result->SetNumberField(TEXT("resetTime"), ResetTime);
  }
  Result->SetArrayField(TEXT("variablesAdded"), VariablesAdded);
  Result->SetArrayField(TEXT("appliedProperties"), AppliedProperties);
  Result->SetBoolField(TEXT("configured"), true);
  Result->SetStringField(TEXT("switchPath"), SwitchPath);
  McpHandlerUtils::AddVerification(Result, Blueprint);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Switch properties configured"), Result);
#else
  SendAutomationError(Socket, RequestId, TEXT("configure_switch_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

/**
 * create_chest_actor
 * -------------------
 * Creates a chest Blueprint actor with SCS component hierarchy:
 *   Root (SceneComponent)
 *     ├── ChestBase (StaticMeshComponent)
 *     ├── LidPivot (SceneComponent)
 *     │   └── LidMesh (StaticMeshComponent)
 *     └── InteractionTrigger (SphereComponent, radius=150, OverlapAll)
 *
 * Scaffolds the chest variables (bIsLocked, bIsOpen, LidOpenAngle, OpenTime,
 * LootTable) and seeds bIsLocked/LidOpenAngle/OpenTime.
 *
 * Payload: { "name": string, "folder"?: string, "locked"?: bool }
 * Response: { "chestPath": string, "blueprintPath": string, "locked": bool,
 *            "appliedProperties": string[], assetVerification... }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionCreateChestActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetJsonStringField(Payload, TEXT("name"));
  FString Folder = GetJsonStringField(Payload, TEXT("folder"),
      GetJsonStringField(Payload, TEXT("path"),
          GetJsonStringField(Payload, TEXT("savePath"), TEXT("/Game/Interactables"))));
  bool Locked = GetJsonBoolField(Payload, TEXT("locked"), false);

  if (Name.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  FString PackagePath = Folder.IsEmpty() ? TEXT("/Game/Interactables") : Folder;
  if (!PackagePath.StartsWith(TEXT("/"))) { PackagePath = TEXT("/Game/") + PackagePath; }
  FString PackageName = PackagePath / Name;
  UPackage* Package = CreatePackage(*PackageName);
  if (!Package) {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
  Factory->ParentClass = AActor::StaticClass();
  UBlueprint* ChestBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));

  if (ChestBP) {
    USimpleConstructionScript* SCS = ChestBP->SimpleConstructionScript;
    
    // Step 1: Create all nodes
    USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
    USCS_Node* BaseMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("ChestBase"));
    USCS_Node* LidPivotNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("LidPivot"));
    USCS_Node* LidMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("LidMesh"));
    USCS_Node* TriggerNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionTrigger"));

    // Step 2: Configure component templates
    if (USphereComponent* TriggerTemplate = Cast<USphereComponent>(TriggerNode->ComponentTemplate)) {
      TriggerTemplate->SetSphereRadius(150.0f);
      TriggerTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
      TriggerTemplate->SetGenerateOverlapEvents(true);
    }

    // Step 3: Add nodes - Root First
    SCS->AddNode(RootNode);

    SCS->AddNode(BaseMeshNode);
    BaseMeshNode->SetParent(RootNode);

    SCS->AddNode(LidPivotNode);
    LidPivotNode->SetParent(RootNode);

    SCS->AddNode(LidMeshNode);
    LidMeshNode->SetParent(LidPivotNode);

    SCS->AddNode(TriggerNode);
    TriggerNode->SetParent(RootNode);

    // Scaffold the chest variables and seed the requested (or default) values
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

    McpInteractionEnsureVar(ChestBP, TEXT("bIsLocked"), BoolType);
    McpInteractionEnsureVar(ChestBP, TEXT("bIsOpen"), BoolType);
    McpInteractionEnsureVar(ChestBP, TEXT("LidOpenAngle"), FloatType);
    McpInteractionEnsureVar(ChestBP, TEXT("OpenTime"), FloatType);
    McpInteractionEnsureVar(ChestBP, TEXT("LootTable"), SoftObjectType);

    if (!McpSafeCompileBlueprint(ChestBP)) {
      SendAutomationError(Socket, RequestId, TEXT("Chest blueprint was created but failed to compile; chest properties were not written"), TEXT("COMPILE_FAILED"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> AppliedProperties;
    auto ApplyVar = [&](const TCHAR* ParamName, const FName VarName, const FString& Value) -> bool {
      FString ApplyError;
      if (!McpInteractionApplyVarDefault(ChestBP, VarName, Value, ApplyError)) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Failed to apply '%s': %s"), ParamName, *ApplyError),
                            TEXT("PROPERTY_WRITE_FAILED"));
        return false;
      }
      AppliedProperties.Add(MakeShared<FJsonValueString>(VarName.ToString()));
      return true;
    };

    if (!ApplyVar(TEXT("locked"), TEXT("bIsLocked"), Locked ? TEXT("true") : TEXT("false"))) {
      return true;
    }
    if (!ApplyVar(TEXT("lidOpenAngle"), TEXT("LidOpenAngle"), FString::SanitizeFloat(90.0))) {
      return true;
    }
    if (!ApplyVar(TEXT("openTime"), TEXT("OpenTime"), FString::SanitizeFloat(0.5))) {
      return true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(ChestBP);
    SaveLoadedAssetThrottled(ChestBP);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("chestPath"), ChestBP->GetPathName());
    Result->SetStringField(TEXT("blueprintPath"), ChestBP->GetPathName());
    Result->SetBoolField(TEXT("locked"), Locked);
    Result->SetArrayField(TEXT("appliedProperties"), AppliedProperties);
    McpHandlerUtils::AddVerification(Result, ChestBP);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Chest actor created"), Result);
  } else {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create chest blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
  }
#else
  SendAutomationError(Socket, RequestId, TEXT("create_chest_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

/**
 * configure_chest_properties
 * ---------------------------
 * Ensures the chest property variables (bIsLocked, bIsOpen, LidOpenAngle,
 * OpenTime, LootTable) exist and writes defaults for the parameters present
 * in the payload; everything else is left untouched.
 *
 * Payload: { "chestPath": string, "locked"?: bool, "openAngle"?: number,
 *            "openTime"?: number, "lootTablePath"?: string }
 * Response: { "locked"?: bool, "openAngle"?: number, "openTime"?: number,
 *            "lootTablePath"?: string, "variablesAdded": string[],
 *            "appliedProperties": string[], "configured": bool,
 *            "chestPath": string, assetVerification... }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionConfigureChestProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString ChestPath = GetJsonStringField(Payload, TEXT("chestPath"));
  if (ChestPath.IsEmpty()) {
    ChestPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
  }
  bool Locked = false;
  const bool bHasLocked = Payload->TryGetBoolField(TEXT("locked"), Locked);
  double OpenAngle = 0.0;
  const bool bHasOpenAngle = Payload->TryGetNumberField(TEXT("openAngle"), OpenAngle);
  double OpenTime = 0.0;
  const bool bHasOpenTime = Payload->TryGetNumberField(TEXT("openTime"), OpenTime);
  FString LootTablePath = GetJsonStringField(Payload, TEXT("lootTablePath"));

  if (ChestPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: chestPath"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  FString ResolvedPath, LoadError;
  UBlueprint* Blueprint = LoadBlueprintAsset(ChestPath, ResolvedPath, LoadError);
  if (!Blueprint) {
    SendAutomationError(Socket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  // Resolve the loot table before mutating anything
  FString LootTableObjectPath = LootTablePath;
  if (!LootTablePath.IsEmpty()) {
    if (!LootTableObjectPath.Contains(TEXT("."))) {
      LootTableObjectPath += TEXT(".") + FPaths::GetBaseFilename(LootTableObjectPath);
    }
    if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(LootTableObjectPath))) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("lootTablePath '%s' does not exist"), *LootTablePath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
  }

  FEdGraphPinType BoolType;
  BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

  FEdGraphPinType FloatType;
  FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
  FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

  FEdGraphPinType SoftObjectType;
  SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

  TArray<TSharedPtr<FJsonValue>> VariablesAdded;
  if (McpInteractionEnsureVar(Blueprint, TEXT("bIsLocked"), BoolType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("bIsLocked")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("bIsOpen"), BoolType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("bIsOpen")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("LidOpenAngle"), FloatType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("LidOpenAngle")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("OpenTime"), FloatType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("OpenTime")));
  }
  if (McpInteractionEnsureVar(Blueprint, TEXT("LootTable"), SoftObjectType)) {
    VariablesAdded.Add(MakeShared<FJsonValueString>(TEXT("LootTable")));
  }

  if (!McpSafeCompileBlueprint(Blueprint)) {
    SendAutomationError(Socket, RequestId, TEXT("Blueprint failed to compile; chest properties were not written"), TEXT("COMPILE_FAILED"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> AppliedProperties;
  auto ApplyVar = [&](const TCHAR* ParamName, const FName VarName, const FString& Value) -> bool {
    FString ApplyError;
    if (!McpInteractionApplyVarDefault(Blueprint, VarName, Value, ApplyError)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Failed to apply '%s': %s"), ParamName, *ApplyError),
                          TEXT("PROPERTY_WRITE_FAILED"));
      return false;
    }
    AppliedProperties.Add(MakeShared<FJsonValueString>(VarName.ToString()));
    return true;
  };

  if (bHasLocked && !ApplyVar(TEXT("locked"), TEXT("bIsLocked"), Locked ? TEXT("true") : TEXT("false"))) {
    return true;
  }
  if (bHasOpenAngle && !ApplyVar(TEXT("openAngle"), TEXT("LidOpenAngle"), FString::SanitizeFloat(OpenAngle))) {
    return true;
  }
  if (bHasOpenTime && !ApplyVar(TEXT("openTime"), TEXT("OpenTime"), FString::SanitizeFloat(OpenTime))) {
    return true;
  }
  if (!LootTablePath.IsEmpty() && !ApplyVar(TEXT("lootTablePath"), TEXT("LootTable"), LootTableObjectPath)) {
    return true;
  }

  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
  SaveLoadedAssetThrottled(Blueprint);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  if (bHasLocked) {
    Result->SetBoolField(TEXT("locked"), Locked);
  }
  if (bHasOpenAngle) {
    Result->SetNumberField(TEXT("openAngle"), OpenAngle);
  }
  if (bHasOpenTime) {
    Result->SetNumberField(TEXT("openTime"), OpenTime);
  }
  if (!LootTablePath.IsEmpty()) {
    Result->SetStringField(TEXT("lootTablePath"), LootTableObjectPath);
  }
  Result->SetArrayField(TEXT("variablesAdded"), VariablesAdded);
  Result->SetArrayField(TEXT("appliedProperties"), AppliedProperties);
  Result->SetBoolField(TEXT("configured"), true);
  Result->SetStringField(TEXT("chestPath"), ChestPath);
  McpHandlerUtils::AddVerification(Result, Blueprint);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Chest properties configured"), Result);
#else
  SendAutomationError(Socket, RequestId, TEXT("configure_chest_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

/**
 * create_lever_actor
 * -------------------
 * Creates a lever Blueprint actor with SCS component hierarchy:
 *   Root (SceneComponent)
 *     ├── LeverBase (StaticMeshComponent)
 *     ├── LeverPivot (SceneComponent)
 *     │   └── LeverHandle (StaticMeshComponent)
 *     └── InteractionTrigger (SphereComponent, radius=100, OverlapAll)
 *
 * Payload: { "name": string, "folder"?: string }
 * Response: { "leverPath": string, "blueprintPath": string }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionCreateLeverActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetJsonStringField(Payload, TEXT("name"));
  FString Folder = GetJsonStringField(Payload, TEXT("folder"),
      GetJsonStringField(Payload, TEXT("path"),
          GetJsonStringField(Payload, TEXT("savePath"), TEXT("/Game/Interactables"))));

  if (Name.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  FString PackagePath = Folder.IsEmpty() ? TEXT("/Game/Interactables") : Folder;
  if (!PackagePath.StartsWith(TEXT("/"))) { PackagePath = TEXT("/Game/") + PackagePath; }
  FString PackageName = PackagePath / Name;
  UPackage* Package = CreatePackage(*PackageName);
  if (!Package) {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
  Factory->ParentClass = AActor::StaticClass();
  UBlueprint* LeverBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));

  if (LeverBP) {
    USimpleConstructionScript* SCS = LeverBP->SimpleConstructionScript;
    
    // Step 1: Create all nodes
    USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
    USCS_Node* BaseMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("LeverBase"));
    USCS_Node* PivotNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("LeverPivot"));
    USCS_Node* HandleMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("LeverHandle"));
    USCS_Node* TriggerNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionTrigger"));

    if (USphereComponent* TriggerTemplate = Cast<USphereComponent>(TriggerNode->ComponentTemplate)) {
      TriggerTemplate->SetSphereRadius(100.0f);
      TriggerTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
      TriggerTemplate->SetGenerateOverlapEvents(true);
    }

    // Step 3: Add nodes - Root First
    SCS->AddNode(RootNode);

    SCS->AddNode(BaseMeshNode);
    BaseMeshNode->SetParent(RootNode);

    SCS->AddNode(PivotNode);
    PivotNode->SetParent(RootNode);

    SCS->AddNode(HandleMeshNode);
    HandleMeshNode->SetParent(PivotNode);

    SCS->AddNode(TriggerNode);
    TriggerNode->SetParent(RootNode);

    McpFinalizeBlueprint(LeverBP, /*bStructural=*/false, /*bSave=*/true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("leverPath"), LeverBP->GetPathName());
    Result->SetStringField(TEXT("blueprintPath"), LeverBP->GetPathName());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Lever actor created"), Result);
  } else {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create lever blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
  }
#else
  SendAutomationError(Socket, RequestId, TEXT("create_lever_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

// ===========================================================================
// 18.3 Destructibles
// ===========================================================================

/**
 * setup_destructible_mesh
 * ------------------------
 * Configures destructible mesh settings on an existing actor in the editor world.
 * Finds the actor by name/label and sets up destruction parameters.
 *
 * Payload: { "actorName": string }
 * Response: { "actorName": string, "configured": bool }
 */

/**
 * add_destruction_component
 * --------------------------
 * Adds a destruction SceneComponent to a Blueprint via SCS and creates
 * health/destruction-related variables: Health, MaxHealth, bIsDestroyed, DestructionStage.
 *
 * Payload: { "blueprintPath": string, "componentName"?: string }
 * Response: { "componentAdded": bool, "componentName": string,
 *            "blueprintPath": string, "variablesAdded": string[] }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionAddDestructionComponent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
  FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("DestructionComponent"));

#if WITH_EDITOR
  FString ResolvedPath, LoadError;
  UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
  if (!Blueprint) {
    SendAutomationError(Socket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
  if (!SCS) {
    SendAutomationError(Socket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("NO_SCS"));
    return true;
  }

  // Create a SceneComponent for destruction (allows hierarchy and proper transform)
  USCS_Node* Node = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
  if (Node) {
    SCS->AddNode(Node);

    // Add destruction-related Blueprint variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType IntType;
    IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

    // Add Health variable
    bool bHealthExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("Health")) {
        bHealthExists = true;
        break;
      }
    }
    if (!bHealthExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("Health"), FloatType);
    }

    // Add MaxHealth variable
    bool bMaxHealthExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("MaxHealth")) {
        bMaxHealthExists = true;
        break;
      }
    }
    if (!bMaxHealthExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MaxHealth"), FloatType);
    }

    // Add bIsDestroyed variable
    bool bDestroyedExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("bIsDestroyed")) {
        bDestroyedExists = true;
        break;
      }
    }
    if (!bDestroyedExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bIsDestroyed"), BoolType);
    }

    // Add DestructionStage variable
    bool bStageExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("DestructionStage")) {
        bStageExists = true;
        break;
      }
    }
    if (!bStageExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("DestructionStage"), IntType);
    }

    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, /*bSave=*/true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("componentAdded"), true);
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

    TArray<TSharedPtr<FJsonValue>> AddedVars;
    AddedVars.Add(MakeShared<FJsonValueString>(TEXT("Health")));
    AddedVars.Add(MakeShared<FJsonValueString>(TEXT("MaxHealth")));
    AddedVars.Add(MakeShared<FJsonValueString>(TEXT("bIsDestroyed")));
    AddedVars.Add(MakeShared<FJsonValueString>(TEXT("DestructionStage")));
    Result->SetArrayField(TEXT("variablesAdded"), AddedVars);

    SendAutomationResponse(Socket, RequestId, true, TEXT("Destruction component added"), Result);
  } else {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create destruction component"), TEXT("COMPONENT_CREATE_FAILED"));
  }
#else
  SendAutomationError(Socket, RequestId, TEXT("add_destruction_component is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

// ===========================================================================
// 18.4 Trigger System
// ===========================================================================

/**
 * create_trigger_actor
 * ---------------------
 * Creates a trigger Blueprint actor with a configurable shape component
 * (box, sphere, or capsule) as the root trigger volume.
 *
 * Supported shapes:
 *   - "sphere": USphereComponent (radius=200)
 *   - "capsule": UCapsuleComponent (radius=50, halfHeight=100)
 *   - "box" (default): UBoxComponent (extent=100x100x100)
 *
 * Payload: { "name": string, "folder"?: string, "triggerShape"?: string }
 * Response: { "triggerPath": string, "blueprintPath": string, "triggerShape": string }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionCreateTriggerActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetJsonStringField(Payload, TEXT("name"));
  FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Triggers"));
  FString TriggerShape = GetJsonStringField(Payload, TEXT("triggerShape"), TEXT("box"));

  if (Name.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
    return true;
  }

#if WITH_EDITOR
  FString PackagePath = Folder.IsEmpty() ? TEXT("/Game/Triggers") : Folder;
  if (!PackagePath.StartsWith(TEXT("/"))) { PackagePath = TEXT("/Game/") + PackagePath; }
  FString PackageName = PackagePath / Name;
  UPackage* Package = CreatePackage(*PackageName);
  if (!Package) {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
  Factory->ParentClass = AActor::StaticClass();
  UBlueprint* TriggerBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));

  if (TriggerBP) {
    USCS_Node* RootNode = nullptr;
    if (TriggerShape == TEXT("sphere")) {
      RootNode = TriggerBP->SimpleConstructionScript->CreateNode(USphereComponent::StaticClass(), TEXT("TriggerVolume"));
      if (RootNode) {
        USphereComponent* SphereTemplate = Cast<USphereComponent>(RootNode->ComponentTemplate);
        if (SphereTemplate) {
          SphereTemplate->SetSphereRadius(200.0f);
          SphereTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
          SphereTemplate->SetGenerateOverlapEvents(true);
        }
      }
    } else if (TriggerShape == TEXT("capsule")) {
      RootNode = TriggerBP->SimpleConstructionScript->CreateNode(UCapsuleComponent::StaticClass(), TEXT("TriggerVolume"));
      if (RootNode) {
        UCapsuleComponent* CapsuleTemplate = Cast<UCapsuleComponent>(RootNode->ComponentTemplate);
        if (CapsuleTemplate) {
          CapsuleTemplate->SetCapsuleSize(50.0f, 100.0f);
          CapsuleTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
          CapsuleTemplate->SetGenerateOverlapEvents(true);
        }
      }
    } else {
      RootNode = TriggerBP->SimpleConstructionScript->CreateNode(UBoxComponent::StaticClass(), TEXT("TriggerVolume"));
      if (RootNode) {
        UBoxComponent* BoxTemplate = Cast<UBoxComponent>(RootNode->ComponentTemplate);
        if (BoxTemplate) {
          BoxTemplate->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
          BoxTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
          BoxTemplate->SetGenerateOverlapEvents(true);
        }
      }
    }

    if (RootNode) { TriggerBP->SimpleConstructionScript->AddNode(RootNode); }

    McpFinalizeBlueprint(TriggerBP, /*bStructural=*/false, /*bSave=*/true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("triggerPath"), TriggerBP->GetPathName());
    Result->SetStringField(TEXT("blueprintPath"), TriggerBP->GetPathName());
    Result->SetStringField(TEXT("triggerShape"), TriggerShape);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Trigger actor created"), Result);
  } else {
    SendAutomationError(Socket, RequestId, TEXT("Failed to create trigger blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
  }
#else
  SendAutomationError(Socket, RequestId, TEXT("create_trigger_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
  return true;
}

/**
 * configure_trigger_events
 * -------------------------
 * Configures trigger event setup on a Blueprint. Marks the Blueprint as modified
 * and saves it.
 *
 * Payload: { "triggerPath": string }
 * Response: { "configured": bool }
 */

// ===========================================================================

/**
 * configure_destruction_levels
 * -----------------------------
 * Configures destruction levels on an actor.
 */

/**
 * configure_destruction_effects
 * -------------------------------
 * Configures destruction effects on an actor.
 */

/**
 * configure_destruction_damage
 * -----------------------------
 * Configures destruction damage on an actor.
 */

/**
 * configure_trigger_filter
 * -------------------------
 * Configures trigger filter on a Blueprint.
 */

/**
 * configure_trigger_response
 * ----------------------------
 * Configures trigger response on a Blueprint.
 */

// Section 5: Utility Handlers
// ===========================================================================

/**
 * get_info
 * ---------------------
 * Retrieves interaction information for a Blueprint or actor, including the
 * current value of every Blueprint variable (read from the CDO, or from the
 * actor instance when queried by actorName) and the SCS component list.
 * Can query by blueprintPath, actorName, doorPath, switchPath, chestPath, or triggerPath.
 *
 * Payload: { "blueprintPath"?: string, "actorName"?: string, "doorPath"?: string,
 *            "switchPath"?: string, "chestPath"?: string, "triggerPath"?: string }
 * Response: { "assetType": string, "name"?: string, "variables"?: object,
 *            "variableCount"?: number, "components"?: object[],
 *            "actorName"?: string, "actorClass"?: string, ... }
 */
bool UMcpAutomationBridgeSubsystem::HandleInteractionGetInfo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
  FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
  FString DoorPath = GetJsonStringField(Payload, TEXT("doorPath"));
  FString SwitchPath = GetJsonStringField(Payload, TEXT("switchPath"));
  FString ChestPath = GetJsonStringField(Payload, TEXT("chestPath"));
  FString TriggerPath = GetJsonStringField(Payload, TEXT("triggerPath"));

  // Validate that at least one path is provided
  if (BlueprintPath.IsEmpty() && ActorName.IsEmpty() && DoorPath.IsEmpty() &&
      SwitchPath.IsEmpty() && ChestPath.IsEmpty() && TriggerPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("At least one path parameter is required (blueprintPath, actorName, doorPath, switchPath, chestPath, or triggerPath)"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if WITH_EDITOR
  if (BlueprintPath.IsEmpty() && !ActorName.IsEmpty()) {
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationError(Socket, RequestId, TEXT("No editor world available"), TEXT("NO_WORLD"));
      return true;
    }
    AActor* FoundActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (It->GetActorLabel() == ActorName || It->GetName() == ActorName) { FoundActor = *It; break; }
    }
    if (!FoundActor) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Actor not found: %s"), *ActorName),
                          TEXT("ACTOR_NOT_FOUND"));
      return true;
    }
    Result->SetStringField(TEXT("assetType"), TEXT("Actor"));
    Result->SetStringField(TEXT("actorName"), FoundActor->GetName());
    Result->SetStringField(TEXT("actorClass"), FoundActor->GetClass()->GetName());
    if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(FoundActor->GetClass())) {
      if (UBlueprint* ActorBP = Cast<UBlueprint>(BPGC->ClassGeneratedBy)) {
        McpInteractionAddVariableReadback(Result, ActorBP, FoundActor);
      }
    }
  } else {
    const TCHAR* AssetType = TEXT("Blueprint");
    const TCHAR* PathField = TEXT("blueprintPath");
    FString Path = BlueprintPath;
    if (BlueprintPath.IsEmpty()) {
      if (!DoorPath.IsEmpty()) {
        AssetType = TEXT("Door");
        PathField = TEXT("doorPath");
        Path = DoorPath;
      } else if (!SwitchPath.IsEmpty()) {
        AssetType = TEXT("Switch");
        PathField = TEXT("switchPath");
        Path = SwitchPath;
      } else if (!ChestPath.IsEmpty()) {
        AssetType = TEXT("Chest");
        PathField = TEXT("chestPath");
        Path = ChestPath;
      } else {
        AssetType = TEXT("Trigger");
        PathField = TEXT("triggerPath");
        Path = TriggerPath;
      }
    }

    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(Path, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(Socket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }
    Result->SetStringField(TEXT("assetType"), AssetType);
    Result->SetStringField(PathField, Path);
    Result->SetStringField(TEXT("name"), Blueprint->GetName());
    Result->SetStringField(TEXT("blueprintName"), Blueprint->GetName());
    McpInteractionAddVariableReadback(Result, Blueprint);
    McpInteractionAddComponentReadback(Result, Blueprint);
  }
#endif

  SendAutomationResponse(Socket, RequestId, true, TEXT("Interaction info retrieved"), Result);
  return true;
}
