// McpTool_Inspect.cpp — inspect tool definition (32 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_Inspect : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("inspect"); }

	FString GetDescription() const override
	{
		return TEXT("Inspect any UObject: read/write properties, list components, export snapshots, "
			"and query class info. Actions: inspect_cdo (Blueprint CDO properties + all components "
			"without spawning an actor; use blueprintPath, optional detailed/componentName/propertyNames), "
			"inspect_class (class metadata), inspect_object (world actor), get_property/set_property "
			"(set_property saves the owning asset package by default; 'saved' reports the on-disk result), "
			"get_components, list_objects, find_by_class, find_by_tag, runtime_report, "
			"ui_focus (CommonUI PIE runtime snapshot: focused widget + path, the activatable "
			"owning focus + its desired-focus target, active activatable stack, current input "
			"type, and the active root's bound actions), find_objects (loaded-object discovery: "
			"className + optional pathContains substring + propertyNames readback — use for "
			"widget-tree template subobjects, slot objects, and live PIE widget state), "
				"diff_asset (structural diff of two on-disk .uasset versions: oldFilePath vs "
				"newFilePath — parent/interfaces/components/variables/graphs + CDO defaults + "
				"gasSignals).");
	}

	FString GetCategory() const override { return TEXT("core"); }

	// Pattern A: registered as "inspect" in O(1) map

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		// schema-from-decls: every param comes from a classed action's authored
		// AppendSchema() fragment (see Calls/McpCalls_Inspect.cpp). The fold
		// dedups shared params; this facade only owns the cross-cutting 'action'.
		FMcpSchemaBuilder B;
		B.StringEnum(TEXT("action"), McpConsolidatedActions::Inspect(), TEXT("Action"));
		for (FMcpCall* Call : FMcpCallRegistry::Get().CallsForTool(TEXT("inspect")))
		{
			Call->AppendSchema(B);
		}
		// Per-action required params live in each action's derived decl; the flat
		// tool 'required' only carries 'action' (fragments' Required() pollute B).
		return B.ClearRequired().Required({TEXT("action")}).Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_Inspect);
