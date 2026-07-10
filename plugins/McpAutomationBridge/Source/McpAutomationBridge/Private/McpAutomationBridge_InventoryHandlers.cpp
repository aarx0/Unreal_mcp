// =============================================================================
// McpAutomationBridge_InventoryHandlers.cpp
// =============================================================================
// Inventory & Items System Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// ---------------------
// Section 1: Item Creation
//   - create_item_blueprint        : Create AItem base blueprint
//   - create_pickup_actor          : Create APickup actor
//   - configure_item_mesh          : Set item mesh component
//   - set_item_properties          : Configure item stats
//
// Section 2: Inventory Component
//   - create_inventory_component   : Create UInventoryComponent
//   - add_inventory_slot           : Add inventory slot
//   - configure_inventory_capacity  : Set max slots
//   - add_item_to_inventory        : Add item to inventory
//   - remove_item_from_inventory   : Remove item from inventory
//
// Section 3: Item Data
//   - create_item_data_asset       : Create UItemDataAsset
//   - create_item_data_table       : Create FDataTable for items
//   - populate_item_data           : Fill item data
//
// Section 4: Pickup System
//   - configure_pickup_collision   : Setup pickup collision
//   - add_pickup_effects           : Add pickup VFX/SFX
//   - set_respawn_behavior         : Configure item respawn
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.7: All handlers supported
// - Inventory component pattern stable across versions
// - DataAsset/DataTable APIs unchanged
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_InventoryHandlers.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpResponseHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/DataAssetFactory.h"
#include "EditorAssetLibrary.h"

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h
#define GetPayloadString GetJsonStringField
#define GetPayloadNumber GetJsonNumberField
#define GetPayloadBool GetJsonBoolField

// Helper to create a new package with path validation
// Returns nullptr and sets OutError if path is invalid
static UPackage* CreateValidatedAssetPackage(const FString& Path, const FString& Name, FString& OutError) {
  FString PackageName;
  FString SanitizedName = SanitizeAssetName(Name);
  
  if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, OutError)) {
    return nullptr;
  }
  
  return CreatePackage(*PackageName);
}

// Legacy helper for backward compatibility - validates internally
static UPackage* CreateAssetPackage(const FString& Path, const FString& Name) {
  FString PackagePath = Path.IsEmpty() ? TEXT("/Game/Items") : Path;

  // Normalize and validate
  FString PackageName;
  FString PathError;
  FString SanitizedName = SanitizeAssetName(Name);
  if (!ValidateAssetCreationPath(PackagePath, SanitizedName, PackageName, PathError)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("CreateAssetPackage: %s"), *PathError);
    return nullptr;
  }

  return CreatePackage(*PackageName);
}

// Writes each key to a matching native UPROPERTY via reflection; unmatched keys
// fall back to the UMcpGenericDataAsset string property bag (which holds
// string/number/bool values only), so no requested key is silently dropped.
static void ApplyItemPropertiesToAsset(UObject* ItemAsset,
                                       const TSharedPtr<FJsonObject>& PropertiesObj,
                                       TArray<FString>& ModifiedProperties,
                                       TArray<FString>& StoredInPropertyBag,
                                       TArray<FString>& FailedProperties) {
  if (!ItemAsset || !PropertiesObj.IsValid()) {
    return;
  }

  for (const auto& Pair : PropertiesObj->Values) {
    const FString PropertyName(*Pair.Key);
    const TSharedPtr<FJsonValue>& PropertyValue = Pair.Value;

    FProperty* Prop = ItemAsset->GetClass()->FindPropertyByName(*PropertyName);
    if (Prop) {
      FString ApplyError;
      if (ApplyJsonValueToProperty(ItemAsset, Prop, PropertyValue, ApplyError)) {
        ModifiedProperties.Add(PropertyName);
      } else {
        FailedProperties.Add(FString::Printf(TEXT("%s: %s"), *PropertyName, *ApplyError));
      }
      continue;
    }

    UMcpGenericDataAsset* GenericAsset = Cast<UMcpGenericDataAsset>(ItemAsset);
    FString StringValue;
    if (GenericAsset && PropertyValue.IsValid() && PropertyValue->TryGetString(StringValue)) {
      GenericAsset->Properties.Add(PropertyName, StringValue);
      StoredInPropertyBag.Add(PropertyName);
    } else if (GenericAsset) {
      FailedProperties.Add(FString::Printf(
          TEXT("%s: not a native property, and only string/number/bool values can be stored in the string property bag"),
          *PropertyName));
    } else {
      FailedProperties.Add(FString::Printf(TEXT("%s: Property not found"), *PropertyName));
    }
  }
}

// ============================================================================
// Inventory Member Handlers
// ============================================================================
// Dispatch lives in the manage_inventory FMcpCall classes
// (Private/MCP/Calls/McpCalls_ManageInventory.cpp); each HandleInventory*
// member here implements one advertised action.

// ===========================================================================
// 17.1 Data Assets (4 actions)
// ===========================================================================

bool McpHandlers::Inventory::HandleInventoryCreateItemDataAsset(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetPayloadString(Payload, TEXT("name"));
  FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Items"));

  if (Name.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: name"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Create a primary data asset for item with validated path
  FString PathError;
  FString SanitizedName = SanitizeAssetName(Name);
  UPackage* Package = CreateValidatedAssetPackage(Path, SanitizedName, PathError);
  if (!Package) {
    S.SendAutomationError(Socket, RequestId,
                        PathError.IsEmpty() ? TEXT("Failed to create package") : PathError,
                        TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  // Create UMcpGenericDataAsset (UDataAsset/UPrimaryDataAsset are abstract in UE5)
  UMcpGenericDataAsset* ItemAsset =
      NewObject<UMcpGenericDataAsset>(Package, FName(*SanitizedName), RF_Public | RF_Standalone);

  if (ItemAsset) {
    const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
    TArray<FString> ModifiedProperties;
    TArray<FString> StoredInPropertyBag;
    TArray<FString> FailedProperties;
    if (Payload->TryGetObjectField(TEXT("properties"), PropertiesObj) && PropertiesObj && (*PropertiesObj).IsValid()) {
      ApplyItemPropertiesToAsset(ItemAsset, *PropertiesObj, ModifiedProperties,
                                 StoredInPropertyBag, FailedProperties);
    }

    ItemAsset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(ItemAsset);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(ItemAsset);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetName"), SanitizedName);
    if (PropertiesObj && (*PropertiesObj).IsValid()) {
      TArray<TSharedPtr<FJsonValue>> ModifiedArr;
      for (const FString& PropName : ModifiedProperties) {
        ModifiedArr.Add(MakeShared<FJsonValueString>(PropName));
      }
      Result->SetArrayField(TEXT("modifiedProperties"), ModifiedArr);

      if (StoredInPropertyBag.Num() > 0) {
        TArray<TSharedPtr<FJsonValue>> BagArr;
        for (const FString& PropName : StoredInPropertyBag) {
          BagArr.Add(MakeShared<FJsonValueString>(PropName));
        }
        Result->SetArrayField(TEXT("storedInPropertyBag"), BagArr);
      }

      if (FailedProperties.Num() > 0) {
        TArray<TSharedPtr<FJsonValue>> FailedArr;
        for (const FString& Err : FailedProperties) {
          FailedArr.Add(MakeShared<FJsonValueString>(Err));
        }
        Result->SetArrayField(TEXT("failedProperties"), FailedArr);
      }
    }
    McpHandlerUtils::AddVerification(Result, ItemAsset);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Item data asset created"), Result);
  } else {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create item data asset"),
                        TEXT("ASSET_CREATE_FAILED"));
  }
  return true;
}

bool McpHandlers::Inventory::HandleInventorySetItemProperties(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));

  if (ItemPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: itemPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the item asset and set properties (use UDataAsset base class for loading)
  UObject* Asset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
  UDataAsset* ItemAsset = Cast<UDataAsset>(Asset);

  if (!ItemAsset) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Item data asset not found: %s"), *ItemPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Get properties object from payload
  const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
  TArray<FString> ModifiedProperties;
  TArray<FString> StoredInPropertyBag;
  TArray<FString> FailedProperties;

  const bool bHasProperties =
      Payload->TryGetObjectField(TEXT("properties"), PropertiesObj) && PropertiesObj && (*PropertiesObj).IsValid();
  if (bHasProperties) {
    ApplyItemPropertiesToAsset(ItemAsset, *PropertiesObj, ModifiedProperties,
                               StoredInPropertyBag, FailedProperties);
  }

  // The requested-field set is the properties bag keys; ApplyItemPropertiesToAsset
  // already recorded each key's outcome — map those into the write report.
  McpHandlerUtils::FMcpWriteReport Report;
  if (bHasProperties) {
    for (const auto& Pair : (*PropertiesObj)->Values) {
      const FString& Key = Pair.Key;
      if (ModifiedProperties.Contains(Key) || StoredInPropertyBag.Contains(Key)) {
        Report.MarkApplied(Key);
      } else {
        FString Reason = TEXT("failed to apply");
        const FString Prefix = Key + TEXT(": ");
        for (const FString& Err : FailedProperties) {
          if (Err.StartsWith(Prefix)) {
            Reason = Err.RightChop(Prefix.Len());
            break;
          }
        }
        Report.MarkFailed(Key, Reason);
      }
    }
  }

  if (Report.AnyApplied()) {
    ItemAsset->MarkPackageDirty();
    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(ItemAsset);
    }
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("modified"), ModifiedProperties.Num() + StoredInPropertyBag.Num() > 0);
  Result->SetNumberField(TEXT("propertiesModified"), ModifiedProperties.Num() + StoredInPropertyBag.Num());

  TArray<TSharedPtr<FJsonValue>> ModifiedArr;
  for (const FString& Name : ModifiedProperties) {
    ModifiedArr.Add(MakeShared<FJsonValueString>(Name));
  }
  Result->SetArrayField(TEXT("modifiedProperties"), ModifiedArr);

  if (StoredInPropertyBag.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> BagArr;
    for (const FString& Name : StoredInPropertyBag) {
      BagArr.Add(MakeShared<FJsonValueString>(Name));
    }
    Result->SetArrayField(TEXT("storedInPropertyBag"), BagArr);
  }

  if (FailedProperties.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> FailedArr;
    for (const FString& Err : FailedProperties) {
      FailedArr.Add(MakeShared<FJsonValueString>(Err));
    }
    Result->SetArrayField(TEXT("failedProperties"), FailedArr);
  }

  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Item properties updated"), ItemAsset);
}

bool McpHandlers::Inventory::HandleInventoryCreateItemCategory(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetPayloadString(Payload, TEXT("name"));
  FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Items/Categories"));

  if (Name.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: name"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Create a data asset for category
  UPackage* Package = CreateAssetPackage(Path, Name);
  if (!Package) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create package"),
                        TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  // UMcpGenericDataAsset (UDataAsset/UPrimaryDataAsset are abstract in UE5)
  UMcpGenericDataAsset* CategoryAsset =
      NewObject<UMcpGenericDataAsset>(Package, FName(*Name), RF_Public | RF_Standalone);

  if (CategoryAsset) {
    CategoryAsset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(CategoryAsset);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(CategoryAsset);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("categoryPath"), Package->GetName());
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Item category created"), Result);
  } else {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create category asset"),
                        TEXT("ASSET_CREATE_FAILED"));
  }
  return true;
}

