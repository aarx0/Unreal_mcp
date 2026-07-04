// Action declarations for manage_inventory — the server's contract: which params
// each action reads, and which are required. Fleet-authored from handler
// source (three-witness cross-check, 2026-07-04), hand-maintained since:
// adding an action = adding its declaration here (the boot validation and
// tests/schema/action-decl-lint.ps1 enforce both directions).
// UnverifiedDecl = no reachable read path was attributable; validation skips
// those actions and the lint nags until someone verifies or removes them.
#pragma once

#include "MCP/McpCallRegistry.h"

namespace McpDecls
{
inline const FMcpParamDecl P_ManageInventory_0[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_1[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_2[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_3[] = { { TEXT("lootTablePath"), EMcpParamKind::String, true }, { TEXT("itemPath"), EMcpParamKind::String, true }, { TEXT("lootWeight"), EMcpParamKind::Number, false }, { TEXT("minQuantity"), EMcpParamKind::Number, false }, { TEXT("maxQuantity"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_4[] = { { TEXT("recipePath"), EMcpParamKind::String, true }, { TEXT("ingredientItemPath"), EMcpParamKind::String, true }, { TEXT("quantity"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_5[] = { { TEXT("itemPath"), EMcpParamKind::String, true }, { TEXT("categoryPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_6[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("statModifiers"), EMcpParamKind::Bool, false }, { TEXT("abilityGrants"), EMcpParamKind::Bool, false }, { TEXT("passiveEffects"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_7[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("attachToSocket"), EMcpParamKind::Bool, false }, { TEXT("defaultSocket"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_8[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_9[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("slotCount"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_10[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("maxWeight"), EMcpParamKind::Number, false }, { TEXT("enableWeight"), EMcpParamKind::Bool, false }, { TEXT("encumbranceSystem"), EMcpParamKind::Bool, false }, { TEXT("encumberanceSystem"), EMcpParamKind::Bool, false }, { TEXT("encumbranceThreshold"), EMcpParamKind::Number, false }, { TEXT("encumberanceThreshold"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_11[] = { { TEXT("itemPath"), EMcpParamKind::String, true }, { TEXT("stackable"), EMcpParamKind::Bool, false }, { TEXT("maxStackSize"), EMcpParamKind::Number, false }, { TEXT("uniqueItems"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_12[] = { { TEXT("actorPath"), EMcpParamKind::String, true }, { TEXT("lootTablePath"), EMcpParamKind::String, true }, { TEXT("dropCount"), EMcpParamKind::Number, false }, { TEXT("dropRadius"), EMcpParamKind::Number, false }, { TEXT("dropOnDeath"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_13[] = { { TEXT("pickupPath"), EMcpParamKind::String, true }, { TEXT("bobbing"), EMcpParamKind::Bool, false }, { TEXT("rotation"), EMcpParamKind::Bool, false }, { TEXT("glowEffect"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_14[] = { { TEXT("pickupPath"), EMcpParamKind::String, true }, { TEXT("interactionType"), EMcpParamKind::String, false }, { TEXT("prompt"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_15[] = { { TEXT("pickupPath"), EMcpParamKind::String, true }, { TEXT("respawnable"), EMcpParamKind::Bool, false }, { TEXT("respawnTime"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_16[] = { { TEXT("recipePath"), EMcpParamKind::String, true }, { TEXT("requiredLevel"), EMcpParamKind::Number, false }, { TEXT("requiredStation"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_17[] = { { TEXT("stationPath"), EMcpParamKind::String, true }, { TEXT("recipePaths"), EMcpParamKind::Array, false }, { TEXT("stationType"), EMcpParamKind::String, false }, { TEXT("craftingSpeedMultiplier"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_18[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("outputItemPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("outputQuantity"), EMcpParamKind::Number, false }, { TEXT("craftTime"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_19[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("stationType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_20[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_21[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("slotCount"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_22[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_23[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_24[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_25[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_26[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("slots"), EMcpParamKind::Array, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_27[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("itemPath"), EMcpParamKind::String, false }, { TEXT("lootTablePath"), EMcpParamKind::String, false }, { TEXT("recipePath"), EMcpParamKind::String, false }, { TEXT("pickupPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageInventory_28[] = { { TEXT("lootTablePath"), EMcpParamKind::String, true }, { TEXT("entryIndex"), EMcpParamKind::Number, false }, { TEXT("itemPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_29[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("replicated"), EMcpParamKind::Bool, false }, { TEXT("replicationCondition"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_30[] = { { TEXT("itemPath"), EMcpParamKind::String, true }, { TEXT("iconPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_31[] = { { TEXT("itemPath"), EMcpParamKind::String, true }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInventory_32[] = { { TEXT("lootTablePath"), EMcpParamKind::String, true }, { TEXT("tiers"), EMcpParamKind::Array, false }, { TEXT("save"), EMcpParamKind::Bool, false } };

inline const FMcpCallDecl GManageInventory[] =
{
	{ TEXT("manage_inventory"), TEXT("add_crafting_component"), P_ManageInventory_0, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("add_equipment_functions"), P_ManageInventory_1, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("add_inventory_functions"), P_ManageInventory_2, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("add_loot_entry"), P_ManageInventory_3, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("add_recipe_ingredient"), P_ManageInventory_4, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("assign_item_category"), P_ManageInventory_5, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_equipment_effects"), P_ManageInventory_6, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_equipment_visuals"), P_ManageInventory_7, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_inventory_events"), P_ManageInventory_8, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_inventory_slots"), P_ManageInventory_9, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_inventory_weight"), P_ManageInventory_10, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_item_stacking"), P_ManageInventory_11, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_loot_drop"), P_ManageInventory_12, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_pickup_effects"), P_ManageInventory_13, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_pickup_interaction"), P_ManageInventory_14, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_pickup_respawn"), P_ManageInventory_15, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_recipe_requirements"), P_ManageInventory_16, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("configure_station_recipes"), P_ManageInventory_17, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("create_crafting_recipe"), P_ManageInventory_18, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("create_crafting_station"), P_ManageInventory_19, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("create_equipment_component"), P_ManageInventory_20, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("create_inventory_component"), P_ManageInventory_21, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("create_item_category"), P_ManageInventory_22, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("create_item_data_asset"), P_ManageInventory_23, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("create_loot_table"), P_ManageInventory_24, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("create_pickup_actor"), P_ManageInventory_25, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("define_equipment_slots"), P_ManageInventory_26, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("get_inventory_info"), P_ManageInventory_27, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("remove_loot_entry"), P_ManageInventory_28, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("set_inventory_replication"), P_ManageInventory_29, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("set_item_icon"), P_ManageInventory_30, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("set_item_properties"), P_ManageInventory_31, EMcpCallFlags::None },
	{ TEXT("manage_inventory"), TEXT("set_loot_quality_tiers"), P_ManageInventory_32, EMcpCallFlags::None },
};
}
