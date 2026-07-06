// LINT-TOOL: manage_level
// manage_level as FMcpCall classes — fourth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageLevel
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Authored from the handler bodies' actual payload reads
// (McpAutomationBridge_LevelHandlers.cpp; create_light delegates to the full
// implementation in McpAutomationBridge_LightingHandlers.cpp). Actions that
// accept one-of-several params (delete's levelPath/path, export_level's
// exportPath/destinationPath, rename_level's destinationPath/newName, ...)
// declare each optional: the requirement is "at least one", which the decl
// vocabulary cannot express and the handler enforces itself. unload does not
// declare shouldBeLoaded/shouldBeVisible — the handler force-overrides both
// to false, so sending them is inert.

inline const FMcpParamDecl P_Load[] = { { TEXT("levelPath"), EMcpParamKind::String, true }, { TEXT("saveDirtyPackages"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SaveAs[] = { { TEXT("savePath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateLevel[] = { { TEXT("levelName"), EMcpParamKind::String, false }, { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("useWorldPartition"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_Stream[] = { { TEXT("levelName"), EMcpParamKind::String, false }, { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("shouldBeLoaded"), EMcpParamKind::Bool, false }, { TEXT("shouldBeVisible"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_Unload[] = { { TEXT("levelName"), EMcpParamKind::String, false }, { TEXT("levelPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BuildLighting[] = { { TEXT("quality"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetMetadata[] = { { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("metadata"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_Validate[] = { { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Export[] = { { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("exportPath"), EMcpParamKind::String, false }, { TEXT("destinationPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Import[] = { { TEXT("sourcePath"), EMcpParamKind::String, false }, { TEXT("packagePath"), EMcpParamKind::String, false }, { TEXT("destinationPath"), EMcpParamKind::String, false }, { TEXT("overwrite"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddSublevel[] = { { TEXT("sublevelPath"), EMcpParamKind::String, false }, { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("streamingMethod"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Delete[] = { { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Rename[] = { { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("sourcePath"), EMcpParamKind::String, false }, { TEXT("destinationPath"), EMcpParamKind::String, false }, { TEXT("newName"), EMcpParamKind::String, false }, { TEXT("overwrite"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_Duplicate[] = { { TEXT("sourcePath"), EMcpParamKind::String, false }, { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("destinationPath"), EMcpParamKind::String, true }, { TEXT("overwrite"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetSummary[] = { { TEXT("levelPath"), EMcpParamKind::String, false }, { TEXT("level_path"), EMcpParamKind::String, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────

#define MCP_ML_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)       \
class FMcpCall_ManageLevel_##ClassSuffix final : public FMcpCall                          \
{                                                                                         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl Decl{ TEXT("manage_level"), TEXT(ActionLiteral),        \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                  \
		return Decl;                                                                      \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return S.HandlerFn(RequestId, Payload, Socket);                                   \
	}                                                                                     \
};

MCP_ML_CALL(Load, "load", P_Load, HandleLevelLoad, EMcpCallFlags::Mutating)
MCP_ML_CALL(Save, "save", {}, HandleLevelSave, EMcpCallFlags::Mutating)
MCP_ML_CALL(SaveAs, "save_as", P_SaveAs, HandleLevelSaveAs, EMcpCallFlags::Mutating)
MCP_ML_CALL(CreateLevel, "create_level", P_CreateLevel, HandleLevelCreate, EMcpCallFlags::Mutating)
MCP_ML_CALL(Stream, "stream", P_Stream, HandleLevelStream, EMcpCallFlags::Mutating)
MCP_ML_CALL(Unload, "unload", P_Unload, HandleLevelUnload, EMcpCallFlags::Mutating)
MCP_ML_CALL(BuildLighting, "build_lighting", P_BuildLighting, HandleLevelBuildLighting, EMcpCallFlags::Mutating)
MCP_ML_CALL(SetMetadata, "set_metadata", P_SetMetadata, HandleLevelSetMetadata, EMcpCallFlags::Mutating)
MCP_ML_CALL(ExportLevel, "export_level", P_Export, HandleLevelExport, EMcpCallFlags::Mutating)
MCP_ML_CALL(ImportLevel, "import_level", P_Import, HandleLevelImport, EMcpCallFlags::Mutating)
MCP_ML_CALL(AddSublevel, "add_sublevel", P_AddSublevel, HandleLevelAddSublevel, EMcpCallFlags::Mutating)
MCP_ML_CALL(Delete, "delete", P_Delete, HandleLevelDelete, EMcpCallFlags::Mutating)
MCP_ML_CALL(RenameLevel, "rename_level", P_Rename, HandleLevelRename, EMcpCallFlags::Mutating)
MCP_ML_CALL(DuplicateLevel, "duplicate_level", P_Duplicate, HandleLevelDuplicate, EMcpCallFlags::Mutating)
MCP_ML_CALL(ListLevels, "list_levels", {}, HandleLevelList, EMcpCallFlags::None)
MCP_ML_CALL(GetSummary, "get_summary", P_GetSummary, HandleLevelGetSummary, EMcpCallFlags::None)
MCP_ML_CALL(GetCurrentLevel, "get_current_level", {}, HandleLevelGetCurrent, EMcpCallFlags::None)
MCP_ML_CALL(ValidateLevel, "validate_level", P_Validate, HandleLevelValidate, EMcpCallFlags::None)

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
