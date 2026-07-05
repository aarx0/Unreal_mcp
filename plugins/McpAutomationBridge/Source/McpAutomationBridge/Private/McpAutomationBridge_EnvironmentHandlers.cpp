// =============================================================================
// McpAutomationBridge_EnvironmentHandlers.cpp
// =============================================================================
// Environment, Console, and Inspection System Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// --------------------
// Section 1: Build Environment Member Handlers
//   - HandleEnvironment* members       : build_environment core actions (classed
//     in MCP/Calls/McpCalls_BuildEnvironment.cpp) — six inline bodies
//     (export_snapshot, import_snapshot, delete, create_sky_sphere,
//     set_time_of_day, create_fog_volume) plus 15 wrappers forwarding to the
//     dedicated foliage/landscape members
//
// Section 4: Environment Utilities
//   - HandleBakeLightmap               : Lightmap baking via BUILD_LIGHTING
//   - HandleCreateProceduralTerrain    : Procedural terrain mesh generation
//   - HandleInspect* members           : inspect family bodies (classed in MCP/Calls/McpCalls_Inspect.cpp)
//
// PAYLOAD/RESPONSE FORMATS:
// -------------------------
// build_environment:
//   Payload: { "action": "<sub-action>", ... }
//   Response: { "success": bool, "action": string, ... }
//
// inspect:
//   Payload: { "action": "<sub-action>", "objectPath"?: string }
//   Response: { "success": bool, "objectPath"?: string, "className"?: string }
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.7: All handlers supported
// - ProceduralMeshComponent available in all versions
// - UEditorActorSubsystem available via conditional includes
// - Console command execution uses GEngine->Exec()
//
// REFACTORING NOTES:
// ------------------
// - Extracted utility functions should use McpHandlerUtils namespace
// - File path validation uses SanitizeProjectFilePath()
// - Actor operations use FindActorByName() helper
// - Component lookups use FindComponentByName() helper
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "UObject/UObjectIterator.h" // find_objects: TObjectIterator discovery
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformMemory.h"
#include "Misc/App.h"

// =============================================================================
// Editor-Only Includes
// =============================================================================
#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Slate/SceneViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Selection.h"

// Subsystem includes with version-specific paths
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#endif

// =============================================================================
// Engine Component Includes
// =============================================================================
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"

// =============================================================================
// Editor & Asset Includes
// =============================================================================
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "EditorValidatorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GeneralProjectSettings.h"

// =============================================================================
// Procedural & Mesh Includes
// =============================================================================
#include "KismetProceduralMeshLibrary.h"
#include "Misc/FileHelper.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"

// =============================================================================
// Landscape Includes (for foliage dispatch)
// =============================================================================
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeGrassType.h"
#include "AssetRegistry/AssetRegistryModule.h"

#endif // WITH_EDITOR

// =============================================================================
// Logging Category
// =============================================================================
DEFINE_LOG_CATEGORY_STATIC(LogMcpEnvironmentHandlers, Log, All);

#if WITH_EDITOR
static TSharedPtr<FJsonObject> McpMakeVectorObject(const FVector &Vector)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    Obj->SetNumberField(TEXT("x"), Vector.X);
    Obj->SetNumberField(TEXT("y"), Vector.Y);
    Obj->SetNumberField(TEXT("z"), Vector.Z);
    return Obj;
}

static TSharedPtr<FJsonObject> McpMakeRotatorObject(const FRotator &Rotator)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    Obj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
    Obj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
    Obj->SetNumberField(TEXT("roll"), Rotator.Roll);
    return Obj;
}

static TSharedPtr<FJsonObject> McpMakeTransformObject(const FTransform &Transform)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    Obj->SetObjectField(TEXT("location"), McpMakeVectorObject(Transform.GetLocation()));
    Obj->SetObjectField(TEXT("rotation"), McpMakeRotatorObject(Transform.GetRotation().Rotator()));
    Obj->SetObjectField(TEXT("scale"), McpMakeVectorObject(Transform.GetScale3D()));
    return Obj;
}

static UWorld *McpGetRuntimeInspectionWorld()
{
    if (!GEditor)
    {
        return nullptr;
    }

    if (GEditor->PlayWorld)
    {
        return GEditor->PlayWorld.Get();
    }

    if (GEngine)
    {
        for (const FWorldContext &Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
            {
                if (UWorld *World = Context.World())
                {
                    return World;
                }
            }
        }
    }

    return GEditor->GetEditorWorldContext().World();
}

static FString McpGetWorldTypeName(UWorld *World)
{
    if (!World)
    {
        return TEXT("None");
    }

    switch (World->WorldType)
    {
    case EWorldType::PIE:
        return TEXT("PIE");
    case EWorldType::Game:
        return TEXT("Game");
    case EWorldType::Editor:
        return TEXT("Editor");
    case EWorldType::EditorPreview:
        return TEXT("EditorPreview");
    case EWorldType::GamePreview:
        return TEXT("GamePreview");
    case EWorldType::GameRPC:
        return TEXT("GameRPC");
    case EWorldType::Inactive:
        return TEXT("Inactive");
    default:
        return TEXT("Unknown");
    }
}

static void McpAddActorTags(TSharedPtr<FJsonObject> Obj, const AActor *Actor)
{
    TArray<TSharedPtr<FJsonValue>> TagsArray;
    if (Actor)
    {
        for (const FName &Tag : Actor->Tags)
        {
            TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
        }
    }
    Obj->SetArrayField(TEXT("tags"), TagsArray);
}

static TSharedPtr<FJsonObject> McpDescribeRuntimeComponent(UActorComponent *Component, const TArray<FString> &PropertyNames)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    if (!Component)
    {
        return Obj;
    }

    Obj->SetStringField(TEXT("name"), Component->GetName());
    Obj->SetStringField(TEXT("path"), Component->GetPathName());
    Obj->SetStringField(TEXT("class"), Component->GetClass() ? Component->GetClass()->GetName() : TEXT(""));
    Obj->SetStringField(TEXT("classPath"), Component->GetClass() ? Component->GetClass()->GetPathName() : TEXT(""));
    Obj->SetBoolField(TEXT("isActive"), Component->IsActive());

    if (USceneComponent *SceneComp = Cast<USceneComponent>(Component))
    {
        Obj->SetBoolField(TEXT("isSceneComponent"), true);
        Obj->SetBoolField(TEXT("isVisible"), SceneComp->IsVisible());
        Obj->SetObjectField(TEXT("transform"), McpMakeTransformObject(SceneComp->GetComponentTransform()));
        Obj->SetStringField(TEXT("attachParent"), SceneComp->GetAttachParent() ? SceneComp->GetAttachParent()->GetName() : TEXT(""));
    }

    if (UCameraComponent *CameraComp = Cast<UCameraComponent>(Component))
    {
        Obj->SetBoolField(TEXT("isCamera"), true);
        Obj->SetNumberField(TEXT("fieldOfView"), CameraComp->FieldOfView);
        Obj->SetBoolField(TEXT("isActive"), CameraComp->IsActive());
    }

    if (USpringArmComponent *SpringArm = Cast<USpringArmComponent>(Component))
    {
        Obj->SetBoolField(TEXT("isSpringArm"), true);
        Obj->SetNumberField(TEXT("targetArmLength"), SpringArm->TargetArmLength);
        Obj->SetBoolField(TEXT("usePawnControlRotation"), SpringArm->bUsePawnControlRotation);
    }

    if (PropertyNames.Num() > 0)
    {
        TSharedPtr<FJsonObject> PropertiesObj = McpHandlerUtils::CreateResultObject();
        for (const FString &PropertyName : PropertyNames)
        {
            if (PropertyName.IsEmpty())
            {
                continue;
            }
            McpHandlerUtils::FPropertyResolveResult PropResult = McpHandlerUtils::ResolveProperty(Component, PropertyName);
            if (PropResult.IsValid())
            {
                if (TSharedPtr<FJsonValue> Value = ExportPropertyToJsonValue(PropResult.Container, PropResult.Property))
                {
                    PropertiesObj->SetField(PropertyName, Value);
                }
            }
        }
        Obj->SetObjectField(TEXT("properties"), PropertiesObj);
    }

    return Obj;
}

