// LINT-TOOL: inspect
// LINT-SCHEMA-DERIVED
// inspect as FMcpCall classes — sixth classed family, now on schema-from-decls
// (docs/action-declarations.md). Each class AUTHORS its schema fragment in a
// S_<Suffix>() function; the published facade schema folds those fragments and
// GetDecl() derives the validation decl from the same fragment via
// McpDeriveDecl(), so schema and decl are one source and cannot drift. Run()
// delegates to the subsystem member handlers until the module split de-members
// those bodies: the global reads live in EnvironmentHandlers.cpp
// (HandleInspect*), the twelve actor actions delegate to the shared
// control_actor handlers after normalizing the target aliases, and
// set_property/get_property/inspect_cdo/diff_asset/ui_focus delegate to their
// dedicated handlers in PropertyHandlers.cpp / FocusInputHandlers.cpp.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_ControlHandlers.h"
#include "McpAutomationBridge_FocusInputHandlers.h"
#include "McpAutomationBridge_EnvironmentHandlers.h"
#include "McpAutomationBridge_PropertyHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
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

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. Notes carried over from
// the retired shim declarations: list_objects mirrors find_objects'
// className/pathContains/limit filter-and-bound over world actors (className
// optional); pie_report shares
// runtime_report's contract like it shares its implementation; the target
// aliases actorName/name/objectPath are uniformly optional (requiring one
// spelling would false-reject the others the normalization accepts), and the
// "at least one alias" requirement is beyond the decl vocabulary and stays
// handler-enforced; get_component_property.propertyName is optional
// (propertyPath is the alternative); the detail actions all resolve
// objectPath/actorName/name, so none is required. Two recurring alias trios are
// factored into shared appenders.

static void AppendActorAliases(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("objectPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."));
}

