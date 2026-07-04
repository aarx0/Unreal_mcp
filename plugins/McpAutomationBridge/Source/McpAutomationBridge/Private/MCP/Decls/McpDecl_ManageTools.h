// Action declarations for manage_tools — the server's contract: which params
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

inline const FMcpCallDecl GManageTools[] =
{
	{ TEXT("manage_tools"), TEXT("disable_category"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_tools"), TEXT("disable_tools"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_tools"), TEXT("enable_category"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_tools"), TEXT("enable_tools"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_tools"), TEXT("get_status"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_tools"), TEXT("list_categories"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_tools"), TEXT("list_tools"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_tools"), TEXT("reset"), {}, EMcpCallFlags::UnverifiedDecl },
};
}
