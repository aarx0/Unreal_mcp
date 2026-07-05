// Registration entry points for classed actions (FMcpCall subclasses).
// One function per converted family; called from the subsystem right after
// McpRegisterAllActionDecls(), before the transport starts.
#pragma once

void McpRegisterControlActorCalls();
void McpRegisterControlEditorCalls();
void McpRegisterInspectCalls();
void McpRegisterManageEffectCalls();
void McpRegisterManageInteractionCalls();
void McpRegisterManageLevelCalls();
void McpRegisterManageSequenceCalls();
void McpRegisterSystemControlCalls();