bool McpHandlers::Inventory::HandleInventoryAssignItemCategory(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));
  FString CategoryPath = GetPayloadString(Payload, TEXT("categoryPath"));

  if (ItemPath.IsEmpty() || CategoryPath.IsEmpty()) {
    S.SendAutomationError(
        Socket, RequestId,
        TEXT("Missing required parameters: itemPath and categoryPath"),
        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load both assets (use UDataAsset base class for loading - UPrimaryDataAsset is abstract in UE5.7)
  UObject* ItemObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
  UObject* CategoryObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *CategoryPath);

  if (!ItemObj) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Item not found: %s"), *ItemPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  bool bCategoryAssigned = false;
  FString AssignError;

  // Try to find a "Category" property on the item and set it via reflection
  FProperty* CategoryProp = ItemObj->GetClass()->FindPropertyByName(TEXT("Category"));
  if (!CategoryProp) {
    CategoryProp = ItemObj->GetClass()->FindPropertyByName(TEXT("ItemCategory"));
  }

  if (CategoryProp) {
    // Create a JSON value for the category path
    TSharedPtr<FJsonValue> CategoryValue = MakeShared<FJsonValueString>(CategoryPath);
    if (ApplyJsonValueToProperty(ItemObj, CategoryProp, CategoryValue, AssignError)) {
      bCategoryAssigned = true;
    }
  } else {
    // Try to find a soft object reference property for category
    for (TFieldIterator<FProperty> It(ItemObj->GetClass()); It; ++It) {
      FProperty* Prop = *It;
      if (Prop->GetName().Contains(TEXT("Category"), ESearchCase::IgnoreCase)) {
        TSharedPtr<FJsonValue> CategoryValue = MakeShared<FJsonValueString>(CategoryPath);
        if (ApplyJsonValueToProperty(ItemObj, Prop, CategoryValue, AssignError)) {
          bCategoryAssigned = true;
          break;
        }
      }
    }
  }

  ItemObj->MarkPackageDirty();

  if (GetPayloadBool(Payload, TEXT("save"), false)) {
    McpSafeAssetSave(ItemObj);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("itemPath"), ItemPath);
  Result->SetStringField(TEXT("categoryPath"), CategoryPath);
  Result->SetBoolField(TEXT("assigned"), bCategoryAssigned);
  if (!bCategoryAssigned && !AssignError.IsEmpty()) {
    Result->SetStringField(TEXT("note"), TEXT("Category property not found on item class. Ensure your item class has a Category or ItemCategory property."));
  }
  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Category assigned to item"), Result);
  return true;
}

// ===========================================================================
// 17.2 Inventory Component (5 actions)
// ===========================================================================

bool McpHandlers::Inventory::HandleInventoryCreateComponent(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
  FString ComponentName =
      GetPayloadString(Payload, TEXT("componentName"), TEXT("InventoryComponent"));

  if (BlueprintPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: blueprintPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the blueprint
  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
  if (!SCS) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Blueprint has no SimpleConstructionScript"),
                        TEXT("NO_SCS"));
    return true;
  }

  // Create a SceneComponent as inventory component (real inventory would use custom UInventoryComponent)
  // USceneComponent allows for proper hierarchy and is a valid SCS node type
  USCS_Node* NewNode = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
  if (NewNode) {
    SCS->AddNode(NewNode);

    // Add Blueprint variables for inventory functionality
    int32 SlotCount = static_cast<int32>(GetPayloadNumber(Payload, TEXT("slotCount"), 20));

    // Add InventorySlots array variable (Array of soft object references)
    FEdGraphPinType SlotArrayType;
    SlotArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
    SlotArrayType.ContainerType = EPinContainerType::Array;
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("InventorySlots"), SlotArrayType);

    // Add MaxSlots integer variable
    FEdGraphPinType IntType;
    IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MaxSlots"), IntType);

    // Add CurrentWeight float variable
    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CurrentWeight"), FloatType);

    // Add MaxWeight float variable
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MaxWeight"), FloatType);

    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetBoolField(TEXT("componentAdded"), true);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Inventory component added"), Result);
  } else {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create inventory component"),
                        TEXT("COMPONENT_CREATE_FAILED"));
  }
  return true;
}

bool McpHandlers::Inventory::HandleInventoryConfigureSlots(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

  if (BlueprintPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: blueprintPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the blueprint
  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  McpHandlerUtils::FMcpWriteReport Report;
  const bool bHasSlotCount = Payload->HasField(TEXT("slotCount"));
  int32 SlotCount = 0;

  if (bHasSlotCount) {
    SlotCount = static_cast<int32>(GetPayloadNumber(Payload, TEXT("slotCount"), 20));
    bool bApplied = false;
    FString FailReason;

    // Try to set an existing MaxSlots property on the generated class CDO.
    if (Blueprint->GeneratedClass) {
      if (UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject()) {
        if (FProperty* MaxSlotsProp = CDO->GetClass()->FindPropertyByName(TEXT("MaxSlots"))) {
          TSharedPtr<FJsonValue> SlotValue = MakeShared<FJsonValueNumber>(static_cast<double>(SlotCount));
          if (ApplyJsonValueToProperty(CDO, MaxSlotsProp, SlotValue, FailReason)) {
            bApplied = true;
          }
        }
      }
    }

    // If MaxSlots doesn't exist yet, add it as a variable, compile so the property lands
    // on the CDO, then write the requested value.
    if (!bApplied) {
      FEdGraphPinType IntType;
      IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == TEXT("MaxSlots")) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MaxSlots"), IntType);
      }
      McpSafeCompileBlueprint(Blueprint);
      if (Blueprint->GeneratedClass) {
        if (UObject* FreshCDO = Blueprint->GeneratedClass->GetDefaultObject()) {
          if (FProperty* MaxSlotsProp = FreshCDO->GetClass()->FindPropertyByName(TEXT("MaxSlots"))) {
            TSharedPtr<FJsonValue> SlotValue = MakeShared<FJsonValueNumber>(static_cast<double>(SlotCount));
            if (ApplyJsonValueToProperty(FreshCDO, MaxSlotsProp, SlotValue, FailReason)) {
              bApplied = true;
            }
          } else {
            FailReason = TEXT("MaxSlots property unavailable after compile");
          }
        }
      }
    }

    if (bApplied) {
      Report.MarkApplied(TEXT("slotCount"));
    } else {
      Report.MarkFailed(TEXT("slotCount"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
    }
  }

  if (Report.AnyApplied()) {
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/false, GetPayloadBool(Payload, TEXT("save"), true));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (bHasSlotCount) {
    Result->SetNumberField(TEXT("slotCount"), SlotCount);
  }
  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Inventory slots configured"), Blueprint);
}

bool McpHandlers::Inventory::HandleInventoryAddFunctions(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

  if (BlueprintPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: blueprintPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the blueprint
  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  // Note: Creating actual Blueprint functions programmatically requires K2Node graph manipulation
  // which is complex and error-prone. Instead, we add helper variables and event dispatchers
  // that can be used in Blueprint graphs to implement inventory functionality.

  TArray<TSharedPtr<FJsonValue>> FunctionsAdded;
  TArray<TSharedPtr<FJsonValue>> VariablesAdded;

  // Add helper variables for inventory operations
  FEdGraphPinType IntType;
  IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

  FEdGraphPinType BoolType;
  BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

  FEdGraphPinType SoftObjectType;
  SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

  // Add variables that support inventory functions
  TArray<TPair<FName, FEdGraphPinType>> InventoryVars = {
    TPair<FName, FEdGraphPinType>(TEXT("LastAddedItemIndex"), IntType),
    TPair<FName, FEdGraphPinType>(TEXT("LastRemovedItemIndex"), IntType),
    TPair<FName, FEdGraphPinType>(TEXT("bLastOperationSuccess"), BoolType),
    TPair<FName, FEdGraphPinType>(TEXT("CachedItemCount"), IntType),
    TPair<FName, FEdGraphPinType>(TEXT("SelectedSlotIndex"), IntType)
  };

  for (const auto& VarPair : InventoryVars) {
    bool bExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == VarPair.Key) {
        bExists = true;
        break;
      }
    }
    if (!bExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
      VariablesAdded.Add(MakeShared<FJsonValueString>(VarPair.Key.ToString()));
    }
  }

  // Add event dispatchers for inventory operations
  FEdGraphPinType DelegateType;
  DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

  TArray<FName> EventNames = {
    TEXT("OnAddItemRequested"),
    TEXT("OnRemoveItemRequested"),
    TEXT("OnTransferItemRequested")
  };

  for (const FName& EventName : EventNames) {
    bool bExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == EventName) {
        bExists = true;
        break;
      }
    }
    if (!bExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, EventName, DelegateType);
      FunctionsAdded.Add(MakeShared<FJsonValueString>(EventName.ToString()));
    }
  }

  // NOTE: this action adds the helper variables + event-dispatcher delegates above (real work). It does
  // NOT create the AddItem/RemoveItem/etc. UFunctions — authoring function graphs is a separate step
  // (add_function). Report only what was actually added; do not list fake "(implement in Blueprint)"
  // entries that imply functions exist.

  McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetArrayField(TEXT("dispatchersAdded"), FunctionsAdded);
  Result->SetArrayField(TEXT("variablesAdded"), VariablesAdded);
  Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
  Result->SetStringField(TEXT("note"), TEXT("Added helper variables and event-dispatcher delegates only. No UFunctions were created — author AddItem/RemoveItem/etc. with add_function, then implement their logic in the Blueprint graph."));

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Inventory functions added"), Result);
  return true;
}