static void AppendObjectDetailAliases(FMcpSchemaBuilder& B)
{
	B.String(TEXT("objectPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("name"), TEXT("Name identifier."));
}

// Global reads (EnvironmentHandlers.cpp)
static void S_FindObjects(FMcpSchemaBuilder& B)
{
	B.String(TEXT("className"), TEXT(""))
	 .String(TEXT("pathContains"), TEXT("find_objects: case-insensitive substring filter on the object's full path."))
	 .Bool(TEXT("exactClass"), TEXT("find_objects: require the exact class (default false = IsA semantics)."))
	 .Bool(TEXT("includeCdo"), TEXT("find_objects: include class default objects (default false)."))
	 .Integer(TEXT("limit"), TEXT("find_objects: max objects returned (default 50, cap 200)."))
	 .Array(TEXT("propertyNames"), TEXT(""))
	 .Required({TEXT("className")});
}

static void S_GetProjectSettings(FMcpSchemaBuilder&) {}
static void S_GetEditorSettings(FMcpSchemaBuilder&) {}
static void S_GetWorldSettings(FMcpSchemaBuilder&) {}
static void S_GetViewportInfo(FMcpSchemaBuilder&) {}
static void S_GetSelectedActors(FMcpSchemaBuilder&) {}
static void S_GetSceneStats(FMcpSchemaBuilder&) {}
static void S_GetPerformanceStats(FMcpSchemaBuilder&) {}
static void S_GetMemoryStats(FMcpSchemaBuilder&) {}

static void S_ListObjects(FMcpSchemaBuilder& B)
{
	B.String(TEXT("className"), TEXT("list_objects: optional class filter (IsA); loaded short name or fully-qualified path. Omit to list every actor."))
	 .String(TEXT("pathContains"), TEXT("list_objects: case-insensitive substring filter on the actor's full path."))
	 .Integer(TEXT("limit"), TEXT("list_objects: max objects returned (default 50, cap 200)."));
}

static void S_FindByClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("className"), TEXT(""))
	 .String(TEXT("class"), TEXT("Actor class name (alias of className)."))
	 .String(TEXT("classPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Integer(TEXT("limit"), TEXT("find_by_class: max actors returned (default 50, cap 200)."));
}

static void S_FindByTag(FMcpSchemaBuilder& B)
{
	B.String(TEXT("tag"), TEXT("Name of the tag."))
	 .Integer(TEXT("limit"), TEXT("find_by_tag: max actors returned (default 50, cap 200)."));
}

static void S_InspectClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("className"), TEXT(""))
	 .String(TEXT("classPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("detailed"), TEXT(""));
}

static void S_RuntimeReport(FMcpSchemaBuilder& B)
{
	B.String(TEXT("filter"), TEXT(""))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Array(TEXT("componentNames"), TEXT("Component names to include detailed property readback for."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .String(TEXT("propertyPath"), TEXT(""))
	 .Array(TEXT("propertyNames"), TEXT(""));
}

// pie_report shares runtime_report's contract and implementation.
static void S_PieReport(FMcpSchemaBuilder& B) { S_RuntimeReport(B); }

// Self-contained delegates (FocusInputHandlers.cpp / PropertyHandlers.cpp)
static void S_UiFocus(FMcpSchemaBuilder&) {}

static void S_DiffAsset(FMcpSchemaBuilder& B)
{
	B.String(TEXT("oldFilePath"), TEXT("diff_asset: filesystem path to the OLD .uasset version (e.g. a git revision extracted to a temp file)."))
	 .String(TEXT("newFilePath"), TEXT("diff_asset: filesystem path to the NEW .uasset version (e.g. the working-tree file)."))
	 .String(TEXT("assetName"), TEXT("diff_asset: object name inside the package (default = newFilePath filename stem)."))
	 .Bool(TEXT("includeDefaults"), TEXT("diff_asset: also diff CDO default properties (default true)."))
	 .Required({TEXT("oldFilePath"), TEXT("newFilePath")});
}

static void S_InspectCdo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Bool(TEXT("detailed"), TEXT(""))
	 .Array(TEXT("componentNames"), TEXT("Component names to include detailed property readback for."))
	 .String(TEXT("propertyPath"), TEXT(""))
	 .Array(TEXT("propertyNames"), TEXT(""))
	 .Required({TEXT("blueprintPath")});
}

// Object detail (one generic body, identical output for all eight)
static void S_InspectObject(FMcpSchemaBuilder& B)       { AppendObjectDetailAliases(B); }
static void S_GetActorDetails(FMcpSchemaBuilder& B)     { AppendObjectDetailAliases(B); }
static void S_GetBlueprintDetails(FMcpSchemaBuilder& B) { AppendObjectDetailAliases(B); }
static void S_GetComponentDetails(FMcpSchemaBuilder& B) { AppendObjectDetailAliases(B); }
static void S_GetLevelDetails(FMcpSchemaBuilder& B)     { AppendObjectDetailAliases(B); }
static void S_GetMaterialDetails(FMcpSchemaBuilder& B)  { AppendObjectDetailAliases(B); }
static void S_GetMeshDetails(FMcpSchemaBuilder& B)      { AppendObjectDetailAliases(B); }
static void S_GetTextureDetails(FMcpSchemaBuilder& B)   { AppendObjectDetailAliases(B); }

// Actor actions (shared control_actor handlers, ControlHandlers.cpp)
static void S_GetComponents(FMcpSchemaBuilder& B) { AppendActorAliases(B); }

static void S_GetComponentProperty(FMcpSchemaBuilder& B)
{
	AppendActorAliases(B);
	B.String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .String(TEXT("propertyPath"), TEXT(""))
	 .Required({TEXT("componentName")});
}

static void S_SetComponentProperty(FMcpSchemaBuilder& B)
{
	AppendActorAliases(B);
	B.String(TEXT("componentName"), TEXT("Name of the component."))
	 .Object(TEXT("properties"), TEXT("set_component_property: map of component property name to value (alternative to a single propertyName/value pair)."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .String(TEXT("propertyPath"), TEXT(""))
	 // Discriminated value (single propertyName/value form): populate exactly ONE.
	 .Bool(TEXT("boolValue"), TEXT("Set a bool property."))
	 .Integer(TEXT("intValue"), TEXT("Set an integer property."))
	 .Number(TEXT("floatValue"), TEXT("Set a float/double property."))
	 .String(TEXT("stringValue"), TEXT("Set a string / name / text / enum / object-path property."))
	 .Object(TEXT("vectorValue"), TEXT("Set an FVector property (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("componentName")});
}

static void S_GetMetadata(FMcpSchemaBuilder& B) { AppendActorAliases(B); }

static void S_AddTag(FMcpSchemaBuilder& B)
{
	AppendActorAliases(B);
	B.String(TEXT("tag"), TEXT("Name of the tag."))
	 .Required({TEXT("tag")});
}

static void S_CreateSnapshot(FMcpSchemaBuilder& B)
{
	AppendActorAliases(B);
	B.String(TEXT("snapshotName"), TEXT(""))
	 .Required({TEXT("snapshotName")});
}

static void S_RestoreSnapshot(FMcpSchemaBuilder& B) { S_CreateSnapshot(B); }

static void S_Export(FMcpSchemaBuilder& B) { AppendActorAliases(B); }

static void S_DeleteObject(FMcpSchemaBuilder& B)
{
	B.Array(TEXT("actorNames"), TEXT("delete_object: actor names for bulk deletion (alternative to a single actorName)."));
	AppendActorAliases(B);
}

static void S_GetBoundingBox(FMcpSchemaBuilder& B) { AppendActorAliases(B); }

// Property pair (PropertyHandlers.cpp)
static void S_SetProperty(FMcpSchemaBuilder& B)
{
	B.String(TEXT("objectPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .String(TEXT("propertyPath"), TEXT(""))
	 // Discriminated value: populate exactly ONE typed field matching the target property.
	 .Bool(TEXT("boolValue"), TEXT("Set a bool property."))
	 .Integer(TEXT("intValue"), TEXT("Set an integer property."))
	 .Number(TEXT("floatValue"), TEXT("Set a float/double property."))
	 .String(TEXT("stringValue"), TEXT("Set a string / name / text / enum / object-reference-by-path property."))
	 .Object(TEXT("colorValue"), TEXT("Set an FLinearColor/FColor property (r,g,b,a, 0..1)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Object(TEXT("vectorValue"), TEXT("Set an FVector property (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("structValue"), TEXT("Set a struct / instanced subobject ({\"__class\",...}) / map property."))
	 .Array(TEXT("arrayValue"), TEXT("Set an array property (items are structs/instanced objects)."), TEXT("object"))
	 .Bool(TEXT("markDirty"), TEXT("set_property: mark the owning package dirty (default true)."))
	 .Bool(TEXT("save"), TEXT("set_property: persist the owning asset package to disk after the write (default true; defaults to false when markDirty is false). Level packages are never auto-saved — use control_editor save_all."))
	 .RequiredAnyOf({TEXT("propertyName"), TEXT("propertyPath")});
}

static void S_GetProperty(FMcpSchemaBuilder& B)
{
	B.String(TEXT("objectPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .String(TEXT("propertyPath"), TEXT(""));
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row: the global bodies read GEditor-owned
// state and every delegation target resolves through editor-world lookups. Each
// class AUTHORS its schema fragment (AppendSchema) and derives its validation
// decl from the same fragment (McpDeriveDecl), so the two cannot drift.

#define MCP_INSPECT_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                \
class FMcpCall_Inspect_##ClassSuffix final : public FMcpCall                              \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("inspect"),                     \
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

// Actor actions: normalize the target aliases, then delegate to the shared
// control_actor handlers (typed direct calls — migration rule 5).
#define MCP_INSPECT_ACTOR_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)         \
class FMcpCall_Inspect_##ClassSuffix final : public FMcpCall                              \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("inspect"),                     \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),            \
			&S_##ClassSuffix);                                                            \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		NormalizeActorAlias(Payload, /*bAlsoObjectPath*/ false);                          \
		return HandlerFn(S, RequestId, Payload, Socket);                                   \
	}                                                                                     \
};

// The property pair: aliases also fill objectPath, and the 4-arg property
// handlers dispatch on their internal set_object_property/get_object_property
// keys — the literals are load-bearing.
#define MCP_INSPECT_PROP_CALL(ClassSuffix, ActionLiteral, HandlerFn, InternalLiteral, ExtraFlags) \
class FMcpCall_Inspect_##ClassSuffix final : public FMcpCall                              \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("inspect"),                     \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),            \
			&S_##ClassSuffix);                                                            \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		NormalizeActorAlias(Payload, /*bAlsoObjectPath*/ true);                           \
		return HandlerFn(S, RequestId, TEXT(InternalLiteral), Payload, Socket);            \
	}                                                                                     \
};

// Global reads (EnvironmentHandlers.cpp)
MCP_INSPECT_CALL(FindObjects, "find_objects", McpHandlers::Inspect::HandleInspectFindObjects, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetProjectSettings, "get_project_settings", McpHandlers::Inspect::HandleInspectGetProjectSettings, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetEditorSettings, "get_editor_settings", McpHandlers::Inspect::HandleInspectGetEditorSettings, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetWorldSettings, "get_world_settings", McpHandlers::Inspect::HandleInspectGetWorldSettings, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetViewportInfo, "get_viewport_info", McpHandlers::Inspect::HandleInspectGetViewportInfo, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetSelectedActors, "get_selected_actors", McpHandlers::Inspect::HandleInspectGetSelectedActors, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetSceneStats, "get_scene_stats", McpHandlers::Inspect::HandleInspectGetSceneStats, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetPerformanceStats, "get_performance_stats", McpHandlers::Inspect::HandleInspectGetPerformanceStats, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetMemoryStats, "get_memory_stats", McpHandlers::Inspect::HandleInspectGetMemoryStats, EMcpCallFlags::None)
MCP_INSPECT_CALL(ListObjects, "list_objects", McpHandlers::Inspect::HandleInspectListObjects, EMcpCallFlags::None)
MCP_INSPECT_CALL(FindByClass, "find_by_class", McpHandlers::Inspect::HandleInspectFindByClass, EMcpCallFlags::None)
MCP_INSPECT_CALL(FindByTag, "find_by_tag", McpHandlers::Inspect::HandleInspectFindByTag, EMcpCallFlags::None)
MCP_INSPECT_CALL(InspectClass, "inspect_class", McpHandlers::Inspect::HandleInspectClassInfo, EMcpCallFlags::None)
MCP_INSPECT_CALL(RuntimeReport, "runtime_report", McpHandlers::Inspect::HandleInspectRuntimeReport, EMcpCallFlags::None)
MCP_INSPECT_CALL(PieReport, "pie_report", McpHandlers::Inspect::HandleInspectRuntimeReport, EMcpCallFlags::None)

// Self-contained delegates (FocusInputHandlers.cpp / PropertyHandlers.cpp)
MCP_INSPECT_CALL(UiFocus, "ui_focus", McpHandlers::Inspect::HandleInspectUiFocus, EMcpCallFlags::None)
MCP_INSPECT_CALL(DiffAsset, "diff_asset", McpHandlers::Inspect::HandleDiffAssetAction, EMcpCallFlags::None)
MCP_INSPECT_CALL(InspectCdo, "inspect_cdo", McpHandlers::Inspect::HandleInspectCdoAction, EMcpCallFlags::None)

// Object detail (one generic body, identical output for all eight)
MCP_INSPECT_CALL(InspectObject, "inspect_object", McpHandlers::Inspect::HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetActorDetails, "get_actor_details", McpHandlers::Inspect::HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetBlueprintDetails, "get_blueprint_details", McpHandlers::Inspect::HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetComponentDetails, "get_component_details", McpHandlers::Inspect::HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetLevelDetails, "get_level_details", McpHandlers::Inspect::HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetMaterialDetails, "get_material_details", McpHandlers::Inspect::HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetMeshDetails, "get_mesh_details", McpHandlers::Inspect::HandleInspectObjectGeneric, EMcpCallFlags::None)
MCP_INSPECT_CALL(GetTextureDetails, "get_texture_details", McpHandlers::Inspect::HandleInspectObjectGeneric, EMcpCallFlags::None)

// Actor actions (shared control_actor handlers, ControlHandlers.cpp)
MCP_INSPECT_ACTOR_CALL(GetComponents, "get_components", McpHandlers::ControlActor::HandleControlActorGetComponents, EMcpCallFlags::None)
MCP_INSPECT_ACTOR_CALL(GetComponentProperty, "get_component_property", McpHandlers::ControlActor::HandleControlActorGetComponentProperty, EMcpCallFlags::None)
MCP_INSPECT_ACTOR_CALL(SetComponentProperty, "set_component_property", McpHandlers::ControlActor::HandleControlActorSetComponentProperties, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(GetMetadata, "get_metadata", McpHandlers::ControlActor::HandleControlActorGetMetadata, EMcpCallFlags::None)
MCP_INSPECT_ACTOR_CALL(AddTag, "add_tag", McpHandlers::ControlActor::HandleControlActorAddTag, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(CreateSnapshot, "create_snapshot", McpHandlers::ControlActor::HandleControlActorCreateSnapshot, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(RestoreSnapshot, "restore_snapshot", McpHandlers::ControlActor::HandleControlActorRestoreSnapshot, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(Export, "export", McpHandlers::ControlActor::HandleControlActorExport, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(DeleteObject, "delete_object", McpHandlers::ControlActor::HandleControlActorDelete, EMcpCallFlags::Mutating)
MCP_INSPECT_ACTOR_CALL(GetBoundingBox, "get_bounding_box", McpHandlers::ControlActor::HandleControlActorGetBoundingBox, EMcpCallFlags::None)

// Property pair (PropertyHandlers.cpp)
MCP_INSPECT_PROP_CALL(SetProperty, "set_property", McpHandlers::Inspect::HandleSetObjectProperty, "set_object_property", EMcpCallFlags::Mutating)
MCP_INSPECT_PROP_CALL(GetProperty, "get_property", McpHandlers::Inspect::HandleGetObjectProperty, "get_object_property", EMcpCallFlags::None)

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
