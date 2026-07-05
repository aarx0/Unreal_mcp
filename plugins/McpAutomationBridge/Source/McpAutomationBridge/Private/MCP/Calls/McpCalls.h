// Registration entry points for classed actions (FMcpCall subclasses).
// One function per converted family; called from the subsystem right after
// McpRegisterAllActionDecls(), before the transport starts.
#pragma once

void McpRegisterControlActorCalls();
void McpRegisterControlEditorCalls();
void McpRegisterManageSequenceCalls();