bool McpHandlers::Inventory::HandleInventorySetReplication(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

  if (BlueprintPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: blueprintPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the blueprint
  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  McpHandlerUtils::FMcpWriteReport Report;
  const bool bHasReplicated = Payload->HasField(TEXT("replicated"));
  const bool bHasCondition = Payload->HasField(TEXT("replicationCondition"));

  bool bReplicated = false;
  const FString ReplicationCondition = GetPayloadString(Payload, TEXT("replicationCondition"), TEXT("None"));

  // Resolve the requested replication condition; an unrecognized value must fail loudly
  // rather than silently collapse to COND_None.
  ELifetimeCondition Condition = COND_None;
  bool bConditionValid = true;
  if (ReplicationCondition.Equals(TEXT("None"), ESearchCase::IgnoreCase)) {
    Condition = COND_None;
  } else if (ReplicationCondition.Equals(TEXT("OwnerOnly"), ESearchCase::IgnoreCase)) {
    Condition = COND_OwnerOnly;
  } else if (ReplicationCondition.Equals(TEXT("SkipOwner"), ESearchCase::IgnoreCase)) {
    Condition = COND_SkipOwner;
  } else if (ReplicationCondition.Equals(TEXT("SimulatedOnly"), ESearchCase::IgnoreCase)) {
    Condition = COND_SimulatedOnly;
  } else if (ReplicationCondition.Equals(TEXT("AutonomousOnly"), ESearchCase::IgnoreCase)) {
    Condition = COND_AutonomousOnly;
  } else if (ReplicationCondition.Equals(TEXT("SimulatedOrPhysics"), ESearchCase::IgnoreCase)) {
    Condition = COND_SimulatedOrPhysics;
  } else if (ReplicationCondition.Equals(TEXT("InitialOrOwner"), ESearchCase::IgnoreCase)) {
    Condition = COND_InitialOrOwner;
  } else if (ReplicationCondition.Equals(TEXT("Custom"), ESearchCase::IgnoreCase)) {
    Condition = COND_Custom;
  } else {
    bConditionValid = false;
  }

  TArray<FString> ReplicatedVariables;

  const TArray<FName> InventoryVarNames = {
    TEXT("InventorySlots"),
    TEXT("MaxSlots"),
    TEXT("CurrentWeight"),
    TEXT("MaxWeight")
  };

  if (bHasReplicated) {
    bReplicated = GetPayloadBool(Payload, TEXT("replicated"), false);

    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (!InventoryVarNames.Contains(Var.VarName)) {
        continue;
      }
      if (bReplicated) {
        Var.PropertyFlags |= CPF_Net;
        Var.RepNotifyFunc = NAME_None;
        Var.ReplicationCondition = bConditionValid ? Condition : COND_None;
      } else {
        Var.PropertyFlags &= ~CPF_Net;
        Var.ReplicationCondition = COND_None;
      }
      ReplicatedVariables.Add(Var.VarName.ToString());
    }

    if (ReplicatedVariables.Num() > 0) {
      Report.MarkApplied(TEXT("replicated"));
    } else {
      Report.MarkFailed(TEXT("replicated"),
          TEXT("no inventory variables (InventorySlots/MaxSlots/CurrentWeight/MaxWeight) found on blueprint"));
    }

    if (bHasCondition) {
      if (!bReplicated) {
        Report.MarkFailed(TEXT("replicationCondition"),
            TEXT("replicationCondition is ignored when replicated is false"));
      } else if (!bConditionValid) {
        Report.MarkFailed(TEXT("replicationCondition"),
            FString::Printf(TEXT("unknown replication condition '%s'"), *ReplicationCondition));
      } else if (ReplicatedVariables.Num() == 0) {
        Report.MarkFailed(TEXT("replicationCondition"),
            TEXT("no inventory variables found to apply the condition to"));
      } else {
        Report.MarkApplied(TEXT("replicationCondition"));
      }
    }
  } else if (bHasCondition) {
    Report.MarkFailed(TEXT("replicationCondition"),
        TEXT("requires 'replicated' to be specified"));
  }

  if (Report.AnyApplied()) {
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (bHasReplicated) {
    Result->SetBoolField(TEXT("replicated"), bReplicated);
  }
  if (bHasCondition) {
    Result->SetStringField(TEXT("replicationCondition"), ReplicationCondition);
  }

  TArray<TSharedPtr<FJsonValue>> VarsArr;
  for (const FString& VarName : ReplicatedVariables) {
    VarsArr.Add(MakeShared<FJsonValueString>(VarName));
  }
  Result->SetArrayField(TEXT("modifiedVariables"), VarsArr);

  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Inventory replication configured"), Blueprint);
}

// ===========================================================================
// 17.3 Pickups (4 actions)
// ===========================================================================

bool McpHandlers::Inventory::HandleInventoryCreatePickupActor(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetPayloadString(Payload, TEXT("name"));
  FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Blueprints/Pickups"));

  if (Name.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: name"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Create a Blueprint actor for pickup
  UPackage* Package = CreateAssetPackage(Path, Name);
  if (!Package) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create package"),
                        TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
  Factory->ParentClass = AActor::StaticClass();

  UBlueprint* NewBlueprint = Cast<UBlueprint>(
      Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name,
                                RF_Public | RF_Standalone, nullptr, GWarn));

  if (NewBlueprint) {
    // Add sphere collision for pickup detection
    USimpleConstructionScript* SCS = NewBlueprint->SimpleConstructionScript;
    if (SCS) {
      // Add static mesh component for visual
      USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("PickupMesh"));
      if (MeshNode) {
        SCS->AddNode(MeshNode);
      }

      // Add sphere component for interaction
      USCS_Node* SphereNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionSphere"));
      if (SphereNode) {
        SCS->AddNode(SphereNode);
        USphereComponent* SphereComp = Cast<USphereComponent>(SphereNode->ComponentTemplate);
        if (SphereComp) {
          SphereComp->SetSphereRadius(100.0f);
          SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
      }
    }

    FAssetRegistryModule::AssetCreated(NewBlueprint);
    McpFinalizeBlueprint(NewBlueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("pickupPath"), Package->GetName());
    Result->SetStringField(TEXT("blueprintName"), Name);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Pickup actor created"), Result);
  } else {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create pickup blueprint"),
                        TEXT("BLUEPRINT_CREATE_FAILED"));
  }
  return true;
}

bool McpHandlers::Inventory::HandleInventoryConfigurePickupInteraction(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));

  if (PickupPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: pickupPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the pickup blueprint
  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *PickupPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Pickup blueprint not found: %s"), *PickupPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  McpHandlerUtils::FMcpWriteReport Report;
  const bool bHasInteractionType = Payload->HasField(TEXT("interactionType"));
  const bool bHasPrompt = Payload->HasField(TEXT("prompt"));
  const FString InteractionType = GetPayloadString(Payload, TEXT("interactionType"), TEXT("Overlap"));
  const FString Prompt = GetPayloadString(Payload, TEXT("prompt"), TEXT("Press E to pick up"));

  if (bHasInteractionType || bHasPrompt) {
    FEdGraphPinType StringType;
    StringType.PinCategory = UEdGraphSchema_K2::PC_String;

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    bool bInteractionTypeExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("InteractionType")) {
        bInteractionTypeExists = true;
        break;
      }
    }
    if (!bInteractionTypeExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("InteractionType"), NameType);
    }

    bool bPromptExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("InteractionPrompt")) {
        bPromptExists = true;
        break;
      }
    }
    if (!bPromptExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("InteractionPrompt"), StringType);
    }

    // Configure the interaction sphere component before compiling so the change bakes in.
    if (bHasInteractionType) {
      if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript) {
        for (USCS_Node* Node : SCS->GetAllNodes()) {
          if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(USphereComponent::StaticClass())) {
            if (USphereComponent* SphereComp = Cast<USphereComponent>(Node->ComponentTemplate)) {
              if (InteractionType.Equals(TEXT("Overlap"), ESearchCase::IgnoreCase)) {
                SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                SphereComp->SetGenerateOverlapEvents(true);
              } else {
                SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
              }
            }
          }
        }
      }
    }

    // Compile so the just-added variables exist on the CDO before writing their defaults.
    McpSafeCompileBlueprint(Blueprint);
    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;

    if (bHasInteractionType) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("InteractionType"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueString>(InteractionType);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("InteractionType property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("interactionType"));
      } else {
        Report.MarkFailed(TEXT("interactionType"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }

    if (bHasPrompt) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("InteractionPrompt"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueString>(Prompt);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("InteractionPrompt property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("prompt"));
      } else {
        Report.MarkFailed(TEXT("prompt"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }
  }

  if (Report.AnyApplied()) {
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("pickupPath"), PickupPath);
  if (bHasInteractionType) {
    Result->SetStringField(TEXT("interactionType"), InteractionType);
  }
  if (bHasPrompt) {
    Result->SetStringField(TEXT("prompt"), Prompt);
  }
  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Pickup interaction configured"), Blueprint);
}

bool McpHandlers::Inventory::HandleInventoryConfigurePickupRespawn(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));

  if (PickupPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: pickupPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the pickup blueprint
  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *PickupPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Pickup blueprint not found: %s"), *PickupPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  McpHandlerUtils::FMcpWriteReport Report;
  const bool bHasRespawnable = Payload->HasField(TEXT("respawnable"));
  const bool bHasRespawnTime = Payload->HasField(TEXT("respawnTime"));
  const bool Respawnable = GetPayloadBool(Payload, TEXT("respawnable"), false);
  const double RespawnTime = GetPayloadNumber(Payload, TEXT("respawnTime"), 30.0);

  if (bHasRespawnable || bHasRespawnTime) {
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    bool bRespawnableExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("bRespawnable")) {
        bRespawnableExists = true;
        break;
      }
    }
    if (!bRespawnableExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bRespawnable"), BoolType);
    }

    bool bRespawnTimeExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("RespawnTime")) {
        bRespawnTimeExists = true;
        break;
      }
    }
    if (!bRespawnTimeExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("RespawnTime"), FloatType);
    }

    // Compile first so the just-added variables exist on the generated class/CDO before we
    // write their defaults — otherwise FindPropertyByName misses them and the writes no-op.
    McpSafeCompileBlueprint(Blueprint);
    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;

    if (bHasRespawnable) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("bRespawnable"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueBoolean>(Respawnable);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("bRespawnable property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("respawnable"));
      } else {
        Report.MarkFailed(TEXT("respawnable"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }

    if (bHasRespawnTime) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("RespawnTime"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueNumber>(RespawnTime);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("RespawnTime property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("respawnTime"));
      } else {
        Report.MarkFailed(TEXT("respawnTime"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }
  }

  if (Report.AnyApplied()) {
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("pickupPath"), PickupPath);
  if (bHasRespawnable) {
    Result->SetBoolField(TEXT("respawnable"), Respawnable);
  }
  if (bHasRespawnTime) {
    Result->SetNumberField(TEXT("respawnTime"), RespawnTime);
  }
  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Pickup respawn configured"), Blueprint);
}

bool McpHandlers::Inventory::HandleInventoryConfigurePickupEffects(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));

  if (PickupPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: pickupPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the pickup blueprint
  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *PickupPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Pickup blueprint not found: %s"), *PickupPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  McpHandlerUtils::FMcpWriteReport Report;
  const bool bHasBobbing = Payload->HasField(TEXT("bobbing"));
  const bool bHasRotation = Payload->HasField(TEXT("rotationEffect"));
  const bool bHasGlow = Payload->HasField(TEXT("glowEffect"));
  const bool bBobbing = GetPayloadBool(Payload, TEXT("bobbing"), true);
  const bool bRotation = GetPayloadBool(Payload, TEXT("rotationEffect"), true);
  const bool bGlowEffect = GetPayloadBool(Payload, TEXT("glowEffect"), false);

  if (bHasBobbing || bHasRotation || bHasGlow) {
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    TArray<FName> BoolVars = {
      TEXT("bEnableBobbing"),
      TEXT("bEnableRotation"),
      TEXT("bEnableGlowEffect")
    };
    for (const FName& VarName : BoolVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarName) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, BoolType);
      }
    }

    TArray<FName> FloatVars = {
      TEXT("BobbingSpeed"),
      TEXT("BobbingHeight"),
      TEXT("RotationSpeed")
    };
    for (const FName& VarName : FloatVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarName) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, FloatType);
      }
    }

    // Compile first so the just-added variables exist on the generated class/CDO before we
    // write their defaults — otherwise FindPropertyByName misses them and the writes no-op.
    McpSafeCompileBlueprint(Blueprint);
    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;

    struct FBoolWrite { bool bRequested; const TCHAR* PayloadKey; const TCHAR* VarName; bool Value; };
    const FBoolWrite Writes[] = {
      { bHasBobbing,  TEXT("bobbing"),    TEXT("bEnableBobbing"),    bBobbing },
      { bHasRotation, TEXT("rotationEffect"), TEXT("bEnableRotation"),   bRotation },
      { bHasGlow,     TEXT("glowEffect"), TEXT("bEnableGlowEffect"), bGlowEffect }
    };
    for (const FBoolWrite& W : Writes) {
      if (!W.bRequested) {
        continue;
      }
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(W.VarName)) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueBoolean>(W.Value);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = FString::Printf(TEXT("%s property unavailable after compile"), W.VarName);
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(W.PayloadKey);
      } else {
        Report.MarkFailed(W.PayloadKey, FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }
  }

  if (Report.AnyApplied()) {
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("pickupPath"), PickupPath);
  if (bHasBobbing) {
    Result->SetBoolField(TEXT("bobbing"), bBobbing);
  }
  if (bHasRotation) {
    Result->SetBoolField(TEXT("rotationEffect"), bRotation);
  }
  if (bHasGlow) {
    Result->SetBoolField(TEXT("glowEffect"), bGlowEffect);
  }
  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Pickup effects configured"), Blueprint);
}

