// Action declarations for control_actor — the server's contract: which params
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
inline const FMcpParamDecl P_ControlActor_0[] = { { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("params"), EMcpParamKind::Object, false }, { TEXT("payloadBase64"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("componentClass"), EMcpParamKind::String, false }, { TEXT("attachTo"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentType"), EMcpParamKind::String, true }, { TEXT("meshPath"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ControlActor_1[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ControlActor_2[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("force"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ControlActor_3[] = { { TEXT("childActor"), EMcpParamKind::String, true }, { TEXT("parentActor"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ControlActor_5[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("arguments"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_ControlActor_6[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("snapshotName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ControlActor_7[] = { { TEXT("actorNames"), EMcpParamKind::Array, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlActor_8[] = { { TEXT("tag"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ControlActor_10[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("childActor"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlActor_12[] = { { TEXT("sourcePath"), EMcpParamKind::String, true }, { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("overwrite"), EMcpParamKind::Bool, false }, { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, false }, { TEXT("offset"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ControlActor_16[] = { { TEXT("className"), EMcpParamKind::String, false }, { TEXT("class"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlActor_17[] = { { TEXT("name"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ControlActor_18[] = { { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("matchType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlActor_19[] = { { TEXT("classPath"), EMcpParamKind::String, false }, { TEXT("className"), EMcpParamKind::String, false }, { TEXT("actorClass"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("meshPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("actorNames"), EMcpParamKind::Array, false }, { TEXT("force"), EMcpParamKind::Object, false }, { TEXT("visible"), EMcpParamKind::Bool, false }, { TEXT("componentType"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("newName"), EMcpParamKind::String, false }, { TEXT("offset"), EMcpParamKind::Object, false }, { TEXT("childActor"), EMcpParamKind::String, true }, { TEXT("parentActor"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("matchType"), EMcpParamKind::String, false }, { TEXT("variables"), EMcpParamKind::Object, true }, { TEXT("snapshotName"), EMcpParamKind::String, true }, { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("limit"), EMcpParamKind::Number, false }, { TEXT("class"), EMcpParamKind::String, false }, { TEXT("actor_name"), EMcpParamKind::String, false }, { TEXT("component_name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlActor_22[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlActor_23[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlActor_24[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ControlActor_25[] = { { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("limit"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ControlActor_26[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("actor_name"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("component_name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlActor_27[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ControlActor_28[] = { { TEXT("classPath"), EMcpParamKind::String, false }, { TEXT("className"), EMcpParamKind::String, false }, { TEXT("actorClass"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("meshPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("actorNames"), EMcpParamKind::Array, false }, { TEXT("force"), EMcpParamKind::Object, false }, { TEXT("visible"), EMcpParamKind::Bool, false }, { TEXT("componentType"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("newName"), EMcpParamKind::String, false }, { TEXT("offset"), EMcpParamKind::Object, false }, { TEXT("childActor"), EMcpParamKind::String, true }, { TEXT("parentActor"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("matchType"), EMcpParamKind::String, false }, { TEXT("variables"), EMcpParamKind::Object, true }, { TEXT("snapshotName"), EMcpParamKind::String, true }, { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("limit"), EMcpParamKind::Number, false }, { TEXT("class"), EMcpParamKind::String, false }, { TEXT("actor_name"), EMcpParamKind::String, false }, { TEXT("component_name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlActor_34[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("variables"), EMcpParamKind::Object, true } };
inline const FMcpParamDecl P_ControlActor_36[] = { { TEXT("classPath"), EMcpParamKind::String, false }, { TEXT("className"), EMcpParamKind::String, false }, { TEXT("actorClass"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("meshPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("actorNames"), EMcpParamKind::Array, false }, { TEXT("force"), EMcpParamKind::Object, false }, { TEXT("visible"), EMcpParamKind::Bool, false }, { TEXT("componentType"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("newName"), EMcpParamKind::String, false }, { TEXT("offset"), EMcpParamKind::Object, false }, { TEXT("childActor"), EMcpParamKind::String, true }, { TEXT("parentActor"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("matchType"), EMcpParamKind::String, false }, { TEXT("variables"), EMcpParamKind::Object, true }, { TEXT("snapshotName"), EMcpParamKind::String, true }, { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("limit"), EMcpParamKind::Number, false }, { TEXT("class"), EMcpParamKind::String, false }, { TEXT("actor_name"), EMcpParamKind::String, false }, { TEXT("component_name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlActor_37[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ControlActor_38[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("visible"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ControlActor_39[] = { { TEXT("classPath"), EMcpParamKind::String, false }, { TEXT("className"), EMcpParamKind::String, false }, { TEXT("actorClass"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("meshPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ControlActor_41[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false } };

inline const FMcpCallDecl GControlActor[] =
{
	{ TEXT("control_actor"), TEXT("add_component"), P_ControlActor_0, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("add_tag"), P_ControlActor_1, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("apply_force"), P_ControlActor_2, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("attach"), P_ControlActor_3, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("call_actor_function"), P_ControlActor_5, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("create_snapshot"), P_ControlActor_6, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("delete"), P_ControlActor_7, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("delete_by_tag"), P_ControlActor_8, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("detach"), P_ControlActor_10, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("duplicate"), P_ControlActor_12, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("find_by_class"), P_ControlActor_16, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("find_by_name"), P_ControlActor_17, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("find_by_tag"), P_ControlActor_18, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("get_actor_bounds"), P_ControlActor_19, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("get_component_property"), P_ControlActor_22, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("get_components"), P_ControlActor_23, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("get_transform"), P_ControlActor_24, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("list"), P_ControlActor_25, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("remove_component"), P_ControlActor_26, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("remove_tag"), P_ControlActor_27, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("set_actor_collision"), P_ControlActor_28, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("set_blueprint_variables"), P_ControlActor_34, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("set_component_property"), P_ControlActor_36, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("set_transform"), P_ControlActor_37, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("set_visibility"), P_ControlActor_38, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("spawn"), P_ControlActor_39, EMcpCallFlags::None },
	{ TEXT("control_actor"), TEXT("spawn_blueprint"), P_ControlActor_41, EMcpCallFlags::None },
};
}
