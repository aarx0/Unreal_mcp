// Registration entry points for classed actions (FMcpCall subclasses).
// One function per converted family; called from the subsystem right after
// McpRegisterAllActionDecls(), before the transport starts.
#pragma once

void McpRegisterBuildEnvironmentCalls();
void McpRegisterControlActorCalls();
void McpRegisterControlEditorCalls();
void McpRegisterInspectCalls();
void McpRegisterManageCharacterCalls();
void McpRegisterManageCombatCalls();
void McpRegisterManageEffectCalls();
void McpRegisterManageGasCalls();
void McpRegisterManageInteractionCalls();
void McpRegisterManageInventoryCalls();
void McpRegisterManageLevelCalls();
void McpRegisterManageLevelStructureCalls();
void McpRegisterManageNetworkingCalls();
void McpRegisterManageSequenceCalls();
void McpRegisterSystemControlCalls();
