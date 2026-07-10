// =============================================================================
// McpAutomationBridge_PropertyHandlers.cpp
// =============================================================================
// Property and Container Manipulation Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// ---------------------
// Section 1: Property Access
//   - set_object_property            : Set property value on any UObject
//   - get_object_property            : Get property value from UObject
//   - set_property_by_path           : Set nested property via path
//   - get_property_by_path           : Get nested property via path
//
// Section 2: Array Operations
//   - array_append                   : Append element to array
//   - array_insert                   : Insert element at index
//   - array_remove                   : Remove element at index
//   - array_clear                    : Clear all elements
//   - array_get                      : Get element at index
//   - array_set                      : Set element at index
//   - array_length                   : Get array length
//
// Section 3: Map Operations
//   - map_set                        : Set key-value pair
//   - map_get                        : Get value by key
//   - map_remove                     : Remove by key
//   - map_has                        : Check key existence
//   - map_keys                       : Get all keys
//   - map_clear                      : Clear all entries
//
// Section 4: Set Operations
//   - set_add                        : Add element to set
//   - set_remove                     : Remove element from set
//   - set_contains                   : Check element existence
//   - set_clear                      : Clear all elements
//
// PAYLOAD/RESPONSE FORMATS:
// -------------------------
// set_object_property:
//   Payload: { "objectPath": string, "propertyName": string, "value": any }
//   Response: { "success": bool, "propertyName": string, "value": any }
//
// array_append:
//   Payload: { "objectPath": string, "propertyName": string, "value": any }
//   Response: { "success": bool, "arrayLength": int }
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.7: All handlers supported
// - FProperty reflection APIs stable across versions
// - Container manipulation via standard UE reflection
//
// REFACTORING NOTES:
// ------------------
// - Uses McpHandlerUtils for standardized error responses
// - Uses McpPropertyReflection for property conversion
// - Consistent parameter validation patterns
// - Shared object resolution logic extracted to helpers
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first

#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_PropertyHandlers.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

#if WITH_EDITOR
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node.h"
#include "EdGraph/EdGraphPin.h"
#include "DiffUtils.h"
#include "Misc/PackagePath.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Engine/Engine.h"
#endif

// =============================================================================
// Local Helper Functions
// =============================================================================
namespace
{
    /**
     * Add verification data to result based on object type.
     */
    void AddObjectVerification(TSharedPtr<FJsonObject>& Result, UObject* Object)
    {
#if WITH_EDITOR
        if (AActor* AsActor = Cast<AActor>(Object))
        {
            McpHandlerUtils::AddVerification(Result, AsActor);
        }
        else
        {
            McpHandlerUtils::AddVerification(Result, Object);
        }
#endif
    }

    // Live level-actor writes land in the unsaved level, never directly on disk.
    void AddLevelActorPersistenceFields(TSharedPtr<FJsonObject>& Result)
    {
        Result->SetBoolField(TEXT("saved"), false);
        Result->SetBoolField(TEXT("markedDirty"), true);
        Result->SetStringField(TEXT("note"),
            TEXT("Level actors persist when the level is saved (e.g. control_editor save_all)."));
    }
}

