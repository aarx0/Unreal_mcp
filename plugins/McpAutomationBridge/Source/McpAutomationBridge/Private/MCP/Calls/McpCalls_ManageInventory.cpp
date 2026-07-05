// LINT-TOOL: manage_inventory
// manage_inventory as FMcpCall classes — eleventh classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers (HandleInventory*, InventoryHandlers.cpp) until the
// module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageInventory
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations
// (McpDecl_ManageInventory.h) and re-verified against the extracted
// HandleInventory* bodies — every declared param is read by its member.
// The retired dispatcher's only shared prologue read was subAction (no
// common name/path/blueprintPath block), so the rows port unchanged.
// get_inventory_info's five resolver spellings are all optional with the
// at-least-one requirement handler-enforced; configure_inventory_weight
// declares both encumbrance spellings because the body reads the
// historical 'encumberance' (sic) fallbacks; remove_loot_entry's
// entryIndex-or-itemPath one-of is likewise handler-enforced.

inline const FMcpParamDecl P_CreateItemDataAsset[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetItemProperties[] = { { TEXT("itemPath"), EMcpParamKind::String, true }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateItemCategory[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AssignItemCategory[] = { { TEXT("itemPath"), EMcpParamKind::String, true }, { TEXT("categoryPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateInventoryComponent[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("slotCount"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureInventorySlots[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("slotCount"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddInventoryFunctions[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureInventoryEvents[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetInventoryReplication[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("replicated"), EMcpParamKind::Bool, false }, { TEXT("replicationCondition"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreatePickupActor[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigurePickupInteraction[] = { { TEXT("pickupPath"), EMcpParamKind::String, true }, { TEXT("interactionType"), EMcpParamKind::String, false }, { TEXT("prompt"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigurePickupRespawn[] = { { TEXT("pickupPath"), EMcpParamKind::String, true }, { TEXT("respawnable"), EMcpParamKind::Bool, false }, { TEXT("respawnTime"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigurePickupEffects[] = { { TEXT("pickupPath"), EMcpParamKind::String, true }, { TEXT("bobbing"), EMcpParamKind::Bool, false }, { TEXT("rotation"), EMcpParamKind::Bool, false }, { TEXT("glowEffect"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateEquipmentComponent[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_DefineEquipmentSlots[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("slots"), EMcpParamKind::Array, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureEquipmentEffects[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("statModifiers"), EMcpParamKind::Bool, false }, { TEXT("abilityGrants"), EMcpParamKind::Bool, false }, { TEXT("passiveEffects"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddEquipmentFunctions[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureEquipmentVisuals[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("attachToSocket"), EMcpParamKind::Bool, false }, { TEXT("defaultSocket"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateLootTable[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddLootEntry[] = { { TEXT("lootTablePath"), EMcpParamKind::String, true }, { TEXT("itemPath"), EMcpParamKind::String, true }, { TEXT("lootWeight"), EMcpParamKind::Number, false }, { TEXT("minQuantity"), EMcpParamKind::Number, false }, { TEXT("maxQuantity"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureLootDrop[] = { { TEXT("actorPath"), EMcpParamKind::String, true }, { TEXT("lootTablePath"), EMcpParamKind::String, true }, { TEXT("dropCount"), EMcpParamKind::Number, false }, { TEXT("dropRadius"), EMcpParamKind::Number, false }, { TEXT("dropOnDeath"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetLootQualityTiers[] = { { TEXT("lootTablePath"), EMcpParamKind::String, true }, { TEXT("tiers"), EMcpParamKind::Array, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateCraftingRecipe[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("outputItemPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("outputQuantity"), EMcpParamKind::Number, false }, { TEXT("craftTime"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureRecipeRequirements[] = { { TEXT("recipePath"), EMcpParamKind::String, true }, { TEXT("requiredLevel"), EMcpParamKind::Number, false }, { TEXT("requiredStation"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateCraftingStation[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("stationType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddCraftingComponent[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureItemStacking[] = { { TEXT("itemPath"), EMcpParamKind::String, true }, { TEXT("stackable"), EMcpParamKind::Bool, false }, { TEXT("maxStackSize"), EMcpParamKind::Number, false }, { TEXT("uniqueItems"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetItemIcon[] = { { TEXT("itemPath"), EMcpParamKind::String, true }, { TEXT("iconPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddRecipeIngredient[] = { { TEXT("recipePath"), EMcpParamKind::String, true }, { TEXT("ingredientItemPath"), EMcpParamKind::String, true }, { TEXT("quantity"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_RemoveLootEntry[] = { { TEXT("lootTablePath"), EMcpParamKind::String, true }, { TEXT("entryIndex"), EMcpParamKind::Number, false }, { TEXT("itemPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureInventoryWeight[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("maxWeight"), EMcpParamKind::Number, false }, { TEXT("enableWeight"), EMcpParamKind::Bool, false }, { TEXT("encumbranceSystem"), EMcpParamKind::Bool, false }, { TEXT("encumberanceSystem"), EMcpParamKind::Bool, false }, { TEXT("encumbranceThreshold"), EMcpParamKind::Number, false }, { TEXT("encumberanceThreshold"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureStationRecipes[] = { { TEXT("stationPath"), EMcpParamKind::String, true }, { TEXT("recipePaths"), EMcpParamKind::Array, false }, { TEXT("stationType"), EMcpParamKind::String, false }, { TEXT("craftingSpeedMultiplier"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetInventoryInfo[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("itemPath"), EMcpParamKind::String, false }, { TEXT("lootTablePath"), EMcpParamKind::String, false }, { TEXT("recipePath"), EMcpParamKind::String, false }, { TEXT("pickupPath"), EMcpParamKind::String, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is deliberately NOT baked: the implementation TU
// (InventoryHandlers.cpp) carries no editor gate — no #if WITH_EDITOR and no
// GEditor read (the module itself is editor-only, so WITH_EDITOR always
// holds) — and flagging would newly reject GEditor-less commandlet runs the
// shim family served. Mutating on all writers; the only reader is
// get_inventory_info.

#define MCP_IV_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)        \
class FMcpCall_ManageInventory_##ClassSuffix final : public FMcpCall                       \
{                                                                                          \
	const FMcpCallDecl& GetDecl() const override                                           \
	{                                                                                      \
		static const FMcpCallDecl Decl{ TEXT("manage_inventory"), TEXT(ActionLiteral),     \
			ParamsArray, (ExtraFlags) };                                                   \
		return Decl;                                                                       \
	}                                                                                      \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                   \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override   \
	{                                                                                      \
		return S.HandlerFn(RequestId, Payload, Socket);                                    \
	}                                                                                      \
};

// Data assets (17.1)
MCP_IV_CALL(CreateItemDataAsset, "create_item_data_asset", P_CreateItemDataAsset, HandleInventoryCreateItemDataAsset, EMcpCallFlags::Mutating)
MCP_IV_CALL(SetItemProperties, "set_item_properties", P_SetItemProperties, HandleInventorySetItemProperties, EMcpCallFlags::Mutating)
MCP_IV_CALL(CreateItemCategory, "create_item_category", P_CreateItemCategory, HandleInventoryCreateItemCategory, EMcpCallFlags::Mutating)
MCP_IV_CALL(AssignItemCategory, "assign_item_category", P_AssignItemCategory, HandleInventoryAssignItemCategory, EMcpCallFlags::Mutating)

// Inventory component (17.2)
MCP_IV_CALL(CreateComponent, "create_inventory_component", P_CreateInventoryComponent, HandleInventoryCreateComponent, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureSlots, "configure_inventory_slots", P_ConfigureInventorySlots, HandleInventoryConfigureSlots, EMcpCallFlags::Mutating)
MCP_IV_CALL(AddFunctions, "add_inventory_functions", P_AddInventoryFunctions, HandleInventoryAddFunctions, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureEvents, "configure_inventory_events", P_ConfigureInventoryEvents, HandleInventoryConfigureEvents, EMcpCallFlags::Mutating)
MCP_IV_CALL(SetReplication, "set_inventory_replication", P_SetInventoryReplication, HandleInventorySetReplication, EMcpCallFlags::Mutating)

// Pickups (17.3)
MCP_IV_CALL(CreatePickupActor, "create_pickup_actor", P_CreatePickupActor, HandleInventoryCreatePickupActor, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigurePickupInteraction, "configure_pickup_interaction", P_ConfigurePickupInteraction, HandleInventoryConfigurePickupInteraction, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigurePickupRespawn, "configure_pickup_respawn", P_ConfigurePickupRespawn, HandleInventoryConfigurePickupRespawn, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigurePickupEffects, "configure_pickup_effects", P_ConfigurePickupEffects, HandleInventoryConfigurePickupEffects, EMcpCallFlags::Mutating)

// Equipment system (17.4)
MCP_IV_CALL(CreateEquipmentComponent, "create_equipment_component", P_CreateEquipmentComponent, HandleInventoryCreateEquipmentComponent, EMcpCallFlags::Mutating)
MCP_IV_CALL(DefineEquipmentSlots, "define_equipment_slots", P_DefineEquipmentSlots, HandleInventoryDefineEquipmentSlots, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureEquipmentEffects, "configure_equipment_effects", P_ConfigureEquipmentEffects, HandleInventoryConfigureEquipmentEffects, EMcpCallFlags::Mutating)
MCP_IV_CALL(AddEquipmentFunctions, "add_equipment_functions", P_AddEquipmentFunctions, HandleInventoryAddEquipmentFunctions, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureEquipmentVisuals, "configure_equipment_visuals", P_ConfigureEquipmentVisuals, HandleInventoryConfigureEquipmentVisuals, EMcpCallFlags::Mutating)

// Loot system (17.5)
MCP_IV_CALL(CreateLootTable, "create_loot_table", P_CreateLootTable, HandleInventoryCreateLootTable, EMcpCallFlags::Mutating)
MCP_IV_CALL(AddLootEntry, "add_loot_entry", P_AddLootEntry, HandleInventoryAddLootEntry, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureLootDrop, "configure_loot_drop", P_ConfigureLootDrop, HandleInventoryConfigureLootDrop, EMcpCallFlags::Mutating)
MCP_IV_CALL(SetLootQualityTiers, "set_loot_quality_tiers", P_SetLootQualityTiers, HandleInventorySetLootQualityTiers, EMcpCallFlags::Mutating)

// Crafting system (17.6)
MCP_IV_CALL(CreateCraftingRecipe, "create_crafting_recipe", P_CreateCraftingRecipe, HandleInventoryCreateCraftingRecipe, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureRecipeRequirements, "configure_recipe_requirements", P_ConfigureRecipeRequirements, HandleInventoryConfigureRecipeRequirements, EMcpCallFlags::Mutating)
MCP_IV_CALL(CreateCraftingStation, "create_crafting_station", P_CreateCraftingStation, HandleInventoryCreateCraftingStation, EMcpCallFlags::Mutating)
MCP_IV_CALL(AddCraftingComponent, "add_crafting_component", P_AddCraftingComponent, HandleInventoryAddCraftingComponent, EMcpCallFlags::Mutating)

// Additional actions (17.7)
MCP_IV_CALL(ConfigureItemStacking, "configure_item_stacking", P_ConfigureItemStacking, HandleInventoryConfigureItemStacking, EMcpCallFlags::Mutating)
MCP_IV_CALL(SetItemIcon, "set_item_icon", P_SetItemIcon, HandleInventorySetItemIcon, EMcpCallFlags::Mutating)
MCP_IV_CALL(AddRecipeIngredient, "add_recipe_ingredient", P_AddRecipeIngredient, HandleInventoryAddRecipeIngredient, EMcpCallFlags::Mutating)
MCP_IV_CALL(RemoveLootEntry, "remove_loot_entry", P_RemoveLootEntry, HandleInventoryRemoveLootEntry, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureWeight, "configure_inventory_weight", P_ConfigureInventoryWeight, HandleInventoryConfigureWeight, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureStationRecipes, "configure_station_recipes", P_ConfigureStationRecipes, HandleInventoryConfigureStationRecipes, EMcpCallFlags::Mutating)

// Utility
MCP_IV_CALL(GetInfo, "get_inventory_info", P_GetInventoryInfo, HandleInventoryGetInfo, EMcpCallFlags::None)

#undef MCP_IV_CALL

} // namespace McpCalls::ManageInventory

void McpRegisterManageInventoryCalls()
{
	using namespace McpCalls::ManageInventory;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_CreateItemDataAsset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_SetItemProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_CreateItemCategory>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_AssignItemCategory>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_CreateComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigureSlots>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_AddFunctions>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigureEvents>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_SetReplication>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_CreatePickupActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigurePickupInteraction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigurePickupRespawn>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigurePickupEffects>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_CreateEquipmentComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_DefineEquipmentSlots>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigureEquipmentEffects>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_AddEquipmentFunctions>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigureEquipmentVisuals>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_CreateLootTable>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_AddLootEntry>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigureLootDrop>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_SetLootQualityTiers>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_CreateCraftingRecipe>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigureRecipeRequirements>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_CreateCraftingStation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_AddCraftingComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigureItemStacking>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_SetItemIcon>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_AddRecipeIngredient>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_RemoveLootEntry>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigureWeight>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigureStationRecipes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_GetInfo>());
}
