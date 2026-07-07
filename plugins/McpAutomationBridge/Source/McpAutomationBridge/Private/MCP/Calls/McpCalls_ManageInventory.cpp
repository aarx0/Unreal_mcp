// LINT-TOOL: manage_inventory
// LINT-SCHEMA-DERIVED
// manage_inventory as FMcpCall classes — eleventh classed family
// (docs/action-declarations.md). Adopts schema-from-decls: each class AUTHORS
// its schema fragment in a S_<Suffix>() function; the published facade schema
// folds those fragments and GetDecl() derives the validation decl from the same
// fragment via McpDeriveDecl(), so schema and decl are one source and cannot
// drift. Run() delegates to the subsystem member handlers (HandleInventory*,
// InventoryHandlers.cpp) until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageInventory
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. get_info's five resolver
// spellings are all optional with the at-least-one requirement handler-enforced;
// remove_loot_entry's entryIndex-or-itemPath one-of is likewise handler-enforced.
// configure_inventory_weight authors both encumbrance spellings because the body
// reads the historical 'encumberance' (sic) fallbacks (InventoryHandlers.cpp).

static void S_CreateItemDataAsset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .FreeformObject(TEXT("properties"), TEXT("Properties to apply to an item data asset "
		"(create_item_data_asset, set_item_properties). Keys matching native properties are "
		"written via reflection; other string/number/bool values are stored in the asset's "
		"string property bag (reported as storedInPropertyBag)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_SetItemProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("itemPath"), TEXT("Path to item data asset."))
	 .FreeformObject(TEXT("properties"), TEXT("Properties to apply to an item data asset "
		"(create_item_data_asset, set_item_properties). Keys matching native properties are "
		"written via reflection; other string/number/bool values are stored in the asset's "
		"string property bag (reported as storedInPropertyBag)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("itemPath")});
}

static void S_CreateItemCategory(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_AssignItemCategory(FMcpSchemaBuilder& B)
{
	B.String(TEXT("itemPath"), TEXT("Path to item data asset."))
	 .String(TEXT("categoryPath"), TEXT("Path to item category asset."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("itemPath"), TEXT("categoryPath")});
}

static void S_CreateComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Number(TEXT("slotCount"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureSlots(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("slotCount"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("blueprintPath")});
}

static void S_AddFunctions(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("blueprintPath")});
}


static void S_SetReplication(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("replicated"), TEXT("Whether to replicate."))
	 .StringEnum(TEXT("replicationCondition"), {
		TEXT("None"),
		TEXT("OwnerOnly"),
		TEXT("SkipOwner"),
		TEXT("SimulatedOnly"),
		TEXT("AutonomousOnly"),
		TEXT("Custom")
	 }, TEXT("Replication condition for inventory."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("blueprintPath")});
}

static void S_CreatePickupActor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_ConfigurePickupInteraction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("pickupPath"), TEXT("Path to pickup actor Blueprint."))
	 .StringEnum(TEXT("interactionType"), {
		TEXT("Overlap"),
		TEXT("Interact"),
		TEXT("Key"),
		TEXT("Hold")
	 }, TEXT("How player picks up item."))
	 .String(TEXT("prompt"), TEXT("Prompt text."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("pickupPath")});
}

static void S_ConfigurePickupRespawn(FMcpSchemaBuilder& B)
{
	B.String(TEXT("pickupPath"), TEXT("Path to pickup actor Blueprint."))
	 .Bool(TEXT("respawnable"), TEXT(""))
	 .Number(TEXT("respawnTime"), TEXT("Respawn time in seconds."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("pickupPath")});
}

static void S_ConfigurePickupEffects(FMcpSchemaBuilder& B)
{
	B.String(TEXT("pickupPath"), TEXT("Path to pickup actor Blueprint."))
	 .Bool(TEXT("bobbing"), TEXT("Enable bobbing animation."))
	 .Bool(TEXT("rotation"), TEXT("Enable rotation animation."))
	 .Bool(TEXT("glowEffect"), TEXT("Enable glow effect."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("pickupPath")});
}

static void S_CreateEquipmentComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("blueprintPath")});
}


static void S_ConfigureEquipmentEffects(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("statModifiers"), TEXT("Enable stat modifier support when equipped."))
	 .Bool(TEXT("abilityGrants"), TEXT("Enable ability grant support when equipped."))
	 .Bool(TEXT("passiveEffects"), TEXT("Enable passive effect support when equipped."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("blueprintPath")});
}

static void S_AddEquipmentFunctions(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureEquipmentVisuals(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("attachToSocket"), TEXT("Attach mesh to socket when equipped."))
	 .String(TEXT("defaultSocket"), TEXT("Default socket for equipped item attachment."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("blueprintPath")});
}

