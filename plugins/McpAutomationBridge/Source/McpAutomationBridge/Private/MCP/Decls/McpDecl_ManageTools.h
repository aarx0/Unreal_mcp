// Action declarations for manage_tools — the server's contract: which params
// each action reads, and which are required. HAND-AUTHORED 2026-07-04 (this
// tool is implemented by the transport's McpDynamicToolManager intercept, not
// a handler file, so the extraction fleet never saw it): reads `tools`
// (McpDynamicToolManager.cpp TryGetArrayField) and `category`
// (TryGetStringField); the list/status/reset actions take no params.
// Hand-maintained: adding an action = adding its declaration here.
#pragma once

#include "MCP/McpCallRegistry.h"

namespace McpDecls
{
inline const FMcpParamDecl P_ManageTools_Tools[] = { { TEXT("tools"), EMcpParamKind::Array, true } };
inline const FMcpParamDecl P_ManageTools_Category[] = { { TEXT("category"), EMcpParamKind::String, true } };

inline const FMcpCallDecl GManageTools[] =
{
	{ TEXT("manage_tools"), TEXT("disable_category"), P_ManageTools_Category, EMcpCallFlags::None },
	{ TEXT("manage_tools"), TEXT("disable_tools"), P_ManageTools_Tools, EMcpCallFlags::None },
	{ TEXT("manage_tools"), TEXT("enable_category"), P_ManageTools_Category, EMcpCallFlags::None },
	{ TEXT("manage_tools"), TEXT("enable_tools"), P_ManageTools_Tools, EMcpCallFlags::None },
	{ TEXT("manage_tools"), TEXT("get_status"), {}, EMcpCallFlags::None },
	{ TEXT("manage_tools"), TEXT("list_categories"), {}, EMcpCallFlags::None },
	{ TEXT("manage_tools"), TEXT("list_tools"), {}, EMcpCallFlags::None },
	{ TEXT("manage_tools"), TEXT("reset"), {}, EMcpCallFlags::None },
};
}