bool McpHandlers::Inspect::HandleSetObjectProperty(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_object_property"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_object_property")))
    return false;

  if (!Payload.IsValid())
  {
      S.SendAutomationError(RequestingSocket, RequestId,
          TEXT("set_object_property payload missing."),
          TEXT("INVALID_PAYLOAD"));
      return true;
  }

  // --- Parameter Validation (using McpHandlerUtils patterns) ---
  FString ObjectPath;
  // objectPath is optional when blueprintPath is provided
  Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
  ObjectPath.TrimStartAndEndInline();

  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  BlueprintPath.TrimStartAndEndInline();

  if (ObjectPath.IsEmpty() && BlueprintPath.IsEmpty())
  {
      S.SendAutomationError(RequestingSocket, RequestId,
          TEXT("Either objectPath or blueprintPath is required."),
          TEXT("INVALID_OBJECT"));
      return true;
  }

  FString PropertyName;
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  PropertyName.TrimStartAndEndInline();
  if (PropertyName.IsEmpty())
  {
      Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
      PropertyName.TrimStartAndEndInline();
  }
  if (PropertyName.IsEmpty())
  {
      S.SendAutomationError(RequestingSocket, RequestId,
          TEXT("propertyName or propertyPath is required."),
          TEXT("INVALID_PROPERTY"));
      return true;
  }

  McpPropertyReflection::FMcpTypedValue PropTyped;
  FString PropParseDetail;
  switch (McpPropertyReflection::ReadDiscriminatedValue(Payload, PropTyped, PropParseDetail)) {
  case McpPropertyReflection::EMcpTypedValueParse::None:
      S.SendAutomationError(RequestingSocket, RequestId,
          TEXT("set exactly one typed value field: boolValue, intValue, floatValue, stringValue, colorValue, vectorValue, structValue, or arrayValue."),
          TEXT("INVALID_VALUE"));
      return true;
  case McpPropertyReflection::EMcpTypedValueParse::Ambiguous:
      S.SendAutomationError(RequestingSocket, RequestId,
          *FString::Printf(TEXT("set exactly one typed value field, got: %s"), *PropParseDetail),
          TEXT("AMBIGUOUS_VALUE"));
      return true;
  case McpPropertyReflection::EMcpTypedValueParse::Ok:
      break;
  }
  const TSharedPtr<FJsonValue> ValueField = PropTyped.Json;

  // --- Object Resolution ---
  UObject* RootObject = nullptr;

  // Priority 1: blueprintPath → load Blueprint → get CDO
  if (!BlueprintPath.IsEmpty())
  {
      FString NormalizedPath, LoadError;
      UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
      if (!Blueprint)
      {
          S.SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Blueprint not found: %s (%s)"), *BlueprintPath, *LoadError),
              TEXT("BLUEPRINT_NOT_FOUND"));
          return true;
      }

      UClass* GeneratedClass = Blueprint->GeneratedClass;
      if (!GeneratedClass)
      {
          S.SendAutomationError(RequestingSocket, RequestId,
              TEXT("Blueprint has no GeneratedClass (not compiled?)"),
              TEXT("CDO_NOT_FOUND"));
          return true;
      }

      RootObject = GeneratedClass->GetDefaultObject();
      if (!RootObject)
      {
          S.SendAutomationError(RequestingSocket, RequestId,
              TEXT("Failed to get Class Default Object"),
              TEXT("CDO_NOT_FOUND"));
          return true;
      }

      ObjectPath = RootObject->GetPathName();
  }
  else
  {
      // Priority 2: objectPath → standard resolution
      FString ResolvedPath;
      RootObject = McpHandlerUtils::ResolveObjectFromPath(ObjectPath, &ResolvedPath);
      if (!RootObject)
      {
          S.SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath),
              TEXT("OBJECT_NOT_FOUND"));
          return true;
      }
      if (!ResolvedPath.IsEmpty())
      {
          ObjectPath = ResolvedPath;
      }
  }

  // --- Special Actor Property Handling ---
  // Handle properties that require setter methods instead of direct property access
  // CDOs don't support runtime setters — changes won't persist to Blueprint defaults
  const bool bIsClassDefaultObject = RootObject->HasAnyFlags(RF_ClassDefaultObject);
  if (AActor *Actor = Cast<AActor>(RootObject))
  {
    if (bIsClassDefaultObject &&
        (PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase) ||
         PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase) ||
         PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
         PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase))) {
      S.SendAutomationError(RequestingSocket, RequestId,
          TEXT("Cannot modify runtime transform on a Blueprint CDO. Edit defaults on the root component or SCS template instead."),
          TEXT("CDO_TRANSFORM"));
      return true;
    }
    if (!bIsClassDefaultObject &&
        PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase)) {
          if (ValueField->Type != EJson::Object && ValueField->Type != EJson::Array)
          {
              S.SendAutomationError(RequestingSocket, RequestId,
                  TEXT("ActorLocation requires vectorValue {x,y,z} or arrayValue [x,y,z]."),
                  TEXT("VALUE_TYPE_MISMATCH"));
              return true;
          }
          FVector NewLoc = FVector::ZeroVector;
          if (ValueField->Type == EJson::Object)
          {
              McpPropertyReflection::JsonToVector(ValueField->AsObject(), NewLoc);
          }
          else if (ValueField->Type == EJson::Array)
          {
              McpPropertyReflection::JsonArrayToVector(ValueField->AsArray(), NewLoc);
          }

          Actor->Modify();
          Actor->SetActorLocation(NewLoc);

          TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
          ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
          AddLevelActorPersistenceFields(ResultPayload);
          ResultPayload->SetObjectField(TEXT("value"), McpPropertyReflection::VectorToJson(NewLoc));
          AddObjectVerification(ResultPayload, Actor);
          S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor location updated."), ResultPayload);
          return true;
      }
      
      // ActorRotation
      if (PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase))
      {
          if (ValueField->Type != EJson::Object && ValueField->Type != EJson::Array)
          {
              S.SendAutomationError(RequestingSocket, RequestId,
                  TEXT("ActorRotation requires vectorValue {pitch,yaw,roll} or arrayValue [pitch,yaw,roll]."),
                  TEXT("VALUE_TYPE_MISMATCH"));
              return true;
          }
          FRotator NewRot = FRotator::ZeroRotator;
          if (ValueField->Type == EJson::Object)
          {
              McpPropertyReflection::JsonToRotator(ValueField->AsObject(), NewRot);
          }
          else if (ValueField->Type == EJson::Array)
          {
              McpPropertyReflection::JsonArrayToRotator(ValueField->AsArray(), NewRot);
          }

          Actor->Modify();
          Actor->SetActorRotation(NewRot);

          TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
          ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
          AddLevelActorPersistenceFields(ResultPayload);
          ResultPayload->SetObjectField(TEXT("value"), McpPropertyReflection::RotatorToJson(NewRot));
          AddObjectVerification(ResultPayload, Actor);
          S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor rotation updated."), ResultPayload);
          return true;
      }
      
      // ActorScale / ActorScale3D
      if (PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
          PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase))
      {
          if (ValueField->Type != EJson::Object && ValueField->Type != EJson::Array)
          {
              S.SendAutomationError(RequestingSocket, RequestId,
                  TEXT("ActorScale requires vectorValue {x,y,z} or arrayValue [x,y,z]."),
                  TEXT("VALUE_TYPE_MISMATCH"));
              return true;
          }
          FVector NewScale = FVector::OneVector;
          if (ValueField->Type == EJson::Object)
          {
              McpPropertyReflection::JsonToVector(ValueField->AsObject(), NewScale);
          }
          else if (ValueField->Type == EJson::Array)
          {
              McpPropertyReflection::JsonArrayToVector(ValueField->AsArray(), NewScale);
          }

          Actor->Modify();
          Actor->SetActorScale3D(NewScale);

          TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
          ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
          AddLevelActorPersistenceFields(ResultPayload);
          ResultPayload->SetObjectField(TEXT("value"), McpPropertyReflection::VectorToJson(NewScale));
          AddObjectVerification(ResultPayload, Actor);
          S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor scale updated."), ResultPayload);
          return true;
      }
      
      // bHidden (visibility) — skip runtime setter for CDOs, let generic path handle it
      if (!bIsClassDefaultObject && PropertyName.Equals(TEXT("bHidden"), ESearchCase::IgnoreCase))
      {
          if (ValueField->Type != EJson::Boolean)
          {
              S.SendAutomationError(RequestingSocket, RequestId,
                  TEXT("bHidden requires boolValue true/false."),
                  TEXT("VALUE_TYPE_MISMATCH"));
              return true;
          }
          const bool bHidden = ValueField->AsBool();

          Actor->Modify();
          Actor->SetActorHiddenInGame(bHidden);

          TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
          ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
          AddLevelActorPersistenceFields(ResultPayload);
          ResultPayload->SetBoolField(TEXT("value"), bHidden);
          AddObjectVerification(ResultPayload, Actor);
          S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor visibility updated."), ResultPayload);
          return true;
      }
  }


  void* TargetContainer = nullptr;
  FProperty* Property = nullptr;

  if (PropertyName.Contains(TEXT("."))) {
      // Nested property path (e.g., "MyComponent.PropertyName")
      FString ResolveError;
      Property = ResolveNestedPropertyPath(RootObject, PropertyName, TargetContainer, ResolveError);
      if (!Property || !TargetContainer) {
          S.SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Failed to resolve nested property path '%s': %s"), *PropertyName, *ResolveError),
              TEXT("PROPERTY_NOT_FOUND"));
          return true;
      }
  }
  else
  {
      // Simple property name - look it up directly
      TargetContainer = RootObject;
      Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
      if (!Property) {
          S.SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Property '%s' not found on object '%s'."), *PropertyName, *ObjectPath),
              TEXT("PROPERTY_NOT_FOUND"));
          return true;
      }
  }

  // Cross-check the sent value kind against the resolved property's real type
  // before applying — a mismatch fails loud instead of silently coercing (mirrors
  // the control_actor set-variable path).
  if (!McpPropertyReflection::PropertyMatchesValueKind(Property, PropTyped.Kind))
  {
      S.SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("%s: you sent %sValue but the property type is '%s'."),
                          *PropertyName, *PropTyped.Kind, *Property->GetCPPType()),
          TEXT("VALUE_TYPE_MISMATCH"));
      return true;
  }

  // --- Apply Value ---
#if WITH_EDITOR
  RootObject->Modify();
#endif

  FString ConversionError;
  if (!ApplyJsonValueToProperty(TargetContainer, Property, ValueField, ConversionError))
  {
      S.SendAutomationError(RequestingSocket, RequestId, ConversionError, TEXT("PROPERTY_CONVERSION_FAILED"));
      return true;
  }

  // --- Mark Dirty (optional) ---
  const bool bMarkDirty = GetJsonBoolField(Payload, TEXT("markDirty"), true);
  if (bMarkDirty)
  {
      RootObject->MarkPackageDirty();
  }

#if WITH_EDITOR
  RootObject->PostEditChange();

  // Refresh stale node title cache for K2Node types whose displayed title is
  // computed from a UPROPERTY we just wrote — otherwise the editor keeps
  // rendering the cached title (e.g. "EnhancedInputAction None") until the
  // user manually clicks the node, which reads as "the property write didn't
  // take" even though it did.
  //
  // FNodeTextCache (EdGraphNodeUtils.h) treats CachedText as valid as long as
  // the schema's visualization cache ID matches; PostEditChange does not bump
  // that ID. ForceVisualizationCacheClear on the schema is what makes
  // FNodeTextCache::IsOutOfDate return true on the next access so GetNodeTitle
  // is re-computed.
  //
  // NARROW WHITELIST ONLY — calling ReconstructNode on arbitrary K2Nodes
  // would risk breaking already-connected pins on nodes whose authors did not
  // design for ReconstructNode after a single property write. Match by class
  // name string so this code path stays independent of the optional
  // InputBlueprintNodes plugin module (referencing
  // UK2Node_EnhancedInputAction::StaticClass() directly would add a hard
  // module dependency just for the type check).
  if (UK2Node *K2Node = Cast<UK2Node>(RootObject)) {
      static const TSet<FString> RefreshableTitleNodeClassNames = {
          TEXT("K2Node_EnhancedInputAction"),
          // Future additions: any K2Node whose GetNodeTitle reads a UPROPERTY
          // we expose via set_object_property and that doesn't auto-invalidate.
      };
      if (RefreshableTitleNodeClassNames.Contains(K2Node->GetClass()->GetName())) {
          K2Node->ReconstructNode();
          if (UEdGraph *Graph = K2Node->GetGraph()) {
              if (const UEdGraphSchema *Schema = Graph->GetSchema()) {
                  Schema->ForceVisualizationCacheClear();
              }
              Graph->NotifyGraphChanged();
          }
      }
  }
