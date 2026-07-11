// LINT-TOOL: manage_level
// LINT-SCHEMA-DERIVED
// manage_level as FMcpCall classes — adopts schema-from-decls
// (docs/action-declarations.md). Each class AUTHORS its schema fragment in a
// S_<Suffix>() function; the published facade schema folds those fragments and
// GetDecl() derives the validation decl from the same fragment via
// McpDeriveDecl(), so schema and decl are one source and cannot drift. Run()
// delegates to the subsystem member handlers until the module split de-members
// those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_LevelHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageLevel
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. Actions that accept
// one-of-several params (delete's levelPath/path, export_level's exportPath/
// destinationPath, rename_level's destinationPath/newName, ...) author each
// optional: the requirement is "at least one", which the decl vocabulary cannot
// express and the handler enforces itself. unload does not author
// shouldBeLoaded/shouldBeVisible — the handler force-overrides both to false,
// so sending them is inert.

static void S_Load(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelPath"), TEXT("Level asset path."))
	 .Bool(TEXT("saveDirtyPackages"), TEXT("load: save dirty world/content "
		"packages before loading (required in headless/unattended mode)."))
	 .Required({TEXT("levelPath")});
}

static void S_Save(FMcpSchemaBuilder&) {}

static void S_SaveAs(FMcpSchemaBuilder& B)
{
	B.String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .Required({TEXT("savePath")});
}

static void S_CreateLevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelName"),
		TEXT("create_level: name for the new level (path separators are "
			"stripped; saved under levelPath, or /Game/Maps for a bare name). "
			"stream/unload: target streaming level name (levelPath used when "
			"omitted)."))
	 .String(TEXT("levelPath"), TEXT("Level asset path."))
	 .Bool(TEXT("useWorldPartition"),
		TEXT("create_level: create the level with World Partition enabled "
			"(default false)."));
}

static void S_Stream(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelName"),
		TEXT("create_level: name for the new level (path separators are "
			"stripped; saved under levelPath, or /Game/Maps for a bare name). "
			"stream/unload: target streaming level name (levelPath used when "
			"omitted)."))
	 .String(TEXT("levelPath"), TEXT("Level asset path."))
	 .Bool(TEXT("shouldBeLoaded"),
		TEXT("stream: whether the streaming level should be loaded "
			"(default true)."))
	 .Bool(TEXT("shouldBeVisible"),
		TEXT("stream: whether the streaming level should be visible "
			"(default true)."));
}

static void S_Unload(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelName"),
		TEXT("create_level: name for the new level (path separators are "
			"stripped; saved under levelPath, or /Game/Maps for a bare name). "
			"stream/unload: target streaming level name (levelPath used when "
			"omitted)."))
	 .String(TEXT("levelPath"), TEXT("Level asset path."));
}

static void S_BuildLighting(FMcpSchemaBuilder& B)
{
	B.String(TEXT("quality"), TEXT("Lighting build quality label."));
}

static void S_SetMetadata(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path for metadata or validation aliases."))
	 .String(TEXT("levelPath"), TEXT("Level asset path."))
	 .Object(TEXT("metadata"), TEXT("Metadata key/value object."));
}

static void S_ExportLevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelPath"), TEXT("Level asset path."))
	 .String(TEXT("exportPath"), TEXT("Export file path."))
	 .String(TEXT("destinationPath"), TEXT("Destination path for move/copy."));
}

static void S_ImportLevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sourcePath"), TEXT("Source path for import/move/copy."))
	 .String(TEXT("packagePath"), TEXT("Path to a directory."))
	 .String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
	 .Bool(TEXT("overwrite"), TEXT("Allow replacing an existing destination level."));
}

static void S_AddSublevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sublevelPath"), TEXT("Level asset path alias for add_sublevel."))
	 .String(TEXT("levelPath"), TEXT("Level asset path."))
	 .String(TEXT("streamingMethod"),
		TEXT("add_sublevel: 'AlwaysLoaded' adds the sublevel always-loaded; "
			"any other value (default 'Blueprint') adds it as a dynamic "
			"streaming level."));
}

static void S_Delete(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelPath"), TEXT("Level asset path."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."));
}

static void S_RenameLevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelPath"), TEXT("Level asset path."))
	 .String(TEXT("sourcePath"), TEXT("Source path for import/move/copy."))
	 .String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
	 .String(TEXT("newName"), TEXT("New asset name for rename operations."))
	 .Bool(TEXT("overwrite"), TEXT("Allow replacing an existing destination level."));
}