static TSharedPtr<FJsonObject> McpDescribeRuntimeActor(AActor *Actor, const TArray<FString> &ComponentNames, const TArray<FString> &PropertyNames)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    if (!Actor)
    {
        return Obj;
    }

    Obj->SetStringField(TEXT("name"), Actor->GetName());
    Obj->SetStringField(TEXT("label"), Actor->GetActorLabel());
    Obj->SetStringField(TEXT("path"), Actor->GetPathName());
    Obj->SetStringField(TEXT("class"), Actor->GetClass() ? Actor->GetClass()->GetName() : TEXT(""));
    Obj->SetStringField(TEXT("classPath"), Actor->GetClass() ? Actor->GetClass()->GetPathName() : TEXT(""));
    Obj->SetObjectField(TEXT("transform"), McpMakeTransformObject(Actor->GetActorTransform()));
    McpAddActorTags(Obj, Actor);

    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    TInlineComponentArray<UActorComponent *> Components;
    Actor->GetComponents(Components);
    for (UActorComponent *Component : Components)
    {
        if (!Component)
        {
            continue;
        }

        const bool bRequestedByName = ComponentNames.Num() == 0 || ComponentNames.ContainsByPredicate([Component](const FString &RequestedName) {
            return Component->GetName().Equals(RequestedName, ESearchCase::IgnoreCase);
        });
        const bool bAlwaysReportCameraState = Component->IsA<UCameraComponent>() || Component->IsA<USpringArmComponent>();
        if (bRequestedByName || bAlwaysReportCameraState)
        {
            ComponentsArray.Add(MakeShared<FJsonValueObject>(McpDescribeRuntimeComponent(Component, PropertyNames)));
        }
    }
    Obj->SetArrayField(TEXT("components"), ComponentsArray);
    Obj->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    return Obj;
}
#endif

// =============================================================================
// Section 1: Build Environment Member Handlers
// =============================================================================
// Dispatch lives in the build_environment FMcpCall classes
// (Private/MCP/Calls/McpCalls_BuildEnvironment.cpp); each HandleEnvironment*
// member here owns one advertised core action. Six bodies are extracted
// verbatim from the retired dispatcher chain, replicating its editor-build
// stub per member; the other 15 forward to the family's dedicated 4-arg
// members exactly as the retired branches did (same internal action
// literals and payload re-shaping).

// add_foliage_instances
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentAddFoliageInstances(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString FoliageTypePath;
    if (!Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath) ||
        FoliageTypePath.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
    }

    TSharedPtr<FJsonObject> FoliagePayload = McpHandlerUtils::CreateResultObject();
    if (!FoliageTypePath.IsEmpty())
    {
        FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
    }

    // Preserve full transform data so callers can specify rotation and scale.
    const TArray<TSharedPtr<FJsonValue>> *Transforms = nullptr;
    Payload->TryGetArrayField(TEXT("transforms"), Transforms);
    if (Transforms)
    {
        FoliagePayload->SetArrayField(TEXT("transforms"), *Transforms);
    }

    const TArray<TSharedPtr<FJsonValue>> *Locations = nullptr;
    if (Payload->TryGetArrayField(TEXT("locations"), Locations) && Locations)
    {
        FoliagePayload->SetArrayField(TEXT("locations"), *Locations);
    }

    return HandleAddFoliageInstances(RequestId, TEXT("add_foliage_instances"),
                                     FoliagePayload, RequestingSocket);
}

// get_foliage_instances
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentGetFoliageInstances(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString FoliageTypePath;
    Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
    TSharedPtr<FJsonObject> FoliagePayload = McpHandlerUtils::CreateResultObject();
    if (!FoliageTypePath.IsEmpty())
    {
        FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
    }
    return HandleGetFoliageInstances(RequestId, TEXT("get_foliage_instances"),
                                     FoliagePayload, RequestingSocket);
}

// remove_foliage
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentRemoveFoliage(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString FoliageTypePath;
    Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
    bool bRemoveAll = false;
    Payload->TryGetBoolField(TEXT("removeAll"), bRemoveAll);
    
    TSharedPtr<FJsonObject> FoliagePayload = McpHandlerUtils::CreateResultObject();
    if (!FoliageTypePath.IsEmpty())
    {
        FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
    }
    FoliagePayload->SetBoolField(TEXT("removeAll"), bRemoveAll);
    return HandleRemoveFoliage(RequestId, TEXT("remove_foliage"),
                               FoliagePayload, RequestingSocket);
}

// paint_foliage
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentPaintFoliage(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandlePaintFoliage(RequestId, TEXT("paint_foliage"), Payload,
                              RequestingSocket);
}

// create_procedural_foliage
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentCreateProceduralFoliage(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleCreateProceduralFoliage(RequestId,
                                         TEXT("create_procedural_foliage"),
                                         Payload, RequestingSocket);
}

// create_procedural_terrain
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentCreateProceduralTerrain(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleCreateProceduralTerrain(RequestId,
                                         TEXT("create_procedural_terrain"),
                                         Payload, RequestingSocket);
}

// add_foliage
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentAddFoliage(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleAddFoliageType(RequestId, TEXT("add_foliage_type"),
                                Payload, RequestingSocket);
}

// create_landscape
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentCreateLandscape(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleCreateLandscape(RequestId, TEXT("create_landscape"),
                                 Payload, RequestingSocket);
}

// paint_landscape
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentPaintLandscape(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandlePaintLandscapeLayer(RequestId, TEXT("paint_landscape_layer"),
                                     Payload, RequestingSocket);
}

// sculpt
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentSculpt(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleSculptLandscape(RequestId, TEXT("sculpt_landscape"), Payload,
                                 RequestingSocket);
}

// modify_heightmap
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentModifyHeightmap(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleModifyHeightmap(RequestId, TEXT("modify_heightmap"), Payload,
                                 RequestingSocket);
}

// set_landscape_material
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentSetLandscapeMaterial(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleSetLandscapeMaterial(RequestId, TEXT("set_landscape_material"),
                                      Payload, RequestingSocket);
}

// create_landscape_grass_type
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentCreateLandscapeGrassType(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleCreateLandscapeGrassType(RequestId,
                                          TEXT("create_landscape_grass_type"),
                                          Payload, RequestingSocket);
}

// generate_lods
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentGenerateLODs(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleGenerateLODs(RequestId, TEXT("generate_lods"), Payload,
                              RequestingSocket);
}

// bake_lightmap
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentBakeLightmap(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    return HandleBakeLightmap(RequestId, TEXT("bake_lightmap"), Payload,
                              RequestingSocket);
}

// export_snapshot: Export environment snapshot to JSON file
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentExportSnapshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("action"), TEXT("export_snapshot"));
    bool bSuccess = true;
    FString Message = TEXT("Environment action 'export_snapshot' completed");
    FString ErrorCode;

    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);
    
    if (Path.IsEmpty())
    {
        bSuccess = false;
        Message = TEXT("path required for export_snapshot");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
        // SECURITY: Validate file path to prevent directory traversal
        FString SafePath = SanitizeProjectFilePath(Path);
        if (SafePath.IsEmpty())
        {
            bSuccess = false;
            Message = FString::Printf(
                TEXT("Invalid or unsafe path: %s. Path must be relative to project (e.g., /Temp/snapshot.json)"),
                *Path);
            ErrorCode = TEXT("SECURITY_VIOLATION");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            // Convert project-relative path to absolute file path
            FString AbsolutePath = FPaths::ProjectDir() / SafePath;
            FPaths::MakeStandardFilename(AbsolutePath);

            // CRITICAL: Convert to absolute path for proper comparison
            // This prevents path traversal via leading slash (e.g., /etc/passwd)
            AbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
            FPaths::NormalizeFilename(AbsolutePath);

            FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
            FPaths::NormalizeDirectoryName(NormalizedProjectDir);
            if (!NormalizedProjectDir.EndsWith(TEXT("/")))
            {
                NormalizedProjectDir += TEXT("/");
            }

            if (!AbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase))
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Invalid or unsafe path: %s. Path escapes project directory."), *Path);
                ErrorCode = TEXT("SECURITY_VIOLATION");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else if (!McpValidateProjectSnapshotFilePath(AbsolutePath, Message))
            {
                bSuccess = false;
                ErrorCode = TEXT("SECURITY_VIOLATION");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                TSharedPtr<FJsonObject> Snapshot = McpHandlerUtils::CreateResultObject();
                Snapshot->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToString());
                Snapshot->SetStringField(TEXT("type"), TEXT("environment_snapshot"));

                FString JsonString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
                if (FJsonSerializer::Serialize(Snapshot.ToSharedRef(), Writer))
                {
                    if (FFileHelper::SaveStringToFile(JsonString, *AbsolutePath))
                    {
                        Resp->SetStringField(TEXT("exportPath"), SafePath);
                        Resp->SetStringField(TEXT("message"), TEXT("Snapshot exported"));
                    }
                    else
                    {
                        bSuccess = false;
                        Message = TEXT("Failed to write snapshot file");
                        ErrorCode = TEXT("WRITE_FAILED");
                        Resp->SetStringField(TEXT("error"), Message);
                    }
                }
                else
                {
                    bSuccess = false;
                    Message = TEXT("Failed to serialize snapshot");
                    ErrorCode = TEXT("SERIALIZE_FAILED");
                    Resp->SetStringField(TEXT("error"), Message);
                }
            }
        }
    }

    Resp->SetBoolField(TEXT("success"), bSuccess);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