#endif

  // --- Persist ---
  // 'saved' reports the observed on-disk result, never an assumption. Level and
  // transient packages are not auto-saved here (that would save the whole map);
  // markDirty:false implies save:false unless save is explicitly true.
  const bool bSaveRequested = GetJsonBoolField(Payload, TEXT("save"), bMarkDirty);
  UPackage* Package = RootObject->GetOutermost();
  const bool bAssetPackage = Package && Package != GetTransientPackage() &&
                             !Package->HasAnyFlags(RF_Transient) &&
                             !Package->ContainsMap();
  bool bSaved = false;
  if (bSaveRequested && bAssetPackage)
  {
      bSaved = McpSafeAssetSave(Package);
      if (!bSaved)
      {
          S.SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Property '%s' was applied in memory but saving package '%s' failed; the package is left dirty. Check the .uasset is writable and not locked, or pass save:false and persist later via control_editor save_all."),
                  *PropertyName, *Package->GetName()),
              TEXT("SAVE_FAILED"));
          return true;
      }
  }

  // --- Build Response ---
  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetBoolField(TEXT("saved"), bSaved);
  ResultPayload->SetBoolField(TEXT("markedDirty"), Package && Package->IsDirty());
  if (bSaveRequested && !bAssetPackage)
  {
      ResultPayload->SetStringField(TEXT("note"),
          TEXT("Object lives in a level or transient package; set_property does not auto-save levels. Persist via control_editor save_all."));
  }
  AddObjectVerification(ResultPayload, RootObject);

  // Include the updated value in response
  if (TSharedPtr<FJsonValue> CurrentValue = ExportPropertyToJsonValue(TargetContainer, Property))
  {
      ResultPayload->SetField(TEXT("value"), CurrentValue);
  }

  S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property value updated."), ResultPayload);
  return true;
}


bool McpHandlers::Inspect::HandleGetObjectProperty(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("get_object_property"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("get_object_property")))
    return false;

  if (!Payload.IsValid()) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_object_property payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // --- Parameter Validation ---
  FString ObjectPath;
  Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
  ObjectPath.TrimStartAndEndInline();

  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  BlueprintPath.TrimStartAndEndInline();

  if (ObjectPath.IsEmpty() && BlueprintPath.IsEmpty())
  {
      S.SendAutomationError(RequestingSocket, RequestId,
          TEXT("Either objectPath or blueprintPath is required."),
          TEXT("INVALID_OBJECT"));
      return true;
  }

  FString PropertyName;
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  PropertyName.TrimStartAndEndInline();
  if (PropertyName.IsEmpty())
  {
      Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
      PropertyName.TrimStartAndEndInline();
  }
  if (PropertyName.IsEmpty()) {
    S.SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("get_object_property requires a non-empty propertyName or propertyPath."),
        TEXT("INVALID_PROPERTY"));
    return true;
  }

  // --- Object Resolution ---
  UObject* RootObject = nullptr;

  // Priority 1: blueprintPath → load Blueprint → get CDO
  if (!BlueprintPath.IsEmpty())
  {
      FString NormalizedPath, LoadError;
      UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
      if (!Blueprint)
      {
          S.SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Blueprint not found: %s (%s)"), *BlueprintPath, *LoadError),
              TEXT("BLUEPRINT_NOT_FOUND"));
          return true;
      }

      UClass* GeneratedClass = Blueprint->GeneratedClass;
      if (!GeneratedClass)
      {
          S.SendAutomationError(RequestingSocket, RequestId,
              TEXT("Blueprint has no GeneratedClass (not compiled?)"),
              TEXT("CDO_NOT_FOUND"));
          return true;
      }

      RootObject = GeneratedClass->GetDefaultObject();
      if (!RootObject)
      {
          S.SendAutomationError(RequestingSocket, RequestId,
              TEXT("Failed to get Class Default Object"),
              TEXT("CDO_NOT_FOUND"));
          return true;
      }

      ObjectPath = RootObject->GetPathName();
  }
  else
  {
      // Priority 2: objectPath → standard resolution
      FString ResolvedPath;
      RootObject = McpHandlerUtils::ResolveObjectFromPath(ObjectPath, &ResolvedPath);
      if (!RootObject)
      {
          S.SendAutomationError(
              RequestingSocket, RequestId,
              FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath),
              TEXT("OBJECT_NOT_FOUND"));
          return true;
      }
      if (!ResolvedPath.IsEmpty())
      {
          ObjectPath = ResolvedPath;
      }
  }

  // Special handling for common AActor properties that are actually functions
  // or require setters — CDOs don't have valid runtime transform data
  const bool bIsCDO = RootObject->HasAnyFlags(RF_ClassDefaultObject);
  if (AActor *Actor = Cast<AActor>(RootObject)) {
    if (bIsCDO && (PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase) ||
                   PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase) ||
                   PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
                   PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase))) {
      S.SendAutomationError(RequestingSocket, RequestId,
          TEXT("Cannot read runtime transform from a Blueprint CDO. Query the SCS template or a spawned instance instead."),
          TEXT("CDO_TRANSFORM"));
      return true;
    }
    if (PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase)) {
      FVector Loc = Actor->GetActorLocation();
      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      McpHandlerUtils::AddVerification(ResultPayload, Actor);

      TSharedPtr<FJsonObject> ValObj = McpHandlerUtils::CreateResultObject();
      ValObj->SetNumberField(TEXT("x"), Loc.X);
      ValObj->SetNumberField(TEXT("y"), Loc.Y);
      ValObj->SetNumberField(TEXT("z"), Loc.Z);
      ResultPayload->SetField(TEXT("value"),
                              MakeShared<FJsonValueObject>(ValObj));

      S.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Actor location retrieved."), ResultPayload,
                             FString());
      return true;
    } else if (PropertyName.Equals(TEXT("ActorRotation"),
                                   ESearchCase::IgnoreCase)) {
      FRotator Rot = Actor->GetActorRotation();
      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      McpHandlerUtils::AddVerification(ResultPayload, Actor);

      TSharedPtr<FJsonObject> ValObj = McpHandlerUtils::CreateResultObject();
      ValObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
      ValObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
      ValObj->SetNumberField(TEXT("roll"), Rot.Roll);
      ResultPayload->SetField(TEXT("value"),
                              MakeShared<FJsonValueObject>(ValObj));

      S.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Actor rotation retrieved."), ResultPayload,
                             FString());
      return true;
    } else if (PropertyName.Equals(TEXT("ActorScale"),
                                   ESearchCase::IgnoreCase) ||
               PropertyName.Equals(TEXT("ActorScale3D"),
                                   ESearchCase::IgnoreCase)) {
      FVector Scale = Actor->GetActorScale3D();
      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      McpHandlerUtils::AddVerification(ResultPayload, Actor);

      TSharedPtr<FJsonObject> ValObj = McpHandlerUtils::CreateResultObject();
      ValObj->SetNumberField(TEXT("x"), Scale.X);
      ValObj->SetNumberField(TEXT("y"), Scale.Y);
      ValObj->SetNumberField(TEXT("z"), Scale.Z);
      ResultPayload->SetField(TEXT("value"),
                              MakeShared<FJsonValueObject>(ValObj));

      S.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Actor scale retrieved."), ResultPayload,
                             FString());
      return true;
    }
  }

  // Support nested property paths (e.g., "MyComponent.PropertyName")
  McpHandlerUtils::FPropertyResolveResult PropResult = McpHandlerUtils::ResolveProperty(RootObject, PropertyName);
  if (!PropResult.IsValid())
  {
      S.SendAutomationError(
          RequestingSocket, RequestId,
          PropResult.Error,
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
  }

  const TSharedPtr<FJsonValue> CurrentValue =
      ExportPropertyToJsonValue(PropResult.Container, PropResult.Property);
  if (!CurrentValue.IsValid()) {
    S.SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Unable to export property %s."), *PropertyName),
        TEXT("PROPERTY_EXPORT_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetField(TEXT("value"), CurrentValue);
  
  // Add verification based on object type
  if (AActor* AsActor = Cast<AActor>(RootObject)) {
    McpHandlerUtils::AddVerification(ResultPayload, AsActor);
  } else {
    McpHandlerUtils::AddVerification(ResultPayload, RootObject);
  }

  S.SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Property value retrieved."), ResultPayload,
                         FString());
  return true;
}







// Map operation handlers






// Set operation handlers




// Asset dependency graph traversal


// =============================================================================
// inspect_cdo - Blueprint Class Default Object Inspection
// =============================================================================

#if WITH_EDITOR
namespace
{

// Builds a JSON summary for a single component template.
// Summary mode: name, class, source, transform, and key asset fields.
// Detailed mode or propertyNames filter: adds full/selective property export.
TSharedPtr<FJsonObject> BuildComponentSummary(
    UActorComponent* Template,
    const FString& DisplayName,
    const FString& Source,
    bool bDetailed,
    const TArray<FName>& PropertyFilter)
{
    TSharedPtr<FJsonObject> CompObj = McpHandlerUtils::CreateResultObject();
    CompObj->SetStringField(TEXT("name"), DisplayName);
    CompObj->SetStringField(TEXT("class"), Template->GetClass()->GetName());
    CompObj->SetStringField(TEXT("source"), Source);

    // Transform via existing repo helpers
    if (USceneComponent* SceneComp = Cast<USceneComponent>(Template))
    {
        TSharedPtr<FJsonObject> TransformObj = McpHandlerUtils::CreateResultObject();
        TransformObj->SetObjectField(TEXT("location"),
            McpHandlerUtils::VectorToJson(SceneComp->GetRelativeLocation()));
        TransformObj->SetObjectField(TEXT("rotation"),
            McpHandlerUtils::RotatorToJson(SceneComp->GetRelativeRotation()));
        TransformObj->SetObjectField(TEXT("scale"),
            McpHandlerUtils::VectorToJson(SceneComp->GetRelativeScale3D()));
        CompObj->SetObjectField(TEXT("transform"), TransformObj);
    }

    // Key asset fields for common mesh component types
    if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Template))
    {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        USkeletalMesh* Mesh = SkelComp->GetSkeletalMeshAsset();
#else
        USkeletalMesh* Mesh = SkelComp->SkeletalMesh;
#endif
        CompObj->SetStringField(TEXT("skeletalMesh"),
            Mesh ? Mesh->GetPathName() : TEXT("None"));
        CompObj->SetStringField(TEXT("animClass"),
            SkelComp->AnimClass ? SkelComp->AnimClass->GetPathName() : TEXT("None"));
    }

    if (UStaticMeshComponent* StaticComp = Cast<UStaticMeshComponent>(Template))
    {
        CompObj->SetStringField(TEXT("staticMesh"),
            StaticComp->GetStaticMesh()
                ? StaticComp->GetStaticMesh()->GetPathName()
                : TEXT("None"));
    }

    // Full/selective property export only when requested
    if (PropertyFilter.Num() > 0)
    {
        TSharedPtr<FJsonObject> Props =
            McpPropertyReflection::ExportPropertiesToJson(Template, PropertyFilter);
        if (Props.IsValid())
        {
            CompObj->SetObjectField(TEXT("properties"), Props);
        }
    }
    else if (bDetailed)
    {
        TSharedPtr<FJsonObject> Props =
            McpPropertyReflection::ExportObjectToJson(Template, false);
        if (Props.IsValid())
        {
            CompObj->SetObjectField(TEXT("properties"), Props);
        }
    }

    return CompObj;
}