static void S_DuplicateLevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("sourcePath"), TEXT("Source path for import/move/copy."))
	 .String(TEXT("levelPath"), TEXT("Level asset path."))
	 .String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
	 .Bool(TEXT("overwrite"), TEXT("Allow replacing an existing destination level."))
	 .Required({TEXT("destinationPath")});
}

static void S_ListLevels(FMcpSchemaBuilder&) {}

static void S_GetSummary(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelPath"), TEXT("Level asset path."))
	 .String(TEXT("level_path"), TEXT("get_summary: snake_case alias for levelPath."));
}

static void S_GetCurrentLevel(FMcpSchemaBuilder&) {}

static void S_ValidateLevel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("levelPath"), TEXT("Level asset path."))
	 .String(TEXT("assetPath"), TEXT("Asset path for metadata or validation aliases."));
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are authored per action: RequiresEditor on every action (the chain was
// whole-editor-gated), Mutating on the writers only; the readers (list_levels,
// get_summary, get_current_level, validate_level) stay unflagged.

#define MCP_ML_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                     \
class FMcpCall_ManageLevel_##ClassSuffix final : public FMcpCall                          \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_level"),                \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),           \
			&S_##ClassSuffix);                                                            \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return HandlerFn(S, RequestId, Payload, Socket);                                  \
	}                                                                                     \
};

MCP_ML_CALL(Load, "load", McpHandlers::Level::HandleLevelLoad, EMcpCallFlags::Mutating)
MCP_ML_CALL(Save, "save", McpHandlers::Level::HandleLevelSave, EMcpCallFlags::Mutating)
MCP_ML_CALL(SaveAs, "save_as", McpHandlers::Level::HandleLevelSaveAs, EMcpCallFlags::Mutating)
MCP_ML_CALL(CreateLevel, "create_level", McpHandlers::Level::HandleLevelCreate, EMcpCallFlags::Mutating)
MCP_ML_CALL(Stream, "stream", McpHandlers::Level::HandleLevelStream, EMcpCallFlags::Mutating)
MCP_ML_CALL(Unload, "unload", McpHandlers::Level::HandleLevelUnload, EMcpCallFlags::Mutating)
MCP_ML_CALL(BuildLighting, "build_lighting", McpHandlers::Level::HandleLevelBuildLighting, EMcpCallFlags::Mutating)
MCP_ML_CALL(SetMetadata, "set_metadata", McpHandlers::Level::HandleLevelSetMetadata, EMcpCallFlags::Mutating)
MCP_ML_CALL(ExportLevel, "export_level", McpHandlers::Level::HandleLevelExport, EMcpCallFlags::Mutating)
MCP_ML_CALL(ImportLevel, "import_level", McpHandlers::Level::HandleLevelImport, EMcpCallFlags::Mutating)
MCP_ML_CALL(AddSublevel, "add_sublevel", McpHandlers::Level::HandleLevelAddSublevel, EMcpCallFlags::Mutating)
MCP_ML_CALL(Delete, "delete", McpHandlers::Level::HandleLevelDelete, EMcpCallFlags::Mutating)
MCP_ML_CALL(RenameLevel, "rename_level", McpHandlers::Level::HandleLevelRename, EMcpCallFlags::Mutating)
MCP_ML_CALL(DuplicateLevel, "duplicate_level", McpHandlers::Level::HandleLevelDuplicate, EMcpCallFlags::Mutating)
MCP_ML_CALL(ListLevels, "list_levels", McpHandlers::Level::HandleLevelList, EMcpCallFlags::None)
MCP_ML_CALL(GetSummary, "get_summary", McpHandlers::Level::HandleLevelGetSummary, EMcpCallFlags::None)
MCP_ML_CALL(GetCurrentLevel, "get_current_level", McpHandlers::Level::HandleLevelGetCurrent, EMcpCallFlags::None)
MCP_ML_CALL(ValidateLevel, "validate_level", McpHandlers::Level::HandleLevelValidate, EMcpCallFlags::None)

#undef MCP_ML_CALL

} // namespace McpCalls::ManageLevel

void McpRegisterManageLevelCalls()
{
	using namespace McpCalls::ManageLevel;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_Load>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_Save>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_SaveAs>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_CreateLevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_Stream>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_Unload>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_BuildLighting>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_SetMetadata>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_ExportLevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_ImportLevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_AddSublevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_Delete>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_RenameLevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_DuplicateLevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_ListLevels>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_GetSummary>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_GetCurrentLevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageLevel_ValidateLevel>());
}