#else
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Environment building actions require editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// import_snapshot: Import environment snapshot from JSON file
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentImportSnapshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("action"), TEXT("import_snapshot"));
    bool bSuccess = true;
    FString Message = TEXT("Environment action 'import_snapshot' completed");
    FString ErrorCode;

    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);
    
    if (Path.IsEmpty())
    {
        bSuccess = false;
        Message = TEXT("path required for import_snapshot");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
        // SECURITY: Validate file path to prevent directory traversal
        FString SafePath = SanitizeProjectFilePath(Path);
        if (SafePath.IsEmpty())
        {
            bSuccess = false;
            Message = FString::Printf(
                TEXT("Invalid or unsafe path: %s. Path must be relative to project (e.g., /Temp/snapshot.json)"),
                *Path);
            ErrorCode = TEXT("SECURITY_VIOLATION");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            FString AbsolutePath = FPaths::ProjectDir() / SafePath;
            FPaths::MakeStandardFilename(AbsolutePath);

            // CRITICAL: Convert to absolute path for proper comparison
            // This prevents path traversal via leading slash (e.g., /etc/passwd)
            AbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
            FPaths::NormalizeFilename(AbsolutePath);

            FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
            FPaths::NormalizeDirectoryName(NormalizedProjectDir);
            if (!NormalizedProjectDir.EndsWith(TEXT("/")))
            {
                NormalizedProjectDir += TEXT("/");
            }

            if (!AbsolutePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase))
            {
                bSuccess = false;
                Message = FString::Printf(TEXT("Invalid or unsafe path: %s. Path escapes project directory."), *Path);
                ErrorCode = TEXT("SECURITY_VIOLATION");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else if (!McpValidateProjectSnapshotFilePath(AbsolutePath, Message))
            {
                bSuccess = false;
                ErrorCode = TEXT("SECURITY_VIOLATION");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                FString JsonString;
                if (!FFileHelper::LoadFileToString(JsonString, *AbsolutePath))
                {
                    bSuccess = false;
                    Message = TEXT("Failed to read snapshot file");
                    ErrorCode = TEXT("LOAD_FAILED");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    TSharedPtr<FJsonObject> SnapshotObj;
                    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
                    if (!FJsonSerializer::Deserialize(Reader, SnapshotObj) || !SnapshotObj.IsValid())
                    {
                        bSuccess = false;
                        Message = TEXT("Failed to parse snapshot");
                        ErrorCode = TEXT("PARSE_FAILED");
                        Resp->SetStringField(TEXT("error"), Message);
                    }
                    else
                    {
                        Resp->SetObjectField(TEXT("snapshot"), SnapshotObj.ToSharedRef());
                        Resp->SetStringField(TEXT("message"), TEXT("Snapshot imported"));
                    }
                }
            }
        }
    }

    Resp->SetBoolField(TEXT("success"), bSuccess);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
#else
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Environment building actions require editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// delete: Delete environment actors by name
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentDelete(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("action"), TEXT("delete"));
    bool bSuccess = true;
    FString Message = TEXT("Environment action 'delete' completed");
    FString ErrorCode;

    const TArray<TSharedPtr<FJsonValue>> *NamesArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("names"), NamesArray) || !NamesArray)
    {
        bSuccess = false;
        Message = TEXT("names array required for delete");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
    }
    else if (!GEditor)
    {
        bSuccess = false;
        Message = TEXT("Editor not available");
        ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
        UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
        if (!ActorSS)
        {
            bSuccess = false;
            Message = TEXT("EditorActorSubsystem not available");
            ErrorCode = TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            TArray<FString> Deleted;
            TArray<FString> Missing;

            for (const TSharedPtr<FJsonValue> &Val : *NamesArray)
            {
                if (Val.IsValid() && Val->Type == EJson::String)
                {
                    FString Name = Val->AsString();
                    TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
                    bool bRemoved = false;

                    for (AActor *A : AllActors)
                    {
                        if (A && A->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase))
                        {
                            if (ActorSS->DestroyActor(A))
                            {
                                Deleted.Add(Name);
                                bRemoved = true;
                            }
                            break;
                        }
                    }

                    if (!bRemoved)
                    {
                        Missing.Add(Name);
                    }
                }
            }

            // Build response arrays
            TArray<TSharedPtr<FJsonValue>> DeletedArray;
            for (const FString &Name : Deleted)
            {
                DeletedArray.Add(MakeShared<FJsonValueString>(Name));
            }
            Resp->SetArrayField(TEXT("deleted"), DeletedArray);
            Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

            if (Missing.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> MissingArray;
                for (const FString &Name : Missing)
                {
                    MissingArray.Add(MakeShared<FJsonValueString>(Name));
                }
                Resp->SetArrayField(TEXT("missing"), MissingArray);
                bSuccess = false;
                Message = TEXT("Some environment actors could not be removed");
                ErrorCode = TEXT("DELETE_PARTIAL");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                Message = TEXT("Environment actors deleted");
            }
        }
    }

    Resp->SetBoolField(TEXT("success"), bSuccess);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
