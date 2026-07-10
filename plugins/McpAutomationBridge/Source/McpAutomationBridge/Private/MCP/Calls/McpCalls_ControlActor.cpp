// LINT-TOOL: control_actor
// LINT-SCHEMA-DERIVED
// control_actor as FMcpCall classes — the pilot family (docs/action-declarations.md),
// now on schema-from-decls. Each class AUTHORS its schema fragment in a S_<Suffix>()
// function; the published facade schema folds those fragments and GetDecl() derives the
// validation decl from the same fragment via McpDeriveDecl(), so schema and decl are one
// source and cannot drift. Run() delegates to the subsystem member handlers until the
// module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_ControlHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ControlActor
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. Actions that accept
// one-of-several aliases (delete, remove_component, get_components, find_by_class,
// detach, set_actor_collision) author every alias optional: the requirement is
// "at least one", which the decl vocabulary cannot express and the handler
// enforces itself.

static void S_Spawn(FMcpSchemaBuilder& B)
{
	B.String(TEXT("classPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("className"), TEXT("Actor class name."))
	 .String(TEXT("actorClass"), TEXT("Actor class alias or path."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("name"), TEXT("find_by_name query; alias of actorName for spawn actions."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .String(TEXT("meshPath"), TEXT("Mesh asset path."));
}

static void S_SpawnBlueprint(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("name"), TEXT("find_by_name query; alias of actorName for spawn actions."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("blueprintPath")});
}

static void S_Delete(FMcpSchemaBuilder& B)
{
	B.Array(TEXT("actorNames"), TEXT("Actor names for bulk actor operations."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."));
}

static void S_DeleteByTag(FMcpSchemaBuilder& B)
{
	B.String(TEXT("tag"), TEXT("Name of the tag."))
	 .Required({TEXT("tag")});
}

static void S_Duplicate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("newName"), TEXT("New name for renaming."))
	 .Object(TEXT("offset"), TEXT("3D offset (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("actorName")});
}

static void S_ApplyForce(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("force"), TEXT("3D vector."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("actorName")});
}

static void S_SetTransform(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("actorName")});
}

static void S_GetTransform(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Required({TEXT("actorName")});
}

static void S_SetVisibility(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Bool(TEXT("visible"), TEXT("Whether the item/actor is visible."))
	 .Required({TEXT("actorName")});
}

static void S_AddComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("componentType"), TEXT(""))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("meshPath"), TEXT("Mesh asset path (StaticMeshComponent convenience)."))
	 .Required({TEXT("actorName"), TEXT("componentType")});
}

static void S_RemoveComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("actor_name"), TEXT("snake_case alias of actorName."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("component_name"), TEXT("snake_case alias of componentName."));
}

static void S_SetComponentProperty(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("propertyName"), TEXT("Name of the property to set."))
	 .String(TEXT("propertyPath"), TEXT("Dotted path to a scalar leaf (alternative to propertyName)."))
	 // Discriminated value: populate exactly ONE typed field. Each field is both a
	 // type tag and a real schema type, so it transmits correctly -- vectorValue
	 // arrives as a real object, never a stringified blob. The server cross-checks
	 // the populated field against the property's reflected type.
	 .Bool(TEXT("boolValue"), TEXT("Set a bool property."))
	 .Number(TEXT("intValue"), TEXT("Set an integer property."))
	 .Number(TEXT("floatValue"), TEXT("Set a float/double property."))
	 .String(TEXT("stringValue"), TEXT("Set a string / name / text / enum property."))
	 .Object(TEXT("vectorValue"), TEXT("Set an FVector property (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("actorName"), TEXT("componentName")});
}

static void S_GetComponentProperty(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .String(TEXT("propertyPath"), TEXT("Dotted path to the property (alternative to propertyName)."))
	 .Required({TEXT("actorName"), TEXT("componentName")});
}

static void S_GetComponents(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("objectPath"), TEXT("Object path of the actor (alternative to actorName)."));
}

static void S_GetActorBounds(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Required({TEXT("actorName")});
}

