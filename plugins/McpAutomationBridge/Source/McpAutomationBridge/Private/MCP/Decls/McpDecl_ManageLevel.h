// Action declarations for manage_level — the server's contract: which params
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
inline const FMcpParamDecl P_ManageLevel_0[] = { { TEXT("subLevelPath"), EMcpParamKind::String, true }, { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("streamingMethod"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageLevel_1[] = { { TEXT("quality"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageLevel_4[] = { { TEXT("levelPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageLevel_5[] = { { TEXT("levelPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageLevel_6[] = { { TEXT("sourcePath"), EMcpParamKind::String, true }, { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("overwrite"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageLevel_7[] = { { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("exportPath"), EMcpParamKind::String, true }, { TEXT("destinationPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageLevel_10[] = { { TEXT("destinationPath"), EMcpParamKind::String, false }, { TEXT("sourcePath"), EMcpParamKind::String, true }, { TEXT("packagePath"), EMcpParamKind::String, false }, { TEXT("overwrite"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageLevel_12[] = { { TEXT("levelPath"), EMcpParamKind::String, true }, { TEXT("saveDirtyPackages"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageLevel_13[] = { { TEXT("levelPath"), EMcpParamKind::String, true }, { TEXT("saveDirtyPackages"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageLevel_14[] = { { TEXT("levelPath"), EMcpParamKind::String, true }, { TEXT("sourcePath"), EMcpParamKind::String, false }, { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, false }, { TEXT("overwrite"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageLevel_16[] = { { TEXT("savePath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageLevel_18[] = { { TEXT("savePath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageLevel_19[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("levelPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageLevel_23[] = { { TEXT("levelPath"), EMcpParamKind::String, true }, { TEXT("assetPath"), EMcpParamKind::String, false } };

inline const FMcpCallDecl GManageLevel[] =
{
	{ TEXT("manage_level"), TEXT("add_sublevel"), P_ManageLevel_0, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("build_lighting"), P_ManageLevel_1, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("create_level"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_level"), TEXT("create_light"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_level"), TEXT("delete"), P_ManageLevel_4, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("delete_level"), P_ManageLevel_5, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("duplicate_level"), P_ManageLevel_6, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("export_level"), P_ManageLevel_7, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("get_current_level"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_level"), TEXT("get_summary"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_level"), TEXT("import_level"), P_ManageLevel_10, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("list_levels"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_level"), TEXT("load"), P_ManageLevel_12, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("load_level"), P_ManageLevel_13, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("rename_level"), P_ManageLevel_14, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("save"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_level"), TEXT("save_as"), P_ManageLevel_16, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("save_level"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_level"), TEXT("save_level_as"), P_ManageLevel_18, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("set_metadata"), P_ManageLevel_19, EMcpCallFlags::None },
	{ TEXT("manage_level"), TEXT("stream"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_level"), TEXT("unload"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_level"), TEXT("unload_level"), {}, EMcpCallFlags::UnverifiedDecl },
	{ TEXT("manage_level"), TEXT("validate_level"), P_ManageLevel_23, EMcpCallFlags::None },
};
}