// Builds a set of SCS variable names for source classification.
// Returns a map: variable name -> source label ("SCS" or "SCS_Inherited").
TMap<FString, FString> BuildScsSourceMap(UBlueprint* Blueprint)
{
    TMap<FString, FString> SourceMap;
    for (UBlueprint* Bp = Blueprint; Bp != nullptr;)
    {
        if (Bp->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Bp->SimpleConstructionScript->GetAllNodes())
            {
                if (!Node) continue;
                const FString VarName = Node->GetVariableName().ToString();
                if (!SourceMap.Contains(VarName))
                {
                    SourceMap.Add(VarName,
                        (Bp == Blueprint) ? TEXT("SCS") : TEXT("SCS_Inherited"));
                }
            }
        }
        UClass* ParentClass = Bp->ParentClass;
        Bp = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }
    return SourceMap;
}

// Finds a component by name: first on CDO (native), then SCS templates (BP-added).
UActorComponent* FindCdoComponent(
    UBlueprint* Blueprint,
    UObject* CDO,
    const FString& ComponentName)
{
    // Search native CDO components first (effective overrides)
    if (AActor* DefaultActor = Cast<AActor>(CDO))
    {
        TInlineComponentArray<UActorComponent*> Components;
        DefaultActor->GetComponents(Components);
        for (UActorComponent* Comp : Components)
        {
            if (Comp && Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
            {
                return Comp;
            }
        }
    }

    // Search SCS node templates (BP-added components)
    for (UBlueprint* Bp = Blueprint; Bp != nullptr;)
    {
        if (Bp->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Bp->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate &&
                    Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
                {
                    return Node->ComponentTemplate;
                }
            }
        }
        UClass* ParentClass = Bp->ParentClass;
        Bp = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }
    return nullptr;
}

} // anonymous namespace
#endif // WITH_EDITOR

