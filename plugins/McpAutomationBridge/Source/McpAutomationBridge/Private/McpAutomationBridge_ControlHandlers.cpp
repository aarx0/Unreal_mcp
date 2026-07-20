// =============================================================================
// McpAutomationBridge_ControlHandlers.cpp
// =============================================================================
// Editor control, viewport, PIE, camera, and actor manipulation handlers.
// Both families are classed: dispatch lives in MCP/Calls/McpCalls_ControlActor.cpp
// and McpCalls_ControlEditor.cpp; the HandleControlActor*/HandleControlEditor*
// members here are the implementations their Run() delegates to.
//
// REFACTORING NOTES:
//   - Uses McpVersionCompatibility.h for UE 5.0-5.7 API abstraction
//   - Uses McpHandlerUtils for standardized JSON parsing/responses
//   - Editor subsystems paths vary by UE version
//   - LevelEditor module is optional (may not be available in some contexts)
//
// VERSION COMPATIBILITY:
//   - EditorActorSubsystem: Path varies (Subsystems/ vs root)
//   - LevelEditorSubsystem: UE 5.0+ (optional, conditional include)
//   - UnrealEditorSubsystem: UE 5.0+ (optional, conditional include)
//   - LevelEditorPlaySettings: UE 5.0+ (optional, conditional include)
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"
#include "Dom/JsonObject.h"
#include "Async/Async.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpResponseHelpers.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_ControlHandlers.h"
#include "McpLandscapeMetadataTags.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR

// -----------------------------------------------------------------------------
// Editor-only Includes: Asset & Engine Utilities
// -----------------------------------------------------------------------------
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Editor Subsystems (paths vary by UE version)
// -----------------------------------------------------------------------------
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Viewport Control
// -----------------------------------------------------------------------------
#include "Components/LightComponent.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "IAssetViewport.h"  // For IAssetViewport::GetAssetViewportClient()

// -----------------------------------------------------------------------------
// Editor-only Includes: Level Editor (optional, may not be available)
// -----------------------------------------------------------------------------
#if __has_include("LevelEditor.h")
#include "LevelEditor.h"
#define MCP_HAS_LEVEL_EDITOR_MODULE 1
#else
#define MCP_HAS_LEVEL_EDITOR_MODULE 0
#endif
#if __has_include("Settings/LevelEditorPlaySettings.h")
#include "Settings/LevelEditorPlaySettings.h"
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 1
#else
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Components & Actors
// -----------------------------------------------------------------------------
#include "Components/PrimitiveComponent.h"
#include "EditorViewportClient.h"
#include "Engine/Blueprint.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"

#if __has_include("FileHelpers.h")
#include "FileHelpers.h"
#endif
#include "Animation/SkeletalMeshActor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Export & Output
// -----------------------------------------------------------------------------
#include "Exporters/Exporter.h"
#include "Misc/OutputDevice.h"
#include "UnrealClient.h" // For FViewport::ReadPixels
#include "ImageUtils.h" // For FImageUtils::PNGCompressImageArray / ImageResize (sync screenshot)
#include "Misc/FileHelper.h" // For FFileHelper::SaveArrayToFile
#include "Editor/EditorPerformanceSettings.h" // For background-throttle override (headless screenshots)

#endif // WITH_EDITOR

#if WITH_EDITOR
namespace {
bool IsSafeConsoleArgumentToken(const FString &Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  return !Trimmed.IsEmpty() && !Trimmed.Contains(TEXT("\n")) &&
         !Trimmed.Contains(TEXT("\r")) && !Trimmed.Contains(TEXT("&&")) &&
         !Trimmed.Contains(TEXT("||")) && !Trimmed.Contains(TEXT(";")) &&
         !Trimmed.Contains(TEXT("|")) && !Trimmed.Contains(TEXT("`")) &&
         !Trimmed.Contains(TEXT(" ")) && !Trimmed.Contains(TEXT("\t"));
}

FString MakeSafeConsoleName(const FString &RawName, const TCHAR *Prefix) {
  FString CleanName = FPaths::GetBaseFilename(
      FPaths::GetCleanFilename(RawName.TrimStartAndEnd()));
  if (!IsSafeConsoleArgumentToken(CleanName)) {
    CleanName = FString::Printf(
        TEXT("%s_%s"), Prefix,
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }
  return CleanName;
}

FEditorViewportClient* GetActiveEditorViewportClientForMcp() {
#if MCP_HAS_LEVEL_EDITOR_MODULE
  if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
    if (FLevelEditorModule* LevelEditorModule =
            FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"))) {
      TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule->GetFirstActiveViewport();
      if (ActiveViewport.IsValid()) {
        return &ActiveViewport->GetAssetViewportClient();
      }
    }
  }
#endif

  if (GEditor && GEditor->GetActiveViewport()) {
    return static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
  }

  return nullptr;
}
} // namespace
#endif

// Helper class for capturing export output
/* UE5.6: Use built-in FStringOutputDevice from UnrealString.h */

// Helper functions
// (ExtractVectorField and ExtractRotatorField moved to
// McpAutomationBridgeHelpers.h)

AActor *UMcpAutomationBridgeSubsystem::FindActorByName(const FString &Target, bool bExactMatchOnly) {
#if WITH_EDITOR
  if (!GEditor)
    return nullptr;
  UWorld *World = GEditor->PlayWorld
                      ? GEditor->PlayWorld.Get()
                      : GEditor->GetEditorWorldContext().World();
  return McpHandlerUtils::FindActorByName(
      World, Target,
      bExactMatchOnly ? McpHandlerUtils::EMcpActorNameMatch::Exact
                      : McpHandlerUtils::EMcpActorNameMatch::Fuzzy);
#else
  return nullptr;
#endif
}

/**
 * Spawn a native actor from a class or mesh path and apply the requested transform.
 *
 * The handler accepts optional `location`, `rotation`, and `scale` payload fields,
 * applies scale only when explicitly provided, and returns the actual actor scale
 * so callers can verify the resulting transform without an additional query.
 */
bool McpHandlers::ControlActor::HandleControlActorSpawn(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  // The tool schema advertises three aliases for the class (classPath,
  // className, actorClass); coalesce them so all three actually work
  // (mirrors HandleControlActorFindByClass). classPath wins when several
  // are sent since it is the most explicit form.
  FString ClassPath;
  Payload->TryGetStringField(TEXT("classPath"), ClassPath);
  if (ClassPath.IsEmpty())
    Payload->TryGetStringField(TEXT("className"), ClassPath);
  if (ClassPath.IsEmpty())
    Payload->TryGetStringField(TEXT("actorClass"), ClassPath);
  // The schema also declares 'name'; accept it as an alias of actorName.
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty())
    Payload->TryGetStringField(TEXT("name"), ActorName);

  // Idempotency (see docs/pull-architecture.md): a spawn keyed by `actorName` must
  // not create a duplicate when a dropped response causes the caller to retry. If an
  // actor with this label/name already exists, fail fast with a recognizable code
  // instead of spawning a second one — on a retry ACTOR_ALREADY_EXISTS is the
  // caller's signal that the prior spawn landed (re-query state to confirm). Unkeyed
  // spawns (no actorName) keep their create-each-call behavior.
  if (!ActorName.IsEmpty()) {
    if (AActor *Existing = S.FindActorByName(ActorName, /*bExactMatchOnly*/ true)) {
      S.SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("An actor named '%s' already exists (%s); not spawning a " "duplicate. Re-query state to confirm the prior spawn landed."), *ActorName, *Existing->GetPathName()),
                            TEXT("ACTOR_ALREADY_EXISTS"));
      return true;
    }
  }

  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
  const bool bHasScale = Payload->HasField(TEXT("scale"));
  const FVector Scale =
      ExtractVectorField(Payload, TEXT("scale"), FVector::OneVector);

  UClass *ResolvedClass = nullptr;
  FString MeshPath;
  Payload->TryGetStringField(TEXT("meshPath"), MeshPath);
  UStaticMesh *ResolvedStaticMesh = nullptr;
  USkeletalMesh *ResolvedSkeletalMesh = nullptr;

  // Skip LoadAsset for script classes (e.g. /Script/Engine.CameraActor) to
  // avoid LogEditorAssetSubsystem errors
  if ((ClassPath.StartsWith(TEXT("/")) || ClassPath.Contains(TEXT("/"))) &&
      !ClassPath.StartsWith(TEXT("/Script/"))) {
    const FString SafeClassPath = SanitizeProjectRelativePath(ClassPath);
    if (!SafeClassPath.IsEmpty()) {
      if (UObject *Loaded = McpLoadAssetPieSafe(SafeClassPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
          ResolvedClass = BP->GeneratedClass;
        else if (UClass *C = Cast<UClass>(Loaded))
          ResolvedClass = C;
        else if (UStaticMesh *Mesh = Cast<UStaticMesh>(Loaded))
          ResolvedStaticMesh = Mesh;
        else if (USkeletalMesh *SkelMesh = Cast<USkeletalMesh>(Loaded))
          ResolvedSkeletalMesh = SkelMesh;
      }
    }
  }
  if (!ResolvedClass && !ResolvedStaticMesh && !ResolvedSkeletalMesh)
    ResolvedClass = ResolveClassByName(ClassPath);

  // If explicit mesh path provided for a general spawn request
  if (!ResolvedStaticMesh && !ResolvedSkeletalMesh && !MeshPath.IsEmpty()) {
    const FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
    if (!SafeMeshPath.IsEmpty()) {
      if (UObject *MeshObj = McpLoadAssetPieSafe(SafeMeshPath)) {
        ResolvedStaticMesh = Cast<UStaticMesh>(MeshObj);
        if (!ResolvedStaticMesh)
          ResolvedSkeletalMesh = Cast<USkeletalMesh>(MeshObj);
      }
    }
  }

  // Force StaticMeshActor if we have a resolved mesh, regardless of class input
  // (unless it's a specific subclass)
  bool bSpawnStaticMeshActor = (ResolvedStaticMesh != nullptr);
  bool bSpawnSkeletalMeshActor = (ResolvedSkeletalMesh != nullptr);

  if (!bSpawnStaticMeshActor && !bSpawnSkeletalMeshActor && ResolvedClass) {
    bSpawnStaticMeshActor =
        ResolvedClass->IsChildOf(AStaticMeshActor::StaticClass());
    if (!bSpawnStaticMeshActor)
      bSpawnSkeletalMeshActor =
          ResolvedClass->IsChildOf(ASkeletalMeshActor::StaticClass());
  }

  // Explicitly use StaticMeshActor class if we have a mesh but no class, or if
  // we decided to spawn a static mesh actor
  if (bSpawnStaticMeshActor && !ResolvedClass) {
    ResolvedClass = AStaticMeshActor::StaticClass();
  } else if (bSpawnSkeletalMeshActor && !ResolvedClass) {
    ResolvedClass = ASkeletalMeshActor::StaticClass();
  }

  if (!ResolvedClass && !bSpawnStaticMeshActor && !bSpawnSkeletalMeshActor) {
    if (ClassPath.IsEmpty()) {
      // Distinguish "you sent nothing" from "couldn't resolve what you sent" —
      // the old message printed "Class not found: ." here.
      S.SendAutomationError(Socket, RequestId,
                            TEXT("No class specified: pass classPath, className, or actorClass " "(or meshPath for a static/skeletal mesh actor)."),
                            TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const FString ErrorMsg =
        FString::Printf(TEXT("Class not found: %s. Verify plugin is enabled if "
                             "using a plugin class."),
                        *ClassPath);
    S.SendAutomationError(Socket, RequestId, ErrorMsg, TEXT("CLASS_NOT_FOUND"));
    return true;
  }

  AActor *Spawned = nullptr;

  // Use direct world spawning for both PIE and editor worlds. EditorActorSubsystem
  // routes through viewport placement and hit-proxy rendering, which crashes
  // under NullRHI even when automation provides an explicit transform.
  UWorld *TargetWorld = nullptr;
  if (GEditor->PlayWorld) {
    TargetWorld = GEditor->PlayWorld.Get();
  } else {
    TargetWorld = GEditor->GetEditorWorldContext().World();
  }

  if (!TargetWorld) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor world not available"),
                          TEXT("WORLD_NOT_FOUND"));
    return true;
  }

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
  SpawnParams.ObjectFlags |= RF_Transactional;
  if (!GEditor->PlayWorld) {
    SpawnParams.OverrideLevel = TargetWorld->GetCurrentLevel();
    TargetWorld->Modify();
  }

  UClass *ClassToSpawn =
      ResolvedClass
          ? ResolvedClass
          : (bSpawnStaticMeshActor ? AStaticMeshActor::StaticClass()
                                   : (bSpawnSkeletalMeshActor
                                          ? ASkeletalMeshActor::StaticClass()
                                          : AActor::StaticClass()));
  Spawned = TargetWorld->SpawnActor(ClassToSpawn, &Location, &Rotation,
                                    SpawnParams);

  if (Spawned) {
    Spawned->Modify();
    Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                         ETeleportType::TeleportPhysics);
    if (bSpawnStaticMeshActor) {
      if (AStaticMeshActor *StaticMeshActor = Cast<AStaticMeshActor>(Spawned)) {
        if (UStaticMeshComponent *MeshComponent =
                StaticMeshActor->GetStaticMeshComponent()) {
          if (ResolvedStaticMesh) {
            MeshComponent->SetStaticMesh(ResolvedStaticMesh);
          }
          MeshComponent->SetMobility(EComponentMobility::Movable);
          MeshComponent->MarkRenderStateDirty();
        }
      }
    } else if (bSpawnSkeletalMeshActor) {
      if (ASkeletalMeshActor *SkelActor = Cast<ASkeletalMeshActor>(Spawned)) {
        if (USkeletalMeshComponent *SkelComp =
                SkelActor->GetSkeletalMeshComponent()) {
          if (ResolvedSkeletalMesh) {
            SkelComp->SetSkeletalMesh(ResolvedSkeletalMesh);
          }
          SkelComp->SetMobility(EComponentMobility::Movable);
          SkelComp->MarkRenderStateDirty();
        }
      }
    }
  }

  if (!Spawned) {
    S.SendAutomationError(Socket, RequestId, TEXT("Failed to spawn actor"),
                          TEXT("SPAWN_FAILED"));

    return true;
  }

  if (bHasScale) {
    Spawned->SetActorScale3D(Scale);
  }

  if (!ActorName.IsEmpty()) {
    Spawned->SetActorLabel(ActorName);
  } else {
    // Auto-generate a friendly label from the mesh or class name
    FString BaseName;
    if (ResolvedStaticMesh) {
      BaseName = ResolvedStaticMesh->GetName();
    } else if (ResolvedSkeletalMesh) {
      BaseName = ResolvedSkeletalMesh->GetName();
    } else if (ResolvedClass) {
      BaseName = ResolvedClass->GetName();
      if (BaseName.EndsWith(TEXT("_C"))) {
        BaseName.RemoveFromEnd(TEXT("_C"));
      }
    } else {
      BaseName = TEXT("Actor");
    }
    Spawned->SetActorLabel(BaseName);
  }

  // Build response matching the outputWithActor schema:
  // { actor: { id, name, path }, actorPath, classPath?, meshPath? }
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  
  // Actor object with id, name and path
  TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
  ActorObj->SetStringField(TEXT("id"), Spawned->GetPathName());  // Use path as unique ID
  ActorObj->SetStringField(TEXT("name"), Spawned->GetActorLabel());
  ActorObj->SetStringField(TEXT("path"), Spawned->GetPathName());
  Data->SetObjectField(TEXT("actor"), ActorObj);
  
  // actorPath for convenience
  Data->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
  
  // Provide the resolved class path useful for referencing
  if (ResolvedClass)
    Data->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
  else
    Data->SetStringField(TEXT("classPath"), ClassPath);

  if (ResolvedStaticMesh)
    Data->SetStringField(TEXT("meshPath"), ResolvedStaticMesh->GetPathName());
  else if (ResolvedSkeletalMesh)
    Data->SetStringField(TEXT("meshPath"), ResolvedSkeletalMesh->GetPathName());

  auto MakeVectorArray = [](const FVector &Vec) -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Values;
  };
  Data->SetArrayField(TEXT("scale"), MakeVectorArray(Spawned->GetActorScale3D()));
  
  // Add verification data
	McpHandlerUtils::AddVerification(Data, Spawned);

	S.SendAutomationResponse(Socket, RequestId, true, TEXT("Actor spawned"), Data);
  return true;