#else
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Environment building actions require editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// create_sky_sphere: Create sky sphere actor
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentCreateSkySphere(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("action"), TEXT("create_sky_sphere"));
    bool bSuccess = true;
    FString Message = TEXT("Environment action 'create_sky_sphere' completed");
    FString ErrorCode;

    // Initialize to false - only set true on successful creation
    bSuccess = false;
    
    if (!GEditor)
    {
        Message = TEXT("Editor not available");
        ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
    }
    else
    {
        UClass *SkySphereClass = LoadClass<AActor>(
            nullptr, TEXT("/Script/Engine.Blueprint'/Engine/Maps/Templates/"
                          "SkySphere.SkySphere_C'"));
        if (!SkySphereClass)
        {
            FString RequestedName = TEXT("SkySphere");
            Payload->TryGetStringField(TEXT("name"), RequestedName);

            ADirectionalLight *SunLight = Cast<ADirectionalLight>(
                SpawnActorInActiveWorld<AActor>(ADirectionalLight::StaticClass(),
                                                FVector::ZeroVector,
                                                FRotator(-45.0f, -35.0f, 0.0f),
                                                TEXT("SkySunLight")));
            ASkyLight *SkyLight = Cast<ASkyLight>(
                SpawnActorInActiveWorld<AActor>(ASkyLight::StaticClass(),
                                                FVector::ZeroVector,
                                                FRotator::ZeroRotator,
                                                TEXT("SkyLight")));

            if (SunLight && SkyLight)
            {
                SunLight->SetActorLabel(FString::Printf(TEXT("%s_Sun"), *RequestedName));
                SkyLight->SetActorLabel(FString::Printf(TEXT("%s_SkyLight"), *RequestedName));

                if (UDirectionalLightComponent *SunComp =
                        Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
                {
                    SunComp->SetIntensity(10.0f);
                    SunComp->MarkRenderStateDirty();
                }
                if (USkyLightComponent *SkyComp = SkyLight->GetLightComponent())
                {
                    SkyComp->SetIntensity(1.0f);
                    SkyComp->MarkRenderStateDirty();
                }

                bSuccess = true;
                Message = TEXT("Native sky lighting rig created");
                Resp->SetBoolField(TEXT("fallbackUsed"), true);
                Resp->SetStringField(TEXT("missingAsset"), TEXT("/Engine/Maps/Templates/SkySphere"));
                Resp->SetStringField(TEXT("actorName"), RequestedName);
                Resp->SetStringField(TEXT("sunActorName"), SunLight->GetActorLabel());
                Resp->SetStringField(TEXT("skyLightActorName"), SkyLight->GetActorLabel());
                McpHandlerUtils::AddVerification(Resp, SunLight);
            }
            else
            {
                Message = TEXT("SkySphere class not found and native sky rig fallback failed");
                ErrorCode = TEXT("SPAWN_FAILED");
                Resp->SetStringField(TEXT("missingAsset"), TEXT("/Engine/Maps/Templates/SkySphere"));
            }
        }
        else
        {
            AActor *SkySphere = SpawnActorInActiveWorld<AActor>(
                SkySphereClass, FVector::ZeroVector, FRotator::ZeroRotator,
                TEXT("SkySphere"));
            if (SkySphere)
            {
                bSuccess = true;
                Message = TEXT("Sky sphere created");
                Resp->SetStringField(TEXT("actorName"), SkySphere->GetActorLabel());
            }
            else
            {
                Message = TEXT("Failed to spawn sky sphere actor");
                ErrorCode = TEXT("SPAWN_FAILED");
            }
        }
    }

    Resp->SetBoolField(TEXT("success"), bSuccess);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
#else
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Environment building actions require editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// set_time_of_day: Set time of day on sky sphere
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentSetTimeOfDay(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("action"), TEXT("set_time_of_day"));
    bool bSuccess = true;
    FString Message = TEXT("Environment action 'set_time_of_day' completed");
    FString ErrorCode;

    float TimeOfDay = 12.0f;
    if (!Payload->TryGetNumberField(TEXT("time"), TimeOfDay))
    {
        Payload->TryGetNumberField(TEXT("hour"), TimeOfDay);
    }

    if (GEditor)
    {
        UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
        if (ActorSS)
        {
            for (AActor *Actor : ActorSS->GetAllLevelActors())
            {
                if (Actor->GetClass()->GetName().Contains(TEXT("SkySphere")))
                {
                    UFunction *SetTimeFunction = Actor->FindFunction(TEXT("SetTimeOfDay"));
                    if (SetTimeFunction)
                    {
                        float TimeParam = TimeOfDay;
                        Actor->ProcessEvent(SetTimeFunction, &TimeParam);
                        bSuccess = true;
                        Message = FString::Printf(TEXT("Time of day set to %.2f"), TimeOfDay);
                        break;
                    }
                }
            }
        }
    }
    if (!bSuccess)
    {
        bSuccess = false;
        Message = TEXT("Sky sphere not found or time function not available");
        ErrorCode = TEXT("SET_TIME_FAILED");
    }

    Resp->SetBoolField(TEXT("success"), bSuccess);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
#else
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Environment building actions require editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// create_fog_volume: Create exponential height fog
bool UMcpAutomationBridgeSubsystem::HandleEnvironmentCreateFogVolume(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("action"), TEXT("create_fog_volume"));
    bool bSuccess = true;
    FString Message = TEXT("Environment action 'create_fog_volume' completed");
    FString ErrorCode;

    // Initialize to false - only set true on successful creation
    bSuccess = false;
    
    FVector Location(0, 0, 0);
    // Support both top-level x/y/z and location object
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
    {
        (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
        (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }
    else
    {
        Payload->TryGetNumberField(TEXT("x"), Location.X);
        Payload->TryGetNumberField(TEXT("y"), Location.Y);
        Payload->TryGetNumberField(TEXT("z"), Location.Z);
    }

    FString FogName;
    Payload->TryGetStringField(TEXT("name"), FogName);
    if (FogName.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("actorName"), FogName);
    }
    if (FogName.IsEmpty())
    {
        FogName = TEXT("FogVolume");
    }

    if (!GEditor)
    {
        Message = TEXT("Editor not available");
        ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
    }
    else
    {
        UClass *FogClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.ExponentialHeightFog"));
        if (!FogClass)
        {
            Message = TEXT("ExponentialHeightFog class not found");
            ErrorCode = TEXT("CLASS_NOT_FOUND");
        }
        else
        {
            AActor *FogVolume = SpawnActorInActiveWorld<AActor>(
                FogClass, Location, FRotator::ZeroRotator, FogName);
            if (FogVolume)
            {
                bSuccess = true;
                Message = TEXT("Fog volume created");
                Resp->SetStringField(TEXT("actorName"), FogVolume->GetActorLabel());
            }
            else
            {
                Message = TEXT("Failed to spawn fog volume actor");
                ErrorCode = TEXT("SPAWN_FAILED");
            }
        }
    }

    Resp->SetBoolField(TEXT("success"), bSuccess);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
#else
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Environment building actions require editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// Section 4: Environment Utilities
// =============================================================================

/**
 * HandleBakeLightmap
 * -------------------
 * Build lighting via editor function.
 * 
 * Payload:
 *   - quality: string (optional) - Lighting build quality (default: "Preview")
 * 
 * Dispatches to HandleExecuteEditorFunction with BUILD_LIGHTING.
 */
bool UMcpAutomationBridgeSubsystem::HandleBakeLightmap(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("bake_lightmap"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    FString QualityStr = TEXT("Preview");
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("quality"), QualityStr);
    }

    // Reuse HandleExecuteEditorFunction logic
    TSharedPtr<FJsonObject> P = McpHandlerUtils::CreateResultObject();
    P->SetStringField(TEXT("functionName"), TEXT("BUILD_LIGHTING"));
    P->SetStringField(TEXT("quality"), QualityStr);

    return HandleExecuteEditorFunction(RequestId, TEXT("execute_editor_function"),
                                       P, RequestingSocket);

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Requires editor"), nullptr,
                           TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

/**
 * HandleCreateProceduralTerrain
 * -------------------------------
 * Create a procedural terrain mesh with configurable parameters.
 * 
 * Payload:
 *   - sizeX: int (optional, default 100) - Terrain width in grid units
 *   - sizeY: int (optional, default 100) - Terrain depth in grid units
 *   - spacing: float (optional, default 100.0) - Distance between vertices
 *   - heightScale: float (optional, default 500.0) - Maximum height variation
 *   - subdivisions: int (optional, default 50) - Grid subdivisions
 *   - actorName: string (required) - Name for the spawned actor
 *   - location: {x, y, z} (optional) - Spawn location
 *   - rotation: {pitch, yaw, roll} (optional) - Spawn rotation
 *   - material: string (optional) - Material asset path
 * 
 * Response:
 *   - success: bool
 *   - actorName: string - Spawned actor name
 *   - actorPath: string - Actor path in level
 *   - vertices: int - Number of vertices generated
 *   - triangles: int - Number of triangles generated
 */
bool UMcpAutomationBridgeSubsystem::HandleCreateProceduralTerrain(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("create_procedural_terrain"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Editor not available"),
                            TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_procedural_terrain payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Extract terrain parameters
    // -------------------------------------------------------------------------
    int32 SizeX = 100;
    int32 SizeY = 100;
    double Spacing = 100.0;
    double HeightScale = 500.0;
    int32 Subdivisions = 50;
    FString ActorName = TEXT("ProceduralTerrain");

    Payload->TryGetNumberField(TEXT("sizeX"), SizeX);
    Payload->TryGetNumberField(TEXT("sizeY"), SizeY);
    Payload->TryGetNumberField(TEXT("spacing"), Spacing);
    Payload->TryGetNumberField(TEXT("heightScale"), HeightScale);
    Payload->TryGetNumberField(TEXT("subdivisions"), Subdivisions);
    Payload->TryGetStringField(TEXT("actorName"), ActorName);

    // -------------------------------------------------------------------------
    // Validate actorName
    // -------------------------------------------------------------------------
    if (ActorName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("actorName parameter is required for create_procedural_terrain"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Reject invalid characters
    if (ActorName.Contains(TEXT("/")) || ActorName.Contains(TEXT("\\")) ||
        ActorName.Contains(TEXT(":")) || ActorName.Contains(TEXT("*")) ||
        ActorName.Contains(TEXT("?")) || ActorName.Contains(TEXT("\"")) ||
        ActorName.Contains(TEXT("<")) || ActorName.Contains(TEXT(">")) ||
        ActorName.Contains(TEXT("|")))
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("actorName contains invalid characters (/, \\, :, *, ?, \", <, >, |)"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Reject excessive length
    if (ActorName.Len() > 128)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("actorName exceeds maximum length of 128 characters"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Clamp values to reasonable limits
    // -------------------------------------------------------------------------
    SizeX = FMath::Clamp(SizeX, 2, 1000);
    SizeY = FMath::Clamp(SizeY, 2, 1000);
    Subdivisions = FMath::Clamp(Subdivisions, 2, 200);
    Spacing = FMath::Max(Spacing, 1.0);
    HeightScale = FMath::Max(HeightScale, 0.0);

    // -------------------------------------------------------------------------
    // Get world and spawn actor
    // -------------------------------------------------------------------------
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("World not available"),
                            TEXT("WORLD_NOT_AVAILABLE"));
        return true;
    }

    // Extract location/rotation
    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
    {
        double X = 0, Y = 0, Z = 0;
        (*LocObj)->TryGetNumberField(TEXT("x"), X);
        (*LocObj)->TryGetNumberField(TEXT("y"), Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), Z);
        Location = FVector(X, Y, Z);
    }

    FRotator Rotation(0, 0, 0);
    const TSharedPtr<FJsonObject> *RotObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj)
    {
        double Pitch = 0, Yaw = 0, Roll = 0;
        (*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*RotObj)->TryGetNumberField(TEXT("roll"), Roll);
        Rotation = FRotator(Pitch, Yaw, Roll);
    }

    // Spawn actor with requested name
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*ActorName);
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

    AActor *TerrainActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!TerrainActor)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to spawn terrain actor"),
                            TEXT("SPAWN_FAILED"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Add procedural mesh component
    // -------------------------------------------------------------------------
    UProceduralMeshComponent *ProcMesh = NewObject<UProceduralMeshComponent>(TerrainActor);
    if (!ProcMesh)
    {
        TerrainActor->Destroy();
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to create procedural mesh component"),
                            TEXT("COMPONENT_CREATION_FAILED"));
        return true;
    }

    ProcMesh->RegisterComponent();
    TerrainActor->AddInstanceComponent(ProcMesh);
    TerrainActor->SetRootComponent(ProcMesh);

    // -------------------------------------------------------------------------
    // Generate terrain mesh
    // -------------------------------------------------------------------------
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;

    // Create grid of vertices
    for (int32 Y = 0; Y <= Subdivisions; ++Y)
    {
        for (int32 X = 0; X <= Subdivisions; ++X)
        {
            // Calculate normalized position (0 to 1)
            double NormX = static_cast<double>(X) / Subdivisions;
            double NormY = static_cast<double>(Y) / Subdivisions;

            // Calculate world position with spacing
            double WorldX = (NormX - 0.5) * SizeX * Spacing;
            double WorldY = (NormY - 0.5) * SizeY * Spacing;

            // Generate height using simple noise/sine combination
            double WorldZ = FMath::Sin(NormX * 4.0 * PI) * FMath::Cos(NormY * 4.0 * PI) * HeightScale * 0.3 +
                            FMath::Sin(NormX * 8.0 * PI) * FMath::Cos(NormY * 8.0 * PI) * HeightScale * 0.15 +
                            FMath::Sin(NormX * 2.0 * PI + NormY * 3.0 * PI) * HeightScale * 0.25;

            Vertices.Add(FVector(WorldX, WorldY, WorldZ));
            UVs.Add(FVector2D(NormX, NormY));
        }
    }

    // Generate triangles
    for (int32 Y = 0; Y < Subdivisions; ++Y)
    {
        for (int32 X = 0; X < Subdivisions; ++X)
        {
            int32 Current = Y * (Subdivisions + 1) + X;
            int32 Next = Current + Subdivisions + 1;

            // First triangle
            Triangles.Add(Current);
            Triangles.Add(Next);
            Triangles.Add(Current + 1);

            // Second triangle
            Triangles.Add(Current + 1);
            Triangles.Add(Next);
            Triangles.Add(Next + 1);
        }
    }

    // Calculate normals and tangents
    UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);

    // Create the mesh section
    ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, true);

    // -------------------------------------------------------------------------
    // Apply material if specified
    // -------------------------------------------------------------------------
    FString MaterialPath;
    if (Payload->TryGetStringField(TEXT("material"), MaterialPath) && !MaterialPath.IsEmpty())
    {
        UMaterialInterface *Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
        if (Material)
        {
            ProcMesh->SetMaterial(0, Material);
        }
    }

    // Mark the actor as modified
    TerrainActor->MarkPackageDirty();

    // -------------------------------------------------------------------------
    // Build response
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("actorName"), TerrainActor->GetName());
    Resp->SetStringField(TEXT("actorPath"), TerrainActor->GetPathName());
    Resp->SetNumberField(TEXT("vertices"), Vertices.Num());
    Resp->SetNumberField(TEXT("triangles"), Triangles.Num() / 3);
    Resp->SetNumberField(TEXT("sizeX"), SizeX);
    Resp->SetNumberField(TEXT("sizeY"), SizeY);
    Resp->SetNumberField(TEXT("subdivisions"), Subdivisions);

    // Add verification data
    McpHandlerUtils::AddVerification(Resp, TerrainActor);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Procedural terrain created successfully"), Resp, FString());
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("create_procedural_terrain requires editor build"), nullptr,
                           TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// inspect — family implementation bodies. Declared and dispatched by the
