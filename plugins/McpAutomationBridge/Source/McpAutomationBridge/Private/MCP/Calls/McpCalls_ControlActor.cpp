// LINT-TOOL: control_actor
// control_actor as FMcpCall classes — the pilot family (docs/action-declarations.md).
// Each class co-locates the action's declaration (its true param contract, verified
// against the dedicated handler's reads) with its implementation. Run() delegates to
// the subsystem member handlers until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

namespace McpCalls
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// bRequired marks params the handler hard-errors without. Actions that accept
// one-of-several aliases (delete, set_actor_collision, find_by_class) declare
// every alias optional: the requirement is "at least one", which the decl
// vocabulary cannot express and the handler enforces itself.

inline const FMcpParamDecl P_Spawn[] = { { TEXT("classPath"), EMcpParamKind::String, false }, { TEXT("className"), EMcpParamKind::String, false }, { TEXT("actorClass"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false }, { TEXT("meshPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SpawnBlueprint[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_Delete[] = { { TEXT("actorNames"), EMcpParamKind::Array, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_DeleteByTag[] = { { TEXT("tag"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Duplicate[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("newName"), EMcpParamKind::String, false }, { TEXT("offset"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ApplyForce[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("force"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_SetTransform[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("scale"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_GetTransform[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetVisibility[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("visible"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddComponent[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentType"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("meshPath"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_RemoveComponent[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("actor_name"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("component_name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetComponentProperty[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, true }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_GetComponentProperty[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetComponents[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetActorBounds[] = { { TEXT("actorName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_AddTag[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_RemoveTag[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("tag"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_FindByTag[] = { { TEXT("tag"), EMcpParamKind::String, true }, { TEXT("matchType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_FindByName[] = { { TEXT("name"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_FindByClass[] = { { TEXT("className"), EMcpParamKind::String, false }, { TEXT("class"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_List[] = { { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("limit"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetBlueprintVariables[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("variables"), EMcpParamKind::Object, true } };
inline const FMcpParamDecl P_CreateSnapshot[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("snapshotName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Attach[] = { { TEXT("childActor"), EMcpParamKind::String, true }, { TEXT("parentActor"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Detach[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("childActor"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetActorCollision[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("actor_name"), EMcpParamKind::String, false }, { TEXT("collisionEnabled"), EMcpParamKind::Bool, false }, { TEXT("collision_enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CallActorFunction[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("arguments"), EMcpParamKind::Array, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────

#define MCP_CA_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)      \
class FMcpCall_ControlActor_##ClassSuffix final : public FMcpCall                        \
{                                                                                        \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl Decl{ TEXT("control_actor"), TEXT(ActionLiteral),      \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                 \
		return Decl;                                                                     \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

MCP_CA_CALL(Spawn, "spawn", P_Spawn, HandleControlActorSpawn, EMcpCallFlags::Mutating)
MCP_CA_CALL(SpawnBlueprint, "spawn_blueprint", P_SpawnBlueprint, HandleControlActorSpawnBlueprint, EMcpCallFlags::Mutating)
MCP_CA_CALL(Delete, "delete", P_Delete, HandleControlActorDelete, EMcpCallFlags::Mutating)
MCP_CA_CALL(DeleteByTag, "delete_by_tag", P_DeleteByTag, HandleControlActorDeleteByTag, EMcpCallFlags::Mutating)
MCP_CA_CALL(Duplicate, "duplicate", P_Duplicate, HandleControlActorDuplicate, EMcpCallFlags::Mutating)
MCP_CA_CALL(ApplyForce, "apply_force", P_ApplyForce, HandleControlActorApplyForce, EMcpCallFlags::Mutating)
MCP_CA_CALL(SetTransform, "set_transform", P_SetTransform, HandleControlActorSetTransform, EMcpCallFlags::Mutating)
MCP_CA_CALL(GetTransform, "get_transform", P_GetTransform, HandleControlActorGetTransform, EMcpCallFlags::None)
MCP_CA_CALL(SetVisibility, "set_visibility", P_SetVisibility, HandleControlActorSetVisibility, EMcpCallFlags::Mutating)
MCP_CA_CALL(AddComponent, "add_component", P_AddComponent, HandleControlActorAddComponent, EMcpCallFlags::Mutating)
MCP_CA_CALL(RemoveComponent, "remove_component", P_RemoveComponent, HandleControlActorRemoveComponent, EMcpCallFlags::Mutating)
MCP_CA_CALL(SetComponentProperty, "set_component_property", P_SetComponentProperty, HandleControlActorSetComponentProperties, EMcpCallFlags::Mutating)
MCP_CA_CALL(GetComponentProperty, "get_component_property", P_GetComponentProperty, HandleControlActorGetComponentProperty, EMcpCallFlags::None)
MCP_CA_CALL(GetComponents, "get_components", P_GetComponents, HandleControlActorGetComponents, EMcpCallFlags::None)
MCP_CA_CALL(GetActorBounds, "get_actor_bounds", P_GetActorBounds, HandleControlActorGetBoundingBox, EMcpCallFlags::None)
MCP_CA_CALL(AddTag, "add_tag", P_AddTag, HandleControlActorAddTag, EMcpCallFlags::Mutating)
MCP_CA_CALL(RemoveTag, "remove_tag", P_RemoveTag, HandleControlActorRemoveTag, EMcpCallFlags::Mutating)
MCP_CA_CALL(FindByTag, "find_by_tag", P_FindByTag, HandleControlActorFindByTag, EMcpCallFlags::None)
MCP_CA_CALL(FindByName, "find_by_name", P_FindByName, HandleControlActorFindByName, EMcpCallFlags::None)
MCP_CA_CALL(FindByClass, "find_by_class", P_FindByClass, HandleControlActorFindByClass, EMcpCallFlags::None)
MCP_CA_CALL(List, "list", P_List, HandleControlActorList, EMcpCallFlags::None)
MCP_CA_CALL(SetBlueprintVariables, "set_blueprint_variables", P_SetBlueprintVariables, HandleControlActorSetBlueprintVariables, EMcpCallFlags::Mutating)
MCP_CA_CALL(CreateSnapshot, "create_snapshot", P_CreateSnapshot, HandleControlActorCreateSnapshot, EMcpCallFlags::Mutating)
MCP_CA_CALL(Attach, "attach", P_Attach, HandleControlActorAttach, EMcpCallFlags::Mutating)
MCP_CA_CALL(Detach, "detach", P_Detach, HandleControlActorDetach, EMcpCallFlags::Mutating)
MCP_CA_CALL(SetActorCollision, "set_actor_collision", P_SetActorCollision, HandleControlActorSetCollision, EMcpCallFlags::Mutating)
MCP_CA_CALL(CallActorFunction, "call_actor_function", P_CallActorFunction, HandleControlActorCallFunction, EMcpCallFlags::Mutating)

#undef MCP_CA_CALL

} // namespace McpCalls

void McpRegisterControlActorCalls()
{
	using namespace McpCalls;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_Spawn>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_SpawnBlueprint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_Delete>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_DeleteByTag>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_Duplicate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_ApplyForce>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_SetTransform>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_GetTransform>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_SetVisibility>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_AddComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_RemoveComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_SetComponentProperty>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_GetComponentProperty>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_GetComponents>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_GetActorBounds>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_AddTag>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_RemoveTag>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_FindByTag>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_FindByName>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_FindByClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_List>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_SetBlueprintVariables>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_CreateSnapshot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_Attach>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_Detach>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_SetActorCollision>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ControlActor_CallActorFunction>());
}