static void S_AddTag(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("tag"), TEXT("Name of the tag."))
	 .Required({TEXT("actorName"), TEXT("tag")});
}

static void S_RemoveTag(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("tag"), TEXT("Name of the tag."))
	 .Required({TEXT("actorName"), TEXT("tag")});
}

static void S_FindByTag(FMcpSchemaBuilder& B)
{
	B.String(TEXT("tag"), TEXT("Name of the tag."))
	 .String(TEXT("matchType"), TEXT("find_by_tag: match mode, 'exact' (default) or 'contains'."))
	 .Required({TEXT("tag")});
}

static void S_FindByName(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("find_by_name query; alias of actorName for spawn actions."))
	 .Required({TEXT("name")});
}

static void S_FindByClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("className"), TEXT("Actor class name."))
	 .String(TEXT("class"), TEXT("Actor class name (alias of className)."))
	 .String(TEXT("classPath"), TEXT("Actor class path (alias of className)."));
}

static void S_List(FMcpSchemaBuilder& B)
{
	B.String(TEXT("filter"), TEXT("Optional actor label/name filter for list."))
	 .Integer(TEXT("limit"), TEXT("Maximum number of actors to return."));
}

static void S_SetBlueprintVariables(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("variableName"), TEXT("Name of the variable / actor property to set."))
	 // Discriminated value: populate exactly ONE typed field (same shape as
	 // set_component_property). Sets one variable per call, atomically.
	 .Bool(TEXT("boolValue"), TEXT("Set a bool variable."))
	 .Number(TEXT("intValue"), TEXT("Set an integer variable."))
	 .Number(TEXT("floatValue"), TEXT("Set a float/double variable."))
	 .String(TEXT("stringValue"), TEXT("Set a string / name / text / enum variable."))
	 .Object(TEXT("vectorValue"), TEXT("Set an FVector variable (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("actorName"), TEXT("variableName")});
}

static void S_CreateSnapshot(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("snapshotName"), TEXT(""))
	 .Required({TEXT("actorName"), TEXT("snapshotName")});
}

static void S_Attach(FMcpSchemaBuilder& B)
{
	B.String(TEXT("childActor"), TEXT("Name of the child actor (for attach/detach operations)."))
	 .String(TEXT("parentActor"), TEXT("Name of the parent actor (for attach operations)."))
	 .Required({TEXT("childActor"), TEXT("parentActor")});
}

static void S_Detach(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("childActor"), TEXT("Name of the child actor (for attach/detach operations)."));
}

static void S_SetActorCollision(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("actor_name"), TEXT("snake_case alias of actorName."))
	 .Bool(TEXT("collisionEnabled"), TEXT("Whether actor collision is enabled."))
	 .Bool(TEXT("collision_enabled"), TEXT("snake_case alias of collisionEnabled."));
}

static void S_CallActorFunction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("functionName"), TEXT("Name of the function."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Array(TEXT("arguments"), TEXT("Arguments to pass to an actor function."))
	 .Required({TEXT("actorName"), TEXT("functionName")});
}

// ─── Classes ─────────────────────────────────────────────────────────────────

#define MCP_CA_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                    \
class FMcpCall_ControlActor_##ClassSuffix final : public FMcpCall                         \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("control_actor"),               \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),            \
			&S_##ClassSuffix);                                                            \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return HandlerFn(S, RequestId, Payload, Socket);                                   \
	}                                                                                     \
};

