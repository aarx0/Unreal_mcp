// Registers every tool family's action declarations. Called once from the
// subsystem before the transport starts.
#pragma once

#include "MCP/McpCallRegistry.h"

#include "MCP/Decls/McpDecl_ManageAi.h"
#include "MCP/Decls/McpDecl_ManageAudio.h"
#include "MCP/Decls/McpDecl_ManageBlueprint.h"
#include "MCP/Decls/McpDecl_ManageAsset.h"
#include "MCP/Decls/McpDecl_AnimationPhysics.h"
#include "MCP/Decls/McpDecl_ManageGeometry.h"

inline void McpRegisterAllActionDecls()
{
	// manage_level_structure: classed
	// (MCP/Calls/McpCalls_ManageLevelStructure.cpp) — decls register with the
	// call instances via McpRegisterManageLevelStructureCalls().
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageAi);
	// manage_level: classed (MCP/Calls/McpCalls_ManageLevel.cpp) — decls
	// register with the call instances via McpRegisterManageLevelCalls().
	// control_editor: classed (MCP/Calls/McpCalls_ControlEditor.cpp) — decls
	// register with the call instances via McpRegisterControlEditorCalls().
	// manage_character: classed (MCP/Calls/McpCalls_ManageCharacter.cpp) —
	// decls register with the call instances via
	// McpRegisterManageCharacterCalls().
	// control_actor: classed (MCP/Calls/McpCalls_ControlActor.cpp) — decls
	// register with the call instances via McpRegisterControlActorCalls().
	// manage_interaction: classed (MCP/Calls/McpCalls_ManageInteraction.cpp) —
	// decls register with the call instances via
	// McpRegisterManageInteractionCalls().
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageAudio);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageBlueprint);
	// manage_combat: classed (MCP/Calls/McpCalls_ManageCombat.cpp) —
	// decls register with the call instances via
	// McpRegisterManageCombatCalls().
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageAsset);
	// manage_sequence: classed (MCP/Calls/McpCalls_ManageSequence.cpp) — decls
	// register with the call instances via McpRegisterManageSequenceCalls().
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GAnimationPhysics);
	// manage_effect: classed (MCP/Calls/McpCalls_ManageEffect.cpp) — decls
	// register with the call instances via McpRegisterManageEffectCalls().
	// manage_gas: classed (MCP/Calls/McpCalls_ManageGas.cpp) — decls
	// register with the call instances via
	// McpRegisterManageGasCalls().
	// system_control: classed (MCP/Calls/McpCalls_SystemControl.cpp) — decls
	// register with the call instances via McpRegisterSystemControlCalls().
	// manage_networking: classed
	// (MCP/Calls/McpCalls_ManageNetworking.cpp) — decls register with the
	// call instances via McpRegisterManageNetworkingCalls().
	// inspect: classed (MCP/Calls/McpCalls_Inspect.cpp) — decls
	// register with the call instances via McpRegisterInspectCalls().
	// build_environment: classed
	// (MCP/Calls/McpCalls_BuildEnvironment.cpp) — decls register with the
	// call instances via McpRegisterBuildEnvironmentCalls().
	// manage_inventory: classed (MCP/Calls/McpCalls_ManageInventory.cpp) —
	// decls register with the call instances via
	// McpRegisterManageInventoryCalls().
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageGeometry);
}