// FMcpCall classes in MCP/Calls/McpCalls_Inspect.cpp. HandleInspectRuntimeReport
// serves runtime_report and pie_report; HandleInspectObjectGeneric serves
// inspect_object and the seven get_*_details actions (identical output).
// =============================================================================
#if WITH_EDITOR

// -------------------------------------------------------------------------
// find_objects — loaded-object discovery with property readback. Replaces
// the read-only execute_python ObjectIterator probes (template staleness
// hunts, slot-object path discovery, live widget state like
// DynamicEntryBox AllEntries or slider values). Params:
//   className (required) — IsA semantics; exactClass:true for exact match.
//   pathContains (optional) — case-insensitive substring of GetPathName().
//   propertyNames[] (optional) — reflected readback per object.
//   includeCdo (default false) — include class default objects.
//   limit (default 50, max 200).
// -------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleInspectFindObjects(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString ClassName;
    Payload->TryGetStringField(TEXT("className"), ClassName);
    if (ClassName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("find_objects requires 'className'."),
                            TEXT("MISSING_PARAMETER"));
        return true;
    }

    // Resolve the class: loaded short name, /Script/Module.Class, or
    // /Game/....Name_C (same approach as the widget-authoring resolvers).
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    UClass *TargetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);
#else
    UClass *TargetClass = nullptr;
