// Action declarations for inspect — the server's contract: which params
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
inline const FMcpParamDecl P_Inspect_4[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_5[] = { { TEXT("className"), EMcpParamKind::String, false }, { TEXT("classPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_6[] = { { TEXT("tag"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_7[] = { { TEXT("className"), EMcpParamKind::String, true }, { TEXT("pathContains"), EMcpParamKind::String, false }, { TEXT("exactClass"), EMcpParamKind::Bool, false }, { TEXT("includeCdo"), EMcpParamKind::Bool, false }, { TEXT("limit"), EMcpParamKind::Number, false }, { TEXT("propertyNames"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_Inspect_10[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_19[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_29[] = { { TEXT("className"), EMcpParamKind::String, false }, { TEXT("classPath"), EMcpParamKind::String, false }, { TEXT("detailed"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_Inspect_30[] = { { TEXT("objectPath"), EMcpParamKind::String, true }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_31[] = { { TEXT("classPath"), EMcpParamKind::String, false }, { TEXT("className"), EMcpParamKind::String, false }, { TEXT("actorClass"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("meshPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("actorNames"), EMcpParamKind::Array, false }, { TEXT("force"), EMcpParamKind::Object, false }, { TEXT("visible"), EMcpParamKind::Bool, false }, { TEXT("componentType"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("newName"), EMcpParamKind::String, false }, { TEXT("offset"), EMcpParamKind::Object, false }, { TEXT("childActor"), EMcpParamKind::String, true }, { TEXT("parentActor"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("matchType"), EMcpParamKind::String, false }, { TEXT("variables"), EMcpParamKind::Object, true }, { TEXT("snapshotName"), EMcpParamKind::String, true }, { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("limit"), EMcpParamKind::Number, false }, { TEXT("class"), EMcpParamKind::String, false }, { TEXT("actor_name"), EMcpParamKind::String, false }, { TEXT("component_name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_32[] = { { TEXT("className"), EMcpParamKind::String, false }, { TEXT("classPath"), EMcpParamKind::String, false }, { TEXT("tag"), EMcpParamKind::String, false }, { TEXT("detailed"), EMcpParamKind::Bool, false }, { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("componentNames"), EMcpParamKind::Array, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("propertyNames"), EMcpParamKind::Array, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_33[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("snapshotName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Inspect_34[] = { { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("componentNames"), EMcpParamKind::Array, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("propertyNames"), EMcpParamKind::Array, false } };

inline const FMcpParamDecl P_Inspect_D0[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D1[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("snapshotName"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D2[] = { { TEXT("actorNames"), EMcpParamKind::Array, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D3[] = { { TEXT("oldFilePath"), EMcpParamKind::String, true }, { TEXT("newFilePath"), EMcpParamKind::String, true }, { TEXT("assetName"), EMcpParamKind::String, false }, { TEXT("includeDefaults"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_Inspect_D4[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D5[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D6[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D7[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, true }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D8[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D9[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D10[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D11[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D12[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D13[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Inspect_D14[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("detailed"), EMcpParamKind::Bool, false }, { TEXT("componentNames"), EMcpParamKind::Array, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("propertyNames"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_Inspect_D15[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, true }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_Inspect_D16[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, true }, { TEXT("markDirty"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };

inline const FMcpCallDecl GInspect[] =
{
	{ TEXT("inspect"), TEXT("add_tag"), P_Inspect_D0, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("create_snapshot"), P_Inspect_D1, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("delete_object"), P_Inspect_D2, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("diff_asset"), P_Inspect_D3, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("export"), P_Inspect_4, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("find_by_class"), P_Inspect_5, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("find_by_tag"), P_Inspect_6, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("find_objects"), P_Inspect_7, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_actor_details"), P_Inspect_D4, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_blueprint_details"), P_Inspect_D5, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_bounding_box"), P_Inspect_10, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_component_details"), P_Inspect_D6, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_component_property"), P_Inspect_D7, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_components"), P_Inspect_D8, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_editor_settings"), {}, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_level_details"), P_Inspect_D9, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_material_details"), P_Inspect_D10, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_memory_stats"), {}, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_mesh_details"), P_Inspect_D11, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_metadata"), P_Inspect_19, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_performance_stats"), {}, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_project_settings"), {}, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_property"), P_Inspect_D12, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_scene_stats"), {}, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_selected_actors"), {}, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_texture_details"), P_Inspect_D13, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_viewport_info"), {}, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("get_world_settings"), {}, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("inspect_cdo"), P_Inspect_D14, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("inspect_class"), P_Inspect_29, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("inspect_object"), P_Inspect_30, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("list_objects"), P_Inspect_31, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("pie_report"), P_Inspect_32, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("restore_snapshot"), P_Inspect_33, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("runtime_report"), P_Inspect_34, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("set_component_property"), P_Inspect_D15, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("set_property"), P_Inspect_D16, EMcpCallFlags::None },
	{ TEXT("inspect"), TEXT("ui_focus"), {}, EMcpCallFlags::None },
};
}
