// Registers every tool family's action declarations. Called once from the
// subsystem before the transport starts.
#pragma once

#include "MCP/Decls/McpDecl_ManageLevelStructure.h"
#include "MCP/Decls/McpDecl_ManageAi.h"
#include "MCP/Decls/McpDecl_ManageLevel.h"
#include "MCP/Decls/McpDecl_ControlEditor.h"
#include "MCP/Decls/McpDecl_ManageCharacter.h"
#include "MCP/Decls/McpDecl_ManageInteraction.h"
#include "MCP/Decls/McpDecl_ManageAudio.h"
#include "MCP/Decls/McpDecl_ManageBlueprint.h"
#include "MCP/Decls/McpDecl_ManageCombat.h"
#include "MCP/Decls/McpDecl_ManageAsset.h"
#include "MCP/Decls/McpDecl_AnimationPhysics.h"
#include "MCP/Decls/McpDecl_ManageEffect.h"
#include "MCP/Decls/McpDecl_ManageGas.h"
#include "MCP/Decls/McpDecl_SystemControl.h"
#include "MCP/Decls/McpDecl_ManageNetworking.h"
#include "MCP/Decls/McpDecl_Inspect.h"
#include "MCP/Decls/McpDecl_BuildEnvironment.h"
#include "MCP/Decls/McpDecl_ManageInventory.h"
#include "MCP/Decls/McpDecl_ManageGeometry.h"

inline void McpRegisterAllActionDecls()
{
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageLevelStructure);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageAi);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageLevel);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GControlEditor);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageCharacter);
	// control_actor: classed (MCP/Calls/McpCalls_ControlActor.cpp) — decls
	// register with the call instances via McpRegisterControlActorCalls().
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageInteraction);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageAudio);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageBlueprint);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageCombat);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageAsset);
	// manage_sequence: classed (MCP/Calls/McpCalls_ManageSequence.cpp) — decls
	// register with the call instances via McpRegisterManageSequenceCalls().
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GAnimationPhysics);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageEffect);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageGas);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GSystemControl);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageNetworking);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GInspect);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GBuildEnvironment);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageInventory);
	FMcpCallRegistry::Get().RegisterDecls(McpDecls::GManageGeometry);
}