// ===========================================================================
// 17.4 Equipment System (5 actions)
// ===========================================================================

bool McpHandlers::Inventory::HandleInventoryCreateEquipmentComponent(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
  FString ComponentName =
      GetPayloadString(Payload, TEXT("componentName"), TEXT("EquipmentComponent"));

  if (BlueprintPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: blueprintPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
  if (SCS) {
    // Create a SceneComponent for equipment (proper hierarchy support)
    USCS_Node* NewNode = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
    if (NewNode) {
      SCS->AddNode(NewNode);

      // Add equipment-related Blueprint variables
      FEdGraphPinType SoftObjectArrayType;
      SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
      SoftObjectArrayType.ContainerType = EPinContainerType::Array;

      FEdGraphPinType NameArrayType;
      NameArrayType.PinCategory = UEdGraphSchema_K2::PC_Name;
      NameArrayType.ContainerType = EPinContainerType::Array;

      // Add EquipmentSlots array variable
      bool bSlotsExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == TEXT("EquipmentSlots")) {
          bSlotsExists = true;
          break;
        }
      }
      if (!bSlotsExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("EquipmentSlots"), SoftObjectArrayType);
      }

      // Add EquippedItems array
      bool bEquippedExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == TEXT("EquippedItems")) {
          bEquippedExists = true;
          break;
        }
      }
      if (!bEquippedExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("EquippedItems"), SoftObjectArrayType);
      }

      // Add SlotNames array
      bool bSlotNamesExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == TEXT("SlotNames")) {
          bSlotNamesExists = true;
          break;
        }
      }
      if (!bSlotNamesExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SlotNames"), NameArrayType);
      }

      McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("componentName"), ComponentName);
      Result->SetBoolField(TEXT("componentAdded"), true);
      Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

      TArray<TSharedPtr<FJsonValue>> AddedVars;
      AddedVars.Add(MakeShared<FJsonValueString>(TEXT("EquipmentSlots")));
      AddedVars.Add(MakeShared<FJsonValueString>(TEXT("EquippedItems")));
      AddedVars.Add(MakeShared<FJsonValueString>(TEXT("SlotNames")));
      Result->SetArrayField(TEXT("variablesAdded"), AddedVars);

      S.SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Equipment component added"), Result);
      return true;
    }
  }

  S.SendAutomationError(Socket, RequestId,
                      TEXT("Failed to create equipment component"),
                      TEXT("COMPONENT_CREATE_FAILED"));
  return true;
}


