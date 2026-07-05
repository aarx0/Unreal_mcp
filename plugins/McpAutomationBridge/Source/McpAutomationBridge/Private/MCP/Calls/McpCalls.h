// Registration entry points for classed actions (FMcpCall subclasses).
// One function per converted family; called from the subsystem right after
// McpRegisterAllActionDecls(), before the transport starts.
#pragma once

void McpRegisterAnimationPhysicsCalls();
void McpRegisterBuildEnvironmentCalls();
void McpRegisterControlActorCalls();
void McpRegisterControlEditorCalls();
void McpRegisterInspectCalls();
void McpRegisterManageAiCalls();
void McpRegisterManageAudioCalls();
void McpRegisterManageCharacterCalls();
void McpRegisterManageCombatCalls();
void McpRegisterManageEffectCalls();
void McpRegisterManageGasCalls();
void McpRegisterManageGeometryCalls();
void McpRegisterManageInteractionCalls();
void McpRegisterManageInventoryCalls();
void McpRegisterManageLevelCalls();
void McpRegisterManageLevelStructureCalls();
void McpRegisterManageNetworkingCalls();
void McpRegisterManageSequenceCalls();
void McpRegisterSystemControlCalls();
