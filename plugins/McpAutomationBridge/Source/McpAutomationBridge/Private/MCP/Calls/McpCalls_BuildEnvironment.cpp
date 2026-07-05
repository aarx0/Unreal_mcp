// LINT-TOOL: build_environment
// build_environment as FMcpCall classes — fifteenth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers — HandleEnvironment* (EnvironmentHandlers.cpp) for the 21
// core actions, HandleLighting* (LightingHandlers.cpp) for the 12 lighting
// actions, HandleSpline* (SplineHandlers.cpp) for the 22 spline actions —
// until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::BuildEnvironment
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations
// (McpDecl_BuildEnvironment.h) and re-verified against the member bodies.
// Two rows shipped with decl fixes at classing:
// create_landscape_grass_type declared BOTH meshPath AND staticMesh required,
// but the handler resolves them as a one-of alias pair (meshPath first,
// staticMesh fallback) and rejects only when neither is present
// ("meshPath or staticMesh required"), so requiring both false-rejected
// every one-spelling payload — both optional now with the at-least-one
// requirement handler-enforced (name stays required). modify_heightmap
// declared heightData required, but the handler requires it only for the
// 'set' operation ("heightData array required for 'set' operation") and
// serves raise/lower/flatten without it — optional now with the
// per-operation requirement handler-enforced.

