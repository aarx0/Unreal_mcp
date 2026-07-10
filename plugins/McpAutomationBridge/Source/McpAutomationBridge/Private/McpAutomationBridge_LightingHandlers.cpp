// =============================================================================
// McpAutomationBridge_LightingHandlers.cpp
// =============================================================================
// Lighting and Rendering Environment Handlers for MCP Automation Bridge.
//
// This file implements the build_environment Lighting members (HandleLighting*;
// classed in MCP/Calls/McpCalls_BuildEnvironment.cpp):
// - create_light
// - create_sky_light
// - build_lighting
// - ensure_single_sky_light
// - create_lighting_enabled_level
// - create_lightmass_volume
// - setup_volumetric_fog
// - setup_global_illumination
// - configure_shadows
// - set_exposure
// - set_ambient_occlusion
// - list_light_types
//
// UE VERSION COMPATIBILITY:
// - UE 5.0-5.7: Full support for all lighting types
// - UE 5.6+: LightingBuildOptions.h removed, use console exec instead
// - UE 5.7: Use SpawnActorDeferred for safer light spawning
// - Intel GPU: Requires FlushRenderingCommands before spawn to prevent crashes
//
// SECURITY CONSIDERATIONS:
// - Path sanitization for cubemap paths (prevents traversal attacks)
// - Validation of intensity, radius, cone angles to prevent NaN/infinite values
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST BE FIRST - Version compatibility macros
#include "McpHandlerUtils.h"          // Utility functions for JSON parsing

#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_LightingHandlers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UObjectIterator.h"

// =============================================================================
// Engine Includes - Core
// =============================================================================
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/TextureCube.h"

#if WITH_EDITOR

// =============================================================================
// Engine Includes - Editor
// =============================================================================
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "GameFramework/WorldSettings.h"
#include "Lightmass/LightmassImportanceVolume.h"

// =============================================================================
// Engine Includes - Editor Utilities
// =============================================================================
// UE5.6: LightingBuildOptions.h removed; use console exec
#include "Editor/UnrealEd/Public/Editor.h"
#include "FileHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "LevelEditor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "RenderingThread.h"  // FlushRenderingCommands for safe spawning

#endif // WITH_EDITOR

// =============================================================================
// Lighting Member Handlers
// =============================================================================
// Dispatch lives in the build_environment FMcpCall classes
// (Private/MCP/Calls/McpCalls_BuildEnvironment.cpp); each HandleLighting*
// member here owns one advertised action's body, extracted verbatim from
// the retired dispatcher chain, replicating its editor-build stub and
// EditorActorSubsystem precheck per member.

