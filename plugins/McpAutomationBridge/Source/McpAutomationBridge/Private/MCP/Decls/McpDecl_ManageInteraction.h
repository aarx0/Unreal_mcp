// Action declarations for manage_interaction — the server's contract: which params
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
inline const FMcpParamDecl P_ManageInteraction_0[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageInteraction_1[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageInteraction_2[] = { { TEXT("chestPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("locked"), EMcpParamKind::Bool, false }, { TEXT("openAngle"), EMcpParamKind::Number, false }, { TEXT("openTime"), EMcpParamKind::Number, false }, { TEXT("lootTablePath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageInteraction_3[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageInteraction_4[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageInteraction_5[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageInteraction_6[] = { { TEXT("doorPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("openAngle"), EMcpParamKind::Number, false }, { TEXT("openTime"), EMcpParamKind::Number, false }, { TEXT("locked"), EMcpParamKind::Bool, false }, { TEXT("autoClose"), EMcpParamKind::Bool, false }, { TEXT("autoCloseDelay"), EMcpParamKind::Number, false }, { TEXT("requiresKey"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInteraction_7[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("traceType"), EMcpParamKind::String, false }, { TEXT("traceDistance"), EMcpParamKind::Number, false }, { TEXT("traceRadius"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageInteraction_8[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("widgetClass"), EMcpParamKind::String, false }, { TEXT("showOnHover"), EMcpParamKind::Bool, false }, { TEXT("showPromptText"), EMcpParamKind::Bool, false }, { TEXT("promptTextFormat"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageInteraction_9[] = { { TEXT("switchPath"), EMcpParamKind::String, true }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("switchType"), EMcpParamKind::String, false }, { TEXT("canToggle"), EMcpParamKind::Bool, false }, { TEXT("resetTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageInteraction_10[] = { { TEXT("triggerPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageInteraction_11[] = { { TEXT("triggerPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageInteraction_12[] = { { TEXT("triggerPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageInteraction_13[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("locked"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInteraction_14[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("openAngle"), EMcpParamKind::Number, false }, { TEXT("openTime"), EMcpParamKind::Number, false }, { TEXT("autoClose"), EMcpParamKind::Bool, false }, { TEXT("autoCloseDelay"), EMcpParamKind::Number, false }, { TEXT("requiresKey"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageInteraction_15[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageInteraction_16[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("traceDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageInteraction_17[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageInteraction_18[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("switchType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageInteraction_19[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("folder"), EMcpParamKind::String, false }, { TEXT("triggerShape"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageInteraction_20[] = { { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("doorPath"), EMcpParamKind::String, false }, { TEXT("switchPath"), EMcpParamKind::String, false }, { TEXT("chestPath"), EMcpParamKind::String, false }, { TEXT("triggerPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageInteraction_21[] = { { TEXT("actorName"), EMcpParamKind::String, true } };

inline const FMcpCallDecl GManageInteraction[] =
{
	{ TEXT("manage_interaction"), TEXT("add_destruction_component"), P_ManageInteraction_0, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("add_interaction_events"), P_ManageInteraction_1, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("configure_chest_properties"), P_ManageInteraction_2, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("configure_destruction_damage"), P_ManageInteraction_3, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("configure_destruction_effects"), P_ManageInteraction_4, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("configure_destruction_levels"), P_ManageInteraction_5, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("configure_door_properties"), P_ManageInteraction_6, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("configure_interaction_trace"), P_ManageInteraction_7, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("configure_interaction_widget"), P_ManageInteraction_8, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("configure_switch_properties"), P_ManageInteraction_9, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("configure_trigger_events"), P_ManageInteraction_10, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("configure_trigger_filter"), P_ManageInteraction_11, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("configure_trigger_response"), P_ManageInteraction_12, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("create_chest_actor"), P_ManageInteraction_13, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("create_door_actor"), P_ManageInteraction_14, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("create_interactable_interface"), P_ManageInteraction_15, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("create_interaction_component"), P_ManageInteraction_16, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("create_lever_actor"), P_ManageInteraction_17, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("create_switch_actor"), P_ManageInteraction_18, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("create_trigger_actor"), P_ManageInteraction_19, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("get_interaction_info"), P_ManageInteraction_20, EMcpCallFlags::None },
	{ TEXT("manage_interaction"), TEXT("setup_destructible_mesh"), P_ManageInteraction_21, EMcpCallFlags::None },
};
}