inline const FMcpParamDecl P_AddFoliageInstances[] = { { TEXT("foliageTypePath"), EMcpParamKind::String, false }, { TEXT("foliageType"), EMcpParamKind::String, false }, { TEXT("transforms"), EMcpParamKind::Array, false }, { TEXT("locations"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_GetFoliageInstances[] = { { TEXT("foliageType"), EMcpParamKind::String, false }, { TEXT("foliageTypePath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_RemoveFoliage[] = { { TEXT("foliageType"), EMcpParamKind::String, false }, { TEXT("removeAll"), EMcpParamKind::Bool, false }, { TEXT("foliageTypePath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_PaintFoliage[] = { { TEXT("foliageTypePath"), EMcpParamKind::String, false }, { TEXT("foliageType"), EMcpParamKind::String, false }, { TEXT("locations"), EMcpParamKind::Array, false }, { TEXT("location"), EMcpParamKind::Any, false }, { TEXT("position"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateProceduralFoliage[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("bounds"), EMcpParamKind::Object, false }, { TEXT("foliageTypes"), EMcpParamKind::Array, false }, { TEXT("types"), EMcpParamKind::Array, false }, { TEXT("seed"), EMcpParamKind::Number, false }, { TEXT("tileSize"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateProceduralTerrain[] = { { TEXT("sizeX"), EMcpParamKind::Number, false }, { TEXT("sizeY"), EMcpParamKind::Number, false }, { TEXT("spacing"), EMcpParamKind::Number, false }, { TEXT("heightScale"), EMcpParamKind::Number, false }, { TEXT("subdivisions"), EMcpParamKind::Number, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("material"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddFoliage[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("meshPath"), EMcpParamKind::String, true }, { TEXT("density"), EMcpParamKind::Number, false }, { TEXT("minScale"), EMcpParamKind::Number, false }, { TEXT("maxScale"), EMcpParamKind::Number, false }, { TEXT("alignToNormal"), EMcpParamKind::Bool, false }, { TEXT("randomYaw"), EMcpParamKind::Bool, false }, { TEXT("cullDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateLandscape[] = { { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("z"), EMcpParamKind::Number, false }, { TEXT("location"), EMcpParamKind::Any, false }, { TEXT("componentsX"), EMcpParamKind::Number, false }, { TEXT("componentsY"), EMcpParamKind::Number, false }, { TEXT("componentCount"), EMcpParamKind::Any, false }, { TEXT("sizeX"), EMcpParamKind::Number, false }, { TEXT("sizeY"), EMcpParamKind::Number, false }, { TEXT("quadsPerComponent"), EMcpParamKind::Number, false }, { TEXT("quadsPerSection"), EMcpParamKind::Number, false }, { TEXT("sectionSize"), EMcpParamKind::Number, false }, { TEXT("sectionsPerComponent"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("landscapeName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_PaintLandscape[] = { { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("landscapeName"), EMcpParamKind::String, false }, { TEXT("layerName"), EMcpParamKind::String, true }, { TEXT("region"), EMcpParamKind::Object, false }, { TEXT("minX"), EMcpParamKind::Number, false }, { TEXT("minY"), EMcpParamKind::Number, false }, { TEXT("maxX"), EMcpParamKind::Number, false }, { TEXT("maxY"), EMcpParamKind::Number, false }, { TEXT("strength"), EMcpParamKind::Number, false }, { TEXT("skipFlush"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_Sculpt[] = { { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("landscapeName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("position"), EMcpParamKind::Object, false }, { TEXT("toolMode"), EMcpParamKind::String, false }, { TEXT("tool"), EMcpParamKind::String, false }, { TEXT("brushRadius"), EMcpParamKind::Number, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("brushFalloff"), EMcpParamKind::Number, false }, { TEXT("falloff"), EMcpParamKind::Number, false }, { TEXT("strength"), EMcpParamKind::Number, false }, { TEXT("skipFlush"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ModifyHeightmap[] = { { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("landscapeName"), EMcpParamKind::String, false }, { TEXT("operation"), EMcpParamKind::String, false }, { TEXT("region"), EMcpParamKind::Object, false }, { TEXT("minX"), EMcpParamKind::Number, false }, { TEXT("minY"), EMcpParamKind::Number, false }, { TEXT("maxX"), EMcpParamKind::Number, false }, { TEXT("maxY"), EMcpParamKind::Number, false }, { TEXT("heightData"), EMcpParamKind::Array, false }, { TEXT("skipFlush"), EMcpParamKind::Bool, false }, { TEXT("updateNormals"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetLandscapeMaterial[] = { { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("landscapeName"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateLandscapeGrassType[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("meshPath"), EMcpParamKind::String, false }, { TEXT("staticMesh"), EMcpParamKind::String, false }, { TEXT("density"), EMcpParamKind::Number, false }, { TEXT("minScale"), EMcpParamKind::Number, false }, { TEXT("maxScale"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_GenerateLods[] = { { TEXT("landscapePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("assetPaths"), EMcpParamKind::Array, false }, { TEXT("assets"), EMcpParamKind::Array, false }, { TEXT("lodCount"), EMcpParamKind::Number, false }, { TEXT("numLODs"), EMcpParamKind::Number, false }, { TEXT("reductionSettings"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_BakeLightmap[] = { { TEXT("quality"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ExportSnapshot[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ImportSnapshot[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_Delete[] = { { TEXT("names"), EMcpParamKind::Array, true } };
inline const FMcpParamDecl P_CreateSkySphere[] = { { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetTimeOfDay[] = { { TEXT("time"), EMcpParamKind::Number, false }, { TEXT("hour"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateFogVolume[] = { { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("x"), EMcpParamKind::Number, false }, { TEXT("y"), EMcpParamKind::Number, false }, { TEXT("z"), EMcpParamKind::Number, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateLight[] = { { TEXT("lightClass"), EMcpParamKind::String, false }, { TEXT("lightType"), EMcpParamKind::String, false }, { TEXT("type"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateSkyLight[] = { { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("sourceType"), EMcpParamKind::String, false }, { TEXT("cubemapPath"), EMcpParamKind::String, false }, { TEXT("intensity"), EMcpParamKind::Number, false }, { TEXT("recapture"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_BuildLighting[] = { { TEXT("quality"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_EnsureSingleSkyLight[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("recapture"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateLightmassVolume[] = { { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("size"), EMcpParamKind::Object, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetupVolumetricFog[] = { { TEXT("viewDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetupGlobalIllumination[] = { { TEXT("method"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureShadows[] = { { TEXT("virtualShadowMaps"), EMcpParamKind::Bool, false }, { TEXT("rayTracedShadows"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetExposure[] = { { TEXT("minBrightness"), EMcpParamKind::Number, false }, { TEXT("maxBrightness"), EMcpParamKind::Number, false }, { TEXT("compensationValue"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetAmbientOcclusion[] = { { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("intensity"), EMcpParamKind::Number, false }, { TEXT("radius"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateLightingEnabledLevel[] = { { TEXT("path"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateSplineActor[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("rotation"), EMcpParamKind::Object, false }, { TEXT("bClosedLoop"), EMcpParamKind::Bool, false }, { TEXT("splineType"), EMcpParamKind::String, false }, { TEXT("points"), EMcpParamKind::Array, false }, { TEXT("initialPoints"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_AddSplinePoint[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("position"), EMcpParamKind::Object, false }, { TEXT("index"), EMcpParamKind::Number, false }, { TEXT("pointType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_RemoveSplinePoint[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("pointIndex"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetSplinePointPosition[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("pointIndex"), EMcpParamKind::Number, false }, { TEXT("position"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_SetSplinePointTangents[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("pointIndex"), EMcpParamKind::Number, false }, { TEXT("arriveTangent"), EMcpParamKind::Object, false }, { TEXT("leaveTangent"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_SetSplinePointRotation[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("pointIndex"), EMcpParamKind::Number, false }, { TEXT("pointRotation"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_SetSplinePointScale[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("pointIndex"), EMcpParamKind::Number, false }, { TEXT("pointScale"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_SetSplineType[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("splineType"), EMcpParamKind::String, false }, { TEXT("pointIndex"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateSplineMeshComponent[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("meshPath"), EMcpParamKind::String, false }, { TEXT("forwardAxis"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetSplineMeshAsset[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("meshPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ConfigureSplineMeshAxis[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("forwardAxis"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetSplineMeshMaterial[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("materialPath"), EMcpParamKind::String, true }, { TEXT("materialIndex"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ScatterMeshesAlongSpline[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("meshPath"), EMcpParamKind::String, true }, { TEXT("alignToSpline"), EMcpParamKind::Bool, false }, { TEXT("spacing"), EMcpParamKind::Number, false }, { TEXT("useRandomOffset"), EMcpParamKind::Bool, false }, { TEXT("randomOffsetRange"), EMcpParamKind::Number, false }, { TEXT("randomizeScale"), EMcpParamKind::Bool, false }, { TEXT("minScale"), EMcpParamKind::Number, false }, { TEXT("maxScale"), EMcpParamKind::Number, false }, { TEXT("randomizeRotation"), EMcpParamKind::Bool, false }, { TEXT("rotationRange"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureMeshSpacing[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("spacing"), EMcpParamKind::Number, false }, { TEXT("useRandomOffset"), EMcpParamKind::Bool, false }, { TEXT("randomOffsetRange"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureMeshRandomization[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("randomizeScale"), EMcpParamKind::Bool, false }, { TEXT("minScale"), EMcpParamKind::Number, false }, { TEXT("maxScale"), EMcpParamKind::Number, false }, { TEXT("randomizeRotation"), EMcpParamKind::Bool, false }, { TEXT("rotationRange"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateRoadSpline[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateRiverSpline[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateFenceSpline[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateWallSpline[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateCableSpline[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreatePipeSpline[] = { { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("materialPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetSplinesInfo[] = { { TEXT("actorName"), EMcpParamKind::String, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// Flags are authored per action and deliberately mixed. RequiresEditor on
// the 12 Lighting and 22 Spline actions (both chains were whole-editor-gated
// and the members answer their editor-build stubs in non-editor builds) and
// on the six core inline extractions (their bodies sat inside the retired
// dispatcher's editor-gated region and answer its stub); NOT on the 15
// delegating core actions — the retired dispatcher forwarded them outside
// any editor gate, and each 4-arg member self-gates with its own stub.
// Mutating on the writers only — bake_lightmap and build_lighting both kick
// a lighting build; the readers are get_foliage_instances, import_snapshot
// (reads a snapshot file back, writes nothing), list_light_types, and
// get_splines_info.

#define MCP_BE_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, Flags)           \
class FMcpCall_BuildEnvironment_##ClassSuffix final : public FMcpCall                    \
{                                                                                        \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl Decl{ TEXT("build_environment"), TEXT(ActionLiteral),  \
			ParamsArray, (Flags) };                                                      \
		return Decl;                                                                     \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

// Foliage (delegating wrappers @ EnvironmentHandlers.cpp)
MCP_BE_CALL(AddFoliageInstances, "add_foliage_instances", P_AddFoliageInstances, HandleEnvironmentAddFoliageInstances, EMcpCallFlags::Mutating)
MCP_BE_CALL(GetFoliageInstances, "get_foliage_instances", P_GetFoliageInstances, HandleEnvironmentGetFoliageInstances, EMcpCallFlags::None)
MCP_BE_CALL(RemoveFoliage, "remove_foliage", P_RemoveFoliage, HandleEnvironmentRemoveFoliage, EMcpCallFlags::Mutating)
MCP_BE_CALL(PaintFoliage, "paint_foliage", P_PaintFoliage, HandleEnvironmentPaintFoliage, EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateProceduralFoliage, "create_procedural_foliage", P_CreateProceduralFoliage, HandleEnvironmentCreateProceduralFoliage, EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateProceduralTerrain, "create_procedural_terrain", P_CreateProceduralTerrain, HandleEnvironmentCreateProceduralTerrain, EMcpCallFlags::Mutating)
MCP_BE_CALL(AddFoliage, "add_foliage", P_AddFoliage, HandleEnvironmentAddFoliage, EMcpCallFlags::Mutating)
// Landscape
MCP_BE_CALL(CreateLandscape, "create_landscape", P_CreateLandscape, HandleEnvironmentCreateLandscape, EMcpCallFlags::Mutating)
MCP_BE_CALL(PaintLandscape, "paint_landscape", P_PaintLandscape, HandleEnvironmentPaintLandscape, EMcpCallFlags::Mutating)
MCP_BE_CALL(Sculpt, "sculpt", P_Sculpt, HandleEnvironmentSculpt, EMcpCallFlags::Mutating)
MCP_BE_CALL(ModifyHeightmap, "modify_heightmap", P_ModifyHeightmap, HandleEnvironmentModifyHeightmap, EMcpCallFlags::Mutating)
MCP_BE_CALL(SetLandscapeMaterial, "set_landscape_material", P_SetLandscapeMaterial, HandleEnvironmentSetLandscapeMaterial, EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateLandscapeGrassType, "create_landscape_grass_type", P_CreateLandscapeGrassType, HandleEnvironmentCreateLandscapeGrassType, EMcpCallFlags::Mutating)
MCP_BE_CALL(GenerateLods, "generate_lods", P_GenerateLods, HandleEnvironmentGenerateLODs, EMcpCallFlags::Mutating)
MCP_BE_CALL(BakeLightmap, "bake_lightmap", P_BakeLightmap, HandleEnvironmentBakeLightmap, EMcpCallFlags::Mutating)

// Environment inline extractions (EnvironmentHandlers.cpp)
MCP_BE_CALL(ExportSnapshot, "export_snapshot", P_ExportSnapshot, HandleEnvironmentExportSnapshot, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(ImportSnapshot, "import_snapshot", P_ImportSnapshot, HandleEnvironmentImportSnapshot, EMcpCallFlags::RequiresEditor)
MCP_BE_CALL(Delete, "delete", P_Delete, HandleEnvironmentDelete, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateSkySphere, "create_sky_sphere", P_CreateSkySphere, HandleEnvironmentCreateSkySphere, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetTimeOfDay, "set_time_of_day", P_SetTimeOfDay, HandleEnvironmentSetTimeOfDay, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateFogVolume, "create_fog_volume", P_CreateFogVolume, HandleEnvironmentCreateFogVolume, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Lighting (LightingHandlers.cpp)
MCP_BE_CALL(ListLightTypes, "list_light_types", {}, HandleLightingListLightTypes, EMcpCallFlags::RequiresEditor)
MCP_BE_CALL(CreateLight, "create_light", P_CreateLight, HandleLightingCreateLight, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateSkyLight, "create_sky_light", P_CreateSkyLight, HandleLightingCreateSkyLight, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(BuildLighting, "build_lighting", P_BuildLighting, HandleLightingBuildLighting, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(EnsureSingleSkyLight, "ensure_single_sky_light", P_EnsureSingleSkyLight, HandleLightingEnsureSingleSkyLight, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateLightmassVolume, "create_lightmass_volume", P_CreateLightmassVolume, HandleLightingCreateLightmassVolume, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetupVolumetricFog, "setup_volumetric_fog", P_SetupVolumetricFog, HandleLightingSetupVolumetricFog, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetupGlobalIllumination, "setup_global_illumination", P_SetupGlobalIllumination, HandleLightingSetupGlobalIllumination, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(ConfigureShadows, "configure_shadows", P_ConfigureShadows, HandleLightingConfigureShadows, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetExposure, "set_exposure", P_SetExposure, HandleLightingSetExposure, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetAmbientOcclusion, "set_ambient_occlusion", P_SetAmbientOcclusion, HandleLightingSetAmbientOcclusion, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateLightingEnabledLevel, "create_lighting_enabled_level", P_CreateLightingEnabledLevel, HandleLightingCreateLightingEnabledLevel, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Spline creation & points (SplineHandlers.cpp)
MCP_BE_CALL(CreateSplineActor, "create_spline_actor", P_CreateSplineActor, HandleSplineCreateSplineActor, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(AddSplinePoint, "add_spline_point", P_AddSplinePoint, HandleSplineAddSplinePoint, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(RemoveSplinePoint, "remove_spline_point", P_RemoveSplinePoint, HandleSplineRemoveSplinePoint, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplinePointPosition, "set_spline_point_position", P_SetSplinePointPosition, HandleSplineSetSplinePointPosition, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplinePointTangents, "set_spline_point_tangents", P_SetSplinePointTangents, HandleSplineSetSplinePointTangents, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplinePointRotation, "set_spline_point_rotation", P_SetSplinePointRotation, HandleSplineSetSplinePointRotation, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplinePointScale, "set_spline_point_scale", P_SetSplinePointScale, HandleSplineSetSplinePointScale, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplineType, "set_spline_type", P_SetSplineType, HandleSplineSetSplineType, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Spline mesh
MCP_BE_CALL(CreateSplineMeshComponent, "create_spline_mesh_component", P_CreateSplineMeshComponent, HandleSplineCreateSplineMeshComponent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplineMeshAsset, "set_spline_mesh_asset", P_SetSplineMeshAsset, HandleSplineSetSplineMeshAsset, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(ConfigureSplineMeshAxis, "configure_spline_mesh_axis", P_ConfigureSplineMeshAxis, HandleSplineConfigureSplineMeshAxis, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(SetSplineMeshMaterial, "set_spline_mesh_material", P_SetSplineMeshMaterial, HandleSplineSetSplineMeshMaterial, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Mesh scattering
MCP_BE_CALL(ScatterMeshesAlongSpline, "scatter_meshes_along_spline", P_ScatterMeshesAlongSpline, HandleSplineScatterMeshesAlongSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(ConfigureMeshSpacing, "configure_mesh_spacing", P_ConfigureMeshSpacing, HandleSplineConfigureMeshSpacing, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(ConfigureMeshRandomization, "configure_mesh_randomization", P_ConfigureMeshRandomization, HandleSplineConfigureMeshRandomization, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Quick templates
MCP_BE_CALL(CreateRoadSpline, "create_road_spline", P_CreateRoadSpline, HandleSplineCreateRoadSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateRiverSpline, "create_river_spline", P_CreateRiverSpline, HandleSplineCreateRiverSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateFenceSpline, "create_fence_spline", P_CreateFenceSpline, HandleSplineCreateFenceSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateWallSpline, "create_wall_spline", P_CreateWallSpline, HandleSplineCreateWallSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreateCableSpline, "create_cable_spline", P_CreateCableSpline, HandleSplineCreateCableSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BE_CALL(CreatePipeSpline, "create_pipe_spline", P_CreatePipeSpline, HandleSplineCreatePipeSpline, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// Utility
MCP_BE_CALL(GetSplinesInfo, "get_splines_info", P_GetSplinesInfo, HandleSplineGetSplinesInfo, EMcpCallFlags::RequiresEditor)

#undef MCP_BE_CALL

} // namespace McpCalls::BuildEnvironment

void McpRegisterBuildEnvironmentCalls()
{
	using namespace McpCalls::BuildEnvironment;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_AddFoliageInstances>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_GetFoliageInstances>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_RemoveFoliage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_PaintFoliage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateProceduralFoliage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateProceduralTerrain>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_AddFoliage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateLandscape>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_PaintLandscape>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_Sculpt>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ModifyHeightmap>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetLandscapeMaterial>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateLandscapeGrassType>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_GenerateLods>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_BakeLightmap>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ExportSnapshot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ImportSnapshot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_Delete>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateSkySphere>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetTimeOfDay>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateFogVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ListLightTypes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateLight>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateSkyLight>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_BuildLighting>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_EnsureSingleSkyLight>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateLightmassVolume>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetupVolumetricFog>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetupGlobalIllumination>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ConfigureShadows>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetExposure>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetAmbientOcclusion>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateLightingEnabledLevel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateSplineActor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_AddSplinePoint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_RemoveSplinePoint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplinePointPosition>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplinePointTangents>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplinePointRotation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplinePointScale>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplineType>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateSplineMeshComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplineMeshAsset>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ConfigureSplineMeshAxis>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_SetSplineMeshMaterial>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ScatterMeshesAlongSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ConfigureMeshSpacing>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_ConfigureMeshRandomization>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateRoadSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateRiverSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateFenceSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateWallSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreateCableSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_CreatePipeSpline>());
	Registry.RegisterCall(MakeUnique<FMcpCall_BuildEnvironment_GetSplinesInfo>());
}