// =========================================================================
// list_light_types
// =========================================================================
// Discovers all ALight subclasses via reflection and returns available types
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingListLightTypes(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> Types;

    // Add common shortcuts first
    Types.Add(MakeShared<FJsonValueString>(TEXT("DirectionalLight")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("PointLight")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("SpotLight")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("RectLight")));

    // Discover all ALight subclasses via reflection
    TSet<FString> AddedNames;
    AddedNames.Add(TEXT("DirectionalLight"));
    AddedNames.Add(TEXT("PointLight"));
    AddedNames.Add(TEXT("SpotLight"));
    AddedNames.Add(TEXT("RectLight"));

    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (It->IsChildOf(ALight::StaticClass()) &&
            !It->HasAnyClassFlags(CLASS_Abstract) &&
            !AddedNames.Contains(It->GetName()))
        {
            Types.Add(MakeShared<FJsonValueString>(It->GetName()));
            AddedNames.Add(It->GetName());
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetArrayField(TEXT("types"), Types);
    Resp->SetNumberField(TEXT("count"), Types.Num());
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Available light types"), Resp);
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =========================================================================
// create_light
// =========================================================================
// Spawns a light actor with configurable properties:
// - lightClass/lightType/type: Light class name
// - location/rotation: Transform
// - properties: intensity, color, castShadows, type-specific settings
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingCreateLight(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    FString LightClassStr;

    // Support multiple parameter names: lightClass, lightType, type
    // Priority: lightClass > lightType > type
    if (!Payload->TryGetStringField(TEXT("lightClass"), LightClassStr) || LightClassStr.IsEmpty())
    {
        FString LightType;
        if (Payload->TryGetStringField(TEXT("lightType"), LightType) && !LightType.IsEmpty())
        {
            const FString LowerType = LightType.ToLower();
            if (LowerType == TEXT("point") || LowerType == TEXT("pointlight"))
            {
                LightClassStr = TEXT("PointLight");
            }
            else if (LowerType == TEXT("directional") || LowerType == TEXT("directionallight"))
            {
                LightClassStr = TEXT("DirectionalLight");
            }
            else if (LowerType == TEXT("spot") || LowerType == TEXT("spotlight"))
            {
                LightClassStr = TEXT("SpotLight");
            }
            else if (LowerType == TEXT("rect") || LowerType == TEXT("rectlight"))
            {
                LightClassStr = TEXT("RectLight");
            }
            else if (LowerType == TEXT("sky") || LowerType == TEXT("skylight"))
            {
                LightClassStr = TEXT("SkyLight");
            }
            else
            {
                S.SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Invalid lightType: %s. Must be one of: point, directional, spot, rect, sky"), *LightType),
                    TEXT("INVALID_LIGHT_TYPE"));
                return true;
            }
        }
        // Also check for 'type' parameter (common shorthand)
        else if (Payload->TryGetStringField(TEXT("type"), LightType) && !LightType.IsEmpty())
        {
            const FString LowerType = LightType.ToLower();
            if (LowerType == TEXT("point") || LowerType == TEXT("pointlight"))
            {
                LightClassStr = TEXT("PointLight");
            }
            else if (LowerType == TEXT("directional") || LowerType == TEXT("directionallight"))
            {
                LightClassStr = TEXT("DirectionalLight");
            }
            else if (LowerType == TEXT("spot") || LowerType == TEXT("spotlight"))
            {
                LightClassStr = TEXT("SpotLight");
            }
            else if (LowerType == TEXT("rect") || LowerType == TEXT("rectlight"))
            {
                LightClassStr = TEXT("RectLight");
            }
            else if (LowerType == TEXT("sky") || LowerType == TEXT("skylight"))
            {
                LightClassStr = TEXT("SkyLight");
            }
            else
            {
                S.SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Invalid type: %s. Must be one of: point, directional, spot, rect, sky"), *LightType),
                    TEXT("INVALID_LIGHT_TYPE"));
                return true;
            }
        }
    }

    if (LightClassStr.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("lightClass or lightType required"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // CRITICAL: Use explicit StaticClass() for native light types to avoid
    // ResolveUClass resolution issues where TObjectIterator may return wrong class.
    // This ensures SpotLight, DirectionalLight, etc. spawn correctly.
    UClass *LightClass = nullptr;
    const FString LowerClassStr = LightClassStr.ToLower();

    if (LowerClassStr == TEXT("pointlight") || LowerClassStr == TEXT("point"))
    {
        LightClass = APointLight::StaticClass();
    }
    else if (LowerClassStr == TEXT("directionallight") || LowerClassStr == TEXT("directional"))
    {
        LightClass = ADirectionalLight::StaticClass();
    }
    else if (LowerClassStr == TEXT("spotlight") || LowerClassStr == TEXT("spot"))
    {
        LightClass = ASpotLight::StaticClass();
    }
    else if (LowerClassStr == TEXT("rectlight") || LowerClassStr == TEXT("rect"))
    {
        LightClass = ARectLight::StaticClass();
    }
    else if (LowerClassStr == TEXT("skylight") || LowerClassStr == TEXT("sky"))
    {
        LightClass = ASkyLight::StaticClass();
    }
    else
    {
        // Fallback: Dynamic resolution with heuristics for custom light types
        LightClass = ResolveUClass(LightClassStr);

        // Try finding with 'A' prefix (standard Actor prefix)
        if (!LightClass)
        {
            LightClass = ResolveUClass(TEXT("A") + LightClassStr);
        }
    }

    if (!LightClass || !LightClass->IsChildOf(ALight::StaticClass()))
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid light class: %s"), *LightClassStr),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
        TEXT("spawn_light: Resolved lightClass '%s' to %s (path: %s)"),
        *LightClassStr, *LightClass->GetName(), *LightClass->GetPathName());

    // ---------------------------------------------------------------------
    // Parse Location — required: an argless call must not mutate the level.
    // ---------------------------------------------------------------------
    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject> *LocPtr;
    if (Payload->TryGetObjectField(TEXT("location"), LocPtr))
    {
        Location.X = GetJsonNumberField((*LocPtr), TEXT("x"));
        Location.Y = GetJsonNumberField((*LocPtr), TEXT("y"));
        Location.Z = GetJsonNumberField((*LocPtr), TEXT("z"));
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("create_light requires 'location' ({x,y,z}) before it will spawn into the level"),
            TEXT("MISSING_PARAMETER"));
        return true;
    }

    // ---------------------------------------------------------------------
    // Parse Rotation
    // ---------------------------------------------------------------------
    FRotator Rotation = FRotator::ZeroRotator;
    const TSharedPtr<FJsonObject> *RotPtr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotPtr))
    {
        Rotation.Pitch = GetJsonNumberField((*RotPtr), TEXT("pitch"));
        Rotation.Yaw = GetJsonNumberField((*RotPtr), TEXT("yaw"));
        Rotation.Roll = GetJsonNumberField((*RotPtr), TEXT("roll"));
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // CRITICAL: Validate world before spawning to prevent crashes
    // Use the editor world for persistent authoring instead of transient PIE worlds.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || !World->IsValidLowLevel())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("No valid world available for spawning light"),
            TEXT("NO_WORLD"));
        return true;
    }

    // CRITICAL: Flush rendering commands to prevent GPU driver crashes
    // during spawn operations (especially Intel MONZA drivers)
    FlushRenderingCommands();

    // CRITICAL: Use SpawnActorDeferred for safer initialization
    FTransform SpawnTransform(Rotation, Location);
    AActor *NewLight = World->SpawnActorDeferred<AActor>(
        LightClass,
        SpawnTransform,
        nullptr,    // Owner
        nullptr,    // Instigator
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    // CRITICAL: Finish spawning with proper transform
    if (NewLight)
    {
        UGameplayStatics::FinishSpawningActor(NewLight, SpawnTransform);
    }

    // Explicitly set location/rotation
    if (NewLight)
    {
        NewLight->SetActorLabel(LightClassStr);
        NewLight->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
            ETeleportType::TeleportPhysics);
    }

    if (!NewLight)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to spawn light actor"),
            TEXT("SPAWN_FAILED"));
        return true;
    }

    // ---------------------------------------------------------------------
    // Set Name (optional)
    // ---------------------------------------------------------------------
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("actorName"), Name);
    }
    if (!Name.IsEmpty())
    {
        NewLight->SetActorLabel(Name);
    }

    // Default to Movable for immediate feedback
    if (ULightComponent *BaseLightComp = NewLight->FindComponentByClass<ULightComponent>())
    {
        BaseLightComp->SetMobility(EComponentMobility::Movable);
    }

    // ---------------------------------------------------------------------
    // Apply Properties with Validation
    // ---------------------------------------------------------------------
    const TSharedPtr<FJsonObject> *Props;
    if (Payload->TryGetObjectField(TEXT("properties"), Props))
    {
        ULightComponent *LightComp = NewLight->FindComponentByClass<ULightComponent>();
        if (LightComp)
        {
            // Intensity validation: must be finite and non-negative
            double Intensity;
            if ((*Props)->TryGetNumberField(TEXT("intensity"), Intensity))
            {
                if (!FMath::IsFinite(Intensity))
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                        TEXT("spawn_light: Invalid intensity (not finite), using 0"));
                    Intensity = 0.0;
                }
                else if (Intensity < 0.0)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                        TEXT("spawn_light: Negative intensity %.2f clamped to 0"), Intensity);
                    Intensity = 0.0;
                }
                LightComp->SetIntensity((float)Intensity);
            }

            // Color validation: must have finite components
            const TSharedPtr<FJsonObject> *ColorObj;
            if ((*Props)->TryGetObjectField(TEXT("color"), ColorObj))
            {
                FLinearColor Color;
                Color.R = GetJsonNumberField((*ColorObj), TEXT("r"));
                Color.G = GetJsonNumberField((*ColorObj), TEXT("g"));
                Color.B = GetJsonNumberField((*ColorObj), TEXT("b"));
                Color.A = (*ColorObj)->HasField(TEXT("a"))
                    ? GetJsonNumberField((*ColorObj), TEXT("a"))
                    : 1.0f;

                if (!FMath::IsFinite(Color.R) || !FMath::IsFinite(Color.G) ||
                    !FMath::IsFinite(Color.B) || !FMath::IsFinite(Color.A))
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                        TEXT("spawn_light: Invalid color components, using white"));
                    Color = FLinearColor::White;
                }
                LightComp->SetLightColor(Color);
            }

            bool bCastShadows;
            if ((*Props)->TryGetBoolField(TEXT("castShadows"), bCastShadows))
            {
                LightComp->SetCastShadows(bCastShadows);
            }

            // -------------------------------------------------------------
            // DirectionalLight-specific properties
            // -------------------------------------------------------------
            if (UDirectionalLightComponent *DirComp = Cast<UDirectionalLightComponent>(LightComp))
            {
                bool bUseSun = true;
                if ((*Props)->TryGetBoolField(TEXT("useAsAtmosphereSunLight"), bUseSun))
                {
                    DirComp->SetAtmosphereSunLight(bUseSun);
                }
                else
                {
                    DirComp->SetAtmosphereSunLight(true);
                }
            }

            // -------------------------------------------------------------
            // PointLight-specific properties
            // -------------------------------------------------------------
            if (UPointLightComponent *PointComp = Cast<UPointLightComponent>(LightComp))
            {
                double Radius;
                if ((*Props)->TryGetNumberField(TEXT("attenuationRadius"), Radius))
                {
                    // Validate radius: must be positive and finite
                    if (!FMath::IsFinite(Radius) || Radius <= 0.0)
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                            TEXT("spawn_light: Invalid attenuationRadius %.2f, using 1000"), Radius);
                        Radius = 1000.0;
                    }
                    PointComp->SetAttenuationRadius((float)Radius);
                }
            }

            // -------------------------------------------------------------
            // SpotLight-specific properties
            // -------------------------------------------------------------
            if (USpotLightComponent *SpotComp = Cast<USpotLightComponent>(LightComp))
            {
                double InnerCone;
                if ((*Props)->TryGetNumberField(TEXT("innerConeAngle"), InnerCone))
                {
                    // Validate cone angle: 0-180 degrees
                    if (!FMath::IsFinite(InnerCone) || InnerCone < 0.0 || InnerCone > 180.0)
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                            TEXT("spawn_light: Invalid innerConeAngle %.2f, clamping to 0-180"), InnerCone);
                        InnerCone = FMath::Clamp(InnerCone, 0.0, 180.0);
                    }
                    SpotComp->SetInnerConeAngle((float)InnerCone);
                }

                double OuterCone;
                if ((*Props)->TryGetNumberField(TEXT("outerConeAngle"), OuterCone))
                {
                    if (!FMath::IsFinite(OuterCone) || OuterCone < 0.0 || OuterCone > 180.0)
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                            TEXT("spawn_light: Invalid outerConeAngle %.2f, clamping to 0-180"), OuterCone);
                        OuterCone = FMath::Clamp(OuterCone, 0.0, 180.0);
                    }
                    SpotComp->SetOuterConeAngle((float)OuterCone);
                }
            }

            // -------------------------------------------------------------
            // RectLight-specific properties
            // -------------------------------------------------------------
            if (URectLightComponent *RectComp = Cast<URectLightComponent>(LightComp))
            {
                double Width;
                if ((*Props)->TryGetNumberField(TEXT("sourceWidth"), Width))
                {
                    if (!FMath::IsFinite(Width) || Width <= 0.0)
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                            TEXT("spawn_light: Invalid sourceWidth %.2f, using 100"), Width);
                        Width = 100.0;
                    }
                    RectComp->SetSourceWidth((float)Width);
                }

                double Height;
                if ((*Props)->TryGetNumberField(TEXT("sourceHeight"), Height))
                {
                    if (!FMath::IsFinite(Height) || Height <= 0.0)
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                            TEXT("spawn_light: Invalid sourceHeight %.2f, using 100"), Height);
                        Height = 100.0;
                    }
                    RectComp->SetSourceHeight((float)Height);
                }
            }
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), NewLight->GetActorLabel());

    // Add verification data
    McpHandlerUtils::AddVerification(Resp, NewLight);

    S.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Light spawned"), Resp);
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =========================================================================
// create_sky_light
// =========================================================================
// Spawns a SkyLight actor with optional cubemap support
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingCreateSkyLight(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    // Default location to a reasonable height for sky lights
    FVector Location = FVector(0.0f, 0.0f, 500.0f);
    const TSharedPtr<FJsonObject> *LocPtr;
    bool bHasExplicitLocation = Payload->TryGetObjectField(TEXT("location"), LocPtr);
    if (bHasExplicitLocation)
    {
        Location.X = GetJsonNumberField((*LocPtr), TEXT("x"));
        Location.Y = GetJsonNumberField((*LocPtr), TEXT("y"));
        Location.Z = GetJsonNumberField((*LocPtr), TEXT("z"));
    }
    else
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
            TEXT("spawn_sky_light: No location provided, using default (0, 0, 500)"));
    }

    // Parse rotation (optional)
    FRotator Rotation = FRotator::ZeroRotator;
    const TSharedPtr<FJsonObject> *RotPtr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotPtr))
    {
        Rotation.Pitch = GetJsonNumberField((*RotPtr), TEXT("pitch"));
        Rotation.Yaw = GetJsonNumberField((*RotPtr), TEXT("yaw"));
        Rotation.Roll = GetJsonNumberField((*RotPtr), TEXT("roll"));
    }

    AActor *SkyLight = SpawnActorInActiveWorld<AActor>(
        ASkyLight::StaticClass(), Location, Rotation);
    if (!SkyLight)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to spawn SkyLight"),
            TEXT("SPAWN_FAILED"));
        return true;
    }

    // Set name (optional)
    FString Name;
    if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
    {
        SkyLight->SetActorLabel(Name);
    }

    // Configure SkyLight component
    USkyLightComponent *SkyComp = SkyLight->FindComponentByClass<USkyLightComponent>();
    if (SkyComp)
    {
        FString SourceType;
        if (Payload->TryGetStringField(TEXT("sourceType"), SourceType))
        {
            if (SourceType == TEXT("SpecifiedCubemap"))
            {
                SkyComp->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;

                FString CubemapPath;
                if (Payload->TryGetStringField(TEXT("cubemapPath"), CubemapPath) &&
                    !CubemapPath.IsEmpty())
                {
                    // Security: Validate cubemap path to prevent traversal attacks
                    FString SanitizedCubemapPath = SanitizeProjectRelativePath(CubemapPath);
                    if (SanitizedCubemapPath.IsEmpty())
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                            TEXT("spawn_sky_light: Invalid cubemapPath rejected: %s"), *CubemapPath);
                    }
                    else
                    {
                        UTextureCube *Cubemap = Cast<UTextureCube>(StaticLoadObject(
                            UTextureCube::StaticClass(), nullptr, *SanitizedCubemapPath));
                        if (Cubemap)
                        {
                            SkyComp->Cubemap = Cubemap;
                        }
                    }
                }
            }
            else
            {
                SkyComp->SourceType = ESkyLightSourceType::SLS_CapturedScene;
            }
        }

        double Intensity;
        if (Payload->TryGetNumberField(TEXT("intensity"), Intensity))
        {
            SkyComp->SetIntensity((float)Intensity);
        }

        bool bRecapture;
        if (Payload->TryGetBoolField(TEXT("recapture"), bRecapture) && bRecapture)
        {
            SkyComp->RecaptureSky();
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), SkyLight->GetActorLabel());

    // Add verification data
    McpHandlerUtils::AddVerification(Resp, SkyLight);

    S.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("SkyLight spawned"), Resp);
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =========================================================================
// build_lighting
// =========================================================================
// Starts a lighting build with the specified quality
// Quality options: preview/0, medium/1, high/2, production/3
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingBuildLighting(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    if (GEditor && GEditor->GetEditorWorldContext().World())
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();

        // Check if precomputed lighting is disabled in WorldSettings
        if (AWorldSettings* WS = World->GetWorldSettings())
        {
            if (WS->bForceNoPrecomputedLighting)
            {
                // IMPORTANT: Return success=true with skipped=true because this is intentional behavior,
                // not an error. The operation was handled correctly - it was just skipped due to project settings.
                // Tests expecting 'success' should pass when the operation is intentionally skipped.
                TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetBoolField(TEXT("skipped"), true);
                Resp->SetStringField(TEXT("reason"), TEXT("bForceNoPrecomputedLighting is true"));
                Resp->SetStringField(TEXT("suggestion"),
                    TEXT("Set WorldSettings.bForceNoPrecomputedLighting to false to enable lighting builds"));
                S.SendAutomationResponse(RequestingSocket, RequestId, true,
                    TEXT("Lighting build skipped - precomputed lighting disabled in WorldSettings"), Resp);
                return true;
            }
        }

        // Read quality parameter
        FString Quality;
        Payload->TryGetStringField(TEXT("quality"), Quality);

        // Map quality string to console command
        FString QualityCmd = TEXT("Production"); // Default
        if (!Quality.IsEmpty())
        {
            const FString LowerQuality = Quality.ToLower();
            if (LowerQuality == TEXT("preview") || LowerQuality == TEXT("0"))
            {
                QualityCmd = TEXT("Preview");
            }
            else if (LowerQuality == TEXT("medium") || LowerQuality == TEXT("1"))
            {
                QualityCmd = TEXT("Medium");
            }
            else if (LowerQuality == TEXT("high") || LowerQuality == TEXT("2"))
            {
                QualityCmd = TEXT("High");
            }
            else if (LowerQuality == TEXT("production") || LowerQuality == TEXT("3"))
            {
                QualityCmd = TEXT("Production");
            }
            else
            {
                TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
                Err->SetStringField(TEXT("error"), TEXT("unknown_quality"));
                Err->SetStringField(TEXT("quality"), Quality);
                Err->SetStringField(TEXT("validValues"),
                    TEXT("preview/0, medium/1, high/2, production/3"));
                S.SendAutomationResponse(RequestingSocket, RequestId, false,
                    TEXT("Unknown lighting quality"), Err,
                    TEXT("UNKNOWN_QUALITY"));
                return true;
            }
        }

        FString Command = FString::Printf(TEXT("BuildLighting %s"), *QualityCmd);
        GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("quality"), QualityCmd);
        Resp->SetBoolField(TEXT("started"), true);
        S.SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Lighting build started with quality: %s"), *QualityCmd), Resp);
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Editor world not available"),
            TEXT("EDITOR_WORLD_NOT_AVAILABLE"));
    }
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =========================================================================
// ensure_single_sky_light
// =========================================================================
// Ensures exactly one SkyLight exists in the level
// Removes duplicates and optionally spawns one if none exists
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingEnsureSingleSkyLight(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
    TArray<AActor *> SkyLights;
    for (AActor *Actor : AllActors)
    {
        if (Actor && Actor->IsA<ASkyLight>())
        {
            SkyLights.Add(Actor);
        }
    }

    FString TargetName;
    Payload->TryGetStringField(TEXT("name"), TargetName);
    if (TargetName.IsEmpty())
    {
        TargetName = TEXT("SkyLight");
    }

    int32 RemovedCount = 0;
    AActor *KeptActor = nullptr;

    // Two-pass approach: first find exact name match, then destroy others
    for (AActor *SkyLight : SkyLights)
    {
        if (SkyLight->GetActorLabel() == TargetName && !TargetName.IsEmpty())
        {
            KeptActor = SkyLight;
            break;
        }
    }

    // If no exact match, keep first and destroy rest
    for (AActor *SkyLight : SkyLights)
    {
        if (SkyLight == KeptActor)
        {
            continue;
        }
        if (!KeptActor)
        {
            KeptActor = SkyLight;
            if (!TargetName.IsEmpty())
            {
                SkyLight->SetActorLabel(TargetName);
            }
        }
        else
        {
            ActorSS->DestroyActor(SkyLight);
            RemovedCount++;
        }
    }

    if (!KeptActor)
    {
        // Spawn one if none existed
        KeptActor = SpawnActorInActiveWorld<AActor>(
            ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator,
            TargetName);
    }

    if (KeptActor)
    {
        bool bRecapture;
        if (Payload->TryGetBoolField(TEXT("recapture"), bRecapture) && bRecapture)
        {
            if (USkyLightComponent *Comp = KeptActor->FindComponentByClass<USkyLightComponent>())
            {
                Comp->RecaptureSky();
            }
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetNumberField(TEXT("removed"), RemovedCount);

    // Add verification data
    if (KeptActor)
    {
        McpHandlerUtils::AddVerification(Resp, KeptActor);
    }

    S.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Ensured single SkyLight"), Resp);
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =========================================================================
// create_lightmass_volume
// =========================================================================
// Creates a LightmassImportanceVolume at the specified location and size
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingCreateLightmassVolume(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject> *LocObj;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj))
    {
        Location.X = GetJsonNumberField((*LocObj), TEXT("x"));
        Location.Y = GetJsonNumberField((*LocObj), TEXT("y"));
        Location.Z = GetJsonNumberField((*LocObj), TEXT("z"));
    }

    FVector Size = FVector(1000, 1000, 1000);
    const TSharedPtr<FJsonObject> *SizeObj;
    if (Payload->TryGetObjectField(TEXT("size"), SizeObj))
    {
        Size.X = GetJsonNumberField((*SizeObj), TEXT("x"));
        Size.Y = GetJsonNumberField((*SizeObj), TEXT("y"));
        Size.Z = GetJsonNumberField((*SizeObj), TEXT("z"));
    }

    AActor *Volume = SpawnActorInActiveWorld<AActor>(
        ALightmassImportanceVolume::StaticClass(), Location,
        FRotator::ZeroRotator);
    if (Volume)
    {
        Volume->SetActorScale3D(Size / 200.0f); // Brush size adjustment approximation

        FString Name;
        if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
        {
            Volume->SetActorLabel(Name);
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), Volume->GetActorLabel());

        // Add verification data
        McpHandlerUtils::AddVerification(Resp, Volume);

        S.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("LightmassImportanceVolume created"), Resp);
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to spawn LightmassImportanceVolume"),
            TEXT("SPAWN_FAILED"));
    }
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =========================================================================
// setup_volumetric_fog
// =========================================================================
// Enables volumetric fog on an ExponentialHeightFog actor
// Spawns one if none exists
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingSetupVolumetricFog(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    // Find existing or spawn new ExponentialHeightFog
    AExponentialHeightFog *FogActor = nullptr;
    TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
    for (AActor *Actor : AllActors)
    {
        if (Actor && Actor->IsA<AExponentialHeightFog>())
        {
            FogActor = Cast<AExponentialHeightFog>(Actor);
            break;
        }
    }

    if (!FogActor)
    {
        FogActor = Cast<AExponentialHeightFog>(SpawnActorInActiveWorld<AActor>(
            AExponentialHeightFog::StaticClass(), FVector::ZeroVector,
            FRotator::ZeroRotator));
    }

    if (FogActor && FogActor->GetComponent())
    {
        UExponentialHeightFogComponent *FogComp = FogActor->GetComponent();
        FogComp->bEnableVolumetricFog = true;

        double Distance;
        if (Payload->TryGetNumberField(TEXT("viewDistance"), Distance))
        {
            FogComp->VolumetricFogDistance = (float)Distance;
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), FogActor->GetActorLabel());
        Resp->SetBoolField(TEXT("enabled"), true);

        // Add verification data
        McpHandlerUtils::AddVerification(Resp, FogActor);

        S.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Volumetric fog enabled"), Resp);
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to find or spawn ExponentialHeightFog"),
            TEXT("EXECUTION_ERROR"));
    }
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =========================================================================
// setup_global_illumination
// =========================================================================
// Configures global illumination method via console variables
// Options: LumenGI, ScreenSpace, None, RayTraced, Lightmass
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingSetupGlobalIllumination(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    FString Method;
    if (!Payload->TryGetStringField(TEXT("method"), Method) || Method.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("method parameter is required. Valid values: LumenGI, ScreenSpace, None, RayTraced, Lightmass"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    bool bValidMethod = false;
    if (Method == TEXT("LumenGI"))
    {
        IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.DynamicGlobalIlluminationMethod"));
        if (CVar)
        {
            CVar->Set(1); // 1 = Lumen
        }

        IConsoleVariable *CVarRefl = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.ReflectionMethod"));
        if (CVarRefl)
        {
            CVarRefl->Set(1); // 1 = Lumen
        }
        bValidMethod = true;
    }
    else if (Method == TEXT("ScreenSpace"))
    {
        IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.DynamicGlobalIlluminationMethod"));
        if (CVar)
        {
            CVar->Set(2); // SSGI
        }
        bValidMethod = true;
    }
    else if (Method == TEXT("None"))
    {
        IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.DynamicGlobalIlluminationMethod"));
        if (CVar)
        {
            CVar->Set(0);
        }
        bValidMethod = true;
    }
    else if (Method == TEXT("RayTraced"))
    {
        IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.DynamicGlobalIlluminationMethod"));
        if (CVar)
        {
            CVar->Set(3); // 3 = RayTraced (if supported)
        }
        bValidMethod = true;
    }
    else if (Method == TEXT("Lightmass"))
    {
        // Lightmass requires disabling Lumen and enabling static lighting
        IConsoleVariable *CVarGI = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.DynamicGlobalIlluminationMethod"));
        if (CVarGI)
        {
            CVarGI->Set(0); // Disable dynamic GI to use baked
        }
        bValidMethod = true;
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid GI method: %s. Valid values: LumenGI, ScreenSpace, None, RayTraced, Lightmass"), *Method),
            TEXT("INVALID_GI_METHOD"));
        return true;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), bValidMethod);
    Resp->SetStringField(TEXT("method"), Method);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("GI method configured: %s"), *Method), Resp);
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =========================================================================
// configure_shadows
// =========================================================================
// Configures shadow settings (Virtual Shadow Maps)
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingConfigureShadows(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    bool bVirtual = false;
    if (Payload->TryGetBoolField(TEXT("virtualShadowMaps"), bVirtual) ||
        Payload->TryGetBoolField(TEXT("rayTracedShadows"), bVirtual))
    {
        IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.Shadow.Virtual.Enable"));
        if (CVar)
        {
            CVar->Set(bVirtual ? 1 : 0);
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("virtualShadowMaps"), bVirtual);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Shadows configured"), Resp);
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =========================================================================
// set_exposure
// =========================================================================
// Configures auto exposure settings via PostProcessVolume
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingSetExposure(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    // Find unbounded PostProcessVolume or spawn one
    APostProcessVolume *PPV = nullptr;
    TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
    for (AActor *Actor : AllActors)
    {
        if (Actor && Actor->IsA<APostProcessVolume>())
        {
            APostProcessVolume *Candidate = Cast<APostProcessVolume>(Actor);
            if (Candidate->bUnbound)
            {
                PPV = Candidate;
                break;
            }
        }
    }

    if (!PPV)
    {
        PPV = Cast<APostProcessVolume>(SpawnActorInActiveWorld<AActor>(
            APostProcessVolume::StaticClass(), FVector::ZeroVector,
            FRotator::ZeroRotator));
        if (PPV)
        {
            PPV->bUnbound = true;
        }
    }

    if (PPV)
    {
        double MinB = 0.0, MaxB = 0.0;
        if (Payload->TryGetNumberField(TEXT("minBrightness"), MinB))
        {
            PPV->Settings.AutoExposureMinBrightness = (float)MinB;
        }
        if (Payload->TryGetNumberField(TEXT("maxBrightness"), MaxB))
        {
            PPV->Settings.AutoExposureMaxBrightness = (float)MaxB;
        }

        // Bias/Compensation
        double Comp = 0.0;
        if (Payload->TryGetNumberField(TEXT("compensationValue"), Comp))
        {
            PPV->Settings.AutoExposureBias = (float)Comp;
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), PPV->GetActorLabel());

        // Add verification data
        McpHandlerUtils::AddVerification(Resp, PPV);

        S.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Exposure settings applied"), Resp);
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to find/spawn PostProcessVolume"),
            TEXT("EXECUTION_ERROR"));
    }
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =========================================================================
// set_ambient_occlusion
// =========================================================================
// Configures ambient occlusion settings via PostProcessVolume
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingSetAmbientOcclusion(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    // Find unbounded PostProcessVolume or spawn one
    APostProcessVolume *PPV = nullptr;
    TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
    for (AActor *Actor : AllActors)
    {
        if (Actor && Actor->IsA<APostProcessVolume>())
        {
            APostProcessVolume *Candidate = Cast<APostProcessVolume>(Actor);
            if (Candidate->bUnbound)
            {
                PPV = Candidate;
                break;
            }
        }
    }

    if (!PPV)
    {
        PPV = Cast<APostProcessVolume>(SpawnActorInActiveWorld<AActor>(
            APostProcessVolume::StaticClass(), FVector::ZeroVector,
            FRotator::ZeroRotator));
        if (PPV)
        {
            PPV->bUnbound = true;
        }
    }

    if (PPV)
    {
        bool bEnabled = true;
        if (Payload->TryGetBoolField(TEXT("enabled"), bEnabled))
        {
            PPV->Settings.bOverride_AmbientOcclusionIntensity = true;
            PPV->Settings.AmbientOcclusionIntensity =
                bEnabled ? 0.5f : 0.0f; // Default on if enabled, 0 if disabled
        }

        double Intensity;
        if (Payload->TryGetNumberField(TEXT("intensity"), Intensity))
        {
            PPV->Settings.bOverride_AmbientOcclusionIntensity = true;
            PPV->Settings.AmbientOcclusionIntensity = (float)Intensity;
        }

        double Radius;
        if (Payload->TryGetNumberField(TEXT("radius"), Radius))
        {
            PPV->Settings.bOverride_AmbientOcclusionRadius = true;
            PPV->Settings.AmbientOcclusionRadius = (float)Radius;
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), PPV->GetActorLabel());

        // Add verification data
        McpHandlerUtils::AddVerification(Resp, PPV);

        S.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Ambient Occlusion settings configured"), Resp);
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to find/spawn PostProcessVolume"),
            TEXT("EXECUTION_ERROR"));
    }
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =========================================================================
// create_lighting_enabled_level
// =========================================================================
// Creates a new level with basic lighting (DirectionalLight, SkyLight)
// -------------------------------------------------------------------------
bool McpHandlers::BuildEnvironment::HandleLightingCreateLightingEnabledLevel(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    FString Path;
    if (!Payload->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("path required"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Security: Validate path to prevent traversal attacks
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Invalid path: contains traversal or invalid characters"),
            TEXT("INVALID_PATH"));
        return true;
    }
    Path = SanitizedPath;

    FString LevelFilename;
    const bool bHasLevelFilename = FPackageName::TryConvertLongPackageNameToFilename(
        Path, LevelFilename, FPackageName::GetMapPackageExtension());
    const bool bLevelExistsOnDisk = bHasLevelFilename &&
        IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(LevelFilename));
    const bool bLevelExistsInRegistry = FPackageName::DoesPackageExist(Path);
    if (bLevelExistsOnDisk || bLevelExistsInRegistry)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("path"), Path);
        Resp->SetBoolField(TEXT("alreadyExisted"), true);
        Resp->SetBoolField(TEXT("existsAfter"), true);
        Resp->SetStringField(TEXT("levelPath"), Path);

        S.SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Level already exists with lighting path: %s"), *Path), Resp);
        return true;
    }

    if (bHasLevelFilename)
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(LevelFilename), true);
    }

    if (GEditor)
    {
        // Create a new blank map
        GEditor->NewMap();

        // Add basic lighting
        SpawnActorInActiveWorld<AActor>(ADirectionalLight::StaticClass(),
            FVector(0, 0, 500), FRotator(-45, 0, 0), TEXT("Sun"));
        SpawnActorInActiveWorld<AActor>(ASkyLight::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, TEXT("SkyLight"));

        // Save the level using McpSafeLevelSave to prevent Intel GPU driver crashes
        // Explicitly use 5 retries for Intel GPU resilience (max 7.75s total retry time)
        UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
        bool bSaved = EditorWorld && EditorWorld->PersistentLevel &&
            McpSafeLevelSave(EditorWorld->PersistentLevel, Path, 5);
        if (bSaved)
        {
            if (bHasLevelFilename)
            {
                IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
                TArray<FString> FilesToScan;
                FilesToScan.Add(LevelFilename);
                AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
            }

            TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("path"), Path);
            Resp->SetStringField(TEXT("message"), TEXT("Level created with lighting"));

            // Add verification data
            Resp->SetBoolField(TEXT("existsAfter"), true);
            Resp->SetStringField(TEXT("levelPath"), Path);

            S.SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Level created with lighting"), Resp);
        }
        else
        {
            S.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to save level"), TEXT("SAVE_FAILED"));
        }
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Editor not available"),
            TEXT("EDITOR_NOT_AVAILABLE"));
    }
    return true;
#else
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Lighting actions require editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
