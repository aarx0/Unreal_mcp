// McpTool_ManageInventory.cpp — manage_inventory tool definition (33 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageInventory : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_inventory"); }

	FString GetDescription() const override
	{
		return TEXT("Create item data assets, world pickups, loot tables, and crafting "
			"recipes. Component/system actions scaffold convention-named variables "
			"and defaults for game code to consume; they do not implement runtime "
			"inventory logic.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ManageInventory(), TEXT("Inventory action to perform."))
			.String(TEXT("name"), TEXT("Name of the asset to create."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("itemPath"), TEXT("Path to item data asset."))
			.FreeformObject(TEXT("properties"), TEXT("Properties to apply to an item data asset."))
			.Bool(TEXT("stackable"), TEXT("Whether the item can stack."))
			.Number(TEXT("maxStackSize"), TEXT("Maximum stack size."))
			.Bool(TEXT("uniqueItems"), TEXT("Whether each stack entry is unique."))
			.String(TEXT("iconPath"), TEXT("Path to icon texture."))
			.String(TEXT("categoryPath"), TEXT("Path to item category asset."))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.Number(TEXT("slotCount"), TEXT(""))
			.Number(TEXT("maxWeight"), TEXT(""))
			.Bool(TEXT("enableWeight"), TEXT("Enable inventory weight tracking."))
			.Bool(TEXT("encumberanceSystem"), TEXT("Enable encumberance variables."))
			.Number(TEXT("encumberanceThreshold"), TEXT("Encumberance threshold ratio."))
			.Bool(TEXT("replicated"), TEXT("Whether to replicate."))
			.StringEnum(TEXT("replicationCondition"), {
				TEXT("None"),
				TEXT("OwnerOnly"),
				TEXT("SkipOwner"),
				TEXT("SimulatedOnly"),
				TEXT("AutonomousOnly"),
				TEXT("Custom")
			}, TEXT("Replication condition for inventory."))
			.String(TEXT("pickupPath"), TEXT("Path to pickup actor Blueprint."))
			.StringEnum(TEXT("interactionType"), {
				TEXT("Overlap"),
				TEXT("Interact"),
				TEXT("Key"),
				TEXT("Hold")
			}, TEXT("How player picks up item."))
			.String(TEXT("prompt"), TEXT("Prompt text."))
			.Bool(TEXT("respawnable"), TEXT(""))
			.Number(TEXT("respawnTime"), TEXT("Respawn time in seconds."))
			.Bool(TEXT("bobbing"), TEXT("Enable bobbing animation."))
			.Bool(TEXT("rotation"), TEXT("Enable rotation animation."))
			.Bool(TEXT("glowEffect"), TEXT("Enable glow effect."))
			.Array(TEXT("slots"), TEXT("Equipment slot names to configure."))
			.Bool(TEXT("statModifiers"), TEXT("Enable stat modifier support when equipped."))
			.Bool(TEXT("abilityGrants"), TEXT("Enable ability grant support when equipped."))
			.Bool(TEXT("passiveEffects"), TEXT("Enable passive effect support when equipped."))
			.Bool(TEXT("attachToSocket"), TEXT("Attach mesh to socket when equipped."))
			.String(TEXT("defaultSocket"), TEXT("Default socket for equipped item attachment."))
			.String(TEXT("lootTablePath"), TEXT("Path to loot table asset."))
			.Number(TEXT("lootWeight"), TEXT("Weight for drop chance calculation."))
			.Number(TEXT("minQuantity"), TEXT("Minimum drop quantity."))
			.Number(TEXT("maxQuantity"), TEXT("Maximum drop quantity."))
			.String(TEXT("actorPath"), TEXT("Path to actor Blueprint for loot drop."))
			.Number(TEXT("dropCount"), TEXT("Number of drops to roll."))
			.Number(TEXT("dropRadius"), TEXT("Radius for scattered drops."))
			.Bool(TEXT("dropOnDeath"), TEXT("Drop loot when the actor dies."))
			.Number(TEXT("entryIndex"), TEXT("Loot entry index."))
			.ArrayOfObjects(TEXT("tiers"), TEXT("Quality tier definitions."))
			.String(TEXT("recipePath"), TEXT("Path to crafting recipe asset."))
			.String(TEXT("outputItemPath"), TEXT("Path to item produced by recipe."))
			.Number(TEXT("outputQuantity"), TEXT("Quantity produced."))
			.Number(TEXT("craftTime"), TEXT("Time in seconds to craft."))
			.String(TEXT("ingredientItemPath"), TEXT("Item path to add as a recipe ingredient."))
			.Number(TEXT("quantity"), TEXT("Ingredient quantity."))
			.Number(TEXT("requiredLevel"), TEXT("Required player level."))
			.String(TEXT("requiredStation"), TEXT("Required crafting station type."))
			.String(TEXT("stationPath"), TEXT("Path to crafting station blueprint."))
			.Array(TEXT("recipePaths"), TEXT("Recipe paths for crafting station."))
			.Number(TEXT("craftingSpeedMultiplier"), TEXT("Crafting speed multiplier."))
			.String(TEXT("stationType"), TEXT("Type of crafting station."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageInventory);