#endif
    if (!TargetClass && (ClassName.Contains(TEXT(".")) || ClassName.Contains(TEXT("/"))))
    {
        TargetClass = StaticLoadClass(UObject::StaticClass(), nullptr, *ClassName, nullptr, LOAD_NoWarn | LOAD_Quiet);
    }
    if (!TargetClass)
    {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Could not resolve className '%s' to a loaded UClass. Pass a loaded short name or a fully-qualified path."), *ClassName),
            TEXT("CLASS_NOT_FOUND"));
        return true;
    }

    FString PathContains;
    Payload->TryGetStringField(TEXT("pathContains"), PathContains);
    bool bExactClass = false;
    Payload->TryGetBoolField(TEXT("exactClass"), bExactClass);
    bool bIncludeCdo = false;
    Payload->TryGetBoolField(TEXT("includeCdo"), bIncludeCdo);
    int32 Limit = 50;
    {
        double LimitNum = 0.0;
        if (Payload->TryGetNumberField(TEXT("limit"), LimitNum))
        {
            Limit = FMath::Clamp(static_cast<int32>(LimitNum), 1, 200);
        }
    }
    const TArray<TSharedPtr<FJsonValue>> *PropertyNames = nullptr;
    Payload->TryGetArrayField(TEXT("propertyNames"), PropertyNames);

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 Matched = 0;
    bool bTruncated = false;
    for (TObjectIterator<UObject> It; It; ++It)
    {
        UObject *Obj = *It;
        if (!IsValid(Obj))
        {
            continue;
        }
        if (bExactClass ? (Obj->GetClass() != TargetClass) : !Obj->IsA(TargetClass))
        {
            continue;
        }
        if (!bIncludeCdo && Obj->HasAnyFlags(RF_ClassDefaultObject))
        {
            continue;
        }
        const FString Path = Obj->GetPathName();
        if (!PathContains.IsEmpty() && !Path.Contains(PathContains, ESearchCase::IgnoreCase))
        {
            continue;
        }
        ++Matched;
        if (Results.Num() >= Limit)
        {
            bTruncated = true;
            continue; // keep counting matches, stop collecting
        }

        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("path"), Path);
        Entry->SetStringField(TEXT("class"), Obj->GetClass()->GetName());
        if (PropertyNames && PropertyNames->Num() > 0)
        {
            TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
            for (const TSharedPtr<FJsonValue> &NameVal : *PropertyNames)
            {
                FString PropName;
                if (!NameVal.IsValid() || !NameVal->TryGetString(PropName) || PropName.IsEmpty())
                {
                    continue;
                }
                FProperty *Prop = Obj->GetClass()->FindPropertyByName(FName(*PropName));
                if (!Prop)
                {
                    Props->SetField(PropName, MakeShared<FJsonValueNull>());
                    continue;
                }
                // The helper takes the CONTAINER (object) pointer — it
                // resolves offsets itself via the _InContainer accessors.
                TSharedPtr<FJsonValue> Exported =
                    McpPropertyReflection::ExportPropertyToJsonValue(Obj, Prop);
                Props->SetField(PropName, Exported.IsValid()
                                              ? Exported
                                              : MakeShared<FJsonValueNull>());
            }
            Entry->SetObjectField(TEXT("properties"), Props);
        }
        Results.Add(MakeShared<FJsonValueObject>(Entry));
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("className"), TargetClass->GetPathName());
    Resp->SetNumberField(TEXT("matched"), Matched);
    Resp->SetBoolField(TEXT("truncated"), bTruncated);
    Resp->SetArrayField(TEXT("objects"), Results);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Found %d object(s)"), Matched),
                           Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectGetProjectSettings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("action"), TEXT("inspect"));
    Resp->SetStringField(TEXT("subAction"), TEXT("get_project_settings"));
    Resp->SetStringField(TEXT("message"), TEXT("Project settings retrieved"));
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Project settings retrieved"), Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectGetEditorSettings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("action"), TEXT("inspect"));
    Resp->SetStringField(TEXT("subAction"), TEXT("get_editor_settings"));
    Resp->SetStringField(TEXT("message"), TEXT("Editor settings retrieved"));
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Editor settings retrieved"), Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectGetWorldSettings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    if (GEditor && GEditor->GetEditorWorldContext().World())
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        Resp->SetStringField(TEXT("worldName"), World->GetName());
        Resp->SetStringField(TEXT("levelName"), World->GetCurrentLevel()->GetName());
        Resp->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("World settings retrieved"), Resp, FString());
    }
    else
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("No world available"),
                            TEXT("WORLD_NOT_FOUND"));
    }
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectGetViewportInfo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    if (GEditor && GEditor->GetActiveViewport())
    {
        FViewport* Viewport = GEditor->GetActiveViewport();
        Resp->SetNumberField(TEXT("width"), Viewport->GetSizeXY().X);
        Resp->SetNumberField(TEXT("height"), Viewport->GetSizeXY().Y);

        // Report the level viewport camera pose (same source set_camera writes)
        // so callers can read back where the camera is.
        FVector CamLoc(FVector::ZeroVector);
        FRotator CamRot(FRotator::ZeroRotator);
        if (UUnrealEditorSubsystem* UESView =
                GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
        {
            UESView->GetLevelViewportCameraInfo(CamLoc, CamRot);
        }
        TSharedPtr<FJsonObject> LocJson = MakeShared<FJsonObject>();
        LocJson->SetNumberField(TEXT("x"), CamLoc.X);
        LocJson->SetNumberField(TEXT("y"), CamLoc.Y);
        LocJson->SetNumberField(TEXT("z"), CamLoc.Z);
        Resp->SetObjectField(TEXT("location"), LocJson);
        TSharedPtr<FJsonObject> RotJson = MakeShared<FJsonObject>();
        RotJson->SetNumberField(TEXT("pitch"), CamRot.Pitch);
        RotJson->SetNumberField(TEXT("yaw"), CamRot.Yaw);
        RotJson->SetNumberField(TEXT("roll"), CamRot.Roll);
        Resp->SetObjectField(TEXT("rotation"), RotJson);
        Resp->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Viewport info retrieved"), Resp, FString());
    }
    else
    {
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("message"), TEXT("Viewport info not available in this context"));
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Viewport info retrieved"), Resp, FString());
    }
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectGetSelectedActors(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> ActorsArray;
    if (GEditor)
    {
        TArray<AActor*> SelectedActors;
        GEditor->GetSelectedActors()->GetSelectedObjects(SelectedActors);
        for (AActor* Actor : SelectedActors)
        {
            if (Actor)
            {
                TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
                ActorObj->SetStringField(TEXT("name"), Actor->GetName());
                ActorObj->SetStringField(TEXT("path"), Actor->GetPathName());
                ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
            }
        }
    }
    Resp->SetArrayField(TEXT("actors"), ActorsArray);
    Resp->SetNumberField(TEXT("count"), ActorsArray.Num());
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Selected actors retrieved"), Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectGetSceneStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    int32 ActorCount = 0;
    if (GEditor && GEditor->GetEditorWorldContext().World())
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            ActorCount++;
        }
    }
    Resp->SetNumberField(TEXT("actorCount"), ActorCount);
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Scene stats retrieved"), Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectGetPerformanceStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    const double DeltaSeconds = FApp::GetDeltaTime();
    const double FrameTimeMs = DeltaSeconds > 0.0 ? DeltaSeconds * 1000.0 : 0.0;
    const double EstimatedFps = DeltaSeconds > 0.0 ? 1.0 / DeltaSeconds : 0.0;

    int32 ActorCount = 0;
    if (GEditor && GEditor->GetEditorWorldContext().World())
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            ActorCount++;
        }
    }

    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("deltaSeconds"), DeltaSeconds);
    Resp->SetNumberField(TEXT("frameTimeMs"), FrameTimeMs);
    Resp->SetNumberField(TEXT("estimatedFps"), EstimatedFps);
    Resp->SetNumberField(TEXT("actorCount"), ActorCount);
    Resp->SetBoolField(TEXT("isBenchmarking"), FApp::IsBenchmarking());
    Resp->SetBoolField(TEXT("useFixedTimeStep"), FApp::UseFixedTimeStep());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Performance stats retrieved"), Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectGetMemoryStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("totalPhysicalBytes"), static_cast<double>(MemoryStats.TotalPhysical));
    Resp->SetNumberField(TEXT("availablePhysicalBytes"), static_cast<double>(MemoryStats.AvailablePhysical));
    Resp->SetNumberField(TEXT("usedPhysicalBytes"), static_cast<double>(MemoryStats.UsedPhysical));
    Resp->SetNumberField(TEXT("peakUsedPhysicalBytes"), static_cast<double>(MemoryStats.PeakUsedPhysical));
    Resp->SetNumberField(TEXT("totalVirtualBytes"), static_cast<double>(MemoryStats.TotalVirtual));
    Resp->SetNumberField(TEXT("availableVirtualBytes"), static_cast<double>(MemoryStats.AvailableVirtual));
    Resp->SetNumberField(TEXT("usedVirtualBytes"), static_cast<double>(MemoryStats.UsedVirtual));
    Resp->SetNumberField(TEXT("peakUsedVirtualBytes"), static_cast<double>(MemoryStats.PeakUsedVirtual));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Memory stats retrieved"), Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectRuntimeReport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    UWorld *World = McpGetRuntimeInspectionWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("No editor, PIE, or game world available for runtime inspection"),
                            TEXT("WORLD_NOT_FOUND"));
        return true;
    }

    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("name"), ActorName);
    }

    TArray<FString> ComponentNames;
    FString ComponentName;
    if (Payload->TryGetStringField(TEXT("componentName"), ComponentName) && !ComponentName.IsEmpty())
    {
        ComponentNames.Add(ComponentName);
    }
    const TArray<TSharedPtr<FJsonValue>> *ComponentNamesArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("componentNames"), ComponentNamesArray) && ComponentNamesArray)
    {
        for (const TSharedPtr<FJsonValue> &Value : *ComponentNamesArray)
        {
            if (Value.IsValid() && Value->Type == EJson::String)
            {
                ComponentNames.Add(Value->AsString());
            }
        }
    }

    TArray<FString> PropertyNames;
    FString PropertyName;
    if (Payload->TryGetStringField(TEXT("propertyName"), PropertyName) && !PropertyName.IsEmpty())
    {
        PropertyNames.Add(PropertyName);
    }
    else if (Payload->TryGetStringField(TEXT("propertyPath"), PropertyName) && !PropertyName.IsEmpty())
    {
        PropertyNames.Add(PropertyName);
    }
    const TArray<TSharedPtr<FJsonValue>> *PropertyNamesArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("propertyNames"), PropertyNamesArray) && PropertyNamesArray)
    {
        for (const TSharedPtr<FJsonValue> &Value : *PropertyNamesArray)
        {
            if (Value.IsValid() && Value->Type == EJson::String)
            {
                PropertyNames.Add(Value->AsString());
            }
        }
    }

    TSharedPtr<FJsonObject> Report = McpHandlerUtils::CreateResultObject();
    Report->SetBoolField(TEXT("success"), true);
    Report->SetStringField(TEXT("worldName"), World->GetName());
    Report->SetStringField(TEXT("worldType"), McpGetWorldTypeName(World));
    Report->SetStringField(TEXT("worldPath"), World->GetPathName());
    Report->SetBoolField(TEXT("isPIE"), World->WorldType == EWorldType::PIE);

    TArray<TSharedPtr<FJsonValue>> ActorsArray;
    int32 TotalActorCount = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Actor = *It;
        if (!Actor)
        {
            continue;
        }
        ++TotalActorCount;

        const FString Label = Actor->GetActorLabel();
        const FString Name = Actor->GetName();
        const bool bMatchesActor = ActorName.IsEmpty() ||
            Label.Equals(ActorName, ESearchCase::IgnoreCase) ||
            Name.Equals(ActorName, ESearchCase::IgnoreCase) ||
            Actor->GetPathName().Equals(ActorName, ESearchCase::IgnoreCase);
        const bool bMatchesFilter = Filter.IsEmpty() ||
            Label.Contains(Filter) ||
            Name.Contains(Filter) ||
            Actor->GetClass()->GetName().Contains(Filter) ||
            Actor->GetPathName().Contains(Filter);
        if (bMatchesActor && bMatchesFilter)
        {
            ActorsArray.Add(MakeShared<FJsonValueObject>(McpDescribeRuntimeActor(Actor, ComponentNames, PropertyNames)));
        }
    }
    Report->SetArrayField(TEXT("actors"), ActorsArray);
    Report->SetNumberField(TEXT("count"), ActorsArray.Num());
    Report->SetNumberField(TEXT("totalActorCount"), TotalActorCount);

    APlayerController *PlayerController = World->GetFirstPlayerController();
    if (PlayerController)
    {
        TSharedPtr<FJsonObject> ControllerObj = McpDescribeRuntimeActor(PlayerController, ComponentNames, PropertyNames);
        Report->SetObjectField(TEXT("playerController"), ControllerObj);

        if (APawn *Pawn = PlayerController->GetPawn())
        {
            Report->SetObjectField(TEXT("pawn"), McpDescribeRuntimeActor(Pawn, ComponentNames, PropertyNames));
        }

        if (AActor *ViewTarget = PlayerController->GetViewTarget())
        {
            Report->SetObjectField(TEXT("viewTarget"), McpDescribeRuntimeActor(ViewTarget, ComponentNames, PropertyNames));
        }

        if (APlayerCameraManager *CameraManager = PlayerController->PlayerCameraManager)
        {
            TSharedPtr<FJsonObject> CameraManagerObj = McpDescribeRuntimeActor(CameraManager, ComponentNames, PropertyNames);
            CameraManagerObj->SetObjectField(TEXT("cameraLocation"), McpMakeVectorObject(CameraManager->GetCameraLocation()));
            CameraManagerObj->SetObjectField(TEXT("cameraRotation"), McpMakeRotatorObject(CameraManager->GetCameraRotation()));
            Report->SetObjectField(TEXT("playerCameraManager"), CameraManagerObj);
        }
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Runtime inspection report generated"), Report, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectListObjects(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> ObjectsArray;
    if (GEditor && GEditor->GetEditorWorldContext().World())
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
            Obj->SetStringField(TEXT("name"), Actor->GetName());
            Obj->SetStringField(TEXT("path"), Actor->GetPathName());
            Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
            ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
        }
    }
    Resp->SetArrayField(TEXT("objects"), ObjectsArray);
    Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Objects listed"), Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectFindByClass(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    FString ClassName;
    Payload->TryGetStringField(TEXT("className"), ClassName);
    if (ClassName.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("classPath"), ClassName);
    }
    TArray<TSharedPtr<FJsonValue>> ObjectsArray;

    if (GEditor && GEditor->GetEditorWorldContext().World() && !ClassName.IsEmpty())
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (Actor->GetClass()->GetName().Equals(ClassName, ESearchCase::IgnoreCase) ||
                Actor->GetClass()->GetPathName().Contains(ClassName))
            {
                TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
                Obj->SetStringField(TEXT("name"), Actor->GetName());
                Obj->SetStringField(TEXT("path"), Actor->GetPathName());
                Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
            }
        }
    }
    Resp->SetArrayField(TEXT("objects"), ObjectsArray);
    Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Objects found by class"), Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectFindByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    FString Tag;
    Payload->TryGetStringField(TEXT("tag"), Tag);
    TArray<TSharedPtr<FJsonValue>> ObjectsArray;

    if (GEditor && GEditor->GetEditorWorldContext().World() && !Tag.IsEmpty())
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (Actor->ActorHasTag(FName(*Tag)))
            {
                TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
                Obj->SetStringField(TEXT("name"), Actor->GetName());
                Obj->SetStringField(TEXT("path"), Actor->GetPathName());
                Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                ObjectsArray.Add(MakeShared<FJsonValueObject>(Obj));
            }
        }
    }
    Resp->SetArrayField(TEXT("objects"), ObjectsArray);
    Resp->SetNumberField(TEXT("count"), ObjectsArray.Num());
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Objects found by tag"), Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectClassInfo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    FString ClassName;
    Payload->TryGetStringField(TEXT("className"), ClassName);
    if (ClassName.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("classPath"), ClassName);
    }
    if (ClassName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("className is required for inspect_class"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Resolve like find_objects: loaded short name, /Script/Module.Class,
    // /Game/....Name_C, or a Blueprint asset path via its GeneratedClass.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    UClass* TargetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);
#else
    UClass* TargetClass = FindObject<UClass>(nullptr, *ClassName);
#endif
    if (!TargetClass && (ClassName.Contains(TEXT(".")) || ClassName.Contains(TEXT("/"))))
    {
        TargetClass = StaticLoadClass(UObject::StaticClass(), nullptr, *ClassName, nullptr, LOAD_NoWarn | LOAD_Quiet);
    }
    if (!TargetClass && ClassName.StartsWith(TEXT("/")) && !ClassName.EndsWith(TEXT("_C")))
    {
        if (UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ClassName, nullptr, LOAD_NoWarn | LOAD_Quiet, nullptr))
        {
            TargetClass = BP->GeneratedClass;
        }
    }
    if (!TargetClass && !ClassName.Contains(TEXT(".")))
    {
        TargetClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
    }
    if (!TargetClass)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Class not found: %s — pass a loaded short name, a Script path, a Blueprint asset path, or its _C class path"), *ClassName),
                            TEXT("CLASS_NOT_FOUND"));
        return true;
    }

    Resp->SetStringField(TEXT("className"), TargetClass->GetName());
    Resp->SetStringField(TEXT("classPath"), TargetClass->GetPathName());
    Resp->SetStringField(TEXT("parentClass"), TargetClass->GetSuperClass() ? TargetClass->GetSuperClass()->GetName() : TEXT("None"));

    bool bDetailed = false;
    Payload->TryGetBoolField(TEXT("detailed"), bDetailed);
    if (bDetailed)
    {
        TArray<TSharedPtr<FJsonValue>> PropsArr;
        for (TFieldIterator<FProperty> It(TargetClass); It; ++It)
        {
            TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
            PropObj->SetStringField(TEXT("name"), It->GetName());
            PropObj->SetStringField(TEXT("type"), It->GetCPPType());
            if (UClass* OwnerClass = It->GetOwnerClass())
            {
                if (OwnerClass != TargetClass)
                {
                    PropObj->SetStringField(TEXT("inheritedFrom"), OwnerClass->GetName());
                }
            }
            PropsArr.Add(MakeShared<FJsonValueObject>(PropObj));
        }
        Resp->SetArrayField(TEXT("properties"), PropsArr);

        TArray<TSharedPtr<FJsonValue>> FuncsArr;
        for (TFieldIterator<UFunction> It(TargetClass); It; ++It)
        {
            FuncsArr.Add(MakeShared<FJsonValueString>(It->GetName()));
        }
        Resp->SetArrayField(TEXT("functions"), FuncsArr);
    }

    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Class inspected"), Resp, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleInspectObjectGeneric(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
    FString ObjectPath;
    Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
    if (ObjectPath.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("actorName"), ObjectPath);
    }
    if (ObjectPath.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("name"), ObjectPath);
    }
    if (ObjectPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("objectPath, actorName, or name required"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Find the target object using centralized helper
    FString ResolvedPath;
    UObject* TargetObject = McpHandlerUtils::ResolveObjectFromPath(ObjectPath, &ResolvedPath);
    
    if (!TargetObject)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
                            TEXT("OBJECT_NOT_FOUND"));
        return true;
    }
    
    // Update path for error messages
    if (!ResolvedPath.IsEmpty())
    {
        ObjectPath = ResolvedPath;
    }

    // -------------------------------------------------------------------------
    // Build inspection result
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

    // Basic object info
    Resp->SetStringField(TEXT("objectPath"), TargetObject->GetPathName());
    Resp->SetStringField(TEXT("objectName"), TargetObject->GetName());
    Resp->SetStringField(TEXT("className"), TargetObject->GetClass()->GetName());
    Resp->SetStringField(TEXT("classPath"), TargetObject->GetClass()->GetPathName());

    // If it's an actor, add actor-specific info
    if (AActor *Actor = Cast<AActor>(TargetObject))
    {
        Resp->SetStringField(TEXT("actorLabel"), Actor->GetActorLabel());
        Resp->SetBoolField(TEXT("isActor"), true);
        Resp->SetBoolField(TEXT("isHidden"), Actor->IsHidden());
        Resp->SetBoolField(TEXT("isSelected"), Actor->IsSelected());

        // Transform info
        TSharedPtr<FJsonObject> TransformObj = McpHandlerUtils::CreateResultObject();
        const FTransform &Transform = Actor->GetActorTransform();

        TSharedPtr<FJsonObject> LocationObj = McpHandlerUtils::CreateResultObject();
        LocationObj->SetNumberField(TEXT("x"), Transform.GetLocation().X);
        LocationObj->SetNumberField(TEXT("y"), Transform.GetLocation().Y);
        LocationObj->SetNumberField(TEXT("z"), Transform.GetLocation().Z);
        TransformObj->SetObjectField(TEXT("location"), LocationObj);

        TSharedPtr<FJsonObject> RotationObj = McpHandlerUtils::CreateResultObject();
        FRotator Rotator = Transform.GetRotation().Rotator();
        RotationObj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
        RotationObj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
        RotationObj->SetNumberField(TEXT("roll"), Rotator.Roll);
        TransformObj->SetObjectField(TEXT("rotation"), RotationObj);

        TSharedPtr<FJsonObject> ScaleObj = McpHandlerUtils::CreateResultObject();
        ScaleObj->SetNumberField(TEXT("x"), Transform.GetScale3D().X);
        ScaleObj->SetNumberField(TEXT("y"), Transform.GetScale3D().Y);
        ScaleObj->SetNumberField(TEXT("z"), Transform.GetScale3D().Z);
        TransformObj->SetObjectField(TEXT("scale"), ScaleObj);

        Resp->SetObjectField(TEXT("transform"), TransformObj);

        // Components info
        TArray<TSharedPtr<FJsonValue>> ComponentsArray;
        TInlineComponentArray<UActorComponent *> Components;
        Actor->GetComponents(Components);

        for (UActorComponent *Component : Components)
        {
            if (Component)
            {
                TSharedPtr<FJsonObject> CompObj = McpHandlerUtils::CreateResultObject();
                CompObj->SetStringField(TEXT("name"), Component->GetName());
                CompObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
                CompObj->SetBoolField(TEXT("isActive"), Component->IsActive());

                // Add specific info for common component types
                if (USceneComponent *SceneComp = Cast<USceneComponent>(Component))
                {
                    CompObj->SetBoolField(TEXT("isSceneComponent"), true);
                    CompObj->SetBoolField(TEXT("isVisible"), SceneComp->IsVisible());
                }

                if (UStaticMeshComponent *MeshComp = Cast<UStaticMeshComponent>(Component))
                {
                    CompObj->SetBoolField(TEXT("isStaticMesh"), true);
                    if (MeshComp->GetStaticMesh())
                    {
                        CompObj->SetStringField(TEXT("staticMesh"), MeshComp->GetStaticMesh()->GetName());
                    }
                }

                ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
            }
        }
        Resp->SetArrayField(TEXT("components"), ComponentsArray);
        Resp->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    }
    else
    {
        Resp->SetBoolField(TEXT("isActor"), false);
    }

    // Tags - only for Actor-derived classes
    TArray<TSharedPtr<FJsonValue>> TagsArray;
    UClass* ObjClass = TargetObject->GetClass();
    if (ObjClass && ObjClass->IsChildOf(AActor::StaticClass()))
    {
        if (AActor* DefaultActor = ObjClass->GetDefaultObject<AActor>())
        {
            for (const FName &Tag : DefaultActor->Tags)
            {
                TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
            }
        }
    }
    Resp->SetArrayField(TEXT("tags"), TagsArray);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Object inspection completed"), Resp, FString());
    return true;
}