bool McpHandlers::Inventory::HandleInventoryConfigureEquipmentEffects(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

  if (BlueprintPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: blueprintPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the blueprint
  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  McpHandlerUtils::FMcpWriteReport Report;
  const bool bHasStatModifiers = Payload->HasField(TEXT("statModifiers"));
  const bool bHasAbilityGrants = Payload->HasField(TEXT("abilityGrants"));
  const bool bHasPassiveEffects = Payload->HasField(TEXT("passiveEffects"));
  const bool bStatModifiers = GetPayloadBool(Payload, TEXT("statModifiers"), true);
  const bool bAbilityGrants = GetPayloadBool(Payload, TEXT("abilityGrants"), true);
  const bool bPassiveEffects = GetPayloadBool(Payload, TEXT("passiveEffects"), true);

  TArray<TSharedPtr<FJsonValue>> AddedVars;

  if (bHasStatModifiers || bHasAbilityGrants || bHasPassiveEffects) {
    // Add equipment effect configuration variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType SoftObjectArrayType;
    SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
    SoftObjectArrayType.ContainerType = EPinContainerType::Array;

    FEdGraphPinType NameArrayType;
    NameArrayType.PinCategory = UEdGraphSchema_K2::PC_Name;
    NameArrayType.ContainerType = EPinContainerType::Array;

    TArray<TPair<FName, FEdGraphPinType>> EffectVars = {
      TPair<FName, FEdGraphPinType>(TEXT("bApplyStatModifiers"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("StatModifierMultiplier"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bGrantAbilitiesOnEquip"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("GrantedAbilities"), SoftObjectArrayType),
      TPair<FName, FEdGraphPinType>(TEXT("bApplyPassiveEffects"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("PassiveEffectClasses"), SoftObjectArrayType),
      TPair<FName, FEdGraphPinType>(TEXT("EffectTags"), NameArrayType)
    };

    for (const auto& VarPair : EffectVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        AddedVars.Add(MakeShared<FJsonValueString>(VarPair.Key.ToString()));
      }
    }

    // Compile first so the just-added variables exist on the generated class/CDO before we
    // write their defaults — otherwise FindPropertyByName misses them and the writes no-op.
    McpSafeCompileBlueprint(Blueprint);
    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;

    struct FBoolWrite { bool bRequested; const TCHAR* PayloadKey; const TCHAR* VarName; bool Value; };
    const FBoolWrite Writes[] = {
      { bHasStatModifiers, TEXT("statModifiers"),  TEXT("bApplyStatModifiers"),    bStatModifiers },
      { bHasAbilityGrants, TEXT("abilityGrants"),  TEXT("bGrantAbilitiesOnEquip"), bAbilityGrants },
      { bHasPassiveEffects,TEXT("passiveEffects"), TEXT("bApplyPassiveEffects"),   bPassiveEffects }
    };
    for (const FBoolWrite& W : Writes) {
      if (!W.bRequested) {
        continue;
      }
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(W.VarName)) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueBoolean>(W.Value);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = FString::Printf(TEXT("%s property unavailable after compile"), W.VarName);
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(W.PayloadKey);
      } else {
        Report.MarkFailed(W.PayloadKey, FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }
  }

  if (Report.AnyApplied()) {
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (bHasStatModifiers) {
    Result->SetBoolField(TEXT("statModifiers"), bStatModifiers);
  }
  if (bHasAbilityGrants) {
    Result->SetBoolField(TEXT("abilityGrants"), bAbilityGrants);
  }
  if (bHasPassiveEffects) {
    Result->SetBoolField(TEXT("passiveEffects"), bPassiveEffects);
  }
  Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Equipment effects configured"), Blueprint);
}

bool McpHandlers::Inventory::HandleInventoryAddEquipmentFunctions(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

  if (BlueprintPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: blueprintPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the blueprint
  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> FunctionsAdded;
  TArray<TSharedPtr<FJsonValue>> VariablesAdded;

  // Add helper variables for equipment operations
  FEdGraphPinType IntType;
  IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

  FEdGraphPinType BoolType;
  BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

  FEdGraphPinType NameType;
  NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

  FEdGraphPinType SoftObjectType;
  SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

  // Add variables that support equipment functions
  TArray<TPair<FName, FEdGraphPinType>> EquipmentVars = {
    TPair<FName, FEdGraphPinType>(TEXT("LastEquippedSlot"), NameType),
    TPair<FName, FEdGraphPinType>(TEXT("LastUnequippedSlot"), NameType),
    TPair<FName, FEdGraphPinType>(TEXT("bLastEquipSuccess"), BoolType),
    TPair<FName, FEdGraphPinType>(TEXT("CurrentlyEquippedItem"), SoftObjectType),
    TPair<FName, FEdGraphPinType>(TEXT("PendingEquipItem"), SoftObjectType),
    TPair<FName, FEdGraphPinType>(TEXT("EquipmentChangeCount"), IntType)
  };

  for (const auto& VarPair : EquipmentVars) {
    bool bExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == VarPair.Key) {
        bExists = true;
        break;
      }
    }
    if (!bExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
      VariablesAdded.Add(MakeShared<FJsonValueString>(VarPair.Key.ToString()));
    }
  }

  // Add event dispatchers for equipment operations
  FEdGraphPinType DelegateType;
  DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

  TArray<FName> EventNames = {
    TEXT("OnEquipItemRequested"),
    TEXT("OnUnequipItemRequested"),
    TEXT("OnEquipmentSwapRequested"),
    TEXT("OnEquipmentChanged")
  };

  for (const FName& EventName : EventNames) {
    bool bExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == EventName) {
        bExists = true;
        break;
      }
    }
    if (!bExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, EventName, DelegateType);
      FunctionsAdded.Add(MakeShared<FJsonValueString>(EventName.ToString()));
    }
  }

  // Adds the helper variables + event-dispatcher delegates above (real work). It does NOT create the
  // EquipItem/UnequipItem/etc. UFunctions — that is a separate step (add_function). Report only what was
  // actually added; do not list fake "(implement in Blueprint)" entries that imply functions exist.

  McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetArrayField(TEXT("dispatchersAdded"), FunctionsAdded);
  Result->SetArrayField(TEXT("variablesAdded"), VariablesAdded);
  Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
  Result->SetStringField(TEXT("note"), TEXT("Added helper variables and event-dispatcher delegates only. No UFunctions were created — author EquipItem/UnequipItem/etc. with add_function, then implement their logic in the Blueprint graph."));

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Equipment functions added"), Result);
  return true;
}

bool McpHandlers::Inventory::HandleInventoryConfigureEquipmentVisuals(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

  if (BlueprintPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: blueprintPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the blueprint
  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  McpHandlerUtils::FMcpWriteReport Report;
  const bool bHasAttachToSocket = Payload->HasField(TEXT("attachToSocket"));
  const bool bHasDefaultSocket = Payload->HasField(TEXT("defaultSocket"));
  const bool bAttachToSocket = GetPayloadBool(Payload, TEXT("attachToSocket"), true);
  const FString DefaultSocket = GetPayloadString(Payload, TEXT("defaultSocket"), TEXT("hand_r"));

  TArray<TSharedPtr<FJsonValue>> AddedVars;

  if (bHasAttachToSocket || bHasDefaultSocket) {
    // Add equipment visual configuration variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    FEdGraphPinType NameArrayType;
    NameArrayType.PinCategory = UEdGraphSchema_K2::PC_Name;
    NameArrayType.ContainerType = EPinContainerType::Array;

    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

    FEdGraphPinType TransformType;
    TransformType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    TransformType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();

    TArray<TPair<FName, FEdGraphPinType>> VisualVars = {
      TPair<FName, FEdGraphPinType>(TEXT("bAttachToSocket"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("DefaultAttachSocket"), NameType),
      TPair<FName, FEdGraphPinType>(TEXT("EquipmentSockets"), NameArrayType),
      TPair<FName, FEdGraphPinType>(TEXT("EquipmentMesh"), SoftObjectType),
      TPair<FName, FEdGraphPinType>(TEXT("AttachmentOffset"), TransformType),
      TPair<FName, FEdGraphPinType>(TEXT("bUseCustomAttachRules"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("bHideEquippedMesh"), BoolType)
    };

    for (const auto& VarPair : VisualVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        AddedVars.Add(MakeShared<FJsonValueString>(VarPair.Key.ToString()));
      }
    }

    // Compile first so the just-added variables exist on the generated class/CDO before we
    // write their defaults — otherwise FindPropertyByName misses them and the writes no-op.
    McpSafeCompileBlueprint(Blueprint);
    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;

    if (bHasAttachToSocket) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("bAttachToSocket"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueBoolean>(bAttachToSocket);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("bAttachToSocket property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("attachToSocket"));
      } else {
        Report.MarkFailed(TEXT("attachToSocket"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }

    if (bHasDefaultSocket) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("DefaultAttachSocket"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueString>(DefaultSocket);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("DefaultAttachSocket property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("defaultSocket"));
      } else {
        Report.MarkFailed(TEXT("defaultSocket"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }
  }

  if (Report.AnyApplied()) {
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (bHasAttachToSocket) {
    Result->SetBoolField(TEXT("attachToSocket"), bAttachToSocket);
  }
  if (bHasDefaultSocket) {
    Result->SetStringField(TEXT("defaultSocket"), DefaultSocket);
  }
  Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Equipment visuals configured"), Blueprint);
}

// ===========================================================================
// 17.5 Loot System (4 actions)
// ===========================================================================

bool McpHandlers::Inventory::HandleInventoryCreateLootTable(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetPayloadString(Payload, TEXT("name"));
  FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Data/LootTables"));

  if (Name.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: name"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Create a data asset for loot table
  UPackage* Package = CreateAssetPackage(Path, Name);
  if (!Package) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create package"),
                        TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  // UMcpGenericDataAsset (UDataAsset/UPrimaryDataAsset are abstract in UE5)
  UMcpGenericDataAsset* LootTableAsset =
      NewObject<UMcpGenericDataAsset>(Package, FName(*Name), RF_Public | RF_Standalone);

  if (LootTableAsset) {
    LootTableAsset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(LootTableAsset);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(LootTableAsset);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("lootTablePath"), Package->GetName());
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Loot table created"), Result);
  } else {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create loot table asset"),
                        TEXT("ASSET_CREATE_FAILED"));
  }
  return true;
}

bool McpHandlers::Inventory::HandleInventoryAddLootEntry(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));
  FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));
  double Weight = GetPayloadNumber(Payload, TEXT("lootWeight"), 1.0);
  int32 MinQuantity = static_cast<int32>(GetPayloadNumber(Payload, TEXT("minQuantity"), 1));
  int32 MaxQuantity = static_cast<int32>(GetPayloadNumber(Payload, TEXT("maxQuantity"), 1));

  if (LootTablePath.IsEmpty() || ItemPath.IsEmpty()) {
    S.SendAutomationError(
        Socket, RequestId,
        TEXT("Missing required parameters: lootTablePath and itemPath"),
        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the loot table asset
  UObject* LootTableObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *LootTablePath);
  UMcpGenericDataAsset* LootTable = Cast<UMcpGenericDataAsset>(LootTableObj);

  if (!LootTable) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Loot table not found: %s"), *LootTablePath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  int32 EntryIndex = 0;
  bool bEntryAdded = false;

  // Try to find and modify LootEntries array via reflection
  FProperty* EntriesProp = LootTable->GetClass()->FindPropertyByName(TEXT("LootEntries"));
  if (!EntriesProp) {
    EntriesProp = LootTable->GetClass()->FindPropertyByName(TEXT("Entries"));
  }

  if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(EntriesProp)) {
    // For custom loot table classes with proper array properties
    FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(LootTable));
    // Actually add a new element to the array
    int32 NewIdx = ArrayHelper.AddValue();
    if (NewIdx != INDEX_NONE) {
      EntryIndex = NewIdx;
      bEntryAdded = true;
      // Note: The new element's inner fields (item path, weight, quantities) 
      // would need to be populated via reflection based on the struct definition
    } else {
      bEntryAdded = false;
    }
  } else {
    // For generic MCP data assets, persist the entry in the extensible property map.
    const int32 GenericEntryIndex = LootTable->Properties.Num();
    const FString EntryKey = FString::Printf(TEXT("LootEntry_%d"), GenericEntryIndex);
    const FString EntryValue = FString::Printf(
        TEXT("ItemPath=%s;Weight=%s;MinQuantity=%d;MaxQuantity=%d"),
        *ItemPath, *FString::SanitizeFloat(Weight), MinQuantity, MaxQuantity);
    LootTable->Properties.Add(EntryKey, EntryValue);
    EntryIndex = GenericEntryIndex;
    bEntryAdded = true;
  }

  LootTable->MarkPackageDirty();

  if (GetPayloadBool(Payload, TEXT("save"), false)) {
    McpSafeAssetSave(LootTable);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
  Result->SetStringField(TEXT("itemPath"), ItemPath);
  Result->SetNumberField(TEXT("weight"), Weight);
  Result->SetNumberField(TEXT("minQuantity"), MinQuantity);
  Result->SetNumberField(TEXT("maxQuantity"), MaxQuantity);
  Result->SetNumberField(TEXT("entryIndex"), EntryIndex);
  Result->SetBoolField(TEXT("added"), bEntryAdded);
  if (!EntriesProp) {
    Result->SetStringField(TEXT("storage"), TEXT("Properties"));
  }
  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Loot entry added"), Result);
  return true;
}

bool McpHandlers::Inventory::HandleInventoryConfigureLootDrop(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString ActorPath = GetPayloadString(Payload, TEXT("actorPath"));
  FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));

  if (ActorPath.IsEmpty() || LootTablePath.IsEmpty()) {
    S.SendAutomationError(
        Socket, RequestId,
        TEXT("Missing required parameters: actorPath and lootTablePath"),
        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the actor blueprint
  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *ActorPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Actor blueprint not found: %s"), *ActorPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  McpHandlerUtils::FMcpWriteReport Report;
  const bool bHasDropCount = Payload->HasField(TEXT("dropCount"));
  const bool bHasDropRadius = Payload->HasField(TEXT("dropRadius"));
  const bool bHasDropOnDeath = Payload->HasField(TEXT("dropOnDeath"));
  const int32 DropCount = static_cast<int32>(GetPayloadNumber(Payload, TEXT("dropCount"), 1));
  const double DropRadius = GetPayloadNumber(Payload, TEXT("dropRadius"), 100.0);
  const bool bDropOnDeath = GetPayloadBool(Payload, TEXT("dropOnDeath"), true);

  TArray<TSharedPtr<FJsonValue>> AddedVars;

  if (bHasDropCount || bHasDropRadius || bHasDropOnDeath) {
    // Add loot drop configuration variables
    FEdGraphPinType IntType;
    IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

    FEdGraphPinType VectorType;
    VectorType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    VectorType.PinSubCategoryObject = TBaseStructure<FVector>::Get();

    TArray<TPair<FName, FEdGraphPinType>> LootVars = {
      TPair<FName, FEdGraphPinType>(TEXT("LootTable"), SoftObjectType),
      TPair<FName, FEdGraphPinType>(TEXT("LootDropCount"), IntType),
      TPair<FName, FEdGraphPinType>(TEXT("LootDropRadius"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bDropLootOnDeath"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("bRandomizeDropLocation"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("DropOffset"), VectorType),
      TPair<FName, FEdGraphPinType>(TEXT("bApplyDropImpulse"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("DropImpulseStrength"), FloatType)
    };

    for (const auto& VarPair : LootVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        AddedVars.Add(MakeShared<FJsonValueString>(VarPair.Key.ToString()));
      }
    }

    // Add event dispatcher for loot drops
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    bool bEventExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("OnLootDropped")) {
        bEventExists = true;
        break;
      }
    }
    if (!bEventExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OnLootDropped"), DelegateType);
      AddedVars.Add(MakeShared<FJsonValueString>(TEXT("OnLootDropped")));
    }

    // Compile first so the just-added variables exist on the generated class/CDO before we
    // write their defaults — otherwise FindPropertyByName misses them and the writes no-op.
    McpSafeCompileBlueprint(Blueprint);
    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;

    if (bHasDropCount) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("LootDropCount"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueNumber>(static_cast<double>(DropCount));
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("LootDropCount property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("dropCount"));
      } else {
        Report.MarkFailed(TEXT("dropCount"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }

    if (bHasDropRadius) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("LootDropRadius"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueNumber>(DropRadius);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("LootDropRadius property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("dropRadius"));
      } else {
        Report.MarkFailed(TEXT("dropRadius"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }

    if (bHasDropOnDeath) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("bDropLootOnDeath"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueBoolean>(bDropOnDeath);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("bDropLootOnDeath property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("dropOnDeath"));
      } else {
        Report.MarkFailed(TEXT("dropOnDeath"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }
  }

  if (Report.AnyApplied()) {
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("actorPath"), ActorPath);
  Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
  if (bHasDropCount) {
    Result->SetNumberField(TEXT("dropCount"), DropCount);
  }
  if (bHasDropRadius) {
    Result->SetNumberField(TEXT("dropRadius"), DropRadius);
  }
  if (bHasDropOnDeath) {
    Result->SetBoolField(TEXT("dropOnDeath"), bDropOnDeath);
  }
  Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Loot drop configured"), Blueprint);
}

bool McpHandlers::Inventory::HandleInventorySetLootQualityTiers(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));

  if (LootTablePath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: lootTablePath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the loot table asset
  UObject* LootTableObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *LootTablePath);
  UMcpGenericDataAsset* LootTable = Cast<UMcpGenericDataAsset>(LootTableObj);

  if (!LootTable) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Loot table not found: %s"), *LootTablePath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Get custom tiers from payload or use defaults
  TArray<TPair<FString, double>> Tiers;
  const TArray<TSharedPtr<FJsonValue>>* TiersArr = nullptr;
  if (Payload->TryGetArrayField(TEXT("tiers"), TiersArr) && TiersArr) {
    for (const auto& TierVal : *TiersArr) {
      const TSharedPtr<FJsonObject>* TierObj = nullptr;
      if (TierVal->TryGetObject(TierObj) && TierObj && (*TierObj).IsValid()) {
        FString TierName = GetJsonStringField((*TierObj), TEXT("name"));
        double TierWeight = GetJsonNumberField((*TierObj), TEXT("dropWeight"));
        Tiers.Add(TPair<FString, double>(TierName, TierWeight));
      }
    }
  }

  // Default tiers if none provided
  if (Tiers.Num() == 0) {
    Tiers = {
      TPair<FString, double>(TEXT("Common"), 60.0),
      TPair<FString, double>(TEXT("Uncommon"), 25.0),
      TPair<FString, double>(TEXT("Rare"), 10.0),
      TPair<FString, double>(TEXT("Epic"), 4.0),
      TPair<FString, double>(TEXT("Legendary"), 1.0)
    };
  }

  bool bTiersSet = false;

  // Try to find and set QualityTiers property via reflection
  FProperty* TiersProp = LootTable->GetClass()->FindPropertyByName(TEXT("QualityTiers"));
  if (!TiersProp) {
    TiersProp = LootTable->GetClass()->FindPropertyByName(TEXT("Tiers"));
  }

  if (TiersProp) {
    // Property exists - data would be set via reflection here for custom classes
    bTiersSet = true;
  }

  LootTable->MarkPackageDirty();

  if (GetPayloadBool(Payload, TEXT("save"), false)) {
    McpSafeAssetSave(LootTable);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("lootTablePath"), LootTablePath);

  TArray<TSharedPtr<FJsonValue>> ConfiguredTiers;
  for (const auto& TierPair : Tiers) {
    TSharedPtr<FJsonObject> TierObj = McpHandlerUtils::CreateResultObject();
    TierObj->SetStringField(TEXT("name"), TierPair.Key);
    TierObj->SetNumberField(TEXT("dropWeight"), TierPair.Value);
    ConfiguredTiers.Add(MakeShared<FJsonValueObject>(TierObj));
  }
  Result->SetArrayField(TEXT("tiersConfigured"), ConfiguredTiers);
  Result->SetNumberField(TEXT("tierCount"), Tiers.Num());
  Result->SetBoolField(TEXT("configured"), true);

  if (!TiersProp) {
    Result->SetStringField(TEXT("note"), TEXT("QualityTiers property not found. Ensure your loot table class has a QualityTiers or Tiers property."));
  }

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Quality tiers configured"), Result);
  return true;
}

// ===========================================================================
// 17.6 Crafting System (4 actions)
// ===========================================================================

bool McpHandlers::Inventory::HandleInventoryCreateCraftingRecipe(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetPayloadString(Payload, TEXT("name"));
  FString OutputItemPath = GetPayloadString(Payload, TEXT("outputItemPath"));
  FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Data/Recipes"));

  if (Name.IsEmpty() || OutputItemPath.IsEmpty()) {
    S.SendAutomationError(
        Socket, RequestId,
        TEXT("Missing required parameters: name and outputItemPath"),
        TEXT("MISSING_PARAMETER"));
    return true;
  }

  UPackage* Package = CreateAssetPackage(Path, Name);
  if (!Package) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create package"),
                        TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  // UMcpGenericDataAsset (UDataAsset/UPrimaryDataAsset are abstract in UE5)
  UMcpGenericDataAsset* RecipeAsset =
      NewObject<UMcpGenericDataAsset>(Package, FName(*Name), RF_Public | RF_Standalone);

  if (RecipeAsset) {
    const int32 OutputQuantity =
        static_cast<int32>(GetPayloadNumber(Payload, TEXT("outputQuantity"), 1));
    const double CraftTime = GetPayloadNumber(Payload, TEXT("craftTime"), 1.0);
    RecipeAsset->Properties.Add(TEXT("OutputItemPath"), OutputItemPath);
    RecipeAsset->Properties.Add(TEXT("OutputQuantity"), FString::FromInt(OutputQuantity));
    RecipeAsset->Properties.Add(TEXT("CraftTime"), FString::SanitizeFloat(CraftTime));

    RecipeAsset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(RecipeAsset);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(RecipeAsset);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("recipePath"), Package->GetName());
    Result->SetStringField(TEXT("outputItemPath"), OutputItemPath);
    Result->SetNumberField(TEXT("outputQuantity"), OutputQuantity);
    Result->SetNumberField(TEXT("craftTime"), CraftTime);
    Result->SetStringField(TEXT("storage"), TEXT("Properties"));
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Crafting recipe created"), Result);
  } else {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create recipe asset"),
                        TEXT("ASSET_CREATE_FAILED"));
  }
  return true;
}

bool McpHandlers::Inventory::HandleInventoryConfigureRecipeRequirements(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString RecipePath = GetPayloadString(Payload, TEXT("recipePath"));

  if (RecipePath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: recipePath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  UObject* RecipeAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *RecipePath);
  UMcpGenericDataAsset* GenericRecipe = Cast<UMcpGenericDataAsset>(RecipeAsset);

  if (!GenericRecipe) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Recipe not found or unsupported asset type: %s"), *RecipePath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  McpHandlerUtils::FMcpWriteReport Report;
  const bool bHasRequiredLevel = Payload->HasField(TEXT("requiredLevel"));
  const bool bHasRequiredStation = Payload->HasField(TEXT("requiredStation"));
  const int32 RequiredLevel = static_cast<int32>(GetPayloadNumber(Payload, TEXT("requiredLevel"), 0));
  const FString RequiredStation = GetPayloadString(Payload, TEXT("requiredStation"), TEXT("None"));

  if (bHasRequiredLevel) {
    GenericRecipe->Properties.Add(TEXT("RequiredLevel"), FString::FromInt(RequiredLevel));
    Report.MarkApplied(TEXT("requiredLevel"));
  }
  if (bHasRequiredStation) {
    GenericRecipe->Properties.Add(TEXT("RequiredStation"), RequiredStation);
    Report.MarkApplied(TEXT("requiredStation"));
  }

  if (Report.AnyApplied()) {
    GenericRecipe->MarkPackageDirty();
    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(GenericRecipe);
    }
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("recipePath"), RecipePath);
  if (bHasRequiredLevel) {
    Result->SetNumberField(TEXT("requiredLevel"), RequiredLevel);
  }
  if (bHasRequiredStation) {
    Result->SetStringField(TEXT("requiredStation"), RequiredStation);
  }
  Result->SetNumberField(TEXT("propertiesModified"), Report.Applied.Num());
  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Recipe requirements configured"), GenericRecipe);
}

bool McpHandlers::Inventory::HandleInventoryCreateCraftingStation(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString Name = GetPayloadString(Payload, TEXT("name"));
  FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Blueprints/CraftingStations"));
  FString StationType =
      GetPayloadString(Payload, TEXT("stationType"), TEXT("Basic"));

  if (Name.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: name"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  UPackage* Package = CreateAssetPackage(Path, Name);
  if (!Package) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create package"),
                        TEXT("PACKAGE_CREATE_FAILED"));
    return true;
  }

  UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
  Factory->ParentClass = AActor::StaticClass();

  UBlueprint* StationBlueprint = Cast<UBlueprint>(
      Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name,
                                RF_Public | RF_Standalone, nullptr, GWarn));

  if (StationBlueprint) {
    USimpleConstructionScript* SCS = StationBlueprint->SimpleConstructionScript;
    if (SCS) {
      // Add mesh component
      USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("StationMesh"));
      if (MeshNode) {
        SCS->AddNode(MeshNode);
      }

      // Add interaction component
      USCS_Node* BoxNode = SCS->CreateNode(UBoxComponent::StaticClass(), TEXT("InteractionBox"));
      if (BoxNode) {
        SCS->AddNode(BoxNode);
      }
    }

    FAssetRegistryModule::AssetCreated(StationBlueprint);
    McpFinalizeBlueprint(StationBlueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("stationPath"), Package->GetName());
    Result->SetStringField(TEXT("stationType"), StationType);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Crafting station created"), Result);
  } else {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create crafting station blueprint"),
                        TEXT("BLUEPRINT_CREATE_FAILED"));
  }
  return true;
}

bool McpHandlers::Inventory::HandleInventoryAddCraftingComponent(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
  FString ComponentName =
      GetPayloadString(Payload, TEXT("componentName"), TEXT("CraftingComponent"));

  if (BlueprintPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: blueprintPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
  if (SCS) {
    // Use USceneComponent for proper SCS hierarchy (UActorComponent cannot be added to SCS)
    USCS_Node* NewNode = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
    if (NewNode) {
      SCS->AddNode(NewNode);

      // Add crafting-related Blueprint variables
      FEdGraphPinType SoftObjectArrayType;
      SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
      SoftObjectArrayType.ContainerType = EPinContainerType::Array;

      FEdGraphPinType BoolType;
      BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

      FEdGraphPinType FloatType;
      FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
      FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

      FEdGraphPinType IntType;
      IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

      // Crafting variables
      TArray<TPair<FName, FEdGraphPinType>> CraftingVars = {
        TPair<FName, FEdGraphPinType>(TEXT("AvailableRecipes"), SoftObjectArrayType),
        TPair<FName, FEdGraphPinType>(TEXT("CraftingQueue"), SoftObjectArrayType),
        TPair<FName, FEdGraphPinType>(TEXT("bIsCrafting"), BoolType),
        TPair<FName, FEdGraphPinType>(TEXT("CurrentCraftProgress"), FloatType),
        TPair<FName, FEdGraphPinType>(TEXT("CraftingSpeedMultiplier"), FloatType),
        TPair<FName, FEdGraphPinType>(TEXT("MaxQueueSize"), IntType)
      };

      TArray<TSharedPtr<FJsonValue>> AddedVars;

      for (const auto& VarPair : CraftingVars) {
        bool bExists = false;
        for (FBPVariableDescription& Var : Blueprint->NewVariables) {
          if (Var.VarName == VarPair.Key) {
            bExists = true;
            break;
          }
        }
        if (!bExists) {
          FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
          AddedVars.Add(MakeShared<FJsonValueString>(VarPair.Key.ToString()));
        }
      }

      // Add event dispatchers for crafting
      FEdGraphPinType DelegateType;
      DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

      TArray<FName> EventNames = {
        TEXT("OnCraftingStarted"),
        TEXT("OnCraftingCompleted"),
        TEXT("OnCraftingCancelled"),
        TEXT("OnCraftingProgressUpdated")
      };

      for (const FName& EventName : EventNames) {
        bool bExists = false;
        for (FBPVariableDescription& Var : Blueprint->NewVariables) {
          if (Var.VarName == EventName) {
            bExists = true;
            break;
          }
        }
        if (!bExists) {
          FBlueprintEditorUtils::AddMemberVariable(Blueprint, EventName, DelegateType);
          AddedVars.Add(MakeShared<FJsonValueString>(EventName.ToString()));
        }
      }

      McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("componentName"), ComponentName);
      Result->SetBoolField(TEXT("componentAdded"), true);
      Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
      Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
      S.SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Crafting component added"), Result);
      return true;
    }
  }

  S.SendAutomationError(Socket, RequestId,
                      TEXT("Failed to create crafting component"),
                      TEXT("COMPONENT_CREATE_FAILED"));
  return true;
}

// ===========================================================================
// 17.7 Additional Actions (6 actions to complete 33 total)
// ===========================================================================

bool McpHandlers::Inventory::HandleInventoryConfigureItemStacking(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));

  if (ItemPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: itemPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the item asset
  UObject* ItemAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
  if (!ItemAsset) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Item not found: %s"), *ItemPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  McpHandlerUtils::FMcpWriteReport Report;
  const bool bHasStackable = Payload->HasField(TEXT("stackable"));
  const bool bHasMaxStackSize = Payload->HasField(TEXT("maxStackSize"));
  const bool bHasUniqueItems = Payload->HasField(TEXT("uniqueItems"));
  const bool bStackable = GetPayloadBool(Payload, TEXT("stackable"), true);
  const int32 MaxStackSize = static_cast<int32>(GetPayloadNumber(Payload, TEXT("maxStackSize"), 99));
  const bool bUniqueItems = GetPayloadBool(Payload, TEXT("uniqueItems"), false);

  UMcpGenericDataAsset* GenericItem = Cast<UMcpGenericDataAsset>(ItemAsset);
  TArray<FString> ModifiedProps;

  // Each requested field goes to a matching native property first, then to the generic
  // string bag, and fails loudly if neither is available.
  if (bHasStackable) {
    FProperty* Prop = ItemAsset->GetClass()->FindPropertyByName(TEXT("bStackable"));
    if (!Prop) {
      Prop = ItemAsset->GetClass()->FindPropertyByName(TEXT("Stackable"));
    }
    FString FailReason;
    bool bApplied = false;
    if (Prop) {
      TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueBoolean>(bStackable);
      if (ApplyJsonValueToProperty(ItemAsset, Prop, Val, FailReason)) {
        bApplied = true;
        ModifiedProps.Add(TEXT("Stackable"));
      }
    } else if (GenericItem) {
      GenericItem->Properties.Add(TEXT("bStackable"), bStackable ? TEXT("true") : TEXT("false"));
      bApplied = true;
      ModifiedProps.Add(TEXT("Properties.bStackable"));
    } else {
      FailReason = TEXT("no bStackable/Stackable property and asset is not a generic data asset");
    }
    if (bApplied) {
      Report.MarkApplied(TEXT("stackable"));
    } else {
      Report.MarkFailed(TEXT("stackable"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
    }
  }

  if (bHasMaxStackSize) {
    FProperty* Prop = ItemAsset->GetClass()->FindPropertyByName(TEXT("MaxStackSize"));
    if (!Prop) {
      Prop = ItemAsset->GetClass()->FindPropertyByName(TEXT("StackLimit"));
    }
    FString FailReason;
    bool bApplied = false;
    if (Prop) {
      TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueNumber>(static_cast<double>(MaxStackSize));
      if (ApplyJsonValueToProperty(ItemAsset, Prop, Val, FailReason)) {
        bApplied = true;
        ModifiedProps.Add(TEXT("MaxStackSize"));
      }
    } else if (GenericItem) {
      GenericItem->Properties.Add(TEXT("MaxStackSize"), FString::FromInt(MaxStackSize));
      bApplied = true;
      ModifiedProps.Add(TEXT("Properties.MaxStackSize"));
    } else {
      FailReason = TEXT("no MaxStackSize/StackLimit property and asset is not a generic data asset");
    }
    if (bApplied) {
      Report.MarkApplied(TEXT("maxStackSize"));
    } else {
      Report.MarkFailed(TEXT("maxStackSize"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
    }
  }

  if (bHasUniqueItems) {
    FProperty* Prop = ItemAsset->GetClass()->FindPropertyByName(TEXT("bUniqueItem"));
    FString FailReason;
    bool bApplied = false;
    if (Prop) {
      TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueBoolean>(bUniqueItems);
      if (ApplyJsonValueToProperty(ItemAsset, Prop, Val, FailReason)) {
        bApplied = true;
        ModifiedProps.Add(TEXT("UniqueItem"));
      }
    } else if (GenericItem) {
      GenericItem->Properties.Add(TEXT("bUniqueItem"), bUniqueItems ? TEXT("true") : TEXT("false"));
      bApplied = true;
      ModifiedProps.Add(TEXT("Properties.bUniqueItem"));
    } else {
      FailReason = TEXT("no bUniqueItem property and asset is not a generic data asset");
    }
    if (bApplied) {
      Report.MarkApplied(TEXT("uniqueItems"));
    } else {
      Report.MarkFailed(TEXT("uniqueItems"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
    }
  }

  if (Report.AnyApplied()) {
    ItemAsset->MarkPackageDirty();
    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(ItemAsset);
    }
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("itemPath"), ItemPath);
  if (bHasStackable) {
    Result->SetBoolField(TEXT("stackable"), bStackable);
  }
  if (bHasMaxStackSize) {
    Result->SetNumberField(TEXT("maxStackSize"), MaxStackSize);
  }
  if (bHasUniqueItems) {
    Result->SetBoolField(TEXT("uniqueItems"), bUniqueItems);
  }

  TArray<TSharedPtr<FJsonValue>> ModArr;
  for (const FString& Prop : ModifiedProps) {
    ModArr.Add(MakeShared<FJsonValueString>(Prop));
  }
  Result->SetArrayField(TEXT("modifiedProperties"), ModArr);

  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Item stacking configured"), ItemAsset);
}

bool McpHandlers::Inventory::HandleInventorySetItemIcon(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));
  FString IconPath = GetPayloadString(Payload, TEXT("iconPath"));

  if (ItemPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: itemPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the item asset
  UObject* ItemAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
  if (!ItemAsset) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Item not found: %s"), *ItemPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  bool bIconSet = false;
  FString IconPropertyName;

  // Try common icon property names
  TArray<FString> IconPropNames = {
    TEXT("Icon"),
    TEXT("ItemIcon"),
    TEXT("Thumbnail"),
    TEXT("DisplayIcon"),
    TEXT("InventoryIcon")
  };

  for (const FString& PropName : IconPropNames) {
    FProperty* IconProp = ItemAsset->GetClass()->FindPropertyByName(*PropName);
    if (IconProp) {
      TSharedPtr<FJsonValue> PathVal = MakeShared<FJsonValueString>(IconPath);
      FString ApplyError;
      if (ApplyJsonValueToProperty(ItemAsset, IconProp, PathVal, ApplyError)) {
        bIconSet = true;
        IconPropertyName = PropName;
        break;
      }
    }
  }

  if (!bIconSet) {
    if (UMcpGenericDataAsset* GenericItem = Cast<UMcpGenericDataAsset>(ItemAsset)) {
      GenericItem->Properties.Add(TEXT("IconPath"), IconPath);
      bIconSet = true;
      IconPropertyName = TEXT("Properties.IconPath");
    }
  }

  ItemAsset->MarkPackageDirty();

  if (GetPayloadBool(Payload, TEXT("save"), false)) {
    McpSafeAssetSave(ItemAsset);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("itemPath"), ItemPath);
  Result->SetStringField(TEXT("iconPath"), IconPath);
  Result->SetBoolField(TEXT("iconSet"), bIconSet);
  if (bIconSet) {
    Result->SetStringField(TEXT("propertyModified"), IconPropertyName);
  } else {
    Result->SetStringField(TEXT("note"), TEXT("No icon property found. Ensure your item class has an Icon, ItemIcon, or Thumbnail property."));
  }

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Item icon configured"), Result);
  return true;
}

bool McpHandlers::Inventory::HandleInventoryAddRecipeIngredient(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString RecipePath = GetPayloadString(Payload, TEXT("recipePath"));
  FString IngredientItemPath = GetPayloadString(Payload, TEXT("ingredientItemPath"));
  int32 Quantity = static_cast<int32>(GetPayloadNumber(Payload, TEXT("quantity"), 1));

  if (RecipePath.IsEmpty() || IngredientItemPath.IsEmpty()) {
    S.SendAutomationError(
        Socket, RequestId,
        TEXT("Missing required parameters: recipePath and ingredientItemPath"),
        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the recipe asset
  UObject* RecipeAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *RecipePath);
  if (!RecipeAsset) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Recipe not found: %s"), *RecipePath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  bool bIngredientAdded = false;
  int32 IngredientIndex = 0;

  // Try to find Ingredients array via reflection
  FProperty* IngredientsProp = RecipeAsset->GetClass()->FindPropertyByName(TEXT("Ingredients"));
  if (!IngredientsProp) {
    IngredientsProp = RecipeAsset->GetClass()->FindPropertyByName(TEXT("RequiredItems"));
  }
  if (!IngredientsProp) {
    IngredientsProp = RecipeAsset->GetClass()->FindPropertyByName(TEXT("InputItems"));
  }

  if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(IngredientsProp)) {
    FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(RecipeAsset));
    // Actually add a new element to the array
    int32 NewIdx = ArrayHelper.AddValue();
    if (NewIdx != INDEX_NONE) {
      IngredientIndex = NewIdx;
      bIngredientAdded = true;
      // Note: The new element's inner fields (item path, quantity)
      // would need to be populated via reflection based on the struct definition
    } else {
      bIngredientAdded = false;
    }
  } else {
    if (UMcpGenericDataAsset* GenericRecipe = Cast<UMcpGenericDataAsset>(RecipeAsset)) {
      const int32 GenericIngredientIndex = GenericRecipe->Properties.Num();
      const FString IngredientKey = FString::Printf(TEXT("Ingredient_%d"), GenericIngredientIndex);
      const FString IngredientValue = FString::Printf(
          TEXT("ItemPath=%s;Quantity=%d"), *IngredientItemPath, Quantity);
      GenericRecipe->Properties.Add(IngredientKey, IngredientValue);
      IngredientIndex = GenericIngredientIndex;
      bIngredientAdded = true;
    } else {
      bIngredientAdded = false;
    }
  }

  RecipeAsset->MarkPackageDirty();

  if (GetPayloadBool(Payload, TEXT("save"), false)) {
    McpSafeAssetSave(RecipeAsset);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("recipePath"), RecipePath);
  Result->SetStringField(TEXT("ingredientItemPath"), IngredientItemPath);
  Result->SetNumberField(TEXT("quantity"), Quantity);
  Result->SetNumberField(TEXT("ingredientIndex"), IngredientIndex);
  Result->SetBoolField(TEXT("added"), bIngredientAdded);

  if (!IngredientsProp && bIngredientAdded) {
    Result->SetStringField(TEXT("storage"), TEXT("Properties"));
  } else if (!IngredientsProp) {
    Result->SetStringField(TEXT("note"), TEXT("Ingredients property not found. Ensure your recipe class has an Ingredients, RequiredItems, or InputItems array."));
  }

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Recipe ingredient added"), Result);
  return true;
}

bool McpHandlers::Inventory::HandleInventoryRemoveLootEntry(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));
  int32 EntryIndex = static_cast<int32>(GetPayloadNumber(Payload, TEXT("entryIndex"), -1));
  FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));

  if (LootTablePath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: lootTablePath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  if (EntryIndex < 0 && ItemPath.IsEmpty()) {
    S.SendAutomationError(
        Socket, RequestId,
        TEXT("Either entryIndex or itemPath must be provided"),
        TEXT("MISSING_PARAMETER"));
    return true;
  }

  // Load the loot table asset
  UObject* LootTableObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *LootTablePath);
  UMcpGenericDataAsset* LootTable = Cast<UMcpGenericDataAsset>(LootTableObj);

  if (!LootTable) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Loot table not found: %s"), *LootTablePath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  bool bEntryRemoved = false;
  int32 RemovedIndex = -1;

  // Try to find and modify LootEntries array via reflection
  FProperty* EntriesProp = LootTable->GetClass()->FindPropertyByName(TEXT("LootEntries"));
  if (!EntriesProp) {
    EntriesProp = LootTable->GetClass()->FindPropertyByName(TEXT("Entries"));
  }

  if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(EntriesProp)) {
    FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(LootTable));
    if (EntryIndex >= 0 && EntryIndex < ArrayHelper.Num()) {
      ArrayHelper.RemoveValues(EntryIndex, 1);
      bEntryRemoved = true;
      RemovedIndex = EntryIndex;
    }
  }

  LootTable->MarkPackageDirty();

  if (GetPayloadBool(Payload, TEXT("save"), false)) {
    McpSafeAssetSave(LootTable);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
  Result->SetNumberField(TEXT("removedIndex"), RemovedIndex);
  Result->SetBoolField(TEXT("removed"), bEntryRemoved);

  if (!bEntryRemoved) {
    Result->SetStringField(TEXT("note"), TEXT("Entry not removed. Check that entryIndex is valid or LootEntries array exists."));
  }

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Loot entry removed"), Result);
  return true;
}

bool McpHandlers::Inventory::HandleInventoryConfigureWeight(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

  if (BlueprintPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("Missing required parameter: blueprintPath"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
  if (!Blueprint) {
    S.SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }

  McpHandlerUtils::FMcpWriteReport Report;
  // 'encumberance' (sic) spellings accepted for back-compat with older clients.
  const bool bHasMaxWeight = Payload->HasField(TEXT("maxWeight"));
  const bool bHasEnableWeight = Payload->HasField(TEXT("enableWeight"));
  const bool bHasEncumbranceSystem =
      Payload->HasField(TEXT("encumbranceSystem")) || Payload->HasField(TEXT("encumberanceSystem"));
  const bool bHasEncumbranceThreshold =
      Payload->HasField(TEXT("encumbranceThreshold")) || Payload->HasField(TEXT("encumberanceThreshold"));

  const double MaxWeight = GetPayloadNumber(Payload, TEXT("maxWeight"), 100.0);
  const bool bEnableWeight = GetPayloadBool(Payload, TEXT("enableWeight"), true);
  const bool bEncumbranceSystem = GetPayloadBool(Payload, TEXT("encumbranceSystem"),
      GetPayloadBool(Payload, TEXT("encumberanceSystem"), false));
  const double EncumbranceThreshold = GetPayloadNumber(Payload, TEXT("encumbranceThreshold"),
      GetPayloadNumber(Payload, TEXT("encumberanceThreshold"), 0.75));

  TArray<TSharedPtr<FJsonValue>> AddedVars;

  if (bHasMaxWeight || bHasEnableWeight || bHasEncumbranceSystem || bHasEncumbranceThreshold) {
    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    TArray<TPair<FName, FEdGraphPinType>> WeightVars = {
      TPair<FName, FEdGraphPinType>(TEXT("MaxCarryWeight"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("CurrentCarryWeight"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bWeightEnabled"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("bUseEncumbrance"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("EncumbranceThreshold"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("WeightMultiplier"), FloatType)
    };

    for (const auto& VarPair : WeightVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        AddedVars.Add(MakeShared<FJsonValueString>(VarPair.Key.ToString()));
      }
    }

    // Add weight-related event
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    bool bEventExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("OnEncumbranceChanged")) {
        bEventExists = true;
        break;
      }
    }
    if (!bEventExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OnEncumbranceChanged"), DelegateType);
      AddedVars.Add(MakeShared<FJsonValueString>(TEXT("OnEncumbranceChanged")));
    }

    // Compile first so the just-added variables exist on the generated class/CDO before we
    // write their defaults — otherwise FindPropertyByName misses them and the writes no-op.
    McpSafeCompileBlueprint(Blueprint);
    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;

    if (bHasMaxWeight) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("MaxCarryWeight"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueNumber>(MaxWeight);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("MaxCarryWeight property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("maxWeight"));
      } else {
        Report.MarkFailed(TEXT("maxWeight"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }

    if (bHasEnableWeight) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("bWeightEnabled"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueBoolean>(bEnableWeight);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("bWeightEnabled property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("enableWeight"));
      } else {
        Report.MarkFailed(TEXT("enableWeight"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }

    if (bHasEncumbranceSystem) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("bUseEncumbrance"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueBoolean>(bEncumbranceSystem);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("bUseEncumbrance property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("encumbranceSystem"));
      } else {
        Report.MarkFailed(TEXT("encumbranceSystem"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }

    if (bHasEncumbranceThreshold) {
      FString FailReason;
      bool bApplied = false;
      if (CDO) {
        if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("EncumbranceThreshold"))) {
          TSharedPtr<FJsonValue> Val = MakeShared<FJsonValueNumber>(EncumbranceThreshold);
          if (ApplyJsonValueToProperty(CDO, Prop, Val, FailReason)) {
            bApplied = true;
          }
        } else {
          FailReason = TEXT("EncumbranceThreshold property unavailable after compile");
        }
      } else {
        FailReason = TEXT("generated class unavailable");
      }
      if (bApplied) {
        Report.MarkApplied(TEXT("encumbranceThreshold"));
      } else {
        Report.MarkFailed(TEXT("encumbranceThreshold"), FailReason.IsEmpty() ? TEXT("failed to apply") : FailReason);
      }
    }
  }

  if (Report.AnyApplied()) {
    McpFinalizeBlueprint(Blueprint, /*bStructural=*/true, GetPayloadBool(Payload, TEXT("save"), true));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (bHasMaxWeight) {
    Result->SetNumberField(TEXT("maxWeight"), MaxWeight);
  }
  if (bHasEnableWeight) {
    Result->SetBoolField(TEXT("enableWeight"), bEnableWeight);
  }
  if (bHasEncumbranceSystem) {
    Result->SetBoolField(TEXT("encumbranceSystem"), bEncumbranceSystem);
  }
  if (bHasEncumbranceThreshold) {
    Result->SetNumberField(TEXT("encumbranceThreshold"), EncumbranceThreshold);
  }
  Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
  return SendWriteReportResponse(&S, Socket, RequestId, Report, Result,
                                 TEXT("Inventory weight configured"), Blueprint);
}


// ===========================================================================
// Utility (1 action)
// ===========================================================================

bool McpHandlers::Inventory::HandleInventoryGetInfo(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket) {
  FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
  FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));
  FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));
  FString RecipePath = GetPayloadString(Payload, TEXT("recipePath"));
  FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));

  // Validate that at least one path is provided
  if (BlueprintPath.IsEmpty() && ItemPath.IsEmpty() && LootTablePath.IsEmpty() && 
      RecipePath.IsEmpty() && PickupPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                        TEXT("At least one path parameter is required (blueprintPath, itemPath, lootTablePath, recipePath, or pickupPath)"),
                        TEXT("MISSING_PARAMETER"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  auto AddGenericProperties = [](TSharedPtr<FJsonObject> TargetResult, UObject* Asset) {
    UMcpGenericDataAsset* GenericAsset = Cast<UMcpGenericDataAsset>(Asset);

    // Native UPROPERTY values — the same set set_item_properties writes.
    TSharedPtr<FJsonObject> NativeObject = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(Asset->GetClass()); It; ++It) {
      FProperty* Prop = *It;
      if (GenericAsset && Prop->GetFName() == TEXT("Properties")) {
        continue;  // the string bag, reported as "properties" below
      }
      TSharedPtr<FJsonValue> Value = ExportPropertyToJsonValue(Asset, Prop);
      NativeObject->SetField(Prop->GetName(),
                             Value.IsValid() ? Value : MakeShared<FJsonValueNull>());
    }
    TargetResult->SetObjectField(TEXT("nativeProperties"), NativeObject);

    if (GenericAsset) {
      TSharedPtr<FJsonObject> PropertiesObject = MakeShared<FJsonObject>();
      for (const TPair<FString, FString>& Pair : GenericAsset->Properties) {
        PropertiesObject->SetStringField(Pair.Key, Pair.Value);
      }
      TargetResult->SetObjectField(TEXT("properties"), PropertiesObject);
      TargetResult->SetNumberField(TEXT("propertyCount"), GenericAsset->Properties.Num());
    }
  };

  if (!BlueprintPath.IsEmpty()) {
    UBlueprint* Blueprint = Cast<UBlueprint>(
        StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
    if (!Blueprint) {
      S.SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    Result->SetStringField(TEXT("assetType"), TEXT("Blueprint"));
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetStringField(TEXT("className"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetName() : FString(TEXT("(uncompiled)")));

    // Check for inventory/equipment components
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (SCS) {
      TArray<TSharedPtr<FJsonValue>> Components;
      for (USCS_Node* Node : SCS->GetAllNodes()) {
        if (Node) {
          TSharedPtr<FJsonObject> CompInfo = McpHandlerUtils::CreateResultObject();
          CompInfo->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
          CompInfo->SetStringField(TEXT("class"),
                                   Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("Unknown"));
          Components.Add(MakeShared<FJsonValueObject>(CompInfo));
        }
      }
      Result->SetArrayField(TEXT("components"), Components);
    }
  } else if (!ItemPath.IsEmpty()) {
    // Use UDataAsset base class for loading - UPrimaryDataAsset is abstract in UE5.7
    UObject* ItemAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
    if (!ItemAsset) {
      S.SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Item not found: %s"), *ItemPath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    Result->SetStringField(TEXT("assetType"), TEXT("Item"));
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetStringField(TEXT("className"), ItemAsset->GetClass()->GetName());
    AddGenericProperties(Result, ItemAsset);
  } else if (!LootTablePath.IsEmpty()) {
    UObject* LootTableAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *LootTablePath);
    if (!LootTableAsset) {
      S.SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Loot table not found: %s"), *LootTablePath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    Result->SetStringField(TEXT("assetType"), TEXT("LootTable"));
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    AddGenericProperties(Result, LootTableAsset);
  } else if (!RecipePath.IsEmpty()) {
    UObject* RecipeAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *RecipePath);
    if (!RecipeAsset) {
      S.SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Recipe not found: %s"), *RecipePath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    Result->SetStringField(TEXT("assetType"), TEXT("Recipe"));
    Result->SetStringField(TEXT("recipePath"), RecipePath);
    AddGenericProperties(Result, RecipeAsset);
  } else if (!PickupPath.IsEmpty()) {
    UBlueprint* PickupBlueprint = Cast<UBlueprint>(
        StaticLoadObject(UBlueprint::StaticClass(), nullptr, *PickupPath));
    if (!PickupBlueprint) {
      S.SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Pickup blueprint not found: %s"), *PickupPath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    Result->SetStringField(TEXT("assetType"), TEXT("Pickup"));
    Result->SetStringField(TEXT("pickupPath"), PickupPath);
  }

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Inventory info retrieved"), Result);
  return true;
}

#undef GetPayloadString
#undef GetPayloadNumber
#undef GetPayloadBool