static void S_CreateLootTable(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_AddLootEntry(FMcpSchemaBuilder& B)
{
	B.String(TEXT("lootTablePath"), TEXT("Path to loot table asset."))
	 .String(TEXT("itemPath"), TEXT("Path to item data asset."))
	 .Number(TEXT("lootWeight"), TEXT("Weight for drop chance calculation."))
	 .Number(TEXT("minQuantity"), TEXT("Minimum drop quantity."))
	 .Number(TEXT("maxQuantity"), TEXT("Maximum drop quantity."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("lootTablePath"), TEXT("itemPath")});
}

static void S_ConfigureLootDrop(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorPath"), TEXT("Path to actor Blueprint for loot drop."))
	 .String(TEXT("lootTablePath"), TEXT("Path to loot table asset."))
	 .Number(TEXT("dropCount"), TEXT("Number of drops to roll."))
	 .Number(TEXT("dropRadius"), TEXT("Radius for scattered drops."))
	 .Bool(TEXT("dropOnDeath"), TEXT("Drop loot when the actor dies."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("actorPath"), TEXT("lootTablePath")});
}

static void S_SetLootQualityTiers(FMcpSchemaBuilder& B)
{
	B.String(TEXT("lootTablePath"), TEXT("Path to loot table asset."))
	 .ArrayOfObjects(TEXT("tiers"), TEXT("Quality tier definitions."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("lootTablePath")});
}

static void S_CreateCraftingRecipe(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("outputItemPath"), TEXT("Path to item produced by recipe."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Number(TEXT("outputQuantity"), TEXT("Quantity produced."))
	 .Number(TEXT("craftTime"), TEXT("Time in seconds to craft."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name"), TEXT("outputItemPath")});
}

static void S_ConfigureRecipeRequirements(FMcpSchemaBuilder& B)
{
	B.String(TEXT("recipePath"), TEXT("Path to crafting recipe asset."))
	 .Number(TEXT("requiredLevel"), TEXT("Required player level."))
	 .String(TEXT("requiredStation"), TEXT("Required crafting station type."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("recipePath")});
}

static void S_CreateCraftingStation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name of the asset to create."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("stationType"), TEXT("Type of crafting station."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_AddCraftingComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureItemStacking(FMcpSchemaBuilder& B)
{
	B.String(TEXT("itemPath"), TEXT("Path to item data asset."))
	 .Bool(TEXT("stackable"), TEXT("Whether the item can stack."))
	 .Number(TEXT("maxStackSize"), TEXT("Maximum stack size."))
	 .Bool(TEXT("uniqueItems"), TEXT("Whether each stack entry is unique."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("itemPath")});
}

static void S_SetItemIcon(FMcpSchemaBuilder& B)
{
	B.String(TEXT("itemPath"), TEXT("Path to item data asset."))
	 .String(TEXT("iconPath"), TEXT("Path to icon texture."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("itemPath")});
}

static void S_AddRecipeIngredient(FMcpSchemaBuilder& B)
{
	B.String(TEXT("recipePath"), TEXT("Path to crafting recipe asset."))
	 .String(TEXT("ingredientItemPath"), TEXT("Item path to add as a recipe ingredient."))
	 .Number(TEXT("quantity"), TEXT("Ingredient quantity."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("recipePath"), TEXT("ingredientItemPath")});
}

static void S_RemoveLootEntry(FMcpSchemaBuilder& B)
{
	B.String(TEXT("lootTablePath"), TEXT("Path to loot table asset."))
	 .Number(TEXT("entryIndex"), TEXT("Loot entry index."))
	 .String(TEXT("itemPath"), TEXT("Path to item data asset."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("lootTablePath")});
}

static void S_ConfigureWeight(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("maxWeight"), TEXT(""))
	 .Bool(TEXT("enableWeight"), TEXT("Enable inventory weight tracking."))
	 .Bool(TEXT("encumbranceSystem"), TEXT("Enable encumbrance variables."))
	 .Bool(TEXT("encumberanceSystem"), TEXT("Misspelled alias for encumbranceSystem (accepted for back-compat)."))
	 .Number(TEXT("encumbranceThreshold"), TEXT("Encumbrance threshold ratio (misspelled 'encumberance*' params still accepted)."))
	 .Number(TEXT("encumberanceThreshold"), TEXT("Misspelled alias for encumbranceThreshold (accepted for back-compat)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("blueprintPath")});
}


static void S_GetInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("itemPath"), TEXT("Path to item data asset."))
	 .String(TEXT("lootTablePath"), TEXT("Path to loot table asset."))
	 .String(TEXT("recipePath"), TEXT("Path to crafting recipe asset."))
	 .String(TEXT("pickupPath"), TEXT("Path to pickup actor Blueprint."));
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is deliberately NOT baked: the implementation TU
// (InventoryHandlers.cpp) carries no editor gate — no #if WITH_EDITOR and no
// GEditor read (the module itself is editor-only, so WITH_EDITOR always
// holds) — and flagging would newly reject GEditor-less commandlet runs the
// shim family served. Mutating on all writers; the only reader is get_info.
// Flags pass whole to McpDeriveDecl (no editor gate mixed in).

#define MCP_IV_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags)                          \
class FMcpCall_ManageInventory_##ClassSuffix final : public FMcpCall                       \
{                                                                                          \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }         \
	const FMcpCallDecl& GetDecl() const override                                           \
	{                                                                                      \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_inventory"),             \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                              \
		return D;                                                                          \
	}                                                                                      \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                   \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override   \
	{                                                                                      \
		return S.HandlerFn(RequestId, Payload, Socket);                                    \
	}                                                                                      \
};

// Data assets (17.1)
MCP_IV_CALL(CreateItemDataAsset, "create_item_data_asset", HandleInventoryCreateItemDataAsset, EMcpCallFlags::Mutating)
MCP_IV_CALL(SetItemProperties, "set_item_properties", HandleInventorySetItemProperties, EMcpCallFlags::Mutating)
MCP_IV_CALL(CreateItemCategory, "create_item_category", HandleInventoryCreateItemCategory, EMcpCallFlags::Mutating)
MCP_IV_CALL(AssignItemCategory, "assign_item_category", HandleInventoryAssignItemCategory, EMcpCallFlags::Mutating)

// Inventory component (17.2)
MCP_IV_CALL(CreateComponent, "create_inventory_component", HandleInventoryCreateComponent, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureSlots, "configure_inventory_slots", HandleInventoryConfigureSlots, EMcpCallFlags::Mutating)
MCP_IV_CALL(AddFunctions, "add_inventory_functions", HandleInventoryAddFunctions, EMcpCallFlags::Mutating)
MCP_IV_CALL(SetReplication, "set_inventory_replication", HandleInventorySetReplication, EMcpCallFlags::Mutating)

// Pickups (17.3)
MCP_IV_CALL(CreatePickupActor, "create_pickup_actor", HandleInventoryCreatePickupActor, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigurePickupInteraction, "configure_pickup_interaction", HandleInventoryConfigurePickupInteraction, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigurePickupRespawn, "configure_pickup_respawn", HandleInventoryConfigurePickupRespawn, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigurePickupEffects, "configure_pickup_effects", HandleInventoryConfigurePickupEffects, EMcpCallFlags::Mutating)

// Equipment system (17.4)
MCP_IV_CALL(CreateEquipmentComponent, "create_equipment_component", HandleInventoryCreateEquipmentComponent, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureEquipmentEffects, "configure_equipment_effects", HandleInventoryConfigureEquipmentEffects, EMcpCallFlags::Mutating)
MCP_IV_CALL(AddEquipmentFunctions, "add_equipment_functions", HandleInventoryAddEquipmentFunctions, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureEquipmentVisuals, "configure_equipment_visuals", HandleInventoryConfigureEquipmentVisuals, EMcpCallFlags::Mutating)

// Loot system (17.5)
MCP_IV_CALL(CreateLootTable, "create_loot_table", HandleInventoryCreateLootTable, EMcpCallFlags::Mutating)
MCP_IV_CALL(AddLootEntry, "add_loot_entry", HandleInventoryAddLootEntry, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureLootDrop, "configure_loot_drop", HandleInventoryConfigureLootDrop, EMcpCallFlags::Mutating)
MCP_IV_CALL(SetLootQualityTiers, "set_loot_quality_tiers", HandleInventorySetLootQualityTiers, EMcpCallFlags::Mutating)

// Crafting system (17.6)
MCP_IV_CALL(CreateCraftingRecipe, "create_crafting_recipe", HandleInventoryCreateCraftingRecipe, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureRecipeRequirements, "configure_recipe_requirements", HandleInventoryConfigureRecipeRequirements, EMcpCallFlags::Mutating)
MCP_IV_CALL(CreateCraftingStation, "create_crafting_station", HandleInventoryCreateCraftingStation, EMcpCallFlags::Mutating)
MCP_IV_CALL(AddCraftingComponent, "add_crafting_component", HandleInventoryAddCraftingComponent, EMcpCallFlags::Mutating)

// Additional actions (17.7)
MCP_IV_CALL(ConfigureItemStacking, "configure_item_stacking", HandleInventoryConfigureItemStacking, EMcpCallFlags::Mutating)
MCP_IV_CALL(SetItemIcon, "set_item_icon", HandleInventorySetItemIcon, EMcpCallFlags::Mutating)
MCP_IV_CALL(AddRecipeIngredient, "add_recipe_ingredient", HandleInventoryAddRecipeIngredient, EMcpCallFlags::Mutating)
MCP_IV_CALL(RemoveLootEntry, "remove_loot_entry", HandleInventoryRemoveLootEntry, EMcpCallFlags::Mutating)
MCP_IV_CALL(ConfigureWeight, "configure_inventory_weight", HandleInventoryConfigureWeight, EMcpCallFlags::Mutating)

// Utility
MCP_IV_CALL(GetInfo, "get_info", HandleInventoryGetInfo, EMcpCallFlags::None)

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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_SetReplication>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_CreatePickupActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigurePickupInteraction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigurePickupRespawn>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_ConfigurePickupEffects>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_CreateEquipmentComponent>());
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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageInventory_GetInfo>());
}