#else

#define MCP_INSPECT_HANDLER_STUB(Fn)                                          \
  bool UMcpAutomationBridgeSubsystem::Fn(const FString &,                     \
                                         const TSharedPtr<FJsonObject> &,     \
                                         FMcpResponseHandle) {                \
    return false;                                                             \
  }

MCP_INSPECT_HANDLER_STUB(HandleInspectFindObjects)
MCP_INSPECT_HANDLER_STUB(HandleInspectGetProjectSettings)
MCP_INSPECT_HANDLER_STUB(HandleInspectGetEditorSettings)
MCP_INSPECT_HANDLER_STUB(HandleInspectGetWorldSettings)
MCP_INSPECT_HANDLER_STUB(HandleInspectGetViewportInfo)
MCP_INSPECT_HANDLER_STUB(HandleInspectGetSelectedActors)
MCP_INSPECT_HANDLER_STUB(HandleInspectGetSceneStats)
MCP_INSPECT_HANDLER_STUB(HandleInspectGetPerformanceStats)
MCP_INSPECT_HANDLER_STUB(HandleInspectGetMemoryStats)
MCP_INSPECT_HANDLER_STUB(HandleInspectRuntimeReport)
MCP_INSPECT_HANDLER_STUB(HandleInspectListObjects)
MCP_INSPECT_HANDLER_STUB(HandleInspectFindByClass)
MCP_INSPECT_HANDLER_STUB(HandleInspectFindByTag)
MCP_INSPECT_HANDLER_STUB(HandleInspectClassInfo)
MCP_INSPECT_HANDLER_STUB(HandleInspectObjectGeneric)
#undef MCP_INSPECT_HANDLER_STUB

#endif // WITH_EDITOR