MCP_CA_CALL(Spawn, "spawn", McpHandlers::ControlActor::HandleControlActorSpawn, EMcpCallFlags::Mutating)
MCP_CA_CALL(SpawnBlueprint, "spawn_blueprint", McpHandlers::ControlActor::HandleControlActorSpawnBlueprint, EMcpCallFlags::Mutating)
MCP_CA_CALL(Delete, "delete", McpHandlers::ControlActor::HandleControlActorDelete, EMcpCallFlags::Mutating)
MCP_CA_CALL(DeleteByTag, "delete_by_tag", McpHandlers::ControlActor::HandleControlActorDeleteByTag, EMcpCallFlags::Mutating)
MCP_CA_CALL(Duplicate, "duplicate", McpHandlers::ControlActor::HandleControlActorDuplicate, EMcpCallFlags::Mutating)
MCP_CA_CALL(ApplyForce, "apply_force", McpHandlers::ControlActor::HandleControlActorApplyForce, EMcpCallFlags::Mutating)
MCP_CA_CALL(SetTransform, "set_transform", McpHandlers::ControlActor::HandleControlActorSetTransform, EMcpCallFlags::Mutating)
MCP_CA_CALL(GetTransform, "get_transform", McpHandlers::ControlActor::HandleControlActorGetTransform, EMcpCallFlags::None)
MCP_CA_CALL(SetVisibility, "set_visibility", McpHandlers::ControlActor::HandleControlActorSetVisibility, EMcpCallFlags::Mutating)
MCP_CA_CALL(AddComponent, "add_component", McpHandlers::ControlActor::HandleControlActorAddComponent, EMcpCallFlags::Mutating)
MCP_CA_CALL(RemoveComponent, "remove_component", McpHandlers::ControlActor::HandleControlActorRemoveComponent, EMcpCallFlags::Mutating)
MCP_CA_CALL(SetComponentProperty, "set_component_property", McpHandlers::ControlActor::HandleControlActorSetComponentProperties, EMcpCallFlags::Mutating)
MCP_CA_CALL(GetComponentProperty, "get_component_property", McpHandlers::ControlActor::HandleControlActorGetComponentProperty, EMcpCallFlags::None)
MCP_CA_CALL(GetComponents, "get_components", McpHandlers::ControlActor::HandleControlActorGetComponents, EMcpCallFlags::None)
MCP_CA_CALL(GetActorBounds, "get_actor_bounds", McpHandlers::ControlActor::HandleControlActorGetBoundingBox, EMcpCallFlags::None)
MCP_CA_CALL(AddTag, "add_tag", McpHandlers::ControlActor::HandleControlActorAddTag, EMcpCallFlags::Mutating)
MCP_CA_CALL(RemoveTag, "remove_tag", McpHandlers::ControlActor::HandleControlActorRemoveTag, EMcpCallFlags::Mutating)
MCP_CA_CALL(FindByTag, "find_by_tag", McpHandlers::ControlActor::HandleControlActorFindByTag, EMcpCallFlags::None)
MCP_CA_CALL(FindByName, "find_by_name", McpHandlers::ControlActor::HandleControlActorFindByName, EMcpCallFlags::None)
MCP_CA_CALL(FindByClass, "find_by_class", McpHandlers::ControlActor::HandleControlActorFindByClass, EMcpCallFlags::None)
MCP_CA_CALL(List, "list", McpHandlers::ControlActor::HandleControlActorList, EMcpCallFlags::None)
MCP_CA_CALL(SetBlueprintVariables, "set_blueprint_variables", McpHandlers::ControlActor::HandleControlActorSetBlueprintVariables, EMcpCallFlags::Mutating)
MCP_CA_CALL(CreateSnapshot, "create_snapshot", McpHandlers::ControlActor::HandleControlActorCreateSnapshot, EMcpCallFlags::Mutating)
MCP_CA_CALL(Attach, "attach", McpHandlers::ControlActor::HandleControlActorAttach, EMcpCallFlags::Mutating)
MCP_CA_CALL(Detach, "detach", McpHandlers::ControlActor::HandleControlActorDetach, EMcpCallFlags::Mutating)
MCP_CA_CALL(SetActorCollision, "set_actor_collision", McpHandlers::ControlActor::HandleControlActorSetCollision, EMcpCallFlags::Mutating)
MCP_CA_CALL(CallActorFunction, "call_actor_function", McpHandlers::ControlActor::HandleControlActorCallFunction, EMcpCallFlags::Mutating)

#undef MCP_CA_CALL

} // namespace McpCalls::ControlActor

void McpRegisterControlActorCalls()
{
	using namespace McpCalls::ControlActor;
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