bool McpHandlers::Inspect::HandleInspectCdoAction(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("inspect_cdo: payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    BlueprintPath.TrimStartAndEndInline();
    if (BlueprintPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("blueprintPath is required for inspect_cdo"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString NormalizedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(
        BlueprintPath, NormalizedPath, LoadError);
    if (!Blueprint)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Blueprint not found: %s (%s)"),
                                            *BlueprintPath, *LoadError),
                            TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }

    UClass* GeneratedClass = Blueprint->GeneratedClass;
    if (!GeneratedClass)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Blueprint has no GeneratedClass (not compiled?)"),
                            TEXT("CDO_NOT_FOUND"));
        return true;
    }

    UObject* CDO = GeneratedClass->GetDefaultObject();
    if (!CDO)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to get Class Default Object"),
                            TEXT("CDO_NOT_FOUND"));
        return true;
    }

    // Parse optional params
    FString ComponentNameFilter;
    Payload->TryGetStringField(TEXT("componentName"), ComponentNameFilter);
    ComponentNameFilter.TrimStartAndEndInline();
    bool bDetailed = false;
    Payload->TryGetBoolField(TEXT("detailed"), bDetailed);

    // Optional: restrict the all-components dump to these names. The schema advertised
    // componentNames but the loops below ignored it, so a component-heavy CDO (every
    // BodyInstance/collision default per component) could overflow the response.
    TSet<FString> ComponentNamesFilter;
    const TArray<TSharedPtr<FJsonValue>>* CompNamesArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("componentNames"), CompNamesArr) && CompNamesArr)
    {
        for (const auto& Val : *CompNamesArr)
        {
            FString S;
            if (Val->TryGetString(S))
            {
                ComponentNamesFilter.Add(S.TrimStartAndEnd());
            }
        }
    }

    TArray<FName> PropertyNameFilter;
    FString PropertyPathFilter;
    if (Payload->TryGetStringField(TEXT("propertyPath"), PropertyPathFilter))
    {
        PropertyPathFilter.TrimStartAndEndInline();
        if (!PropertyPathFilter.IsEmpty())
        {
            PropertyNameFilter.Add(FName(*PropertyPathFilter));
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* PropNamesArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("propertyNames"), PropNamesArr) && PropNamesArr)
    {
        for (const auto& Val : *PropNamesArr)
        {
            FString S;
            if (Val->TryGetString(S))
            {
                PropertyNameFilter.Add(FName(*S));
            }
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("blueprintPath"), NormalizedPath);
    Resp->SetStringField(TEXT("className"), GeneratedClass->GetName());
    Resp->SetStringField(TEXT("classPath"), GeneratedClass->GetPathName());
    Resp->SetStringField(TEXT("parentClass"),
        GeneratedClass->GetSuperClass()
            ? GeneratedClass->GetSuperClass()->GetName()
            : TEXT("None"));

    // --- Component filter mode: single component dump ---
    if (!ComponentNameFilter.IsEmpty())
    {
        UActorComponent* FoundComp = FindCdoComponent(Blueprint, CDO, ComponentNameFilter);
        if (!FoundComp)
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Component not found: %s"),
                                                *ComponentNameFilter),
                                TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> CompJson;
        if (PropertyNameFilter.Num() > 0)
        {
            CompJson = McpPropertyReflection::ExportPropertiesToJson(
                FoundComp, PropertyNameFilter);
        }
        else
        {
            CompJson = McpPropertyReflection::ExportObjectToJson(
                FoundComp, false);
        }

        // Return both the lookup name and the internal object name
        Resp->SetStringField(TEXT("componentName"), ComponentNameFilter);
        Resp->SetStringField(TEXT("templateObjectName"), FoundComp->GetName());
        Resp->SetStringField(TEXT("componentClass"),
            FoundComp->GetClass()->GetName());
        if (CompJson.IsValid())
        {
            Resp->SetObjectField(TEXT("properties"), CompJson);
        }
        Resp->SetBoolField(TEXT("success"), true);
        S.SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("CDO component inspected"), Resp, FString());
        return true;
    }

    // --- CDO properties (only when detailed or propertyNames given) ---
    if (PropertyNameFilter.Num() > 0)
    {
        TSharedPtr<FJsonObject> CdoProps =
            McpPropertyReflection::ExportPropertiesToJson(CDO, PropertyNameFilter);
        if (CdoProps.IsValid())
        {
            Resp->SetObjectField(TEXT("cdoProperties"), CdoProps);
        }
    }
    else if (bDetailed)
    {
        TSharedPtr<FJsonObject> CdoProps =
            McpPropertyReflection::ExportObjectToJson(CDO, false);
        if (CdoProps.IsValid())
        {
            Resp->SetObjectField(TEXT("cdoProperties"), CdoProps);
        }
    }

    // --- Components: hybrid CDO + SCS ---
    // Native components (C++ constructor) live on the CDO with effective overrides.
    // SCS components (Blueprint-added) only exist as templates on SCS nodes.
    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    TSet<FString> SeenNames;
    TMap<FString, FString> ScsSourceMap = BuildScsSourceMap(Blueprint);

    // 1) Native CDO components (effective override values)
    if (AActor* DefaultActor = Cast<AActor>(CDO))
    {
        TInlineComponentArray<UActorComponent*> CdoComponents;
        DefaultActor->GetComponents(CdoComponents);
        for (UActorComponent* Comp : CdoComponents)
        {
            if (!Comp) continue;
            const FString CompName = Comp->GetName();
            SeenNames.Add(CompName);
            if (ComponentNamesFilter.Num() > 0 && !ComponentNamesFilter.Contains(CompName))
            {
                continue;
            }

            const FString Source = ScsSourceMap.Contains(CompName)
                ? TEXT("Native_Override") : TEXT("Native");

            ComponentsArray.Add(MakeShared<FJsonValueObject>(
                BuildComponentSummary(Comp, CompName, Source,
                                      bDetailed, PropertyNameFilter)));
        }
    }

    // 2) SCS components (Blueprint-added) from SCS node templates.
    //    Walk full parent chain for inherited BP components.
    for (UBlueprint* Bp = Blueprint; Bp != nullptr;)
    {
        if (Bp->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Bp->SimpleConstructionScript->GetAllNodes())
            {
                if (!Node || !Node->ComponentTemplate) continue;
                const FString VarName = Node->GetVariableName().ToString();
                if (SeenNames.Contains(VarName)) continue;
                SeenNames.Add(VarName);
                if (ComponentNamesFilter.Num() > 0 && !ComponentNamesFilter.Contains(VarName))
                {
                    continue;
                }

                const FString Source = (Bp == Blueprint)
                    ? TEXT("SCS") : TEXT("SCS_Inherited");

                TSharedPtr<FJsonObject> CompObj = BuildComponentSummary(
                    Node->ComponentTemplate, VarName, Source,
                    bDetailed, PropertyNameFilter);

                // Add parent attachment info from SCS node
                if (Node->ParentComponentOrVariableName != NAME_None)
                {
                    CompObj->SetStringField(TEXT("attachParent"),
                        Node->ParentComponentOrVariableName.ToString());
                }

                ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
            }
        }
        UClass* ParentClass = Bp->ParentClass;
        Bp = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }

    Resp->SetArrayField(TEXT("components"), ComponentsArray);
    Resp->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    Resp->SetBoolField(TEXT("success"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("CDO inspection completed"), Resp, FString());
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("inspect_cdo requires editor build."),
                        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// diff_asset — structural diff between two on-disk .uasset versions.
//
// Loads each file as an isolated package via DiffUtils::LoadPackageForDiff so
// neither collides with the live (already-loaded) asset, then compares
// Blueprint structure: parent class, implemented interfaces, SCS components,
// variables, function/event graphs, and (optionally) CDO default properties.
// The client supplies the two versions as files (e.g. `git show <ref>:<path>`
// to a temp file for the old side, the working-tree file for the new side);
// the bridge only compares. Non-Blueprint assets fall back to a property diff.
// =============================================================================
#if WITH_EDITOR
namespace McpDiffAssetDetail
{
    // Deterministic stringify of a JSON value (object keys sorted) so CDO
    // property values can be compared for equality across the two versions.
    FString McpStableJsonValueString(const TSharedPtr<FJsonValue>& Value)
    {
        if (!Value.IsValid()) { return TEXT("null"); }
        switch (Value->Type)
        {
            case EJson::String:  return FString::Printf(TEXT("\"%s\""), *Value->AsString());
            case EJson::Number:  return FString::SanitizeFloat(Value->AsNumber());
            case EJson::Boolean: return Value->AsBool() ? TEXT("true") : TEXT("false");
            case EJson::Null:    return TEXT("null");
            case EJson::Array:
            {
                const TArray<TSharedPtr<FJsonValue>>& Arr = Value->AsArray();
                FString Out = TEXT("[");
                for (int32 i = 0; i < Arr.Num(); ++i)
                {
                    if (i) { Out += TEXT(","); }
                    Out += McpStableJsonValueString(Arr[i]);
                }
                return Out + TEXT("]");
            }
            case EJson::Object:
            {
                const TSharedPtr<FJsonObject>& Obj = Value->AsObject();
                TArray<FString> Keys;
                if (Obj.IsValid()) { Obj->Values.GetKeys(Keys); }
                Keys.Sort();
                FString Out = TEXT("{");
                for (int32 i = 0; i < Keys.Num(); ++i)
                {
                    if (i) { Out += TEXT(","); }
                    Out += Keys[i] + TEXT(":") + McpStableJsonValueString(Obj->Values[Keys[i]]);
                }
                return Out + TEXT("}");
            }
            default: return TEXT("");
        }
    }

    // Load a .uasset file as an isolated diff package and return its primary
    // asset object (does not collide with the live loaded asset).
    UObject* McpLoadAssetForDiff(const FString& FilePath, const FString& AssetName)
    {
        const FPackagePath TempPath = FPackagePath::FromLocalPath(FilePath);
        UPackage* Pkg = DiffUtils::LoadPackageForDiff(TempPath, FPackagePath());
        if (!Pkg) { return nullptr; }
        UObject* Obj = FindObject<UObject>(Pkg, *AssetName);
        if (!Obj)
        {
            TArray<UObject*> Objs;
            GetObjectsWithPackage(Pkg, Objs, false);
            for (UObject* O : Objs)
            {
                if (O && O->IsAsset()) { Obj = O; break; }
            }
        }
        return Obj;
    }

    FString McpVarTypeString(const FEdGraphPinType& T)
    {
        FString S = T.PinCategory.ToString();
        if (const UObject* Sub = T.PinSubCategoryObject.Get())
        {
            S += TEXT(":") + Sub->GetName();
        }
        if (T.ContainerType == EPinContainerType::Array) { S += TEXT("[]"); }
        else if (T.ContainerType == EPinContainerType::Set) { S += TEXT("<set>"); }
        else if (T.ContainerType == EPinContainerType::Map) { S += TEXT("<map>"); }
        return S;
    }

    bool McpNameLooksGAS(const FString& In)
    {
        static const TCHAR* Keys[] = { TEXT("AbilitySystem"), TEXT("Attribute"),
            TEXT("GameplayAbility"), TEXT("GameplayEffect"), TEXT("GameplayTag"),
            TEXT("GameplayCue"), TEXT("AbilitySet") };
        for (const TCHAR* K : Keys)
        {
            if (In.Contains(K)) { return true; }
        }
        return false;
    }

    // Release a package loaded purely for diffing: detach its linker so the on-disk
    // file handle is freed (otherwise the temp file stays locked until the next GC /
    // editor restart), then clear standalone/public flags so GC can reclaim it.
    // Without this each diff leaks a package, and a later diff of the same path would
    // return this stale cached copy instead of reloading the file. The interactive
    // editor diff tool keeps its package loaded while the diff window is open and
    // relies on GC after close; a one-shot diff has no such reason to hold it.
    void ReleaseDiffPackage(UObject* AssetObj)
    {
        if (!AssetObj) { return; }
        UPackage* Pkg = AssetObj->GetPackage();
        if (!Pkg || Pkg == GetTransientPackage()) { return; }
        ResetLoaders(Pkg);
        TArray<UObject*> Inner;
        GetObjectsWithPackage(Pkg, Inner, true);
        for (UObject* O : Inner) { if (O) { O->ClearFlags(RF_Standalone | RF_Public); } }
        Pkg->ClearFlags(RF_Standalone | RF_Public);
    }
}
#endif // WITH_EDITOR

bool McpHandlers::Inspect::HandleDiffAssetAction(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    using namespace McpDiffAssetDetail;
    if (!Payload.IsValid())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("diff_asset: payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString OldFilePath, NewFilePath, AssetName;
    Payload->TryGetStringField(TEXT("oldFilePath"), OldFilePath);
    Payload->TryGetStringField(TEXT("newFilePath"), NewFilePath);
    Payload->TryGetStringField(TEXT("assetName"), AssetName);
    OldFilePath.TrimStartAndEndInline();
    NewFilePath.TrimStartAndEndInline();
    AssetName.TrimStartAndEndInline();

    if (OldFilePath.IsEmpty() || NewFilePath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("diff_asset requires 'oldFilePath' and 'newFilePath' (filesystem paths to .uasset files)"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!FPaths::FileExists(OldFilePath))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("oldFilePath not found: %s"), *OldFilePath), TEXT("FILE_NOT_FOUND"));
        return true;
    }
    if (!FPaths::FileExists(NewFilePath))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("newFilePath not found: %s"), *NewFilePath), TEXT("FILE_NOT_FOUND"));
        return true;
    }
    if (AssetName.IsEmpty())
    {
        AssetName = FPaths::GetBaseFilename(NewFilePath);
    }

    bool bIncludeDefaults = true;
    Payload->TryGetBoolField(TEXT("includeDefaults"), bIncludeDefaults);

    // DiffUtils::LoadPackageForDiff can only read files under a mounted content
    // root (the /Temp mount covers <Project>/Saved). Stage any outside file into
    // Saved so arbitrary local paths (e.g. %TEMP% git extracts) diff too.
    TArray<FString> StagedFiles;
    FString OldLoadPath = OldFilePath;
    FString NewLoadPath = NewFilePath;
    auto StageForDiff = [&StagedFiles](FString& InOutFilePath, FString& OutError) -> bool
    {
        FString MountedPackageName;
        if (FPackageName::TryConvertFilenameToLongPackageName(InOutFilePath, MountedPackageName))
        {
            return true;
        }
        FString Ext = FPaths::GetExtension(InOutFilePath);
        if (Ext.IsEmpty()) { Ext = TEXT("uasset"); }
        const FString StagedPath = FPaths::ProjectSavedDir() / TEXT("McpDiffStaging") /
            FString::Printf(TEXT("%s_%s.%s"), *FPaths::GetBaseFilename(InOutFilePath),
                            *FGuid::NewGuid().ToString(EGuidFormats::Digits), *Ext);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(StagedPath), true);
        if (IFileManager::Get().Copy(*StagedPath, *InOutFilePath) != COPY_OK)
        {
            OutError = FString::Printf(
                TEXT("'%s' is outside the project's mounted content roots and staging a copy to '%s' failed. Place the file under <Project>/Saved/ and retry."),
                *InOutFilePath, *StagedPath);
            return false;
        }
        StagedFiles.Add(StagedPath);
        InOutFilePath = StagedPath;
        return true;
    };
    {
        FString StageError;
        if (!StageForDiff(OldLoadPath, StageError) || !StageForDiff(NewLoadPath, StageError))
        {
            for (const FString& Staged : StagedFiles)
            {
                IFileManager::Get().Delete(*Staged, false, true, true);
            }
            S.SendAutomationError(RequestingSocket, RequestId, StageError, TEXT("DIFF_STAGE_FAILED"));
            return true;
        }
    }

    UObject* OldObj = McpLoadAssetForDiff(OldLoadPath, AssetName);
    UObject* NewObj = McpLoadAssetForDiff(NewLoadPath, AssetName);

    // Release both diff packages and purge them synchronously so a later
    // find_objects/list cannot see the stale copies, then drop staged copies.
    auto CleanupDiffArtifacts = [OldObj, NewObj, &StagedFiles]()
    {
        ReleaseDiffPackage(OldObj);
        ReleaseDiffPackage(NewObj);
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        for (const FString& Staged : StagedFiles)
        {
            IFileManager::Get().Delete(*Staged, false, true, true);
        }
    };

    if (!OldObj || !NewObj)
    {
        const FString LoadError = FString::Printf(
            TEXT("Failed to load '%s' for diff (old=%s new=%s; oldFilePath='%s', newFilePath='%s'). Each file must be a valid .uasset/.umap package saved by a compatible engine version."),
            *AssetName, OldObj ? TEXT("ok") : TEXT("null"), NewObj ? TEXT("ok") : TEXT("null"),
            *OldFilePath, *NewFilePath);
        CleanupDiffArtifacts();
        S.SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("DIFF_LOAD_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("assetName"), AssetName);
    Resp->SetStringField(TEXT("oldFilePath"), OldFilePath);
    Resp->SetStringField(TEXT("newFilePath"), NewFilePath);
    Resp->SetStringField(TEXT("oldClass"), OldObj->GetClass()->GetName());
    Resp->SetStringField(TEXT("newClass"), NewObj->GetClass()->GetName());

    bool bAnyChange = false;
    TArray<TSharedPtr<FJsonValue>> GasSignals;

    UBlueprint* OldBP = Cast<UBlueprint>(OldObj);
    UBlueprint* NewBP = Cast<UBlueprint>(NewObj);

    if (OldBP && NewBP)
    {
        Resp->SetStringField(TEXT("assetType"), TEXT("Blueprint"));

        // --- Parent class ---
        const FString OldParent = OldBP->ParentClass ? OldBP->ParentClass->GetName() : TEXT("None");
        const FString NewParent = NewBP->ParentClass ? NewBP->ParentClass->GetName() : TEXT("None");
        {
            TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
            P->SetStringField(TEXT("old"), OldParent);
            P->SetStringField(TEXT("new"), NewParent);
            const bool bChanged = OldParent != NewParent;
            P->SetBoolField(TEXT("changed"), bChanged);
            bAnyChange |= bChanged;
            Resp->SetObjectField(TEXT("parentClass"), P);
        }

        // --- Implemented interfaces ---
        {
            TSet<FString> OldSet, NewSet;
            for (const FBPInterfaceDescription& I : OldBP->ImplementedInterfaces) { if (I.Interface) { OldSet.Add(I.Interface->GetName()); } }
            for (const FBPInterfaceDescription& I : NewBP->ImplementedInterfaces) { if (I.Interface) { NewSet.Add(I.Interface->GetName()); } }
            TArray<TSharedPtr<FJsonValue>> Added, Removed;
            for (const FString& S : NewSet) { if (!OldSet.Contains(S)) { Added.Add(MakeShared<FJsonValueString>(S)); if (McpNameLooksGAS(S)) { GasSignals.Add(MakeShared<FJsonValueString>(TEXT("interface +") + S)); } } }
            for (const FString& S : OldSet) { if (!NewSet.Contains(S)) { Removed.Add(MakeShared<FJsonValueString>(S)); } }
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetArrayField(TEXT("added"), Added);
            O->SetArrayField(TEXT("removed"), Removed);
            bAnyChange |= (Added.Num() + Removed.Num()) > 0;
            Resp->SetObjectField(TEXT("interfaces"), O);
        }

        // --- SCS components ---
        {
            auto ScsMap = [](UBlueprint* BP)
            {
                TMap<FString, USCS_Node*> M;
                if (BP->SimpleConstructionScript)
                {
                    for (USCS_Node* N : BP->SimpleConstructionScript->GetAllNodes())
                    {
                        if (N) { M.Add(N->GetVariableName().ToString(), N); }
                    }
                }
                return M;
            };
            auto NodeClass = [](USCS_Node* N) -> FString
            {
                UClass* CC = N->ComponentTemplate ? N->ComponentTemplate->GetClass() : N->ComponentClass.Get();
                return CC ? CC->GetName() : FString(TEXT("?"));
            };
            // Object refs inside a template stringify with the per-side diff
            // package name, so identical refs would read as changed; strip it.
            const FString OldPkgName = OldObj->GetPackage() ? OldObj->GetPackage()->GetName() : FString();
            const FString NewPkgName = NewObj->GetPackage() ? NewObj->GetPackage()->GetName() : FString();
            auto TemplateValueString = [](const TSharedPtr<FJsonValue>& V, const FString& PkgName)
            {
                FString S = McpStableJsonValueString(V);
                if (!PkgName.IsEmpty()) { S.ReplaceInline(*PkgName, TEXT("<pkg>")); }
                return S;
            };

            TMap<FString, USCS_Node*> OldM = ScsMap(OldBP), NewM = ScsMap(NewBP);
            TArray<TSharedPtr<FJsonValue>> Added, Removed, Changed;
            for (const TPair<FString, USCS_Node*>& Kv : NewM)
            {
                USCS_Node* const* OldNode = OldM.Find(Kv.Key);
                const FString NewClass = NodeClass(Kv.Value);
                if (!OldNode)
                {
                    TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                    E->SetStringField(TEXT("name"), Kv.Key);
                    E->SetStringField(TEXT("class"), NewClass);
                    Added.Add(MakeShared<FJsonValueObject>(E));
                    if (McpNameLooksGAS(NewClass) || McpNameLooksGAS(Kv.Key)) { GasSignals.Add(MakeShared<FJsonValueString>(TEXT("component +") + Kv.Key + TEXT(" (") + NewClass + TEXT(")"))); }
                }
                else if (NodeClass(*OldNode) != NewClass)
                {
                    TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                    E->SetStringField(TEXT("name"), Kv.Key);
                    E->SetStringField(TEXT("oldClass"), NodeClass(*OldNode));
                    E->SetStringField(TEXT("newClass"), NewClass);
                    Changed.Add(MakeShared<FJsonValueObject>(E));
                }
                else if ((*OldNode)->ComponentTemplate && Kv.Value->ComponentTemplate)
                {
                    // Same class on both sides: diff the template's defaults so
                    // component-only edits (e.g. root RelativeLocation) surface.
                    TSharedPtr<FJsonObject> OldProps = McpPropertyReflection::ExportObjectToJson((*OldNode)->ComponentTemplate, false);
                    TSharedPtr<FJsonObject> NewProps = McpPropertyReflection::ExportObjectToJson(Kv.Value->ComponentTemplate, false);
                    TArray<TSharedPtr<FJsonValue>> PropChanges;
                    const int32 PropCap = 50;
                    if (OldProps.IsValid() && NewProps.IsValid())
                    {
                        for (const TPair<FString, TSharedPtr<FJsonValue>>& PropKv : NewProps->Values)
                        {
                            if (PropChanges.Num() >= PropCap) { break; }
                            const TSharedPtr<FJsonValue>* OldV = OldProps->Values.Find(PropKv.Key);
                            const FString NewS = TemplateValueString(PropKv.Value, NewPkgName);
                            const FString OldS = OldV ? TemplateValueString(*OldV, OldPkgName) : FString(TEXT("<absent>"));
                            if (NewS != OldS)
                            {
                                TSharedPtr<FJsonObject> PE = MakeShared<FJsonObject>();
                                PE->SetStringField(TEXT("property"), PropKv.Key);
                                PE->SetStringField(TEXT("old"), OldS.Left(400));
                                PE->SetStringField(TEXT("new"), NewS.Left(400));
                                PropChanges.Add(MakeShared<FJsonValueObject>(PE));
                            }
                        }
                        for (const TPair<FString, TSharedPtr<FJsonValue>>& PropKv : OldProps->Values)
                        {
                            if (PropChanges.Num() >= PropCap) { break; }
                            if (!NewProps->Values.Contains(PropKv.Key))
                            {
                                TSharedPtr<FJsonObject> PE = MakeShared<FJsonObject>();
                                PE->SetStringField(TEXT("property"), PropKv.Key);
                                PE->SetStringField(TEXT("old"), TemplateValueString(PropKv.Value, OldPkgName).Left(400));
                                PE->SetStringField(TEXT("new"), TEXT("<absent>"));
                                PropChanges.Add(MakeShared<FJsonValueObject>(PE));
                            }
                        }
                    }
                    if (PropChanges.Num() > 0)
                    {
                        TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                        E->SetStringField(TEXT("name"), Kv.Key);
                        E->SetStringField(TEXT("class"), NewClass);
                        E->SetArrayField(TEXT("changedProperties"), PropChanges);
                        Changed.Add(MakeShared<FJsonValueObject>(E));
                        if (McpNameLooksGAS(NewClass) || McpNameLooksGAS(Kv.Key)) { GasSignals.Add(MakeShared<FJsonValueString>(TEXT("component ~") + Kv.Key)); }
                    }
                }
            }
            for (const TPair<FString, USCS_Node*>& Kv : OldM)
            {
                if (!NewM.Contains(Kv.Key))
                {
                    TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                    E->SetStringField(TEXT("name"), Kv.Key);
                    E->SetStringField(TEXT("class"), NodeClass(Kv.Value));
                    Removed.Add(MakeShared<FJsonValueObject>(E));
                }
            }
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetArrayField(TEXT("added"), Added);
            O->SetArrayField(TEXT("removed"), Removed);
            O->SetArrayField(TEXT("changed"), Changed);
            bAnyChange |= (Added.Num() + Removed.Num() + Changed.Num()) > 0;
            Resp->SetObjectField(TEXT("components"), O);
        }

        // --- Variables ---
        {
            auto VarMap = [](UBlueprint* BP)
            {
                TMap<FString, FString> M;
                for (const FBPVariableDescription& V : BP->NewVariables) { M.Add(V.VarName.ToString(), McpVarTypeString(V.VarType)); }
                return M;
            };
            TMap<FString, FString> OldM = VarMap(OldBP), NewM = VarMap(NewBP);
            TArray<TSharedPtr<FJsonValue>> Added, Removed, Changed;
            for (const TPair<FString, FString>& Kv : NewM)
            {
                const FString* Old = OldM.Find(Kv.Key);
                if (!Old)
                {
                    TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                    E->SetStringField(TEXT("name"), Kv.Key);
                    E->SetStringField(TEXT("type"), Kv.Value);
                    Added.Add(MakeShared<FJsonValueObject>(E));
                    if (McpNameLooksGAS(Kv.Value) || McpNameLooksGAS(Kv.Key)) { GasSignals.Add(MakeShared<FJsonValueString>(TEXT("variable +") + Kv.Key + TEXT(" : ") + Kv.Value)); }
                }
                else if (*Old != Kv.Value)
                {
                    TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                    E->SetStringField(TEXT("name"), Kv.Key);
                    E->SetStringField(TEXT("oldType"), *Old);
                    E->SetStringField(TEXT("newType"), Kv.Value);
                    Changed.Add(MakeShared<FJsonValueObject>(E));
                }
            }
            for (const TPair<FString, FString>& Kv : OldM)
            {
                if (!NewM.Contains(Kv.Key))
                {
                    TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                    E->SetStringField(TEXT("name"), Kv.Key);
                    E->SetStringField(TEXT("type"), Kv.Value);
                    Removed.Add(MakeShared<FJsonValueObject>(E));
                }
            }
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetArrayField(TEXT("added"), Added);
            O->SetArrayField(TEXT("removed"), Removed);
            O->SetArrayField(TEXT("changed"), Changed);
            bAnyChange |= (Added.Num() + Removed.Num() + Changed.Num()) > 0;
            Resp->SetObjectField(TEXT("variables"), O);
        }

        // --- Function / event graphs (by name + node count) ---
        {
            auto GraphMap = [](UBlueprint* BP)
            {
                TMap<FString, int32> M;
                for (UEdGraph* G : BP->FunctionGraphs) { if (G) { M.Add(G->GetName(), G->Nodes.Num()); } }
                for (UEdGraph* G : BP->UbergraphPages) { if (G) { M.Add(G->GetName(), G->Nodes.Num()); } }
                return M;
            };
            TMap<FString, int32> OldM = GraphMap(OldBP), NewM = GraphMap(NewBP);
            TArray<TSharedPtr<FJsonValue>> Added, Removed, Changed;
            for (const TPair<FString, int32>& Kv : NewM)
            {
                const int32* Old = OldM.Find(Kv.Key);
                if (!Old) { Added.Add(MakeShared<FJsonValueString>(Kv.Key)); if (McpNameLooksGAS(Kv.Key)) { GasSignals.Add(MakeShared<FJsonValueString>(TEXT("graph +") + Kv.Key)); } }
                else if (*Old != Kv.Value)
                {
                    TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                    E->SetStringField(TEXT("name"), Kv.Key);
                    E->SetNumberField(TEXT("oldNodeCount"), *Old);
                    E->SetNumberField(TEXT("newNodeCount"), Kv.Value);
                    Changed.Add(MakeShared<FJsonValueObject>(E));
                }
            }
            for (const TPair<FString, int32>& Kv : OldM) { if (!NewM.Contains(Kv.Key)) { Removed.Add(MakeShared<FJsonValueString>(Kv.Key)); } }
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetArrayField(TEXT("added"), Added);
            O->SetArrayField(TEXT("removed"), Removed);
            O->SetArrayField(TEXT("changed"), Changed);
            bAnyChange |= (Added.Num() + Removed.Num() + Changed.Num()) > 0;
            Resp->SetObjectField(TEXT("graphs"), O);
        }

        // --- CDO default properties ---
        if (bIncludeDefaults)
        {
            UObject* OldCDO = OldBP->GeneratedClass ? OldBP->GeneratedClass->GetDefaultObject() : nullptr;
            UObject* NewCDO = NewBP->GeneratedClass ? NewBP->GeneratedClass->GetDefaultObject() : nullptr;
            if (OldCDO && NewCDO)
            {
                TSharedPtr<FJsonObject> OldProps = McpPropertyReflection::ExportObjectToJson(OldCDO, false);
                TSharedPtr<FJsonObject> NewProps = McpPropertyReflection::ExportObjectToJson(NewCDO, false);
                TArray<TSharedPtr<FJsonValue>> Changed, Added, Removed;
                const int32 Cap = 200;
                if (OldProps.IsValid() && NewProps.IsValid())
                {
                    for (const TPair<FString, TSharedPtr<FJsonValue>>& Kv : NewProps->Values)
                    {
                        const TSharedPtr<FJsonValue>* OldV = OldProps->Values.Find(Kv.Key);
                        if (!OldV)
                        {
                            if (Added.Num() < Cap) { Added.Add(MakeShared<FJsonValueString>(Kv.Key)); }
                        }
                        else
                        {
                            const FString NewS = McpStableJsonValueString(Kv.Value);
                            const FString OldS = McpStableJsonValueString(*OldV);
                            if (NewS != OldS && Changed.Num() < Cap)
                            {
                                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                                E->SetStringField(TEXT("property"), Kv.Key);
                                E->SetStringField(TEXT("old"), OldS.Left(400));
                                E->SetStringField(TEXT("new"), NewS.Left(400));
                                Changed.Add(MakeShared<FJsonValueObject>(E));
                                if (McpNameLooksGAS(Kv.Key)) { GasSignals.Add(MakeShared<FJsonValueString>(TEXT("default ~") + Kv.Key)); }
                            }
                        }
                    }
                    for (const TPair<FString, TSharedPtr<FJsonValue>>& Kv : OldProps->Values) { if (!NewProps->Values.Contains(Kv.Key) && Removed.Num() < Cap) { Removed.Add(MakeShared<FJsonValueString>(Kv.Key)); } }
                }
                TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
                O->SetArrayField(TEXT("changed"), Changed);
                O->SetArrayField(TEXT("added"), Added);
                O->SetArrayField(TEXT("removed"), Removed);
                O->SetStringField(TEXT("note"), TEXT("Default-value diff; internal subobject references may appear changed due to diff-package naming."));
                bAnyChange |= (Changed.Num() + Added.Num() + Removed.Num()) > 0;
                Resp->SetObjectField(TEXT("defaults"), O);
            }
        }
    }
    else
    {
        // Non-Blueprint asset: generic property diff on the asset objects.
        Resp->SetStringField(TEXT("assetType"), TEXT("Object"));
        TSharedPtr<FJsonObject> OldProps = McpPropertyReflection::ExportObjectToJson(OldObj, false);
        TSharedPtr<FJsonObject> NewProps = McpPropertyReflection::ExportObjectToJson(NewObj, false);
        TArray<TSharedPtr<FJsonValue>> Changed, Added, Removed;
        const int32 Cap = 300;
        if (OldProps.IsValid() && NewProps.IsValid())
        {
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Kv : NewProps->Values)
            {
                const TSharedPtr<FJsonValue>* OldV = OldProps->Values.Find(Kv.Key);
                if (!OldV) { if (Added.Num() < Cap) { Added.Add(MakeShared<FJsonValueString>(Kv.Key)); } }
                else
                {
                    const FString NewS = McpStableJsonValueString(Kv.Value);
                    const FString OldS = McpStableJsonValueString(*OldV);
                    if (NewS != OldS && Changed.Num() < Cap)
                    {
                        TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                        E->SetStringField(TEXT("property"), Kv.Key);
                        E->SetStringField(TEXT("old"), OldS.Left(400));
                        E->SetStringField(TEXT("new"), NewS.Left(400));
                        Changed.Add(MakeShared<FJsonValueObject>(E));
                    }
                }
            }
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Kv : OldProps->Values) { if (!NewProps->Values.Contains(Kv.Key) && Removed.Num() < Cap) { Removed.Add(MakeShared<FJsonValueString>(Kv.Key)); } }
        }
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetArrayField(TEXT("changed"), Changed);
        O->SetArrayField(TEXT("added"), Added);
        O->SetArrayField(TEXT("removed"), Removed);
        bAnyChange |= (Changed.Num() + Added.Num() + Removed.Num()) > 0;
        Resp->SetObjectField(TEXT("properties"), O);
    }

    // Everything is extracted into Resp now.
    CleanupDiffArtifacts();

    Resp->SetArrayField(TEXT("gasSignals"), GasSignals);
    Resp->SetBoolField(TEXT("anyChange"), bAnyChange);
    Resp->SetBoolField(TEXT("success"), true);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Asset diff completed"), Resp, FString());
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("diff_asset requires editor build."), TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