#else
  return false;
#endif
}

/**
 * Spawn an actor from a Blueprint class and apply the requested transform fields.
 *
 * Blueprint spawning mirrors the regular actor spawn path by accepting optional
 * `location`, `rotation`, and `scale`, then returning the applied scale in the
 * response payload for client-side verification.
 */
bool McpHandlers::ControlActor::HandleControlActorSpawnBlueprint(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (BlueprintPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("Blueprint path required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // The schema also declares 'name'; accept it as an alias of actorName.
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty())
    Payload->TryGetStringField(TEXT("name"), ActorName);

  // Idempotency (see docs/pull-architecture.md): keyed by `actorName`, fail fast on a
  // name conflict instead of spawning a duplicate when a dropped response causes a
  // retry. On a retry ACTOR_ALREADY_EXISTS confirms the prior spawn landed.
  if (!ActorName.IsEmpty()) {
    if (AActor *Existing = S.FindActorByName(ActorName, /*bExactMatchOnly*/ true)) {
      S.SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("An actor named '%s' already exists (%s); not spawning a " "duplicate. Re-query state to confirm the prior spawn landed."), *ActorName, *Existing->GetPathName()),
                            TEXT("ACTOR_ALREADY_EXISTS"));
      return true;
    }
  }
  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
  const bool bHasScale = Payload->HasField(TEXT("scale"));
  const FVector Scale =
      ExtractVectorField(Payload, TEXT("scale"), FVector::OneVector);

  UClass *ResolvedClass = nullptr;

  // Prefer the same blueprint resolution heuristics used by manage_blueprint
  // so that short names and package paths behave consistently.
  FString NormalizedPath;
  FString LoadError;
  if (!BlueprintPath.IsEmpty()) {
    UBlueprint *BlueprintAsset =
        LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
    if (BlueprintAsset && BlueprintAsset->GeneratedClass) {
      ResolvedClass = BlueprintAsset->GeneratedClass;
    }
  }

  if (!ResolvedClass && (BlueprintPath.StartsWith(TEXT("/")) ||
                         BlueprintPath.Contains(TEXT("/")))) {
    const FString SafeBlueprintPath = SanitizeProjectRelativePath(BlueprintPath);
    if (!SafeBlueprintPath.IsEmpty()) {
      if (UObject *Loaded = McpLoadAssetPieSafe(SafeBlueprintPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
          ResolvedClass = BP->GeneratedClass;
        else if (UClass *C = Cast<UClass>(Loaded))
          ResolvedClass = C;
      }
    }
  }
  if (!ResolvedClass)
    ResolvedClass = ResolveClassByName(BlueprintPath);

  if (!ResolvedClass) {
    S.SendAutomationError(Socket, RequestId, TEXT("Blueprint class not found"),
                          TEXT("CLASS_NOT_FOUND"));
    return true;
  }

  AActor *Spawned = nullptr;
  UWorld *TargetWorld = nullptr;
  if (GEditor->PlayWorld) {
    TargetWorld = GEditor->PlayWorld.Get();
  } else {
    TargetWorld = GEditor->GetEditorWorldContext().World();
  }

  if (!TargetWorld) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor world not available"),
                          TEXT("WORLD_NOT_FOUND"));
    return true;
  }

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
  SpawnParams.ObjectFlags |= RF_Transactional;
  if (!GEditor->PlayWorld) {
    SpawnParams.OverrideLevel = TargetWorld->GetCurrentLevel();
    TargetWorld->Modify();
  }
  Spawned = TargetWorld->SpawnActor(ResolvedClass, &Location, &Rotation,
                                    SpawnParams);
  if (Spawned) {
    Spawned->Modify();
    Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                         ETeleportType::TeleportPhysics);
  }

  if (!Spawned) {
    S.SendAutomationError(Socket, RequestId, TEXT("Failed to spawn blueprint"),
                          TEXT("SPAWN_FAILED"));
    return true;
  }

  if (bHasScale) {
    Spawned->SetActorScale3D(Scale);
  }

  if (!ActorName.IsEmpty())
    Spawned->SetActorLabel(ActorName);

  // Build response matching the outputWithActor schema:
  // { actor: { id, name, path }, actorPath, classPath }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  
  // Actor object with id, name and path
  TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
  ActorObj->SetStringField(TEXT("id"), Spawned->GetPathName());  // Use path as unique ID
  ActorObj->SetStringField(TEXT("name"), Spawned->GetActorLabel());
  ActorObj->SetStringField(TEXT("path"), Spawned->GetPathName());
  Resp->SetObjectField(TEXT("actor"), ActorObj);
  
  // actorPath for convenience
  Resp->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
  Resp->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
  auto MakeVectorArray = [](const FVector &Vec) -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Values;
  };
  Resp->SetArrayField(TEXT("scale"), MakeVectorArray(Spawned->GetActorScale3D()));
  
  // Add verification data
	McpHandlerUtils::AddVerification(Resp, Spawned);

	S.SendAutomationResponse(Socket, RequestId, true, TEXT("Blueprint spawned"),
                         Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorDelete(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  TArray<FString> Targets;
  const TArray<TSharedPtr<FJsonValue>> *NamesArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("actorNames"), NamesArray) && NamesArray) {
    for (const TSharedPtr<FJsonValue> &Entry : *NamesArray) {
      if (Entry.IsValid() && Entry->Type == EJson::String) {
        const FString Value = Entry->AsString().TrimStartAndEnd();
        if (!Value.IsEmpty())
          Targets.AddUnique(Value);
      }
    }
  }

  FString SingleName;
  if (Targets.Num() == 0) {
    Payload->TryGetStringField(TEXT("actorName"), SingleName);
    if (!SingleName.IsEmpty())
      Targets.AddUnique(SingleName);
  }

  if (Targets.Num() == 0) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("actorName or actorNames required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<FString> Deleted;
  TArray<FString> Missing;

  for (const FString &Name : Targets) {
    // CRITICAL FIX: Use exact match only for delete operations to prevent
    // fuzzy matching from deleting wrong actors (e.g., "TestActor_Copy" when
    // searching for "TestActor")
    AActor *Found = S.FindActorByName(Name, true);
    if (!Found) {
      Missing.Add(Name);
      continue;
    }
	if (ActorSS->DestroyActor(Found)) {
			Deleted.Add(Name);
    } else
      Missing.Add(Name);
  }

  const bool bAllDeleted = Missing.Num() == 0;
  const bool bAnyDeleted = Deleted.Num() > 0;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bAllDeleted);
  Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Name : Deleted)
    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
  Resp->SetArrayField(TEXT("deleted"), DeletedArray);

  if (Missing.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> MissingArray;
    for (const FString &Name : Missing)
      MissingArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("missing"), MissingArray);
  }

  FString Message;
  FString ErrorCode;
  if (!bAnyDeleted && Missing.Num() > 0) {
    Message = TEXT("Actors not found");
    ErrorCode = TEXT("NOT_FOUND");
  } else {
    Message = bAllDeleted ? TEXT("Actors deleted")
                          : TEXT("Some actors could not be deleted");
    ErrorCode = bAllDeleted ? FString() : TEXT("DELETE_PARTIAL");
  }

  // Add verification data for delete operations
  Resp->SetBoolField(TEXT("existsAfter"), false);
  Resp->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));

  if (!bAllDeleted && Missing.Num() > 0 && !bAnyDeleted) {
    S.SendAutomationError(Socket, RequestId, Message, ErrorCode);
  } else {
    S.SendAutomationResponse(Socket, RequestId, true, Message, Resp);
  }
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorApplyForce(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FVector ForceVector =
      ExtractVectorField(Payload, TEXT("force"), FVector::ZeroVector);

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  UPrimitiveComponent *Prim =
      Found->FindComponentByClass<UPrimitiveComponent>();
  if (!Prim) {
    if (UStaticMeshComponent *SMC =
            Found->FindComponentByClass<UStaticMeshComponent>())
      Prim = SMC;
  }

  if (!Prim) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("No component to apply force"),
                          TEXT("NO_COMPONENT"));
    return true;
  }

  if (Prim->Mobility == EComponentMobility::Static)
    Prim->SetMobility(EComponentMobility::Movable);

  // Ensure collision is enabled for physics
  if (Prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision) {
    Prim->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  }

  // Check if collision geometry exists (common failure for empty
  // StaticMeshActors)
  if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(Prim)) {
    if (!SMC->GetStaticMesh()) {
      S.SendAutomationError(Socket, RequestId,
                            TEXT("StaticMeshComponent has no StaticMesh assigned."),
                            TEXT("PHYSICS_FAILED"));
      return true;
    }
    if (!SMC->GetStaticMesh()->GetBodySetup()) {
      S.SendAutomationError(Socket, RequestId,
                            TEXT("StaticMesh has no collision geometry (BodySetup is null)."),
                            TEXT("PHYSICS_FAILED"));
      return true;
    }
  }

  if (!Prim->IsSimulatingPhysics()) {
    Prim->SetSimulatePhysics(true);
    // Must recreate physics state for the body to be properly initialized in
    // Editor
    Prim->RecreatePhysicsState();
  }

  Prim->AddForce(ForceVector);
  Prim->WakeAllRigidBodies();
  Prim->MarkRenderStateDirty();

  // Verify physics state
  const bool bIsSimulating = Prim->IsSimulatingPhysics();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("simulating"), bIsSimulating);
  TArray<TSharedPtr<FJsonValue>> Applied;
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.X));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Y));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Z));
  Data->SetArrayField(TEXT("applied"), Applied);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

  if (!bIsSimulating) {
    FString FailureReason = TEXT("Failed to enable physics simulation.");
    if (Prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision) {
      FailureReason += TEXT(" Collision is disabled.");
    } else if (Prim->Mobility != EComponentMobility::Movable) {
      FailureReason += TEXT(" Component is not Movable.");
    }
    S.SendAutomationResponse(Socket, RequestId, false, FailureReason, Data,
                             TEXT("PHYSICS_FAILED"));
    return true;
  }

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	S.SendAutomationResponse(Socket, RequestId, true, TEXT("Force applied"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorSetTransform(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // Only the fields the caller actually sent are applied; a call with none is a
  // no-op and errors rather than faking success.
  const bool bHasLocation = Payload->HasField(TEXT("location"));
  const bool bHasRotation = Payload->HasField(TEXT("rotation"));
  const bool bHasScale = Payload->HasField(TEXT("scale"));
  if (!bHasLocation && !bHasRotation && !bHasScale) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("set_transform needs at least one of location, rotation, scale"),
                          TEXT("NO_CHANGES_REQUESTED"));
    return true;
  }

  // Omitted fields default to the actor's current value, so applying all three
  // is a no-op for the ones not requested. Setting a transform is atomic — the
  // response echoes the resulting transform for the caller to confirm.
  const FVector Location =
      ExtractVectorField(Payload, TEXT("location"), Found->GetActorLocation());
  const FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), Found->GetActorRotation());
  const FVector Scale =
      ExtractVectorField(Payload, TEXT("scale"), Found->GetActorScale3D());

  Found->Modify();
  Found->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
  Found->SetActorRotation(Rotation, ETeleportType::TeleportPhysics);
  Found->SetActorScale3D(Scale);

  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  TArray<TSharedPtr<FJsonValue>> AppliedProperties;
  if (bHasLocation) AppliedProperties.Add(MakeShared<FJsonValueString>(TEXT("location")));
  if (bHasRotation) AppliedProperties.Add(MakeShared<FJsonValueString>(TEXT("rotation")));
  if (bHasScale) AppliedProperties.Add(MakeShared<FJsonValueString>(TEXT("scale")));

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetArrayField(TEXT("appliedProperties"), AppliedProperties);
  Data->SetArrayField(TEXT("location"), MakeArray(Found->GetActorLocation()));
  Data->SetArrayField(TEXT("scale"), MakeArray(Found->GetActorScale3D()));
  McpHandlerUtils::AddVerification(Data, Found);

  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Actor transform updated"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorGetTransform(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  const FTransform Current = Found->GetActorTransform();
  const FVector Location = Current.GetLocation();
  const FRotator Rotation = Current.GetRotation().Rotator();
  const FVector Scale = Current.GetScale3D();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("location"), MakeArray(Location));
  TArray<TSharedPtr<FJsonValue>> RotArray;
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
  Data->SetArrayField(TEXT("rotation"), RotArray);
  Data->SetArrayField(TEXT("scale"), MakeArray(Scale));

  S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Actor transform retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorSetVisibility(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // visible must be explicitly requested — never silently apply the default.
  if (!Payload->HasField(TEXT("visible"))) {
    S.SendAutomationError(Socket, RequestId, TEXT("visible is required"),
                          TEXT("NO_CHANGES_REQUESTED"));
    return true;
  }
  bool bVisible = true;
  Payload->TryGetBoolField(TEXT("visible"), bVisible);

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  Found->Modify();
  Found->SetActorHiddenInGame(!bVisible);
  Found->SetActorEnableCollision(bVisible);

  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp)
      continue;
    if (UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(Comp)) {
      Prim->SetVisibility(bVisible, true);
      Prim->SetHiddenInGame(!bVisible);
    }
  }

  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  const bool bIsHidden = Found->IsHidden();
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("visible"), !bIsHidden);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

  if (bIsHidden != !bVisible) {
    S.SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to set actor visibility"), Data,
                             TEXT("VISIBILITY_MISMATCH"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> AppliedProperties;
  AppliedProperties.Add(MakeShared<FJsonValueString>(TEXT("visible")));
  Data->SetArrayField(TEXT("appliedProperties"), AppliedProperties);
  McpHandlerUtils::AddVerification(Data, Found);

  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Actor visibility updated"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorAddComponent(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ComponentType;
  Payload->TryGetStringField(TEXT("componentType"), ComponentType);
  if (ComponentType.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("componentType required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  UClass *ComponentClass = ResolveClassByName(ComponentType);
  if (!ComponentClass ||
      !ComponentClass->IsChildOf(UActorComponent::StaticClass())) {
    S.SendAutomationError(Socket, RequestId, TEXT("Component class not found"),
                          TEXT("CLASS_NOT_FOUND"));
    return true;
  }

  if (ComponentName.TrimStartAndEnd().IsEmpty())
    ComponentName = FString::Printf(TEXT("%s_%d"), *ComponentClass->GetName(),
                                    FMath::Rand());

  FName DesiredName = FName(*ComponentName);
  UActorComponent *NewComponent = NewObject<UActorComponent>(
      Found, ComponentClass, DesiredName, RF_Transactional);
  if (!NewComponent) {
    S.SendAutomationError(Socket, RequestId, TEXT("Failed to create component"),
                          TEXT("CREATE_COMPONENT_FAILED"));
    return true;
  }

  Found->Modify();
  NewComponent->SetFlags(RF_Transactional);
  Found->AddInstanceComponent(NewComponent);
  NewComponent->OnComponentCreated();

  if (USceneComponent *SceneComp = Cast<USceneComponent>(NewComponent)) {
    if (Found->GetRootComponent() && !SceneComp->GetAttachParent()) {
      SceneComp->SetupAttachment(Found->GetRootComponent());
    }
  }

  // Lights must be movable to work without baking.
  if (NewComponent->IsA(ULightComponent::StaticClass())) {
    if (USceneComponent *SC = Cast<USceneComponent>(NewComponent)) {
      SC->SetMobility(EComponentMobility::Movable);
    }
  }

  // meshPath convenience for a static mesh component.
  if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(NewComponent)) {
    FString MeshPath;
    if (Payload->TryGetStringField(TEXT("meshPath"), MeshPath) &&
        !MeshPath.IsEmpty()) {
      const FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
      if (!SafeMeshPath.IsEmpty()) {
        if (UObject *LoadedMesh = McpLoadAssetPieSafe(SafeMeshPath)) {
          if (UStaticMesh *Mesh = Cast<UStaticMesh>(LoadedMesh)) {
            SMC->SetStaticMesh(Mesh);
          }
        }
      }
    }
  }

  NewComponent->RegisterComponent();
  if (USceneComponent *SceneComp = Cast<USceneComponent>(NewComponent))
    SceneComp->UpdateComponentToWorld();
  NewComponent->MarkPackageDirty();
  Found->MarkPackageDirty();

  // add_component creates + registers only; set initial properties atomically
  // afterwards via set_component_property (typed-params migration -- no init map).
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("componentName"), NewComponent->GetName());
  Resp->SetStringField(TEXT("componentPath"), NewComponent->GetPathName());
  Resp->SetStringField(TEXT("componentClass"), ComponentClass->GetPathName());
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Component added"), Resp,
                         FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorSetComponentProperties(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  if (ComponentName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("componentName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString PropertyName;
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  if (PropertyName.IsEmpty())
    Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
  const TSharedPtr<FJsonObject> *PropertiesMap = nullptr;
  Payload->TryGetObjectField(TEXT("properties"), PropertiesMap);
  const bool bMapMode = PropertiesMap && PropertiesMap->IsValid();
  if (PropertyName.IsEmpty() && !bMapMode) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("propertyName (or propertyPath), or a 'properties' map, required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!PropertyName.IsEmpty() && bMapMode) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("send either propertyName or a 'properties' map, not both"),
                          TEXT("AMBIGUOUS_VALUE"));
    return true;
  }

  // Discriminated value (single-property mode): exactly one typed field carries
  // both the value and the caller's intended type. Each field is a real schema
  // type, so it transmits correctly (no stringified blobs) — vectorValue
  // arrives as a real object. Map mode carries raw JSON values instead; the
  // strict importer type-checks those.
  struct FTypedValue {
    const TCHAR *Field;
    const TCHAR *Kind;
    TSharedPtr<FJsonValue> Json;
  };
  TArray<FTypedValue> Present;
  if (!bMapMode) {
    bool B = false;
    double N = 0.0;
    FString Str;
    const TSharedPtr<FJsonObject> *Obj = nullptr;
    if (Payload->TryGetBoolField(TEXT("boolValue"), B))
      Present.Add({TEXT("boolValue"), TEXT("bool"), MakeShared<FJsonValueBoolean>(B)});
    if (Payload->TryGetNumberField(TEXT("intValue"), N))
      Present.Add({TEXT("intValue"), TEXT("int"), MakeShared<FJsonValueNumber>(N)});
    if (Payload->TryGetNumberField(TEXT("floatValue"), N))
      Present.Add({TEXT("floatValue"), TEXT("float"), MakeShared<FJsonValueNumber>(N)});
    if (Payload->TryGetStringField(TEXT("stringValue"), Str))
      Present.Add({TEXT("stringValue"), TEXT("string"), MakeShared<FJsonValueString>(Str)});
    if (Payload->TryGetObjectField(TEXT("vectorValue"), Obj) && Obj && Obj->IsValid())
      Present.Add({TEXT("vectorValue"), TEXT("vector"), MakeShared<FJsonValueObject>(*Obj)});

    if (Present.Num() == 0) {
      S.SendAutomationError(Socket, RequestId,
                            TEXT("set exactly one typed value field: boolValue, intValue, " "floatValue, stringValue, or vectorValue"),
                            TEXT("NO_CHANGES_REQUESTED"));
      return true;
    }
    if (Present.Num() > 1) {
      TArray<FString> Names;
      for (const FTypedValue &V : Present)
        Names.Add(V.Field);
      S.SendAutomationError(Socket, RequestId,
                            *FString::Printf(TEXT("set exactly one typed value field, got %d: %s"), Present.Num(), *FString::Join(Names, TEXT(", "))),
                            TEXT("AMBIGUOUS_VALUE"));
      return true;
    }
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  UActorComponent *TargetComponent = McpHandlerUtils::FindActorComponentByName(Found, ComponentName);
  if (!TargetComponent) {
    S.SendAutomationError(Socket, RequestId, TEXT("Component not found"),
                          TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  // Cross-check helper: the caller's intended type (which typed field they set)
  // must match a property's real reflected type. A mismatch fails loud.
  // "string" includes object references set by asset path — parity with
  // inspect set_property, whose docs both actions share.
  auto MatchesKind = [](FProperty *P, const FString &Kind) -> bool {
    if (Kind == TEXT("bool"))
      return CastField<FBoolProperty>(P) != nullptr;
    if (Kind == TEXT("int"))
      return CastField<FIntProperty>(P) || CastField<FInt64Property>(P) ||
             CastField<FInt16Property>(P) || CastField<FInt8Property>(P) ||
             CastField<FUInt32Property>(P) || CastField<FByteProperty>(P);
    if (Kind == TEXT("float"))
      return CastField<FFloatProperty>(P) || CastField<FDoubleProperty>(P);
    if (Kind == TEXT("string"))
      return CastField<FStrProperty>(P) || CastField<FNameProperty>(P) ||
             CastField<FTextProperty>(P) || CastField<FEnumProperty>(P) ||
             CastField<FByteProperty>(P) || CastField<FObjectPropertyBase>(P);
    if (Kind == TEXT("vector")) {
      FStructProperty *SP = CastField<FStructProperty>(P);
      return SP && SP->Struct == TBaseStructure<FVector>::Get();
    }
    return false;
  };

  // One property: special-cased setters (SimulatePhysics lives inside
  // BodyInstance with no direct FProperty; Mobility needs component
  // re-registration) then generic reflection. Empty return = success.
  auto ApplyOne = [&](const FString &InName, const TSharedPtr<FJsonValue> &InJson) -> FString {
    if (InName.Equals(TEXT("SimulatePhysics"), ESearchCase::IgnoreCase) ||
        InName.Equals(TEXT("bSimulatePhysics"), ESearchCase::IgnoreCase)) {
      UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(TargetComponent);
      bool bSim = false;
      if (!Prim || !InJson->TryGetBool(bSim))
        return TEXT("SimulatePhysics expects a bool value on a primitive component");
      Prim->SetSimulatePhysics(bSim);
      return FString();
    }
    if (InName.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase)) {
      USceneComponent *SC = Cast<USceneComponent>(TargetComponent);
      FString MobStr;
      if (!SC || !InJson->TryGetString(MobStr))
        return TEXT("Mobility expects a string value (Static, Stationary, or Movable) on a scene component");
      const int64 MobVal =
          StaticEnum<EComponentMobility::Type>()->GetValueByNameString(MobStr);
      if (MobVal == INDEX_NONE)
        return FString::Printf(TEXT("Mobility: '%s' is not valid (Static, Stationary, Movable)"), *MobStr);
      SC->SetMobility((EComponentMobility::Type)MobVal);
      return FString();
    }
    FProperty *Property = TargetComponent->GetClass()->FindPropertyByName(*InName);
    if (!Property)
      return FString::Printf(TEXT("Property not found: %s"), *InName);
    FString ApplyError;
    if (!ApplyJsonValueToProperty(TargetComponent, Property, InJson, ApplyError))
      return ApplyError;
    return FString();
  };

  TargetComponent->Modify();

  TArray<TSharedPtr<FJsonValue>> AppliedArray;
  TArray<TSharedPtr<FJsonValue>> FailedArray;
  if (bMapMode) {
    // Fail-in-place: each entry lands or reports; no rollback.
    for (const auto &Pair : (*PropertiesMap)->Values) {
      const FString EntryError = ApplyOne(Pair.Key, Pair.Value);
      if (EntryError.IsEmpty()) {
        AppliedArray.Add(MakeShared<FJsonValueString>(Pair.Key));
      } else {
        TSharedPtr<FJsonObject> FailObj = MakeShared<FJsonObject>();
        FailObj->SetStringField(TEXT("name"), Pair.Key);
        FailObj->SetStringField(TEXT("error"), EntryError);
        FailedArray.Add(MakeShared<FJsonValueObject>(FailObj));
      }
    }
    if (AppliedArray.Num() + FailedArray.Num() == 0) {
      S.SendAutomationError(Socket, RequestId, TEXT("'properties' map is empty"),
                            TEXT("NO_CHANGES_REQUESTED"));
      return true;
    }
  } else {
    const FTypedValue &Value = Present[0];
    const FString ValueKind(Value.Kind);
    const bool bSpecialCased =
        PropertyName.Equals(TEXT("SimulatePhysics"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("bSimulatePhysics"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase);
    if (!bSpecialCased) {
      FProperty *Property =
          TargetComponent->GetClass()->FindPropertyByName(*PropertyName);
      if (!Property) {
        S.SendAutomationError(Socket, RequestId,
                              *FString::Printf(TEXT("Property not found: %s"), *PropertyName),
                              TEXT("PROPERTY_NOT_FOUND"));
        return true;
      }
      if (!MatchesKind(Property, ValueKind)) {
        S.SendAutomationError(Socket, RequestId,
                              *FString::Printf(TEXT("%s: you sent %sValue but the property type is '%s'"), *PropertyName, Value.Kind, *Property->GetCPPType()),
                              TEXT("VALUE_TYPE_MISMATCH"));
        return true;
      }
    }
    const FString ApplyError = ApplyOne(PropertyName, Value.Json);
    if (!ApplyError.IsEmpty()) {
      S.SendAutomationError(Socket, RequestId,
                            *FString::Printf(TEXT("Failed to set %s: %s"), *PropertyName, *ApplyError),
                            TEXT("PROPERTY_WRITE_FAILED"));
      return true;
    }
    AppliedArray.Add(MakeShared<FJsonValueString>(PropertyName));
  }

  if (USceneComponent *SceneComponent =
          Cast<USceneComponent>(TargetComponent)) {
    SceneComponent->MarkRenderStateDirty();
    SceneComponent->UpdateComponentToWorld();
  }
  TargetComponent->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("appliedProperties"), AppliedArray);
  if (FailedArray.Num() > 0)
    Data->SetArrayField(TEXT("failedProperties"), FailedArray);
  McpHandlerUtils::AddVerification(Data, Found);

  if (FailedArray.Num() > 0) {
    S.SendAutomationResponse(Socket, RequestId, false,
                             FString::Printf(TEXT("Applied %d of %d properties"), AppliedArray.Num(), AppliedArray.Num() + FailedArray.Num()),
                             Data, TEXT("PARTIAL_FAILURE"));
    return true;
  }
  S.SendAutomationResponse(Socket, RequestId, true,
                         AppliedArray.Num() == 1
                             ? TEXT("Component property updated")
                             : *FString::Printf(TEXT("Component properties updated (%d)"), AppliedArray.Num()),
                         Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorGetComponents(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);

  // Also accept "objectPath" as an alias, common in inspections
  if (TargetName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("objectPath"), TargetName);
  }

  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("actorName or objectPath required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  // Fallback: Check if it's a Blueprint asset to inspect CDO components
  if (!Found) {
    const FString SafeTargetPath = SanitizeProjectRelativePath(TargetName);
    if (!SafeTargetPath.IsEmpty()) {
      if (UObject *Asset = McpLoadAssetPieSafe(SafeTargetPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Asset)) {
          if (BP->GeneratedClass) {
            Found = Cast<AActor>(BP->GeneratedClass->GetDefaultObject());
          }
        }
      }
    }
  }

  if (!Found) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("Actor or Blueprint not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> ComponentsArray;
  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp)
      continue;
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("name"), Comp->GetName());
    Entry->SetStringField(TEXT("class"), Comp->GetClass()
                                             ? Comp->GetClass()->GetPathName()
                                             : TEXT(""));
    Entry->SetStringField(TEXT("path"), Comp->GetPathName());
    if (USceneComponent *SceneComp = Cast<USceneComponent>(Comp)) {
      FVector Loc = SceneComp->GetRelativeLocation();
      FRotator Rot = SceneComp->GetRelativeRotation();
      FVector Scale = SceneComp->GetRelativeScale3D();

      TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
      LocObj->SetNumberField(TEXT("x"), Loc.X);
      LocObj->SetNumberField(TEXT("y"), Loc.Y);
      LocObj->SetNumberField(TEXT("z"), Loc.Z);
      Entry->SetObjectField(TEXT("relativeLocation"), LocObj);

      TSharedPtr<FJsonObject> RotObj = McpHandlerUtils::CreateResultObject();
      RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
      RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
      RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
      Entry->SetObjectField(TEXT("relativeRotation"), RotObj);

      TSharedPtr<FJsonObject> ScaleObj = McpHandlerUtils::CreateResultObject();
      ScaleObj->SetNumberField(TEXT("x"), Scale.X);
      ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
      ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
      Entry->SetObjectField(TEXT("relativeScale"), ScaleObj);
    }
    ComponentsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("components"), ComponentsArray);
  Data->SetNumberField(TEXT("count"), ComponentsArray.Num());
  
  // Add verification data
  if (Found) {
    McpHandlerUtils::AddVerification(Data, Found);
  }
  
  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Actor components retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorDuplicate(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // Idempotency (see docs/pull-architecture.md): if the target newName already exists,
  // fail fast instead of creating a second duplicate when a dropped response causes a
  // retry. On a retry ACTOR_ALREADY_EXISTS confirms the prior duplicate landed.
  FString NewName;
  Payload->TryGetStringField(TEXT("newName"), NewName);
  if (!NewName.TrimStartAndEnd().IsEmpty()) {
    if (AActor *ExistingDup = S.FindActorByName(NewName, /*bExactMatchOnly*/ true)) {
      S.SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("An actor named '%s' already exists (%s); not creating a " "duplicate. Re-query state to confirm the prior duplicate landed."), *NewName, *ExistingDup->GetPathName()),
                            TEXT("ACTOR_ALREADY_EXISTS"));
      return true;
    }
  }

  FVector Offset =
      ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  AActor *Duplicated =
      ActorSS->DuplicateActor(Found, Found->GetWorld(), Offset);
  if (!Duplicated) {
    S.SendAutomationError(Socket, RequestId, TEXT("Failed to duplicate actor"),
                          TEXT("DUPLICATE_FAILED"));
    return true;
  }

  if (!NewName.TrimStartAndEnd().IsEmpty())
    Duplicated->SetActorLabel(NewName);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("source"), Found->GetActorLabel());
  Data->SetStringField(TEXT("actorName"), Duplicated->GetActorLabel());
  Data->SetStringField(TEXT("actorPath"), Duplicated->GetPathName());

  // Add verification data
  McpHandlerUtils::AddVerification(Data, Duplicated);

  TArray<TSharedPtr<FJsonValue>> OffsetArray;
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.X));
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Y));
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Z));
  Data->SetArrayField(TEXT("offset"), OffsetArray);

	S.SendAutomationResponse(Socket, RequestId, true, TEXT("Actor duplicated"),
                          Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorAttach(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString ChildName;
  Payload->TryGetStringField(TEXT("childActor"), ChildName);
  FString ParentName;
  Payload->TryGetStringField(TEXT("parentActor"), ParentName);
  if (ChildName.IsEmpty() || ParentName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("childActor and parentActor required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Child = S.FindActorByName(ChildName);
  AActor *Parent = S.FindActorByName(ParentName);
  if (!Child || !Parent) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("Child or parent actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  if (Child == Parent) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("Cannot attach actor to itself"),
                          TEXT("CYCLE_DETECTED"));
    return true;
  }

  USceneComponent *ChildRoot = Child->GetRootComponent();
  USceneComponent *ParentRoot = Parent->GetRootComponent();
  if (!ChildRoot || !ParentRoot) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("Actor missing root component"),
                          TEXT("ROOT_MISSING"));
    return true;
  }

  Child->Modify();
  ChildRoot->Modify();
  ChildRoot->AttachToComponent(ParentRoot,
                               FAttachmentTransformRules::KeepWorldTransform);
  Child->SetOwner(Parent);
  Child->MarkPackageDirty();
  Parent->MarkPackageDirty();

  // Verify attachment
  bool bAttached = false;
  if (Child->GetRootComponent() &&
      Child->GetRootComponent()->GetAttachParent() == ParentRoot) {
    bAttached = true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("child"), Child->GetActorLabel());
  Data->SetStringField(TEXT("parent"), Parent->GetActorLabel());
  Data->SetBoolField(TEXT("attached"), bAttached);

  if (!bAttached) {
    S.SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to attach actor"), Data,
                             TEXT("ATTACH_FAILED"));
    return true;
  }

  // Add verification data for the child actor
	McpHandlerUtils::AddVerification(Data, Child);

	S.SendAutomationResponse(Socket, RequestId, true, TEXT("Actor attached"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorDetach(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  // The schema documents 'childActor' for detach (mirroring attach); accept it
  // alongside actorName.
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
    Payload->TryGetStringField(TEXT("childActor"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("actorName (or childActor) required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  USceneComponent *RootComp = Found->GetRootComponent();
  if (!RootComp || !RootComp->GetAttachParent()) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("note"), TEXT("Actor was not attached"));
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Actor already detached"), Resp, FString());
    return true;
  }

  Found->Modify();
  RootComp->Modify();
  RootComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
  Found->SetOwner(nullptr);
  Found->MarkPackageDirty();

  // Verify detachment
  const bool bDetached = (RootComp->GetAttachParent() == nullptr);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetBoolField(TEXT("detached"), bDetached);

  if (!bDetached) {
    S.SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to detach actor"), Data,
                             TEXT("DETACH_FAILED"));
    return true;
  }

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	S.SendAutomationResponse(Socket, RequestId, true, TEXT("Actor detached"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorFindByTag(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TagValue.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("tag required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Security: Validate tag format - reject path traversal attempts
  if (TagValue.Contains(TEXT("..")) || TagValue.Contains(TEXT("\\")) || TagValue.Contains(TEXT("/"))) {
    S.SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid tag: '%s'. Path separators and traversal characters are not allowed."), *TagValue),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString MatchType;
  Payload->TryGetStringField(TEXT("matchType"), MatchType);
  MatchType = MatchType.ToLower();
  FName TagName(*TagValue);

  int32 Limit = 50;
  {
    double LimitNum = 0.0;
    if (Payload->TryGetNumberField(TEXT("limit"), LimitNum)) {
      Limit = FMath::Clamp(static_cast<int32>(LimitNum), 1, 200);
    }
  }

  TArray<TSharedPtr<FJsonValue>> Matches;
  int32 Matched = 0;
  bool bTruncated = false;

  // Log tag search details
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleControlActorFindByTag: Searching for tag '%s' (FName: %s)"),
         *TagValue, *TagName.ToString());
  bool bIsPieWorld = false;
  UWorld *World = McpHandlerUtils::GetActorLookupWorld(&bIsPieWorld);
  TArray<AActor *> AllActors;
  if (World) {
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (AActor *Actor = *It) {
        AllActors.Add(Actor);
      }
    }
  }

  // Log total actors being searched
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleControlActorFindByTag: Searching %d actors in level"), AllActors.Num());
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    bool bMatches = false;
    if (MatchType == TEXT("contains")) {
      for (const FName &Existing : Actor->Tags) {
        if (Existing.ToString().Contains(TagValue, ESearchCase::IgnoreCase)) {
          bMatches = true;
          break;
        }
      }
    } else {
      bMatches = Actor->ActorHasTag(TagName);
    }

    // Log actor tags for troubleshooting at verbose level
    if (Actor->Tags.Num() > 0) {
      FString TagList;
      for (const FName& T : Actor->Tags) {
        TagList += T.ToString() + TEXT(", ");
      }
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("HandleControlActorFindByTag: Actor '%s' has tags: [%s] - match=%d"),
             *Actor->GetActorLabel(), *TagList, bMatches);
    }
    if (bMatches) {
      ++Matched;
      if (Matches.Num() >= Limit) {
        bTruncated = true;
        continue;
      }
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
      Entry->SetStringField(TEXT("path"), Actor->GetPathName());
      Entry->SetStringField(TEXT("class"),
                            Actor->GetClass() ? Actor->GetClass()->GetPathName()
                                              : TEXT(""));
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetNumberField(TEXT("count"), Matches.Num());
  Data->SetNumberField(TEXT("matched"), Matched);
  Data->SetBoolField(TEXT("truncated"), bTruncated);
  Data->SetBoolField(TEXT("isPieWorld"), bIsPieWorld);
  if (World) {
    Data->SetStringField(TEXT("worldName"), World->GetName());
    Data->SetStringField(TEXT("worldPackage"), World->GetOutermost()->GetName());
    Data->SetBoolField(TEXT("hasBegunPlay"), World->HasBegunPlay());
  }
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Actors found"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorAddTag(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TargetName.IsEmpty() || TagValue.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName and tag required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  const FName TagName(*TagValue);
  const bool bAlreadyHad = Found->Tags.Contains(TagName);

  Found->Modify();
  Found->Tags.AddUnique(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasPresent"), bAlreadyHad);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("tag"), TagName.ToString());

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	S.SendAutomationResponse(Socket, RequestId, true, TEXT("Tag applied to actor"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorFindByName(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString Query;
  Payload->TryGetStringField(TEXT("name"), Query);
  if (Query.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("name required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Security: Validate query format - reject path traversal attempts
  if (Query.Contains(TEXT("..")) || Query.Contains(TEXT("\\")) || Query.Contains(TEXT("/"))) {
    S.SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid name query: '%s'. Path separators and traversal characters are not allowed."), *Query),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 Limit = 50;
  {
    double LimitNum = 0.0;
    if (Payload->TryGetNumberField(TEXT("limit"), LimitNum)) {
      Limit = FMath::Clamp(static_cast<int32>(LimitNum), 1, 200);
    }
  }

  bool bIsPieWorld = false;
  UWorld *World = McpHandlerUtils::GetActorLookupWorld(&bIsPieWorld);
  TArray<AActor *> AllActors;
  if (World) {
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (AActor *Actor = *It) {
        AllActors.Add(Actor);
      }
    }
  }
  TArray<TSharedPtr<FJsonValue>> Matches;
  int32 Matched = 0;
  bool bTruncated = false;
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    const FString Label = Actor->GetActorLabel();
    const FString Name = Actor->GetName();
    const FString Path = Actor->GetPathName();
    const bool bMatches = Label.Contains(Query, ESearchCase::IgnoreCase) ||
                          Name.Contains(Query, ESearchCase::IgnoreCase) ||
                          Path.Contains(Query, ESearchCase::IgnoreCase);
    if (bMatches) {
      ++Matched;
      if (Matches.Num() >= Limit) {
        bTruncated = true;
        continue;
      }
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("label"), Label);
      Entry->SetStringField(TEXT("name"), Name);
      Entry->SetStringField(TEXT("path"), Path);
      Entry->SetStringField(TEXT("class"),
                            Actor->GetClass() ? Actor->GetClass()->GetPathName()
                                              : TEXT(""));
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetNumberField(TEXT("count"), Matches.Num());
  Data->SetNumberField(TEXT("matched"), Matched);
  Data->SetBoolField(TEXT("truncated"), bTruncated);
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetStringField(TEXT("query"), Query);
  Data->SetBoolField(TEXT("isPieWorld"), bIsPieWorld);
  if (World) {
    Data->SetStringField(TEXT("worldName"), World->GetName());
    Data->SetStringField(TEXT("worldPackage"), World->GetOutermost()->GetName());
    Data->SetBoolField(TEXT("hasBegunPlay"), World->HasBegunPlay());
  }
  S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Actor query executed"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorDeleteByTag(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TagValue.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("tag required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FName TagName(*TagValue);
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  const TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  TArray<FString> Deleted;

  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    if (Actor->ActorHasTag(TagName)) {
      const FString Label = Actor->GetActorLabel();
      if (ActorSS->DestroyActor(Actor))
        Deleted.Add(Label);
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("tag"), TagName.ToString());
  Data->SetNumberField(TEXT("deletedCount"), Deleted.Num());
  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Name : Deleted)
    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
  Data->SetArrayField(TEXT("deleted"), DeletedArray);

  // Add verification data for delete operations
  Data->SetBoolField(TEXT("existsAfter"), false);
  Data->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));

  S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Actors deleted by tag"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorSetBlueprintVariables(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString VariableName;
  Payload->TryGetStringField(TEXT("variableName"), VariableName);
  if (VariableName.IsEmpty())
    Payload->TryGetStringField(TEXT("propertyName"), VariableName);
  if (VariableName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("variableName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Discriminated typed value (kills the old free-form `variables` map): set one
  // variable per call, atomically, via a real typed field that transmits cleanly.
  McpPropertyReflection::FMcpTypedValue Value;
  FString Detail;
  switch (McpPropertyReflection::ReadDiscriminatedValue(Payload, Value, Detail)) {
  case McpPropertyReflection::EMcpTypedValueParse::None:
    S.SendAutomationError(Socket, RequestId,
                          TEXT("set exactly one typed value field: boolValue, intValue, " "floatValue, stringValue, or vectorValue"),
                          TEXT("NO_CHANGES_REQUESTED"));
    return true;
  case McpPropertyReflection::EMcpTypedValueParse::Ambiguous:
    S.SendAutomationError(Socket, RequestId,
                          *FString::Printf(TEXT("set exactly one typed value field, got: %s"), *Detail),
                          TEXT("AMBIGUOUS_VALUE"));
    return true;
  case McpPropertyReflection::EMcpTypedValueParse::Ok:
    break;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  FProperty *Property = Found->GetClass()->FindPropertyByName(*VariableName);
  if (!Property) {
    S.SendAutomationError(Socket, RequestId,
                          *FString::Printf(TEXT("Variable not found: %s"), *VariableName),
                          TEXT("PROPERTY_NOT_FOUND"));
    return true;
  }
  if (!McpPropertyReflection::PropertyMatchesValueKind(Property, Value.Kind)) {
    S.SendAutomationError(Socket, RequestId,
                          *FString::Printf(TEXT("%s: you sent %sValue but the variable type is '%s'"), *VariableName, *Value.Kind, *Property->GetCPPType()),
                          TEXT("VALUE_TYPE_MISMATCH"));
    return true;
  }

  // A non-instance-editable variable written on an editor-world instance is a
  // doomed write: it lands and reads back, but the next BP recompile's
  // reinstancing only preserves EDITABLE instance deltas, so the value silently
  // vanishes (the Details panel can't even offer this write — non-editable vars
  // don't show on instances). PIE instances are exempt: transient by nature,
  // written deliberately by live tests.
  const UWorld* FoundWorld = Found->GetWorld();
  const bool bEditorWorldInstance = FoundWorld && FoundWorld->WorldType == EWorldType::Editor;
  if (bEditorWorldInstance && Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance)) {
    S.SendAutomationError(Socket, RequestId,
                          *FString::Printf(TEXT("%s is not instance-editable (CPF_DisableEditOnInstance): the write would land but the next Blueprint recompile's reinstancing discards non-editable instance values. Either make it instance-editable first (manage_blueprint set_variable_metadata {instanceEditable:true}) or set the class default instead (manage_blueprint set_default)."), *VariableName),
                          TEXT("NOT_INSTANCE_EDITABLE"));
    return true;
  }

  Found->Modify();
  FString ApplyError;
  if (!ApplyJsonValueToProperty(Found, Property, Value.Json, ApplyError)) {
    S.SendAutomationError(Socket, RequestId,
                          *FString::Printf(TEXT("Failed to set %s: %s"), *VariableName, *ApplyError),
                          TEXT("PROPERTY_WRITE_FAILED"));
    return true;
  }
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> AppliedArray;
  AppliedArray.Add(MakeShared<FJsonValueString>(VariableName));
  Data->SetArrayField(TEXT("appliedProperties"), AppliedArray);
  McpHandlerUtils::AddVerification(Data, Found);

  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Variable updated"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorCreateSnapshot(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SnapshotName;
  Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
  if (SnapshotName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("snapshotName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  const FString SnapshotKey =
      FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
  S.CachedActorSnapshots.Add(SnapshotKey, Found->GetActorTransform());

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Snapshot created"),
                           Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorRestoreSnapshot(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SnapshotName;
  Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
  if (SnapshotName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("snapshotName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  const FString SnapshotKey =
      FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
  if (!S.CachedActorSnapshots.Contains(SnapshotKey)) {
    S.SendAutomationError(Socket, RequestId, TEXT("Snapshot not found"),
                          TEXT("SNAPSHOT_NOT_FOUND"));
    return true;
  }

  const FTransform &SavedTransform = S.CachedActorSnapshots[SnapshotKey];
  Found->Modify();
  Found->SetActorTransform(SavedTransform);
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Snapshot restored"),
                           Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorExport(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  FMcpOutputCapture OutputCapture;
  UExporter::ExportToOutputDevice(nullptr, Found, nullptr, OutputCapture,
                                  TEXT("T3D"), 0, 0, false);
  FString OutputString = FString::Join(OutputCapture.Consume(), TEXT("\n"));

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("t3d"), OutputString);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Actor exported"),
                           Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorGetBoundingBox(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  FVector Origin, BoxExtent;
  Found->GetActorBounds(false, Origin, BoxExtent);

  auto BuildLandscapeBox = [](const FTransform& Transform, double MinX,
                              double MinY, double MaxX, double MaxY,
                              double MinZ, double MaxZ) {
    FBox LandscapeBox(EForceInit::ForceInit);
    const FVector Corners[] = {
        FVector(MinX, MinY, MinZ), FVector(MinX, MaxY, MinZ),
        FVector(MaxX, MinY, MinZ), FVector(MaxX, MaxY, MinZ),
        FVector(MinX, MinY, MaxZ), FVector(MinX, MaxY, MaxZ),
        FVector(MaxX, MinY, MaxZ), FVector(MaxX, MaxY, MaxZ),
    };
    for (const FVector& Corner : Corners) {
      LandscapeBox += Transform.TransformPosition(Corner);
    }
    return LandscapeBox;
  };

  if (ALandscape* Landscape = Cast<ALandscape>(Found);
      Landscape && BoxExtent.IsNearlyZero()) {
    ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    if (LandscapeInfo && LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)) {
      const FBox LandscapeBox = BuildLandscapeBox(
          Landscape->GetTransform(), MinX, MinY, MaxX, MaxY, -256.0, 256.0);
      Origin = LandscapeBox.GetCenter();
      BoxExtent = LandscapeBox.GetExtent();
    }

    if (BoxExtent.IsNearlyZero()) {
      const FBox CompleteBounds = Landscape->GetCompleteBounds();
      if (CompleteBounds.IsValid) {
        Origin = CompleteBounds.GetCenter();
        BoxExtent = CompleteBounds.GetExtent();
      }
    }

    if (BoxExtent.IsNearlyZero()) {
      FMcpLandscapeMetadata Metadata;
      if (McpLandscapeMetadataTags::DecodeLandscapeMetadata(Landscape, Metadata)) {
        const double MaxXFromMetadata = Metadata.ComponentsX * Metadata.QuadsPerComponent;
        const double MaxYFromMetadata = Metadata.ComponentsY * Metadata.QuadsPerComponent;
        const FBox LandscapeBox = BuildLandscapeBox(
            Landscape->GetTransform(), 0.0, 0.0, MaxXFromMetadata,
            MaxYFromMetadata, -256.0, 256.0);
        Origin = LandscapeBox.GetCenter();
        BoxExtent = LandscapeBox.GetExtent();
      }
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("origin"), MakeArray(Origin));
  Data->SetArrayField(TEXT("extent"), MakeArray(BoxExtent));
  S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Bounding box retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorGetMetadata(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("name"), Found->GetName());
  Data->SetStringField(TEXT("label"), Found->GetActorLabel());
  Data->SetStringField(TEXT("path"), Found->GetPathName());
  Data->SetStringField(TEXT("class"), Found->GetClass()
                                          ? Found->GetClass()->GetPathName()
                                          : TEXT(""));

  TArray<TSharedPtr<FJsonValue>> TagsArray;
  for (const FName &Tag : Found->Tags) {
    TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
  }
  Data->SetArrayField(TEXT("tags"), TagsArray);

  const FTransform Current = Found->GetActorTransform();
  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };
  Data->SetArrayField(TEXT("location"), MakeArray(Current.GetLocation()));

  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Metadata retrieved"),
                           Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorRemoveTag(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TargetName.IsEmpty() || TagValue.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName and tag required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(TargetName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  const FName TagName(*TagValue);
  if (!Found->Tags.Contains(TagName)) {
    // Idempotent success
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("wasPresent"), false);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("tag"), TagValue);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Tag not present (idempotent)"), Resp,
                           FString());
    return true;
  }

  Found->Modify();
  Found->Tags.Remove(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasPresent"), true);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("tag"), TagValue);

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	S.SendAutomationResponse(Socket, RequestId, true, TEXT("Tag removed from actor"), Data);
  return true;
#else
  return false;
#endif
}

// Additional handlers for test compatibility

bool McpHandlers::ControlActor::HandleControlActorFindByClass(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString ClassName;
  Payload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("class"), ClassName);
  }
  if (ClassName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("classPath"), ClassName);
  }

  if (ClassName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("className or class is required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Security: Validate class name format - reject path traversal attempts
  // Valid formats: "/Script/Module.ClassName", "/Game/Path/ClassName.ClassName", "ClassName"
  // Invalid: Contains "..", "\" (Windows paths), or other traversal patterns
  if (ClassName.Contains(TEXT("..")) || ClassName.Contains(TEXT("\\"))) {
    S.SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid class name format: '%s'. Path traversal characters are not allowed."), *ClassName),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Additional security: Reject absolute filesystem paths
  if (ClassName.StartsWith(TEXT("/")) && !ClassName.StartsWith(TEXT("/Script/")) && 
      !ClassName.StartsWith(TEXT("/Game/")) && !ClassName.StartsWith(TEXT("/Engine/"))) {
    // Could be a path traversal attempt disguised as a valid path
    if (ClassName.Contains(TEXT("/etc/")) || ClassName.Contains(TEXT("/usr/")) || 
        ClassName.Contains(TEXT("/var/")) || ClassName.Contains(TEXT("/home/")) ||
        ClassName.Contains(TEXT("/root/")) || ClassName.Contains(TEXT("/tmp/")) ||
        ClassName.Contains(TEXT("C:\\")) || ClassName.Contains(TEXT("D:\\"))) {
      S.SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid class name format: '%s'. Filesystem paths are not allowed."), *ClassName),
                            TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  int32 Limit = 50;
  {
    double LimitNum = 0.0;
    if (Payload->TryGetNumberField(TEXT("limit"), LimitNum)) {
      Limit = FMath::Clamp(static_cast<int32>(LimitNum), 1, 200);
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> ActorsArray;
  int32 Matched = 0;
  bool bTruncated = false;

  bool bIsPieWorld = false;
  UWorld* World = McpHandlerUtils::GetActorLookupWorld(&bIsPieWorld);
  if (World) {
    UClass* ClassToFind = nullptr;

    // CRITICAL FIX: Use ResolveClassByName for proper engine class resolution
    // This handles: full paths, short names like "StaticMeshActor", and loads classes if needed
    // Without this, FindObject only finds already-loaded classes, missing engine classes like
    // AStaticMeshActor, APawn, etc. that haven't been accessed yet
    ClassToFind = ResolveClassByName(ClassName);

    if (ClassToFind) {
      for (TActorIterator<AActor> It(World, ClassToFind); It; ++It) {
        if (AActor* Actor = *It) {
          ++Matched;
          if (ActorsArray.Num() >= Limit) {
            bTruncated = true;
            continue;
          }
          TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
          ActorObj->SetStringField(TEXT("name"), Actor->GetActorLabel());
          ActorObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
        }
      }
    } else {
      // Class not found - return empty result (this is valid for searches)
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleControlActorFindByClass: Class '%s' not found"), *ClassName);
    }
  }

  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  Data->SetNumberField(TEXT("matched"), Matched);
  Data->SetBoolField(TEXT("truncated"), bTruncated);
  // Derived from the SAME resolution as World so the flag and worldName can
  // never disagree. worldPackage exposes the UEDPIE_<instance>_ prefix and
  // hasBegunPlay flags the brief post-OpenLevel window — together they make
  // legitimate mid-PIE level travel (worldName changing between calls)
  // diagnosable instead of looking like a picker bug.
  Data->SetBoolField(TEXT("isPieWorld"), bIsPieWorld);
  if (World) {
    Data->SetStringField(TEXT("worldName"), World->GetName());
    Data->SetStringField(TEXT("worldPackage"), World->GetOutermost()->GetName());
    Data->SetBoolField(TEXT("hasBegunPlay"), World->HasBegunPlay());
  }
  S.SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Found %d actors"), ActorsArray.Num()),
                           Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorRemoveComponent(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("actor_name"), ActorName);
  }
  
  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  if (ComponentName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("component_name"), ComponentName);
  }
  
  if (ActorName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName is required"), TEXT("MISSING_PARAM"));
    return true;
  }
  
  if (ComponentName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("componentName is required"), TEXT("MISSING_PARAM"));
    return true;
  }
  
  AActor* Actor = S.FindActorByName(ActorName);
  if (!Actor) {
    S.SendAutomationError(Socket, RequestId, 
                        FString::Printf(TEXT("Actor not found: %s"), *ActorName),
                        TEXT("ACTOR_NOT_FOUND"));
    return true;
  }
  
  UActorComponent* Component = McpHandlerUtils::FindActorComponentByName(Actor, ComponentName);
  if (Component) {
    Component->DestroyComponent();
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetStringField(TEXT("actorName"), ActorName);
    Data->SetStringField(TEXT("componentName"), ComponentName);

    // Add verification data for delete operations
    Data->SetBoolField(TEXT("existsAfter"), false);
    Data->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));

    S.SendAutomationResponse(Socket, RequestId, true, TEXT("Component removed"),
                             Data);
    return true;
  }
  
  S.SendAutomationError(Socket, RequestId,
                      FString::Printf(TEXT("Component not found: %s"), *ComponentName),
                      TEXT("COMPONENT_NOT_FOUND"));
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorGetComponentProperty(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString ActorName, ComponentName, PropertyName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  if (PropertyName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
  }
  
  if (ActorName.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName, componentName, and propertyName are required"), TEXT("MISSING_PARAM"));
    return true;
  }
  
  AActor* Actor = S.FindActorByName(ActorName);
  if (!Actor) {
    S.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }
  
  // Fuzzy match resolves component names with numeric suffixes (e.g. "StaticMeshComponent0").
  UActorComponent* Component = McpHandlerUtils::FindActorComponentByName(Actor, ComponentName);
  if (!Component) {
    S.SendAutomationError(Socket, RequestId, 
        FString::Printf(TEXT("Component not found: %s on actor: %s"), *ComponentName, *ActorName), 
        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }
  
  // Get property using reflection
  FProperty* Property = Component->GetClass()->FindPropertyByName(*PropertyName);
  if (!Property) {
    S.SendAutomationError(Socket, RequestId, 
        FString::Printf(TEXT("Property not found: %s on component: %s"), *PropertyName, *ComponentName), 
        TEXT("PROPERTY_NOT_FOUND"));
    return true;
  }
  
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  Data->SetStringField(TEXT("componentName"), ComponentName);
  Data->SetStringField(TEXT("propertyName"), PropertyName);
  Data->SetStringField(TEXT("propertyType"), Property->GetClass()->GetName());
  
  // Extract property value using the existing helper function
  TSharedPtr<FJsonValue> PropertyValue = ExportPropertyToJsonValue(Component, Property);
  if (PropertyValue.IsValid()) {
    Data->SetField(TEXT("value"), PropertyValue);
  } else {
    Data->SetStringField(TEXT("value"), TEXT("<unsupported property type>"));
  }
  
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Property retrieved"),
                           Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorSetCollision(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("actor_name"), ActorName);
  }
  if (ActorName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName is required"), TEXT("MISSING_PARAM"));
    return true;
  }

  // collisionEnabled must be explicitly requested — never silently apply the default.
  const bool bHasCollision = Payload->HasField(TEXT("collisionEnabled")) ||
                             Payload->HasField(TEXT("collision_enabled"));
  if (!bHasCollision) {
    S.SendAutomationError(Socket, RequestId, TEXT("collisionEnabled is required"),
                        TEXT("NO_CHANGES_REQUESTED"));
    return true;
  }
  const bool bCollisionEnabled =
      Payload->HasField(TEXT("collisionEnabled"))
          ? GetJsonBoolField(Payload, TEXT("collisionEnabled"), true)
          : GetJsonBoolField(Payload, TEXT("collision_enabled"), true);

  AActor* Actor = S.FindActorByName(ActorName);
  if (!Actor) {
    S.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // Pre-validate before touching anything: an actor with no primitive component
  // fails cleanly with nothing applied (all-or-nothing without needing rollback).
  TArray<UPrimitiveComponent*> PrimComps;
  for (UActorComponent* Comp : Actor->GetComponents()) {
    if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp)) {
      PrimComps.Add(PrimComp);
    }
  }
  if (PrimComps.Num() == 0) {
    S.SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Actor has no primitive component to set collision on: %s"), *ActorName),
        TEXT("NO_PRIMITIVE_COMPONENT"));
    return true;
  }

  const ECollisionEnabled::Type NewCollision =
      bCollisionEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
  Actor->Modify();
  for (UPrimitiveComponent* PrimComp : PrimComps) {
    PrimComp->Modify();
    PrimComp->SetCollisionEnabled(NewCollision);
  }
  Actor->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  TArray<TSharedPtr<FJsonValue>> AppliedProperties;
  AppliedProperties.Add(MakeShared<FJsonValueString>(TEXT("collisionEnabled")));
  Data->SetArrayField(TEXT("appliedProperties"), AppliedProperties);
  Data->SetBoolField(TEXT("collisionEnabled"), bCollisionEnabled);
  Data->SetNumberField(TEXT("componentsModified"), PrimComps.Num());
  S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Collision setting updated"), Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorCallFunction(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString ActorName, FunctionName, ComponentName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("functionName"), FunctionName);
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);

  if (ActorName.IsEmpty() || FunctionName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName and functionName are required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor* Actor = S.FindActorByName(ActorName);
  if (!Actor) {
    S.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // Resolve the call target: the actor by default, or one of its components when
  // componentName is given. UObject::FindFunction only sees functions on the
  // target's own class, so a component UFUNCTION (e.g. AttributeComponent::RemoveMagic)
  // is unreachable through the actor.
  UObject* Target = Actor;
  if (!ComponentName.IsEmpty()) {
    UActorComponent* Component = McpHandlerUtils::FindActorComponentByName(Actor, ComponentName);
    if (!Component) {
      S.SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Component not found: %s on actor: %s"), *ComponentName, *ActorName),
          TEXT("COMPONENT_NOT_FOUND"));
      return true;
    }
    Target = Component;
  }

  UFunction* Function = Target->FindFunction(*FunctionName);
  if (!Function) {
    S.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Function not found: %s"), *FunctionName), TEXT("FUNCTION_NOT_FOUND"));
    return true;
  }

  // Positional arguments, stringified so ImportText can parse each into its
  // matching parameter type (the tool schema sends them as strings).
  TArray<FString> Arguments;
  const TArray<TSharedPtr<FJsonValue>>* ArgsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("arguments"), ArgsArray)) {
    for (const TSharedPtr<FJsonValue>& Arg : *ArgsArray) {
      if (Arg->Type == EJson::Number) {
        const double D = Arg->AsNumber();
        Arguments.Add(FMath::Frac(D) == 0.0 ? FString::Printf(TEXT("%lld"), (int64)D)
                                            : FString::SanitizeFloat(D));
      } else if (Arg->Type == EJson::Boolean) {
        Arguments.Add(Arg->AsBool() ? TEXT("True") : TEXT("False"));
      } else {
        Arguments.Add(Arg->AsString());
      }
    }
  }

  if (Function->ParmsSize > 0) {
    // Construct a real parameter frame so non-POD params (FString, arrays...) are
    // valid, fill input params in declaration order from the supplied arguments,
    // call, then destroy the frame to avoid leaking those params.
    uint8* ParmsBuffer = (uint8*)FMemory::Malloc(Function->ParmsSize, 16);
    FMemory::Memzero(ParmsBuffer, Function->ParmsSize);
    for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It) {
      It->InitializeValue_InContainer(ParmsBuffer);
    }

    int32 ArgIndex = 0;
    for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It) {
      FProperty* Prop = *It;
      // Skip the return value and pure output params — the caller supplies neither.
      if (Prop->PropertyFlags & CPF_ReturnParm) {
        continue;
      }
      if ((Prop->PropertyFlags & CPF_OutParm) && !(Prop->PropertyFlags & CPF_ReferenceParm)) {
        continue;
      }
      if (ArgIndex >= Arguments.Num()) {
        break;
      }
      void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ParmsBuffer);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      Prop->ImportText_Direct(*Arguments[ArgIndex], ValuePtr, Target, PPF_None);
#else
      Prop->ImportText(*Arguments[ArgIndex], ValuePtr, PPF_None, Target);
#endif
      ++ArgIndex;
    }

    Target->ProcessEvent(Function, ParmsBuffer);

    for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It) {
      It->DestroyValue_InContainer(ParmsBuffer);
    }
    FMemory::Free(ParmsBuffer);
  } else {
    Target->ProcessEvent(Function, nullptr);
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  if (!ComponentName.IsEmpty()) {
    Data->SetStringField(TEXT("componentName"), ComponentName);
  }
  Data->SetStringField(TEXT("functionName"), FunctionName);
  Data->SetNumberField(TEXT("argumentsApplied"), Arguments.Num());
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Function called"),
                           Data);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorPlay(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("alreadyPlaying"), true);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Play session already active"), Resp,
                           FString());
    return true;
  }

  FRequestPlaySessionParams PlayParams;
  PlayParams.WorldType = EPlaySessionWorldType::PlayInEditor;
#if MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS
  PlayParams.EditorPlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
#endif
#if MCP_HAS_LEVEL_EDITOR_MODULE
  if (FLevelEditorModule *LevelEditorModule =
          FModuleManager::GetModulePtr<FLevelEditorModule>(
              TEXT("LevelEditor"))) {
    TSharedPtr<IAssetViewport> DestinationViewport =
        LevelEditorModule->GetFirstActiveViewport();
    if (DestinationViewport.IsValid())
      PlayParams.DestinationSlateViewport = DestinationViewport;
  }
#endif

  GEditor->RequestPlaySession(PlayParams);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Play in Editor started"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorStop(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("alreadyStopped"), true);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Play session not active"), Resp, FString());
    return true;
  }

  GEditor->RequestEndPlayMap();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Play in Editor stopped"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorEject(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
    ErrorDetails->SetBoolField(TEXT("notInPIE"), true);
    S.SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Cannot eject: Play session not active"),
                             ErrorDetails, TEXT("NO_ACTIVE_SESSION"));
    return true;
  }

  // Use Eject console command instead of RequestEndPlayMap
  // This ejects the player from the possessed pawn without stopping PIE
  GEditor->Exec(GEditor->PlayWorld, TEXT("Eject"));
  
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("ejected"), true);
  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Ejected from possessed actor"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorPossess(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);

  // Also try "objectPath" as fallback since schema might use that
  if (ActorName.IsEmpty())
    Payload->TryGetStringField(TEXT("objectPath"), ActorName);

  if (ActorName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = S.FindActorByName(ActorName);
  if (!Found) {
    S.SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Actor not found: %s"), *ActorName),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  if (GEditor) {
    GEditor->SelectNone(true, true, false);
    GEditor->SelectActor(Found, true, true, true);
    // 'POSSESS' command works on selected actor in PIE
    if (GEditor->PlayWorld) {
      GEditor->Exec(GEditor->PlayWorld, TEXT("POSSESS"));
      S.SendAutomationResponse(Socket, RequestId, true, TEXT("Possessed actor"),
                             nullptr);
    } else {
      // If not in PIE, we can't possess
      S.SendAutomationError(Socket, RequestId,
                            TEXT("Cannot possess actor while not in PIE"),
                            TEXT("NOT_IN_PIE"));
    }
    return true;
  }

  S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorFocusActor(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
    for (AActor *Actor : Actors) {
      if (!Actor)
        continue;
      if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
          Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)) {
        GEditor->SelectNone(true, true, false);
        GEditor->SelectActor(Actor, true, true, true);
        GEditor->Exec(nullptr, TEXT("EDITORTEMPVIEWPORT"));
        GEditor->MoveViewportCamerasToActor(*Actor, false);
        S.SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Viewport focused on actor"), nullptr,
                               FString());
        return true;
      }
    }
    S.SendAutomationError(Socket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
    return true;
  }
  return false;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSetCamera(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  // Seed with the current pose so a partial request (only location, or only
  // rotation) preserves the other axis instead of snapping it to zero.
  FVector Location(0, 0, 0);
  FRotator Rotation(0, 0, 0);
#if defined(MCP_HAS_UNREALEDITOR_SUBSYSTEM)
  if (UUnrealEditorSubsystem *UESCurrent =
          GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
    UESCurrent->GetLevelViewportCameraInfo(Location, Rotation);
#endif
  // ReadVectorField/ReadRotatorField unwrap the named field themselves: pass the
  // parent payload + field name, not the pre-extracted object with an empty name
  // (empty name finds no nested field and returns the default — a silent no-op).
  ReadVectorField(Payload, TEXT("location"), Location, Location);
  ReadRotatorField(Payload, TEXT("rotation"), Rotation, Rotation);

#if defined(MCP_HAS_UNREALEDITOR_SUBSYSTEM)
  if (UUnrealEditorSubsystem *UES =
          GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) {
    UES->SetLevelViewportCameraInfo(Location, Rotation);
#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
    if (ULevelEditorSubsystem *LES =
            GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
      LES->EditorInvalidateViewports();
#endif
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    S.SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp,
                           FString());
    return true;
  }
#endif
  if (FEditorViewportClient *ViewportClient =
          GEditor->GetActiveViewport()
              ? (FEditorViewportClient *)GEditor->GetActiveViewport()
                    ->GetClient()
              : nullptr) {
    ViewportClient->SetViewLocation(Location);
    ViewportClient->SetViewRotation(Rotation);
    ViewportClient->Invalidate();
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    S.SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp,
                           FString());
    return true;
  }
  return false;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSetViewMode(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString Mode;
  Payload->TryGetStringField(TEXT("viewMode"), Mode);
  FString LowerMode = Mode.ToLower();
  FString Chosen;
  EViewModeIndex ViewModeIndex = VMI_Lit;
  bool bHasNativeViewMode = true;
  if (LowerMode == TEXT("lit"))
  {
    Chosen = TEXT("Lit");
    ViewModeIndex = VMI_Lit;
  }
  else if (LowerMode == TEXT("unlit"))
  {
    Chosen = TEXT("Unlit");
    ViewModeIndex = VMI_Unlit;
  }
  else if (LowerMode == TEXT("wireframe"))
  {
    Chosen = TEXT("Wireframe");
    ViewModeIndex = VMI_Wireframe;
  }
  else if (LowerMode == TEXT("detaillighting"))
  {
    Chosen = TEXT("DetailLighting");
    ViewModeIndex = VMI_Lit_DetailLighting;
  }
  else if (LowerMode == TEXT("lightingonly"))
  {
    Chosen = TEXT("LightingOnly");
    ViewModeIndex = VMI_LightingOnly;
  }
  else if (LowerMode == TEXT("lightcomplexity"))
  {
    Chosen = TEXT("LightComplexity");
    ViewModeIndex = VMI_LightComplexity;
  }
  else if (LowerMode == TEXT("shadercomplexity"))
  {
    Chosen = TEXT("ShaderComplexity");
    ViewModeIndex = VMI_ShaderComplexity;
  }
  else if (LowerMode == TEXT("lightmapdensity"))
  {
    Chosen = TEXT("LightmapDensity");
    ViewModeIndex = VMI_LightmapDensity;
  }
  else if (LowerMode == TEXT("stationarylightoverlap"))
  {
    Chosen = TEXT("StationaryLightOverlap");
    ViewModeIndex = VMI_StationaryLightOverlap;
  }
  else if (LowerMode == TEXT("reflectionoverride"))
  {
    Chosen = TEXT("ReflectionOverride");
    ViewModeIndex = VMI_ReflectionOverride;
  }
  else if (IsSafeConsoleArgumentToken(Mode))
  {
    Chosen = Mode;
    bHasNativeViewMode = false;
  }
  else {
    S.SendAutomationError(Socket, RequestId, TEXT("Invalid viewMode"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (bHasNativeViewMode) {
    if (FEditorViewportClient* ViewportClient = GetActiveEditorViewportClientForMcp()) {
      ViewportClient->SetViewMode(ViewModeIndex);
      ViewportClient->Invalidate();

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("viewMode"), Chosen);
      Resp->SetStringField(TEXT("method"), TEXT("viewport_client"));
      S.SendAutomationResponse(Socket, RequestId, true, TEXT("View mode set"), Resp,
                             FString());
      return true;
    }
  }

  const FString Cmd = FString::Printf(TEXT("viewmode %s"), *Chosen);
  if (GEditor->Exec(nullptr, *Cmd)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("viewMode"), Chosen);
    S.SendAutomationResponse(Socket, RequestId, true, TEXT("View mode set"), Resp,
                           FString());
    return true;
  }
  S.SendAutomationError(Socket, RequestId, TEXT("View mode command failed"),
                        TEXT("EXEC_FAILED"));
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSetCameraFov(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  double Fov = 90.0;
  Payload->TryGetNumberField(TEXT("fov"), Fov);
  if (Fov <= 1.0 || Fov >= 179.0) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("fov must be between 1 and 179 degrees"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (FEditorViewportClient* ViewportClient = GetActiveEditorViewportClientForMcp()) {
    ViewportClient->ViewFOV = static_cast<float>(Fov);
    ViewportClient->FOVAngle = static_cast<float>(Fov);
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("fov"), Fov);
    Resp->SetStringField(TEXT("method"), TEXT("viewport_client"));
    S.SendAutomationResponse(Socket, RequestId, true, TEXT("Camera FOV set"), Resp,
                           FString());
    return true;
  }

  S.SendAutomationError(Socket, RequestId,
                        TEXT("No editor viewport available for FOV update"),
                        TEXT("VIEWPORT_NOT_AVAILABLE"));
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSetGameSpeed(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  double Speed = 1.0;
  Payload->TryGetNumberField(TEXT("speed"), Speed);
  if (Speed <= 0.0 || Speed > 20.0) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("speed must be greater than 0 and no more than 20"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UWorld* World = GEditor && GEditor->PlayWorld
      ? GEditor->PlayWorld.Get()
      : (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
  if (!World || !World->GetWorldSettings()) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("No world available for game speed update"),
                          TEXT("NO_WORLD"));
    return true;
  }

  World->GetWorldSettings()->SetTimeDilation(static_cast<float>(Speed));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("speed"), Speed);
  Resp->SetNumberField(TEXT("timeDilation"), World->GetWorldSettings()->GetEffectiveTimeDilation());
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Game speed set"), Resp,
                         FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorOpenAsset(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("assetPath required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath"),
                          TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UAssetEditorSubsystem *AssetEditorSS =
      GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!AssetEditorSS) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("AssetEditorSubsystem not available"),
                          TEXT("SUBSYSTEM_MISSING"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    S.SendAutomationError(Socket, RequestId, TEXT("Asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = McpLoadAssetPieSafe(AssetPath);
  if (!Asset) {
    S.SendAutomationError(Socket, RequestId, TEXT("Failed to load asset"),
                          TEXT("LOAD_FAILED"));
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
    Resp->SetBoolField(TEXT("loaded"), true);
    Resp->SetBoolField(TEXT("editorOpened"), false);
    Resp->SetBoolField(TEXT("headlessSafe"), true);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Asset loaded; editor window skipped under NullRHI"), Resp,
                           FString());
    return true;
  }

  const bool bOpened = AssetEditorSS->OpenEditorForAsset(Asset);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bOpened);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);

  if (bOpened) {
    S.SendAutomationResponse(Socket, RequestId, true, TEXT("Asset opened"), Resp,
                           FString());
  } else {
    S.SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to open asset editor"), Resp,
                             TEXT("OPEN_FAILED"));
  }
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorScreenshot(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Get optional filename from payload
  FString Filename;
  Payload->TryGetStringField(TEXT("filename"), Filename);
  if (Filename.IsEmpty()) {
    // Generate default filename with timestamp
    Filename = FString::Printf(TEXT("Screenshot_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }

  // SECURITY: Sanitize filename to prevent path traversal
  // Remove any path components and keep only the base filename
  Filename = FPaths::GetCleanFilename(Filename);
  
  // Validate filename doesn't contain suspicious patterns
  if (Filename.Contains(TEXT("..")) || Filename.Contains(TEXT("/")) || Filename.Contains(TEXT("\\"))) {
    // Reject suspicious filename and use default
    Filename = FString::Printf(TEXT("Screenshot_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }

  // Ensure filename ends with .png
  if (!Filename.EndsWith(TEXT(".png"))) {
    Filename += TEXT(".png");
  }

  // Build the full path - save to project's Saved/Screenshots folder
  const FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
  IFileManager::Get().MakeDirectory(*ScreenshotDir, true);
  const FString FullPath = ScreenshotDir / Filename;

  // Get the active viewport
  FViewport* Viewport = GEditor->GetActiveViewport();
  if (!Viewport) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("No active viewport available"),
                          TEXT("VIEWPORT_NOT_AVAILABLE"));
    return true;
  }

  // Keep the editor rendering even when it is NOT the foreground window. The editor
  // throttles background-window rendering ("Use Less CPU in Background"); with that on,
  // the forced Draw() below would not produce a frame while the editor sits behind the
  // client. Live value only (no PostEditChange/save), and restored on scope exit so the
  // user's preference isn't leaked past this capture.
  struct FScopedUnthrottle {
    UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
    bool bPrevious = false;
    FScopedUnthrottle() {
      if (Settings) {
        bPrevious = Settings->bThrottleCPUWhenNotForeground;
        Settings->bThrottleCPUWhenNotForeground = false;
      }
    }
    ~FScopedUnthrottle() {
      if (Settings) {
        Settings->bThrottleCPUWhenNotForeground = bPrevious;
      }
    }
  } ScopedUnthrottle;

  // Optional downscale cap so the PNG stays legible but not huge (0/absent = native).
  int32 MaxWidth = 0;
  Payload->TryGetNumberField(TEXT("maxWidth"), MaxWidth);

  // Synchronous capture: force a fresh draw, then read the pixels back. ReadPixels
  // flushes the render commands, so the bitmap is valid the moment it returns -- unlike
  // FScreenshotRequest, which only writes its file at end-of-frame (useless here, since
  // this handler is blocking the GameThread and that frame can't complete until we
  // return). This guarantees the file exists when we answer, so a caller can read it
  // immediately to evaluate the result.
  Viewport->Draw();
  const FIntPoint ViewportSize = Viewport->GetSizeXY();
  if (ViewportSize.X <= 0 || ViewportSize.Y <= 0) {
    S.SendAutomationError(Socket, RequestId, TEXT("Viewport has zero size"),
                          TEXT("VIEWPORT_NOT_AVAILABLE"));
    return true;
  }

  TArray<FColor> Bitmap;
  FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
  if (!Viewport->ReadPixels(Bitmap, ReadFlags,
          FIntRect(0, 0, ViewportSize.X, ViewportSize.Y)) ||
      Bitmap.Num() < ViewportSize.X * ViewportSize.Y) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("Failed to read viewport pixels"),
                          TEXT("CAPTURE_FAILED"));
    return true;
  }

  // Force opaque so a 0 alpha channel doesn't render the PNG transparent.
  for (FColor& Pixel : Bitmap) {
    Pixel.A = 255;
  }

  int32 OutWidth = ViewportSize.X;
  int32 OutHeight = ViewportSize.Y;
  const TArray<FColor>* Pixels = &Bitmap;
  TArray<FColor> Resized;
  if (MaxWidth > 0 && ViewportSize.X > MaxWidth) {
    OutWidth = MaxWidth;
    OutHeight = FMath::Max(1, FMath::RoundToInt(
        ViewportSize.Y * static_cast<float>(MaxWidth) / ViewportSize.X));
    FImageUtils::ImageResize(ViewportSize.X, ViewportSize.Y, Bitmap, OutWidth, OutHeight,
                             Resized, /*bResizeSRGBinLinearSpace=*/false,
                             /*bForceOpaqueOutput=*/true);
    Pixels = &Resized;
  }

  TArray64<uint8> PngData;
  FImageUtils::PNGCompressImageArray(
      OutWidth, OutHeight,
      TArrayView64<const FColor>(Pixels->GetData(), Pixels->Num()), PngData);

  if (PngData.Num() == 0 || !FFileHelper::SaveArrayToFile(PngData, *FullPath)) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("Failed to encode/write screenshot PNG"),
                          TEXT("CAPTURE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("filename"), Filename);
  Resp->SetStringField(TEXT("path"), FPaths::ConvertRelativePathToFull(FullPath));
  Resp->SetNumberField(TEXT("width"), OutWidth);
  Resp->SetNumberField(TEXT("height"), OutHeight);
  Resp->SetNumberField(TEXT("bytes"), static_cast<double>(PngData.Num()));
  Resp->SetStringField(TEXT("message"),
      TEXT("Screenshot captured synchronously; the PNG is written and ready to read."));

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Screenshot captured"), Resp, FString());
  return true;
#else
  S.SendAutomationError(Socket, RequestId,
                        TEXT("Screenshot requires editor build."),
                        TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorPause(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("No active PIE session to pause"),
                          TEXT("NO_ACTIVE_SESSION"));
    return true;
  }

  // Pause PIE execution
  GEditor->PlayWorld->bDebugPauseExecution = true;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("state"), TEXT("paused"));
  Resp->SetStringField(TEXT("message"), TEXT("PIE session paused"));

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("PIE session paused"), Resp, FString());
  return true;
#else
  S.SendAutomationError(Socket, RequestId, TEXT("Pause requires editor build."),
                        TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorResume(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("No active PIE session to resume"),
                          TEXT("NO_ACTIVE_SESSION"));
    return true;
  }

  // Resume PIE execution
  GEditor->PlayWorld->bDebugPauseExecution = false;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("state"), TEXT("resumed"));
  Resp->SetStringField(TEXT("message"), TEXT("PIE session resumed"));

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("PIE session resumed"), Resp, FString());
  return true;
#else
  S.SendAutomationError(Socket, RequestId,
                        TEXT("Resume requires editor build."),
                        TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorStepFrame(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("No active PIE session to step"),
                          TEXT("NO_ACTIVE_SESSION"));
    return true;
  }

  // Step one frame - set debug step flag and unpause momentarily
  GEditor->PlayWorld->bDebugFrameStepExecution = true;
  GEditor->PlayWorld->bDebugPauseExecution = false;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("message"), TEXT("Stepped one frame"));

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Frame stepped"), Resp, FString());
  return true;
#else
  S.SendAutomationError(Socket, RequestId,
                        TEXT("Step frame requires editor build."),
                        TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorStartRecording(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  FString RecordingName;
  // Accept both 'name' and 'filename' fields for flexibility
  // TS handler sends 'filename', so we check that first
  Payload->TryGetStringField(TEXT("filename"), RecordingName);
  if (RecordingName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("name"), RecordingName);
  }
  if (RecordingName.IsEmpty()) {
    RecordingName = FString::Printf(TEXT("Recording_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  } else {
    RecordingName = MakeSafeConsoleName(RecordingName, TEXT("Recording"));
  }

  // Use console command to start demo recording
  // UE 5.7: TObjectPtr requires explicit cast to UWorld*
  UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
  if (World) {
    FString Command = FString::Printf(TEXT("DemoRec %s"), *RecordingName);
    GEditor->Exec(World, *Command);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("recordingName"), RecordingName);
  Resp->SetStringField(TEXT("message"), TEXT("Recording started"));

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Recording started"), Resp, FString());
  return true;
#else
  S.SendAutomationError(Socket, RequestId,
                        TEXT("Recording requires editor build."),
                        TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorStopRecording(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Use console command to stop demo recording
  // UE 5.7: TObjectPtr requires explicit cast to UWorld*
  UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
  if (World) {
    GEditor->Exec(World, TEXT("DemoStop"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("message"), TEXT("Recording stopped"));

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Recording stopped"), Resp, FString());
  return true;
#else
  S.SendAutomationError(Socket, RequestId,
                        TEXT("Recording requires editor build."),
                        TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorCreateBookmark(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  int32 BookmarkIndex = 0;
  Payload->TryGetNumberField(TEXT("index"), BookmarkIndex);
  if (!Payload->HasField(TEXT("index"))) {
    Payload->TryGetNumberField(TEXT("id"), BookmarkIndex);
  }

  // Clamp to valid bookmark range (0-9)
  BookmarkIndex = FMath::Clamp(BookmarkIndex, 0, 9);

  // Use console command to set bookmark
  FString Command = FString::Printf(TEXT("SetBookmark %d"), BookmarkIndex);
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("index"), BookmarkIndex);
  Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Bookmark %d created"), BookmarkIndex));

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Bookmark created"), Resp, FString());
  return true;
#else
  S.SendAutomationError(Socket, RequestId,
                        TEXT("Bookmarks require editor build."),
                        TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorJumpToBookmark(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  int32 BookmarkIndex = 0;
  Payload->TryGetNumberField(TEXT("index"), BookmarkIndex);
  if (!Payload->HasField(TEXT("index"))) {
    Payload->TryGetNumberField(TEXT("id"), BookmarkIndex);
  }

  // Clamp to valid bookmark range (0-9)
  BookmarkIndex = FMath::Clamp(BookmarkIndex, 0, 9);

  // Use console command to jump to bookmark
  FString Command = FString::Printf(TEXT("JumpToBookmark %d"), BookmarkIndex);
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("index"), BookmarkIndex);
  Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Jumped to bookmark %d"), BookmarkIndex));

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Jumped to bookmark"), Resp, FString());
  return true;
#else
  S.SendAutomationError(Socket, RequestId,
                        TEXT("Bookmarks require editor build."),
                        TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSetPreferences(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  TArray<FString> AppliedSettings;
  TArray<FString> FailedSettings;
  FString Category;
  Payload->TryGetStringField(TEXT("category"), Category);

  // Get preferences object from payload
  const TSharedPtr<FJsonObject>* PrefsPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("preferences"), PrefsPtr) && PrefsPtr && (*PrefsPtr).IsValid()) {
    for (const auto& Pair : (*PrefsPtr)->Values) {
      const FString PreferenceName(*Pair.Key);
      // Try to set via console variable first
      IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*PreferenceName);
      if (CVar) {
        FString Value;
        if (Pair.Value->TryGetString(Value)) {
          CVar->Set(*Value);
          AppliedSettings.Add(PreferenceName);
        } else {
          double NumVal;
          if (Pair.Value->TryGetNumber(NumVal)) {
            CVar->Set((float)NumVal);
            AppliedSettings.Add(PreferenceName);
          } else {
            bool BoolVal;
            if (Pair.Value->TryGetBool(BoolVal)) {
              CVar->Set(BoolVal ? 1 : 0);
              AppliedSettings.Add(PreferenceName);
            } else {
              FailedSettings.Add(PreferenceName);
            }
          }
        }
      } else if (Category.Equals(TEXT("LevelEditor"), ESearchCase::IgnoreCase) && PreferenceName.Equals(TEXT("RealtimeAudio"), ESearchCase::IgnoreCase)) {
        bool BoolVal;
        if (Pair.Value->TryGetBool(BoolVal)) {
          GEditor->MuteRealTimeAudio(!BoolVal);
          AppliedSettings.Add(PreferenceName);
        } else {
          FailedSettings.Add(PreferenceName);
        }
      } else {
        FailedSettings.Add(PreferenceName);
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  const bool bAnyPreferenceApplied = AppliedSettings.Num() > 0;
  const bool bPreferencesUpdated = bAnyPreferenceApplied && FailedSettings.Num() == 0;
  Resp->SetBoolField(TEXT("success"), bPreferencesUpdated);
  Resp->SetNumberField(TEXT("appliedCount"), AppliedSettings.Num());

  if (AppliedSettings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> AppliedArray;
    for (const FString& Name : AppliedSettings)
      AppliedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("applied"), AppliedArray);
  }

  if (FailedSettings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> FailedArray;
    for (const FString& Name : FailedSettings)
      FailedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("failed"), FailedArray);
  }

  const FString ResponseMessage = bPreferencesUpdated
      ? TEXT("Preferences updated")
      : (bAnyPreferenceApplied ? TEXT("Preferences partially updated") : TEXT("No preferences updated"));
  const FString ResponseErrorCode = bPreferencesUpdated
      ? FString()
      : (bAnyPreferenceApplied ? FString(TEXT("PREFERENCES_PARTIALLY_APPLIED")) : FString(TEXT("PREFERENCES_NOT_APPLIED")));
  S.SendAutomationResponse(Socket, RequestId, bPreferencesUpdated,
                         ResponseMessage, Resp, ResponseErrorCode);
  return true;
#else
  S.SendAutomationError(Socket, RequestId,
                        TEXT("Preferences require editor build."),
                        TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSetViewportRealtime(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  bool bRealtime = true;
  Payload->TryGetBoolField(TEXT("realtime"), bRealtime);

#if MCP_HAS_LEVEL_EDITOR_MODULE
  // Get the level editor module and active viewport
  FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
  TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();
  
  if (ActiveViewport.IsValid()) {
    FEditorViewportClient& ViewportClient = ActiveViewport->GetAssetViewportClient();
    ViewportClient.SetRealtime(bRealtime);
    
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("realtime"), bRealtime);
    Resp->SetStringField(TEXT("message"), bRealtime ? TEXT("Viewport realtime enabled") : TEXT("Viewport realtime disabled"));

    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Viewport realtime updated"), Resp, FString());
    return true;
  }
#endif

  // Fallback: use console command
  FString Command = bRealtime ? TEXT("Viewport Realtime") : TEXT("Viewport Realtime 0");
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("realtime"), bRealtime);
  Resp->SetStringField(TEXT("message"), bRealtime ? TEXT("Viewport realtime enabled") : TEXT("Viewport realtime disabled"));

  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Viewport realtime updated"), Resp, FString());
  return true;
#else
  S.SendAutomationError(Socket, RequestId,
                        TEXT("Viewport realtime requires editor build."),
                        TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSimulateInput(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Accept multiple field names for flexibility
  // - 'type': C++ native field (key_down, key_up, mouse_click, mouse_move, analog)
  // - 'inputType': Alternative name
  // - 'inputAction': Action-based naming (pressed, released, click, move)
  // CRITICAL: Do NOT read from 'action' field - that's the routing action (e.g., "simulate_input")
  // and will always be present in the payload. Only use type/inputType/inputAction for input type.
  FString InputType;
  Payload->TryGetStringField(TEXT("type"), InputType);
  if (InputType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("inputType"), InputType);
  }
  if (InputType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("inputAction"), InputType);
  }
  
  // Map action values to C++ expected type values
  InputType = InputType.ToLower();
  if (InputType == TEXT("pressed") || InputType == TEXT("down")) {
    InputType = TEXT("key_down");
  } else if (InputType == TEXT("released") || InputType == TEXT("up")) {
    InputType = TEXT("key_up");
  } else if (InputType == TEXT("click")) {
    InputType = TEXT("mouse_click");
  } else if (InputType == TEXT("move")) {
    InputType = TEXT("mouse_move");
  }

  FString Key;
  Payload->TryGetStringField(TEXT("key"), Key);

  bool bSuccess = false;
  bool bRoutedToPIE = false;
  bool bHandledByPIE = false;
  bool bHandledBySlate = false;
  FString Message;

  auto RouteKeyToPIE = [](const FKey &InputKey, const EInputEvent InputEvent,
                          bool &bOutHandledByPIE) -> bool {
    bOutHandledByPIE = false;
    if (!GEditor || !GEditor->PlayWorld) {
      return false;
    }

    UWorld *PlayWorld = GEditor->PlayWorld.Get();
    if (!PlayWorld) {
      return false;
    }

    APlayerController *PlayerController = PlayWorld->GetFirstPlayerController();
    if (!PlayerController) {
      return false;
    }

    const double AmountDepressed = InputEvent == IE_Released ? 0.0 : 1.0;
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    FInputKeyParams KeyParams(InputKey, InputEvent, AmountDepressed, false);
    bOutHandledByPIE = PlayerController->InputKey(KeyParams);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    return true;
  };

  if (InputType == TEXT("key_down") || InputType == TEXT("keydown")) {
    if (!Key.IsEmpty()) {
      FKey InputKey(*Key);
      if (InputKey.IsValid()) {
        bRoutedToPIE = RouteKeyToPIE(InputKey, IE_Pressed, bHandledByPIE);
        if (!bRoutedToPIE) {
          FSlateApplication& SlateApp = FSlateApplication::Get();
          FKeyEvent KeyEvent(InputKey, FModifierKeysState(), 0, false, 0, 0);
          bHandledBySlate = SlateApp.ProcessKeyDownEvent(KeyEvent);
        }
        bSuccess = true;
        Message = bRoutedToPIE
            ? FString::Printf(TEXT("Key down: %s (delivered to PIE)"), *Key)
            : FString::Printf(TEXT("Key down: %s"), *Key);
      } else {
        Message = FString::Printf(TEXT("Invalid key: %s"), *Key);
      }
    } else {
      Message = TEXT("Key parameter required for key_down");
    }
  } else if (InputType == TEXT("key_up") || InputType == TEXT("keyup")) {
    if (!Key.IsEmpty()) {
      FKey InputKey(*Key);
      if (InputKey.IsValid()) {
        bRoutedToPIE = RouteKeyToPIE(InputKey, IE_Released, bHandledByPIE);
        if (!bRoutedToPIE) {
          FSlateApplication& SlateApp = FSlateApplication::Get();
          FKeyEvent KeyEvent(InputKey, FModifierKeysState(), 0, false, 0, 0);
          bHandledBySlate = SlateApp.ProcessKeyUpEvent(KeyEvent);
        }
        bSuccess = true;
        Message = bRoutedToPIE
            ? FString::Printf(TEXT("Key up: %s (delivered to PIE)"), *Key)
            : FString::Printf(TEXT("Key up: %s"), *Key);
      } else {
        Message = FString::Printf(TEXT("Invalid key: %s"), *Key);
      }
    } else {
      Message = TEXT("Key parameter required for key_up");
    }
  } else if (InputType == TEXT("mouse_click") || InputType == TEXT("click")) {
    double X = 0, Y = 0;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    FString Button = TEXT("left");
    Payload->TryGetStringField(TEXT("button"), Button);

    // UE 5.7: EKeys::Type is deprecated, use FKey directly
    FKey MouseButtonKey = EKeys::LeftMouseButton;
    if (Button.ToLower() == TEXT("right")) MouseButtonKey = EKeys::RightMouseButton;
    else if (Button.ToLower() == TEXT("middle")) MouseButtonKey = EKeys::MiddleMouseButton;

    FSlateApplication& SlateApp = FSlateApplication::Get();
    FVector2D Position((float)X, (float)Y);
    
    // UE 5.7: FPointerEvent constructor signature changed
    // FPointerEvent(uint32 InPointerIndex, ScreenSpacePosition, LastScreenSpacePosition, Delta, PressedButtons, ModifierKeys)
    TSet<FKey> PressedButtons;
    PressedButtons.Add(MouseButtonKey);
    
    // Simulate mouse down then up for a click
    FPointerEvent MouseDownEvent(
        0,  // PointerIndex
        Position,  // ScreenSpacePosition
        Position,  // LastScreenSpacePosition
        FVector2D(0.0f, 0.0f),  // Delta
        PressedButtons,
        FModifierKeysState()
    );
    SlateApp.ProcessMouseButtonDownEvent(nullptr, MouseDownEvent);
    
    TSet<FKey> ReleasedButtons;  // Empty set for mouse up
    FPointerEvent MouseUpEvent(
        0,  // PointerIndex
        Position,  // ScreenSpacePosition
        Position,  // LastScreenSpacePosition
        FVector2D(0.0f, 0.0f),  // Delta
        ReleasedButtons,
        FModifierKeysState()
    );
    SlateApp.ProcessMouseButtonUpEvent(MouseUpEvent);
    bHandledBySlate = true;
    
    bSuccess = true;
    Message = FString::Printf(TEXT("Mouse click at (%f, %f)"), X, Y);
  } else if (InputType == TEXT("mouse_move") || InputType == TEXT("move")) {
    double X = 0, Y = 0;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    FSlateApplication& SlateApp = FSlateApplication::Get();
    FVector2D Position((float)X, (float)Y);
    SlateApp.SetCursorPos(Position);
    bHandledBySlate = true;

    bSuccess = true;
    Message = FString::Printf(TEXT("Mouse moved to (%f, %f)"), X, Y);
  } else if (InputType == TEXT("analog") || InputType == TEXT("axis")) {
    // Axis-value injection: gamepad sticks/triggers (Gamepad_LeftY, ...) and
    // mouse look (MouseX/MouseY). One IE_Axis sample persists in PlayerInput
    // until the next sample — matching hardware's change-driven delivery — so
    // hold = inject once, release = inject value 0.
    double Value = 0.0;
    const bool bHasValue = Payload->TryGetNumberField(TEXT("value"), Value);
    FString Route = TEXT("pie");
    Payload->TryGetStringField(TEXT("route"), Route);
    Route = Route.ToLower();

    if (Key.IsEmpty()) {
      Message = TEXT("Key parameter required for analog");
    } else if (!bHasValue) {
      Message = TEXT("value parameter required for analog (axis value, typically -1..1)");
    } else {
      FKey InputKey(*Key);
      if (!InputKey.IsValid() || !InputKey.IsAxis1D()) {
        Message = FString::Printf(
            TEXT("'%s' is not a 1D axis key (try Gamepad_LeftY, Gamepad_RightX, MouseX, MouseY)"), *Key);
      } else if (Route == TEXT("slate")) {
        // Faithful route: through FSlateApplication and its input preprocessors
        // (CommonUI analog cursor etc.), exactly like foreground hardware input.
        // Same inactive-app bracket as simulate_nav so background editors behave
        // like foreground ones.
        FSlateApplication& SlateApp = FSlateApplication::Get();
        const bool bPrevHandleInactive = SlateApp.GetHandleDeviceInputWhenApplicationNotActive();
        SlateApp.SetHandleDeviceInputWhenApplicationNotActive(true);
        FAnalogInputEvent AnalogEvent(InputKey, FModifierKeysState(), /*UserIndex*/ 0,
                                      /*bIsRepeat*/ false, /*CharacterCode*/ 0,
                                      /*KeyCode*/ 0, static_cast<float>(Value));
        bHandledBySlate = SlateApp.ProcessAnalogInputEvent(AnalogEvent);
        SlateApp.SetHandleDeviceInputWhenApplicationNotActive(bPrevHandleInactive);
        bSuccess = true;
        Message = FString::Printf(TEXT("Analog %s = %.3f (slate route)"), *Key, Value);
      } else {
        // Default route: straight to the PIE player controller — gameplay-level,
        // immune to window focus, bypasses Slate/CommonUI (use route:'slate' to
        // exercise that layer).
        if (GEditor->PlayWorld) {
          if (APlayerController* PC = GEditor->PlayWorld->GetFirstPlayerController()) {
            FInputKeyEventArgs Args = FInputKeyEventArgs::CreateSimulated(
                InputKey, IE_Axis, static_cast<float>(Value), /*NumSamples*/ 1);
            Args.DeltaTime = GEditor->PlayWorld->GetDeltaSeconds();
            bHandledByPIE = PC->InputKey(Args);
            bRoutedToPIE = true;
          }
        }
        if (bRoutedToPIE) {
          bSuccess = true;
          Message = FString::Printf(TEXT("Analog %s = %.3f (delivered to PIE)"), *Key, Value);
        } else {
          Message = TEXT("analog requires an active PIE session with a player controller (or pass route:'slate')");
        }
      }
    }
  } else {
    Message = FString::Printf(TEXT("Unknown input type: %s. Supported: key_down, key_up, mouse_click, mouse_move, analog"), *InputType);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetStringField(TEXT("type"), InputType);
  Resp->SetStringField(TEXT("message"), Message);
  Resp->SetBoolField(TEXT("routedToPIE"), bRoutedToPIE);
  Resp->SetBoolField(TEXT("handledByPIE"), bHandledByPIE);
  Resp->SetBoolField(TEXT("handledBySlate"), bHandledBySlate);

  if (bSuccess) {
    S.SendAutomationResponse(Socket, RequestId, true, Message, Resp, FString());
  } else {
    S.SendAutomationResponse(Socket, RequestId, false, Message, Resp,
                             TEXT("INPUT_FAILED"));
  }
  return true;
#else
  S.SendAutomationError(Socket, RequestId,
                        TEXT("Simulate input requires editor build."),
                        TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorCloseAsset(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("assetPath required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath"),
                          TEXT("SECURITY_VIOLATION"));
    return true;
  }

  UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!AssetEditorSS) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("AssetEditorSubsystem unavailable"),
                          TEXT("SUBSYSTEM_MISSING"));
    return true;
  }

  UObject* Asset = McpLoadAssetPieSafe(AssetPath);
  if (!Asset) {
    S.SendAutomationError(Socket, RequestId, TEXT("Failed to load asset"),
                          TEXT("LOAD_FAILED"));
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetBoolField(TEXT("loaded"), true);
    Resp->SetBoolField(TEXT("editorClosed"), false);
    Resp->SetBoolField(TEXT("headlessSafe"), true);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Asset verified; editor close skipped under NullRHI"), Resp,
                           FString());
    return true;
  }

  AssetEditorSS->CloseAllEditorsForAsset(Asset);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Asset editor closed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSaveAll(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  TArray<UPackage*> DirtyWorldPackages;
  TArray<UPackage*> DirtyContentPackages;
  FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
  FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);

  bool bSuccess = true;
  int32 SavedWorldCount = 0;
  int32 SavedContentCount = 0;
  int32 SkippedCount = 0;
  int32 TotalDirty = 0;
  TArray<FString> SkippedPackages;
  TArray<FString> FailedPackages;
  TSet<UPackage*> ProcessedPackages;

  auto ShouldSkipPackage = [](UPackage* Package) -> bool {
    if (!Package || Package->HasAnyFlags(RF_Transient)) {
      return true;
    }
    const FString PackagePath = Package->GetPathName();
    return PackagePath.StartsWith(TEXT("/Temp/")) ||
           PackagePath.StartsWith(TEXT("/Transient/")) ||
           PackagePath.StartsWith(TEXT("/Engine/Transient"));
  };

  auto ProcessPackage = [&](UPackage* Package) {
    if (!Package || ProcessedPackages.Contains(Package)) {
      return;
    }
    ProcessedPackages.Add(Package);
    TotalDirty++;

    FString PackagePath = Package->GetPathName();
    if (ShouldSkipPackage(Package)) {
      SkippedCount++;
      SkippedPackages.Add(PackagePath);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("HandleControlEditorSaveAll: Skipping transient/temp package: %s"), *PackagePath);
      return;
    }

    UWorld* PackageWorld = UWorld::FindWorldInPackage(Package);
    if (PackageWorld) {
      if (PackageWorld->PersistentLevel && McpSafeLevelSave(PackageWorld->PersistentLevel, PackagePath)) {
        SavedWorldCount++;
      } else {
        bSuccess = false;
        FailedPackages.Add(PackagePath);
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("HandleControlEditorSaveAll: Failed to save world package: %s"), *PackagePath);
      }
      return;
    }

    if (McpSafeAssetSave(Package)) {
      SavedContentCount++;
    } else {
      bSuccess = false;
      FailedPackages.Add(PackagePath);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleControlEditorSaveAll: Failed to save content package: %s"), *PackagePath);
    }
  };

  for (UPackage* Package : DirtyWorldPackages) {
    ProcessPackage(Package);
  }

  for (UPackage* Package : DirtyContentPackages) {
    ProcessPackage(Package);
  }

  auto MakeStringArray = [](const TArray<FString>& Values) {
    TArray<TSharedPtr<FJsonValue>> Result;
    for (const FString& Value : Values) {
      Result.Add(MakeShared<FJsonValueString>(Value));
    }
    return Result;
  };

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetNumberField(TEXT("savedCount"), SavedWorldCount + SavedContentCount);
  Resp->SetNumberField(TEXT("savedWorldCount"), SavedWorldCount);
  Resp->SetNumberField(TEXT("savedContentCount"), SavedContentCount);
  Resp->SetNumberField(TEXT("skippedCount"), SkippedCount);
  Resp->SetNumberField(TEXT("failedCount"), FailedPackages.Num());
  Resp->SetNumberField(TEXT("totalDirty"), TotalDirty);
  Resp->SetArrayField(TEXT("skippedPackages"), MakeStringArray(SkippedPackages));
  Resp->SetArrayField(TEXT("failedPackages"), MakeStringArray(FailedPackages));
  
  // Only report outer success if the operation actually succeeded
  if (bSuccess || TotalDirty == 0) {
    S.SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Saved %d world and %d content packages (skipped %d transient/temp)"), SavedWorldCount, SavedContentCount, SkippedCount),
                           Resp, FString());
  } else {
    S.SendAutomationResponse(Socket, RequestId, false,
                             FString::Printf(TEXT("Failed to save all packages. Saved %d of %d dirty packages."), SavedWorldCount + SavedContentCount, TotalDirty - SkippedCount),
                             Resp, TEXT("SAVE_FAILED"));
  }
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorUndo(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Execute undo via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("Undo"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("command"), TEXT("Undo"));
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Undo executed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorRedo(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Execute redo via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("Redo"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("command"), TEXT("Redo"));
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Redo executed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSetEditorMode(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString Mode;
  Payload->TryGetStringField(TEXT("mode"), Mode);
  if (Mode.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId, TEXT("mode required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!IsSafeConsoleArgumentToken(Mode)) {
    S.SendAutomationError(Socket, RequestId, TEXT("Invalid editor mode"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Execute editor mode command via console
  FString Command = FString::Printf(TEXT("mode %s"), *Mode);
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("mode"), Mode);
  S.SendAutomationResponse(Socket, RequestId, true, 
                         FString::Printf(TEXT("Editor mode set to %s"), *Mode), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorShowStats(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld* World = GEditor->GetEditorWorldContext().World();
  TArray<FString> StatsShown;
  if (World) {
    GEditor->Exec(World, TEXT("Stat FPS"));
    StatsShown.Add(TEXT("FPS"));
    GEditor->Exec(World, TEXT("Stat Unit"));
    StatsShown.Add(TEXT("Unit"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> StatsArray;
  for (const FString& Stat : StatsShown) {
    StatsArray.Add(MakeShared<FJsonValueString>(Stat));
  }
  Resp->SetArrayField(TEXT("statsShown"), StatsArray);
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Stats displayed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorHideStats(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld* World = GEditor->GetEditorWorldContext().World();
  if (World) {
    GEditor->Exec(World, TEXT("Stat None"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("command"), TEXT("Stat None"));
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Stats hidden"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSetGameView(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);

  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Toggle game view via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), 
                bEnabled ? TEXT("ToggleGameView 1") : TEXT("ToggleGameView 0"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("gameViewEnabled"), bEnabled);
  S.SendAutomationResponse(Socket, RequestId, true, 
                         FString::Printf(TEXT("Game view %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")), 
                         Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSetImmersiveMode(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);

  // Toggle immersive mode - this is viewport-specific
  if (GEditor && GEditor->GetActiveViewport()) {
    FViewport* Viewport = GEditor->GetActiveViewport();
    if (Viewport) {
      // Immersive mode toggle via console
      GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("ToggleImmersive"));
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("immersiveModeEnabled"), bEnabled);
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Immersive mode toggled"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorSetFixedDeltaTime(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  double DeltaTime = 0.01667; // Default ~60fps
  if (Payload->HasField(TEXT("deltaTime"))) {
    TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("deltaTime"));
    if (Value.IsValid() && Value->Type == EJson::Number) {
      DeltaTime = Value->AsNumber();
    }
  }

  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Set fixed delta time via console
  FString Command = FString::Printf(TEXT("r.FixedDeltaTime %f"), DeltaTime);
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("fixedDeltaTime"), DeltaTime);
  S.SendAutomationResponse(Socket, RequestId, true, 
                         FString::Printf(TEXT("Fixed delta time set to %f"), DeltaTime), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlEditor::HandleControlEditorOpenLevel(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  FString LevelPath;
  // Accept multiple parameter names for flexibility
  // levelPath is the primary, path and assetPath are aliases
  Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
  if (LevelPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("path"), LevelPath);
  }
  if (LevelPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("assetPath"), LevelPath);
  }
  if (LevelPath.IsEmpty()) {
    S.SendAutomationError(Socket, RequestId,
                          TEXT("levelPath, path, or assetPath required"),
                          TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Normalize the level path
  if (!LevelPath.StartsWith(TEXT("/"))) {
    LevelPath = FString::Printf(TEXT("/Game/%s"), *LevelPath);
  }

  LevelPath = SanitizeProjectRelativePath(LevelPath);
  if (LevelPath.IsEmpty() ||
      !(LevelPath.StartsWith(TEXT("/Game/")) || LevelPath.StartsWith(TEXT("/Engine/")))) {
    S.SendAutomationError(Socket, RequestId, TEXT("Invalid levelPath"),
                          TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Remove map suffix if present
  if (LevelPath.EndsWith(TEXT(".umap"))) {
    LevelPath.LeftChopInline(5);
  }

  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Use FEditorFileUtils to load the map
  FString MapPath = LevelPath + TEXT(".umap");
  
  // CRITICAL FIX: Unreal stores levels in TWO possible path patterns:
  // 1. Folder-based (standard): /Game/Path/LevelName/LevelName.umap
  // 2. Flat (legacy): /Game/Path/LevelName.umap
  // We must check BOTH paths before returning FILE_NOT_FOUND.
  
  // Build both possible paths
  FString FlatMapPath = LevelPath + TEXT(".umap");
  // Check if path is /Engine/ or /Game/ and extract accordingly
  int32 PrefixLen = 6; // Default: "/Game/" is 6 chars
  FString ContentDir = FPaths::ProjectContentDir();
  if (LevelPath.StartsWith(TEXT("/Engine/"))) {
    PrefixLen = 8; // "/Engine/" is 8 chars
    ContentDir = FPaths::EngineContentDir();
  }
  FString FullFlatMapPath = ContentDir + FlatMapPath.Mid(PrefixLen);
  FullFlatMapPath = FPaths::ConvertRelativePathToFull(FullFlatMapPath);
  
  // Folder-based path: /Game/Path/LevelName -> /Game/Path/LevelName/LevelName.umap
  FString LevelName = FPaths::GetBaseFilename(LevelPath);
  FString FolderMapPath = LevelPath + TEXT("/") + LevelName + TEXT(".umap");
  FString FullFolderMapPath = ContentDir + FolderMapPath.Mid(PrefixLen);
  FullFolderMapPath = FPaths::ConvertRelativePathToFull(FullFolderMapPath);
  
  // Check which path exists
  FString MapPathToLoad;
  FString FullMapPath;
  
  // Prefer folder-based path (Unreal's standard) if it exists
  if (FPaths::FileExists(FullFolderMapPath)) {
    MapPathToLoad = FolderMapPath;
    FullMapPath = FullFolderMapPath;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("OpenLevel: Found level at folder-based path: %s"), *FullFolderMapPath);
  } else if (FPaths::FileExists(FullFlatMapPath)) {
    // Fallback to flat path (legacy format)
    MapPathToLoad = FlatMapPath;
    FullMapPath = FullFlatMapPath;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("OpenLevel: Found level at flat path: %s"), *FullFlatMapPath);
  } else {
    // Neither path exists - return detailed error
    TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
    ErrorDetails->SetStringField(TEXT("levelPath"), LevelPath);
    ErrorDetails->SetStringField(TEXT("checkedFolderBased"), FullFolderMapPath);
    ErrorDetails->SetStringField(TEXT("checkedFlat"), FullFlatMapPath);
    ErrorDetails->SetStringField(TEXT("hint"), TEXT("Unreal levels are typically stored as /Game/Path/LevelName/LevelName.umap"));
    S.SendAutomationResponse(Socket, RequestId, false,
                             FString::Printf(TEXT("Level file not found. Checked:\n  Folder: %s\n  Flat: %s"), *FullFolderMapPath, *FullFlatMapPath),
                             ErrorDetails, TEXT("FILE_NOT_FOUND"));
    return true;
  }

  if (FApp::IsUnattended() || IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) {
    TArray<UPackage*> DirtyWorldPackages;
    TArray<UPackage*> DirtyContentPackages;
    FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
    FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);
    auto IsBlockingDirtyPackage = [](UPackage* Package) -> bool {
      if (!Package || Package->HasAnyFlags(RF_Transient)) {
        return false;
      }
      const FString PackagePath = Package->GetPathName();
      return !PackagePath.StartsWith(TEXT("/Temp/")) &&
             !PackagePath.StartsWith(TEXT("/Transient/")) &&
             !PackagePath.StartsWith(TEXT("/Engine/Transient"));
    };
    int32 BlockingWorldPackages = 0;
    int32 BlockingContentPackages = 0;
    for (UPackage* Package : DirtyWorldPackages) {
      if (IsBlockingDirtyPackage(Package)) {
        BlockingWorldPackages++;
      }
    }
    for (UPackage* Package : DirtyContentPackages) {
      if (IsBlockingDirtyPackage(Package)) {
        BlockingContentPackages++;
      }
    }
    if (BlockingWorldPackages + BlockingContentPackages > 0) {
      TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
      Details->SetNumberField(TEXT("dirtyWorldPackages"), BlockingWorldPackages);
      Details->SetNumberField(TEXT("dirtyContentPackages"), BlockingContentPackages);
      Details->SetStringField(TEXT("levelPath"), LevelPath);
      S.SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Cannot open a level in unattended/headless mode while packages are dirty. Save or discard changes first."),
                               Details, TEXT("DIRTY_PACKAGES"));
      return true;
    }
  }
  
  // The response is sent from the ticker below, after the load; without this
  // mark the post-dispatch check fails the still-pending request with
  // HANDLER_NO_RESPONSE even though the level opens fine.
  S.MarkRequestDeferred(RequestId);
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(&S);
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakThis, Socket, RequestId, LevelPath,
                                     MapPathToLoad](float) {
        if (!WeakThis.IsValid()) {
          return false;
        }

        bool bOpened = McpSafeLoadMap(MapPathToLoad);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), bOpened);
        Resp->SetStringField(TEXT("levelPath"), LevelPath);
        Resp->SetStringField(TEXT("loadedPath"), MapPathToLoad);

        if (bOpened) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                 TEXT("OpenLevel: Successfully opened level: %s"), *MapPathToLoad);
          WeakThis->SendAutomationResponse(Socket, RequestId, true,
                                           TEXT("Level opened"), Resp, FString());
        } else {
          WeakThis->SendAutomationResponse(Socket, RequestId, false,
                                           TEXT("Failed to open level"), Resp,
                                           TEXT("OPEN_FAILED"));
        }

        return false;
      }),
      0.0f);
  return true;
#else
  return false;
#endif
}

bool McpHandlers::ControlActor::HandleControlActorList(
    UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationError(Socket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);

  double LimitValue = 0.0;
  Payload->TryGetNumberField(TEXT("limit"), LimitValue);
  const int32 Limit = LimitValue > 0.0
      ? FMath::Max(1, static_cast<int32>(LimitValue))
      : 0;

  TArray<AActor *> AllActors;
  UWorld *SourceWorld = nullptr;
  bool bUsingPieWorld = false;

  if (GEditor->PlayWorld) {
    SourceWorld = GEditor->PlayWorld.Get();
    bUsingPieWorld = true;
    if (!SourceWorld) {
      S.SendAutomationError(Socket, RequestId, TEXT("PIE world unavailable"),
                            TEXT("WORLD_NOT_FOUND"));
      return true;
    }

    for (TActorIterator<AActor> It(SourceWorld); It; ++It) {
      AllActors.Add(*It);
    }
  } else {
    SourceWorld = GEditor->GetEditorWorldContext().World();
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      S.SendAutomationError(Socket, RequestId,
                            TEXT("EditorActorSubsystem unavailable"),
                            TEXT("SUBSYSTEM_MISSING"));
      return true;
    }

    AllActors = ActorSS->GetAllLevelActors();
  }

  TArray<TSharedPtr<FJsonValue>> ActorsArray;
  int32 TotalCount = 0;

  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    const FString Label = Actor->GetActorLabel();
    const FString Name = Actor->GetName();
    if (!Filter.IsEmpty() &&
        !Label.Contains(Filter, ESearchCase::IgnoreCase) &&
        !Name.Contains(Filter, ESearchCase::IgnoreCase))
      continue;
    ++TotalCount;

    if (Limit > 0 && ActorsArray.Num() >= Limit)
      continue;

    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("label"), Label);
    Entry->SetStringField(TEXT("name"), Name);
    Entry->SetStringField(TEXT("path"), Actor->GetPathName());
    Entry->SetStringField(TEXT("class"), Actor->GetClass()
                                             ? Actor->GetClass()->GetPathName()
                                             : TEXT(""));
    ActorsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  Data->SetNumberField(TEXT("totalCount"), TotalCount);
  Data->SetBoolField(TEXT("isPieWorld"), bUsingPieWorld);
  if (SourceWorld)
    Data->SetStringField(TEXT("worldName"), SourceWorld->GetName());
  if (!Filter.IsEmpty())
    Data->SetStringField(TEXT("filter"), Filter);
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Actors listed"), Data);
  return true;
#else
  return false;
#endif
}
