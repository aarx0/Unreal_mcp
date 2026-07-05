// LINT-TOOL: inspect
// inspect as FMcpCall classes — sixth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers until the module split de-members those bodies: the global
// reads live in EnvironmentHandlers.cpp (HandleInspect*), the twelve actor
// actions delegate to the shared control_actor handlers after normalizing the
// target aliases, and set_property/get_property/inspect_cdo/diff_asset/
// ui_focus delegate to their dedicated handlers in PropertyHandlers.cpp /
// FocusInputHandlers.cpp.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::Inspect
{

// The actor/property handlers resolve the target from actorName (the property
// pair from objectPath first), but this family accepts actorName/name/
// objectPath interchangeably: write the first non-empty alias back to
// actorName — and, for the property pair (bAlsoObjectPath), into objectPath
// when neither objectPath nor blueprintPath was sent.
static void NormalizeActorAlias(const TSharedPtr<FJsonObject> &Payload, bool bAlsoObjectPath)
{
	FString ActorAlias;
	Payload->TryGetStringField(TEXT("actorName"), ActorAlias);
	ActorAlias.TrimStartAndEndInline();
	if (ActorAlias.IsEmpty())
	{
		Payload->TryGetStringField(TEXT("name"), ActorAlias);
		ActorAlias.TrimStartAndEndInline();
	}
	if (ActorAlias.IsEmpty())
	{
		Payload->TryGetStringField(TEXT("objectPath"), ActorAlias);
		ActorAlias.TrimStartAndEndInline();
	}
	if (!ActorAlias.IsEmpty())
	{
		Payload->SetStringField(TEXT("actorName"), ActorAlias);
	}

	if (bAlsoObjectPath)
	{
		FString ObjectPath;
		FString BlueprintPath;
		Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
		Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
		if (ObjectPath.IsEmpty() && BlueprintPath.IsEmpty() && !ActorAlias.IsEmpty())
		{
			Payload->SetStringField(TEXT("objectPath"), ActorAlias);
		}
	}
}

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations (fleet-verified against
// the handler bodies), except: list_objects sheds a 33-param mega-bag with
// seven bogus required fields (the body reads nothing); pie_report shares
// runtime_report's contract like it shares its implementation; the target
// aliases actorName/name/objectPath are uniformly optional (requiring one
// spelling would false-reject the others the normalization accepts);
// get_component_property.propertyName is optional (propertyPath is the
// alternative, matching control_actor's own row); inspect_object.objectPath
// is optional (the shared detail body resolves objectPath/actorName/name).
// The "at least one alias" requirement is beyond the decl vocabulary and
// stays handler-enforced.

inline const FMcpParamDecl P_ActorTarget[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ObjectDetail[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddTag[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("tag"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Snapshot[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("snapshotName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_DeleteObject[] = { { TEXT("actorNames"), EMcpParamKind::Array, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetComponentProperty[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, true }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetComponentProperty[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, true }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_GetProperty[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetProperty[] = { { TEXT("objectPath"), EMcpParamKind::String, false }, { TEXT("blueprintPath"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, true }, { TEXT("markDirty"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_DiffAsset[] = { { TEXT("oldFilePath"), EMcpParamKind::String, true }, { TEXT("newFilePath"), EMcpParamKind::String, true }, { TEXT("assetName"), EMcpParamKind::String, false }, { TEXT("includeDefaults"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_InspectCdo[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("detailed"), EMcpParamKind::Bool, false }, { TEXT("componentNames"), EMcpParamKind::Array, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("propertyNames"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_InspectClass[] = { { TEXT("className"), EMcpParamKind::String, false }, { TEXT("classPath"), EMcpParamKind::String, false }, { TEXT("detailed"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_FindObjects[] = { { TEXT("className"), EMcpParamKind::String, true }, { TEXT("pathContains"), EMcpParamKind::String, false }, { TEXT("exactClass"), EMcpParamKind::Bool, false }, { TEXT("includeCdo"), EMcpParamKind::Bool, false }, { TEXT("limit"), EMcpParamKind::Number, false }, { TEXT("propertyNames"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_FindByClass[] = { { TEXT("className"), EMcpParamKind::String, false }, { TEXT("classPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_FindByTag[] = { { TEXT("tag"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_RuntimeReport[] = { { TEXT("filter"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("componentNames"), EMcpParamKind::Array, false }, { TEXT("propertyName"), EMcpParamKind::String, false }, { TEXT("propertyPath"), EMcpParamKind::String, false }, { TEXT("propertyNames"), EMcpParamKind::Array, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row: the global bodies read GEditor-owned
// state and every delegation target resolves through editor-world lookups.

#define MCP_INSPECT_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)   \
class FMcpCall_Inspect_##ClassSuffix final : public FMcpCall                              \
{                                                                                         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl Decl{ TEXT("inspect"), TEXT(ActionLiteral),             \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                  \
		return Decl;                                                                      \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return S.HandlerFn(RequestId, Payload, Socket);                                   \
	}                                                                                     \
};

// Actor actions: normalize the target aliases, then delegate to the shared
// control_actor handlers (typed direct calls — migration rule 5).
#define MCP_INSPECT_ACTOR_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags) \
class FMcpCall_Inspect_##ClassSuffix final : public FMcpCall                              \
{                                                                                         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl Decl{ TEXT("inspect"), TEXT(ActionLiteral),             \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                  \
		return Decl;                                                                      \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		NormalizeActorAlias(Payload, /*bAlsoObjectPath*/ false);                          \
		return S.HandlerFn(RequestId, Payload, Socket);                                   \
	}                                                                                     \
};

// The property pair: aliases also fill objectPath, and the 4-arg property
// handlers dispatch on their internal set_object_property/get_object_property
// keys — the literals are load-bearing.
#define MCP_INSPECT_PROP_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, InternalLiteral, ExtraFlags) \
class FMcpCall_Inspect_##ClassSuffix final : public FMcpCall                              \
{                                                                                         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl Decl{ TEXT("inspect"), TEXT(ActionLiteral),             \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                  \
		return Decl;                                                                      \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		NormalizeActorAlias(Payload, /*bAlsoObjectPath*/ true);                           \
		return S.HandlerFn(RequestId, TEXT(InternalLiteral), Payload, Socket);            \
	}                                                                                     \
};

// Global reads (EnvironmentHandlers.cpp)
MCP_INSPECT_CALL(FindObjects, "find_objects", P_FindObjects, HandleInspectFindObjects, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetProjectSettings, "get_project_settings", {}, HandleInspectGetProjectSettings, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetEditorSettings, "get_editor_settings", {}, HandleInspectGetEditorSettings, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetWorldSettings, "get_world_settings", {}, HandleInspectGetWorldSettings, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetViewportInfo, "get_viewport_info", {}, HandleInspectGetViewportInfo, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetSelectedActors, "get_selected_actors", {}, HandleInspectGetSelectedActors, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetSceneStats, "get_scene_stats", {}, HandleInspectGetSceneStats, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetPerformanceStats, "get_performance_stats", {}, HandleInspectGetPerformanceStats, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetMemoryStats, "get_memory_stats", {}, HandleInspectGetMemoryStats, EMcpCallFlags::None)
MCP_INSPECT_CALL(ListObjects, "list_objects", {}, HandleInspectListObjects, EMcpCallFlags::None)
MCP_INSPECT_CALL(FindByClass, "find_by_class", P_FindByClass, HandleInspectFindByClass, EMcpCallFlags::None)
MCP_INSPECT_CALL(FindByTag, "find_by_tag", P_FindByTag, HandleInspectFindByTag, EMcpCallFlags::None)
MCP_INSPECT_CALL(InspectClass, "inspect_class", P_InspectClass, HandleInspectClassInfo, EMcpCallFlags::None)
MCP_INSPECT_CALL(RuntimeReport, "runtime_report", P_RuntimeReport, HandleInspectRuntimeReport, EMcpCallFlags::None)
MCP_INSPECT_CALL(PieReport, "pie_report", P_RuntimeReport, HandleInspectRuntimeReport, EMcpCallFlags::None)

// Self-contained delegates (FocusInputHandlers.cpp / PropertyHandlers.cpp)
MCP_INSPECT_CALL(UiFocus, "ui_focus", {}, HandleInspectUiFocus, EMcpCallFlags::None)
MCP_INSPECT_CALL(DiffAsset, "diff_asset", P_DiffAsset, HandleDiffAssetAction, EMcpCallFlags::None)
MCP_INSPECT_CALL(InspectCdo, "inspect_cdo", P_InspectCdo, HandleInspectCdoAction, EMcpCallFlags::None)

// Object detail (one generic body, identical output for all eight)
MCP_INSPECT_CALL(InspectObject, "inspect_object", P_ObjectDetail, HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetActorDetails, "get_actor_details", P_ObjectDetail, HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetBlueprintDetails, "get_blueprint_details", P_ObjectDetail, HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetComponentDetails, "get_component_details", P_ObjectDetail, HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetLevelDetails, "get_level_details", P_ObjectDetail, HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetMaterialDetails, "get_material_details", P_ObjectDetail, HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetMeshDetails, "get_mesh_details", P_ObjectDetail, HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetTextureDetails, "get_texture_details", P_ObjectDetail, HandleInspectObjectGeneric, EMcpCallFlags::None)

// Actor actions (shared control_actor handlers, ControlHandlers.cpp)
MCP_INSPECT_ACTOR_CALL(GetComponents, "get_components", P_ActorTarget, HandleControlActorGetComponents, EMcpCallFlags::None)
MCP_INSPECT_ACTOR_CALL(GetComponentProperty, "get_component_property", P_GetComponentProperty, HandleControlActorGetComponentProperty, EMcpCallFlags::None)
MCP_INSPECT_ACTOR_CALL(SetComponentProperty, "set_component_property", P_SetComponentProperty, HandleControlActorSetComponentProperties, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(GetMetadata, "get_metadata", P_ActorTarget, HandleControlActorGetMetadata, EMcpCallFlags::None)
MCP_INSPECT_ACTOR_CALL(AddTag, "add_tag", P_AddTag, HandleControlActorAddTag, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(CreateSnapshot, "create_snapshot", P_Snapshot, HandleControlActorCreateSnapshot, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(RestoreSnapshot, "restore_snapshot", P_Snapshot, HandleControlActorRestoreSnapshot, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(Export, "export", P_ActorTarget, HandleControlActorExport, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(DeleteObject, "delete_object", P_DeleteObject, HandleControlActorDelete, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(GetBoundingBox, "get_bounding_box", P_ActorTarget, HandleControlActorGetBoundingBox, EMcpCallFlags::None)

// Property pair (PropertyHandlers.cpp)
MCP_INSPECT_PROP_CALL(SetProperty, "set_property", P_SetProperty, HandleSetObjectProperty, "set_object_property", EMcpCallFlags::Mutating)
MCP_INSPECT_PROP_CALL(GetProperty, "get_property", P_GetProperty, HandleGetObjectProperty, "get_object_property", EMcpCallFlags::None)

#undef MCP_INSPECT_CALL
#undef MCP_INSPECT_ACTOR_CALL
#undef MCP_INSPECT_PROP_CALL

} // namespace McpCalls::Inspect

void McpRegisterInspectCalls()
{
	using namespace McpCalls::Inspect;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_FindObjects>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetProjectSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetEditorSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetWorldSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetViewportInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetSelectedActors>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetSceneStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetPerformanceStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetMemoryStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_ListObjects>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_FindByClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_FindByTag>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_InspectClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_RuntimeReport>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_PieReport>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_UiFocus>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_DiffAsset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_InspectCdo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_InspectObject>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetActorDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetBlueprintDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetComponentDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetLevelDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetMaterialDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetMeshDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetTextureDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetComponents>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetComponentProperty>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_SetComponentProperty>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetMetadata>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_AddTag>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_CreateSnapshot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_RestoreSnapshot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_Export>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_DeleteObject>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetBoundingBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_SetProperty>());
	Registry.RegisterCall(MakeUnique<FMcpCall_Inspect_GetProperty>());
}
