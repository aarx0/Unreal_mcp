// =============================================================================
// McpAutomationBridge_PerformanceHandlers.cpp
// =============================================================================
// system_control performance member handlers (HandlePerf*). Dispatch lives in
// the FMcpCall classes (Private/MCP/Calls/McpCalls_SystemControl.cpp); each
// member here implements one advertised action.
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0: IStreamingManager::AddViewLocation not available
// UE 5.1+: IStreamingManager::AddViewLocation available for texture streaming boost
//
// SECURITY:
// ---------
// - Stat category names sanitized to prevent console command injection
// - Only alphanumeric characters and underscores allowed in categories
// =============================================================================

// =============================================================================
// Version Compatibility Header (MUST BE FIRST)
// =============================================================================
#include "McpVersionCompatibility.h"

// =============================================================================
// Core Headers
// =============================================================================
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMeshActor.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "MeshMerge/MeshMergingSettings.h"
#else
#include "Engine/MeshMerging.h"
#endif
#include "MeshMergeModule.h"
#include "IMeshMergeUtilities.h"
#include "Misc/PackageName.h"
#include "StaticMeshCompiler.h"
#include "Containers/Ticker.h"

// =============================================================================
// Editor-Only Headers
// =============================================================================
#if WITH_EDITOR

// Content & Streaming
#include "ContentStreaming.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"

// Gameplay & Level
#include "Kismet/GameplayStatics.h"
#include "LevelEditor.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "RenderTimer.h"          // GGameThreadTime / GRenderThreadTime / GRHIThreadTime (RENDERCORE_API)
#include "DynamicRHI.h"           // RHIGetGPUFrameCycles (replaces deprecated GGPUFrameTime)
#include "HAL/PlatformMemory.h"   // FPlatformMemory::GetStats
#include "Misc/App.h"             // FApp::GetDeltaTime

// GAverageFPS/GAverageMS are exported ENGINE_API but declared only inside
// UnrealEngine.cpp (no public header), so we redeclare the externs here exactly
// as the engine's own translation units do.
extern ENGINE_API float GAverageFPS;
extern ENGINE_API float GAverageMS;

#endif // WITH_EDITOR

// =============================================================================
// Handler Implementation
// =============================================================================

#if WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandlePerfGenerateMemoryReport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    bool bDetailed = false;
    Payload->TryGetBoolField(TEXT("detailed"), bDetailed);

    FString OutputPath;
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

    // Execute memreport command
    FString Cmd = bDetailed ? TEXT("memreport -full") : TEXT("memreport");
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    GEngine->Exec(GEditor->GetEditorWorldContext().World(), *Cmd);

    // If output path provided, we might want to move the log file, but
    // memreport writes to a specific location. For now, just acknowledge
    // execution.

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Memory report generated"), nullptr);
    return true;
}

// READ BACK live performance counters as JSON. Unlike start/stop_profiling
// (which write a .ue4stats file to disk the agent can't consume), this returns
// the engine's running perf numbers directly so a caller can see FPS / frame
// timings / memory without leaving the bridge.
bool UMcpAutomationBridgeSubsystem::HandlePerfGetStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);

    // Frame rate / frame time (engine running averages, updated every frame).
    Result->SetNumberField(TEXT("averageFPS"), GAverageFPS);
    Result->SetNumberField(TEXT("averageFrameMs"), GAverageMS);
    Result->SetNumberField(TEXT("deltaSeconds"), FApp::GetDeltaTime());
    Result->SetNumberField(TEXT("frameNumber"), static_cast<double>(GFrameCounter));

    // Per-thread timings (cycles -> ms). Meaningful while frames are rendering
    // (editor viewport or PIE); a thread reads ~0 when idle/disabled.
    TSharedPtr<FJsonObject> Timings = MakeShared<FJsonObject>();
    Timings->SetNumberField(TEXT("gameThreadMs"), FPlatformTime::ToMilliseconds(GGameThreadTime));
    Timings->SetNumberField(TEXT("renderThreadMs"), FPlatformTime::ToMilliseconds(GRenderThreadTime));
    Timings->SetNumberField(TEXT("rhiThreadMs"), FPlatformTime::ToMilliseconds(GRHIThreadTime));
    Timings->SetNumberField(TEXT("gpuMs"), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
    Result->SetObjectField(TEXT("frameTimingsMs"), Timings);

    // Memory (bytes -> MB).
    const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    const double ToMB = 1.0 / (1024.0 * 1024.0);
    TSharedPtr<FJsonObject> Mem = MakeShared<FJsonObject>();
    Mem->SetNumberField(TEXT("usedPhysicalMB"), static_cast<double>(MemStats.UsedPhysical) * ToMB);
    Mem->SetNumberField(TEXT("peakUsedPhysicalMB"), static_cast<double>(MemStats.PeakUsedPhysical) * ToMB);
    Mem->SetNumberField(TEXT("usedVirtualMB"), static_cast<double>(MemStats.UsedVirtual) * ToMB);
    Mem->SetNumberField(TEXT("availablePhysicalMB"), static_cast<double>(MemStats.AvailablePhysical) * ToMB);
    Result->SetObjectField(TEXT("memory"), Mem);

    Result->SetBoolField(TEXT("inPIE"), GEditor && GEditor->PlayWorld != nullptr);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("%.1f FPS (%.2f ms frame)"), GAverageFPS, GAverageMS),
        Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfStartProfiling(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    GEngine->Exec(GEditor->GetEditorWorldContext().World(),
                  TEXT("stat startfile"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Profiling started"), nullptr);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfStopProfiling(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    GEngine->Exec(GEditor->GetEditorWorldContext().World(),
                  TEXT("stat stopfile"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Profiling stopped"), nullptr);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfShowFps(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    // Note: "stat fps" toggles, so we might need check, but mostly users just
    // want to run the command. For explicit set, we can use "stat fps 1" or
    // "stat fps 0" if supported, but typically it's a toggle. Better: use
    // GAreyouSure? No, just exec.
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    GEngine->Exec(GEditor->GetEditorWorldContext().World(), TEXT("stat fps"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("FPS stat toggled"), nullptr);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfShowStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    FString Category;
    if (Payload->TryGetStringField(TEXT("category"), Category) &&
        !Category.IsEmpty()) {
      if (!GEditor)
      {
          SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
          return true;
      }

      // Sanitize category to prevent console command injection
      // Only allow alphanumeric characters and underscores
      bool bIsValidCategory = true;
      for (int32 i = 0; i < Category.Len(); ++i) {
        TCHAR C = Category[i];
        if (!FChar::IsAlnum(C) && C != TEXT('_')) {
          bIsValidCategory = false;
          break;
        }
      }

      if (!bIsValidCategory) {
        SendAutomationError(RequestingSocket, RequestId, 
                            TEXT("Invalid stat category name. Only alphanumeric characters and underscores allowed."),
                            TEXT("INVALID_CATEGORY"));
        return true;
      }

      GEngine->Exec(GEditor->GetEditorWorldContext().World(),
                    *FString::Printf(TEXT("stat %s"), *Category));
      SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("Stat '%s' toggled"), *Category), nullptr);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Category required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
    }
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfSetScalability(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    int32 Level = 3; // Epic
    Payload->TryGetNumberField(TEXT("level"), Level);

    // Simple batch scalability
    Scalability::FQualityLevels Quals;
    Quals.SetFromSingleQualityLevel(Level);
    Scalability::SetQualityLevels(Quals);
    Scalability::SaveState(GEditorIni);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Scalability set"), nullptr);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfSetResolutionScale(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    double Scale = 100.0;
    if (Payload->TryGetNumberField(TEXT("scale"), Scale)) {
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
          TEXT("r.ScreenPercentage"));
      if (CVar)
        CVar->Set((float)Scale);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Resolution scale set"), nullptr);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Scale required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
    }
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfSetVsync(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    IConsoleVariable *CVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
    if (CVar)
      CVar->Set(bEnabled ? 1 : 0);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("VSync configured"), nullptr);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfSetFrameRateLimit(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    double Limit = 0.0;
    if (Payload->TryGetNumberField(TEXT("maxFPS"), Limit)) {
      GEngine->SetMaxFPS((float)Limit);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Max FPS set"), nullptr);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("maxFPS required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
    }
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfConfigureNanite(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    IConsoleVariable *CVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"));
    if (CVar)
      CVar->Set(bEnabled ? 1 : 0);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Nanite configured"), nullptr);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfConfigureLod(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    double LODBias = 0.0;
    if (Payload->TryGetNumberField(TEXT("lodBias"), LODBias)) {
      IConsoleVariable *CVar =
          IConsoleManager::Get().FindConsoleVariable(TEXT("r.MipMapLODBias"));
      if (CVar)
        CVar->Set((float)LODBias);
    }

    double ForceLOD = -1.0;
    if (Payload->TryGetNumberField(TEXT("forceLOD"), ForceLOD)) {
      IConsoleVariable *CVar =
          IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForceLOD"));
      if (CVar)
        CVar->Set((int32)ForceLOD);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("LOD settings configured"), nullptr);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfConfigureTextureStreaming(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    double PoolSize = 0;
    if (Payload->TryGetNumberField(TEXT("poolSize"), PoolSize)) {
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
          TEXT("r.Streaming.PoolSize"));
      if (CVar)
        CVar->Set((float)PoolSize);
    }

    bool bBoost = false;
    if (Payload->TryGetBoolField(TEXT("boostPlayerLocation"), bBoost) &&
        bBoost) {
      // Logic to boost streaming around player
      if (GEditor && GEditor->GetEditorWorldContext().World()) {
        APlayerCameraManager *Cam = UGameplayStatics::GetPlayerCameraManager(
            GEditor->GetEditorWorldContext().World(), 0);
        if (Cam) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
          IStreamingManager::Get().AddViewLocation(Cam->GetCameraLocation());
#else
          // UE 5.0: AddViewLocation not available - use alternative approach
          // Just notify that streaming is enabled without location boost
#endif
        }
      }
    }

    IConsoleVariable *CVarStream =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.TextureStreaming"));
    if (CVarStream)
      CVarStream->Set(bEnabled ? 1 : 0);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Texture streaming configured"), nullptr);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfMergeActors(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    const TArray<TSharedPtr<FJsonValue>> *NamesArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("actors"), NamesArray) || !NamesArray ||
        NamesArray->Num() < 2) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("merge_actors requires an 'actors' array "
                                  "with at least 2 entries"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Editor world not available for merge_actors"), nullptr,
          TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UWorld *World = GEditor->GetEditorWorldContext().World();
    TArray<AActor *> ActorsToMerge;

    auto ResolveActorByName = [World](const FString &Name) -> AActor * {
      if (Name.IsEmpty()) {
        return nullptr;
      }

      // Try to resolve by full object path first
      if (AActor *ByPath = FindObject<AActor>(nullptr, *Name)) {
        return ByPath;
      }

      // Fallback: search the current editor world by label and by name
      for (TActorIterator<AActor> It(World); It; ++It) {
        AActor *Actor = *It;
        if (!Actor) {
          continue;
        }

        const FString Label = Actor->GetActorLabel();
        const FString ObjName = Actor->GetName();
        if (Label.Equals(Name, ESearchCase::IgnoreCase) ||
            ObjName.Equals(Name, ESearchCase::IgnoreCase)) {
          return Actor;
        }
      }

      return nullptr;
    };

    for (const TSharedPtr<FJsonValue> &Val : *NamesArray) {
      if (!Val.IsValid() || Val->Type != EJson::String) {
        continue;
      }

      const FString RawName = Val->AsString().TrimStartAndEnd();
      if (RawName.IsEmpty()) {
        continue;
      }

      if (AActor *Resolved = ResolveActorByName(RawName)) {
        ActorsToMerge.AddUnique(Resolved);
      }
    }

    if (ActorsToMerge.Num() < 2) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("merge_actors resolved fewer than 2 valid actors"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString RequestedPackageName;
    Payload->TryGetStringField(TEXT("packageName"), RequestedPackageName);
    if (RequestedPackageName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("outputPath"), RequestedPackageName);
    }
    if (RequestedPackageName.IsEmpty()) {
      RequestedPackageName = FString::Printf(TEXT("/Game/MCPTest/MergedActors/SM_Merged_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    }
    if (!FPackageName::IsValidLongPackageName(RequestedPackageName)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("merge_actors requires packageName/outputPath to be a valid long package name"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString MergeBasePackageName = RequestedPackageName;
    const FString RequestedAssetName = FPackageName::GetShortName(RequestedPackageName);
    if (RequestedAssetName.StartsWith(TEXT("SM_"))) {
      const FString BaseAssetName = RequestedAssetName.RightChop(3);
      if (!BaseAssetName.IsEmpty()) {
        MergeBasePackageName = FPackageName::GetLongPackagePath(RequestedPackageName) / BaseAssetName;
      }
    }

    if (!FPackageName::IsValidLongPackageName(MergeBasePackageName)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("merge_actors normalized packageName/outputPath to an invalid merge base package name"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TArray<UPrimitiveComponent *> ComponentsToMerge;
    for (AActor *Actor : ActorsToMerge) {
      if (!Actor) {
        continue;
      }
      TArray<UStaticMeshComponent *> StaticMeshComponents;
      Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
      for (UStaticMeshComponent *Component : StaticMeshComponents) {
        if (Component && Component->GetStaticMesh()) {
          ComponentsToMerge.Add(Component);
        }
      }
    }

    if (ComponentsToMerge.Num() < 2) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("merge_actors requires at least 2 static mesh components"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    IMeshMergeModule &MeshMergeModule = FModuleManager::LoadModuleChecked<IMeshMergeModule>(TEXT("MeshMergeUtilities"));
    const IMeshMergeUtilities &MeshMergeUtilities = MeshMergeModule.GetUtilities();

    FMeshMergingSettings MergeSettings;
    MergeSettings.bMergeMaterials = false;
    MergeSettings.bGenerateLightMapUV = true;
    MergeSettings.bBakeVertexDataToMesh = true;
    MergeSettings.bMergePhysicsData = true;
    MergeSettings.LODSelectionType = EMeshLODSelectionType::AllLODs;
    MergeSettings.TargetLightMapResolution = 64;

    TArray<UObject *> AssetsToSync;
    FVector MergedActorLocation = FVector::ZeroVector;
    const float ScreenAreaSize = TNumericLimits<float>::Max();
    MeshMergeUtilities.MergeComponentsToStaticMesh(
        ComponentsToMerge, World, MergeSettings, nullptr, nullptr, MergeBasePackageName,
        AssetsToSync, MergedActorLocation, ScreenAreaSize, true);

    UStaticMesh *MergedMesh = nullptr;
    for (UObject *Asset : AssetsToSync) {
      if (!Asset) {
        continue;
      }
      if (UStaticMesh *StaticMesh = Cast<UStaticMesh>(Asset)) {
        MergedMesh = StaticMesh;
      }
      FAssetRegistryModule::AssetCreated(Asset);
      Asset->MarkPackageDirty();
    }

    if (!MergedMesh) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Actor merge produced no static mesh asset"),
                             nullptr, TEXT("MERGE_FAILED"));
      return true;
    }

    TArray<UStaticMesh *> MeshesToFinish;
    MeshesToFinish.Add(MergedMesh);
    FStaticMeshCompilingManager::Get().FinishCompilation(MeshesToFinish);
    FlushRenderingCommands();

    MergedMesh->SetFlags(RF_Public | RF_Standalone);
    MergedMesh->ClearFlags(RF_Transient);
    MergedMesh->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(MergedMesh);

    bool bSaved = false;
    if (UPackage *MergedPackage = MergedMesh->GetOutermost()) {
      MergedPackage->ClearFlags(RF_Transient);
      MergedPackage->SetDirtyFlag(true);

      bSaved = McpSafeAssetSave(MergedMesh);

      if (bSaved) {
        TArray<FString> PathsToScan;
        PathsToScan.Add(FPaths::GetPath(MergedPackage->GetName()));
        FAssetRegistryModule &AssetRegistryModule =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        AssetRegistryModule.Get().ScanPathsSynchronous(PathsToScan, false);
      }
    }
    if (!bSaved) {
      TSharedPtr<FJsonObject> Failure = McpHandlerUtils::CreateResultObject();
      Failure->SetStringField(TEXT("requestedPackageName"), RequestedPackageName);
      Failure->SetStringField(TEXT("mergeBasePackageName"), MergeBasePackageName);
      Failure->SetStringField(TEXT("actualPackageName"), MergedMesh->GetOutermost()->GetName());
      Failure->SetStringField(TEXT("assetPath"), MergedMesh->GetPathName());
      SendAutomationResponse(RequestingSocket, RequestId, false,
                              TEXT("Merged static mesh was created but could not be saved"),
                              Failure, TEXT("SAVE_FAILED"));
      return true;
    }

    bool bReplaceSources = false;
    Payload->TryGetBoolField(TEXT("replaceSourceActors"), bReplaceSources);
    if (bReplaceSources) {
      for (AActor *Actor : ActorsToMerge) {
        if (Actor) {
          Actor->Destroy();
        }
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetNumberField(TEXT("mergedActorCount"), ActorsToMerge.Num());
    Resp->SetNumberField(TEXT("mergedComponentCount"), ComponentsToMerge.Num());
    Resp->SetBoolField(TEXT("replaceSourceActors"), bReplaceSources);
    Resp->SetStringField(TEXT("requestedPackageName"), RequestedPackageName);
    Resp->SetStringField(TEXT("mergeBasePackageName"), MergeBasePackageName);
    Resp->SetStringField(TEXT("packageName"), MergedMesh->GetOutermost()->GetName());
    Resp->SetStringField(TEXT("assetPath"), MergedMesh->GetPathName());
    Resp->SetBoolField(TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> AssetPaths;
    for (UObject *Asset : AssetsToSync) {
      if (Asset) {
        AssetPaths.Add(MakeShared<FJsonValueString>(Asset->GetPathName()));
      }
    }
    Resp->SetArrayField(TEXT("assets"), AssetPaths);

    if (ActorsToMerge.Num() > 0 && ActorsToMerge[0]) {
      McpHandlerUtils::AddVerification(Resp, ActorsToMerge[0]);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Actors merged to static mesh"), Resp,
                           FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfRunBenchmark(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    double Duration = 60.0;
    Payload->TryGetNumberField(TEXT("duration"), Duration);
    const double BenchmarkDuration = FMath::Max(0.0, Duration);

    FString BenchmarkType = TEXT("all");
    Payload->TryGetStringField(TEXT("type"), BenchmarkType);

    // Start profiling for benchmark
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!GEngine || !World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("NO_WORLD"));
        return true;
    }

    const ERequestOrigin ResponseOrigin = CurrentRequestOrigin;
    GEngine->Exec(World, TEXT("stat startfile"));

    if (BenchmarkType.Equals(TEXT("gpu"), ESearchCase::IgnoreCase) ||
        BenchmarkType.Equals(TEXT("all"), ESearchCase::IgnoreCase))
    {
      GEngine->Exec(World, TEXT("profilegpu"));
    }

    SendProgressUpdate(
        RequestId,
        0.0f,
        FString::Printf(TEXT("Benchmark running for %.0fs"), BenchmarkDuration),
        true,
        ResponseOrigin);

    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [WeakThis, RequestingSocket, RequestId, BenchmarkType, BenchmarkDuration, ResponseOrigin](float) {
              UMcpAutomationBridgeSubsystem* Subsystem = WeakThis.Get();
              if (!Subsystem)
              {
                return false;
              }

              if (!GEditor || !GEngine)
              {
                Subsystem->SendAutomationResponse(
                    RequestingSocket, RequestId, false,
                    TEXT("Editor not available while completing benchmark"),
                    nullptr, TEXT("NO_EDITOR"), ResponseOrigin);
                return false;
              }

              UWorld* StopWorld = GEditor->GetEditorWorldContext().World();
              if (!StopWorld)
              {
                Subsystem->SendAutomationResponse(
                    RequestingSocket, RequestId, false,
                    TEXT("Editor world not available while completing benchmark"),
                    nullptr, TEXT("NO_WORLD"), ResponseOrigin);
                return false;
              }

              GEngine->Exec(StopWorld, TEXT("stat stopfile"));
              GEngine->Exec(StopWorld, TEXT("stat none"));

              TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
              Resp->SetNumberField(TEXT("duration"), BenchmarkDuration);
              Resp->SetStringField(TEXT("type"), BenchmarkType);
              Resp->SetStringField(TEXT("status"), TEXT("completed"));

              Subsystem->SendAutomationResponse(
                  RequestingSocket, RequestId, true,
                  FString::Printf(TEXT("Benchmark completed (type: %s, duration: %.0fs)"),
                                  *BenchmarkType, BenchmarkDuration),
                  Resp, FString(), ResponseOrigin);
              return false;
            }),
        static_cast<float>(BenchmarkDuration));
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfEnableGpuTiming(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    IConsoleVariable *CVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUStatsEnabled"));
    if (CVar) {
      CVar->Set(bEnabled ? 1 : 0);
    }

    // Also toggle stat gpu for visual feedback
    if (bEnabled) {
      if (!GEditor)
      {
          SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
          return true;
      }

      GEngine->Exec(GEditor->GetEditorWorldContext().World(),
                    TEXT("stat gpu"));
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("enabled"), bEnabled);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("GPU timing %s"),
                        bEnabled ? TEXT("enabled") : TEXT("disabled")),
        Resp);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfApplyBaselineSettings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    FString Profile = TEXT("balanced");
    Payload->TryGetStringField(TEXT("profile"), Profile);

    // Common optimization CVars
    auto SetCVar = [](const TCHAR *Name, int32 Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    if (Profile.Equals(TEXT("performance"), ESearchCase::IgnoreCase)) {
      SetCVar(TEXT("r.VSync"), 0);
      SetCVar(TEXT("r.AllowHDR"), 0);
      SetCVar(TEXT("r.MotionBlurQuality"), 0);
      SetCVar(TEXT("r.DepthOfFieldQuality"), 0);
      SetCVar(TEXT("r.BloomQuality"), 0);
      SetCVar(TEXT("r.ShadowQuality"), 1);
      SetCVar(TEXT("r.MaxAnisotropy"), 4);
    } else if (Profile.Equals(TEXT("quality"), ESearchCase::IgnoreCase)) {
      SetCVar(TEXT("r.VSync"), 1);
      SetCVar(TEXT("r.AllowHDR"), 1);
      SetCVar(TEXT("r.MotionBlurQuality"), 4);
      SetCVar(TEXT("r.DepthOfFieldQuality"), 2);
      SetCVar(TEXT("r.BloomQuality"), 5);
      SetCVar(TEXT("r.ShadowQuality"), 5);
      SetCVar(TEXT("r.MaxAnisotropy"), 16);
    } else {
      // Balanced defaults
      SetCVar(TEXT("r.VSync"), 1);
      SetCVar(TEXT("r.AllowHDR"), 1);
      SetCVar(TEXT("r.MotionBlurQuality"), 2);
      SetCVar(TEXT("r.DepthOfFieldQuality"), 1);
      SetCVar(TEXT("r.BloomQuality"), 3);
      SetCVar(TEXT("r.ShadowQuality"), 3);
      SetCVar(TEXT("r.MaxAnisotropy"), 8);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("profile"), Profile);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Baseline settings applied: %s"), *Profile), Resp);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfOptimizeDrawCalls(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    bool bInstancing = true;
    Payload->TryGetBoolField(TEXT("instancing"), bInstancing);

    auto SetCVar = [](const TCHAR *Name, int32 Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    // Draw call optimization CVars
    SetCVar(TEXT("r.MeshDrawCommands.DynamicInstancing"), bInstancing ? 1 : 0);
    SetCVar(TEXT("r.MeshDrawCommands.UseCachedCommands"), bEnabled ? 1 : 0);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("optimized"), bEnabled);
    Resp->SetBoolField(TEXT("instancing"), bInstancing);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Draw call optimizations configured"), Resp);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfConfigureOcclusionCulling(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    double OcclusionSlop = 0.0;
    bool bHasSlop = Payload->TryGetNumberField(TEXT("slop"), OcclusionSlop);

    double MinScreenRadiusForOcclusion = 0.0;
    bool bHasMinRadius = Payload->TryGetNumberField(
        TEXT("minScreenRadius"), MinScreenRadiusForOcclusion);

    auto SetCVar = [](const TCHAR *Name, int32 Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    auto SetCVarFloat = [](const TCHAR *Name, float Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    SetCVar(TEXT("r.AllowOcclusionQueries"), bEnabled ? 1 : 0);

    if (bHasSlop) {
      SetCVarFloat(TEXT("r.OcclusionSlop"), (float)OcclusionSlop);
    }

    if (bHasMinRadius) {
      SetCVarFloat(TEXT("r.OcclusionCullMinScreenRadius"),
                   (float)MinScreenRadiusForOcclusion);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("enabled"), bEnabled);
    if (bHasSlop) {
      Resp->SetNumberField(TEXT("slop"), OcclusionSlop);
    }
    if (bHasMinRadius) {
      Resp->SetNumberField(TEXT("minScreenRadius"), MinScreenRadiusForOcclusion);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Occlusion culling configured"), Resp);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfOptimizeShaders(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    FString Mode = TEXT("changed");
    Payload->TryGetStringField(TEXT("mode"), Mode);

    bool bForceRecompile = false;
    Payload->TryGetBoolField(TEXT("forceRecompile"), bForceRecompile);

    FString Cmd;
    if (bForceRecompile) {
      Cmd = TEXT("recompileshaders all");
    } else if (Mode.Equals(TEXT("material"), ESearchCase::IgnoreCase)) {
      Cmd = TEXT("recompileshaders material");
    } else if (Mode.Equals(TEXT("global"), ESearchCase::IgnoreCase)) {
      Cmd = TEXT("recompileshaders global");
    } else {
      Cmd = TEXT("recompileshaders changed");
    }

    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    GEngine->Exec(GEditor->GetEditorWorldContext().World(), *Cmd);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("mode"), Mode);
    Resp->SetBoolField(TEXT("forceRecompile"), bForceRecompile);
    Resp->SetStringField(TEXT("command"), Cmd);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Shader optimization initiated: %s"), *Cmd),
        Resp);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandlePerfConfigureWorldPartition(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    double CellSize = 0.0;
    bool bHasCellSize = Payload->TryGetNumberField(TEXT("cellSize"), CellSize);

    double LoadingRange = 0.0;
    bool bHasLoadingRange =
        Payload->TryGetNumberField(TEXT("loadingRange"), LoadingRange);

    auto SetCVar = [](const TCHAR *Name, int32 Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    auto SetCVarFloat = [](const TCHAR *Name, float Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    SetCVar(TEXT("wp.Runtime.EnableStreaming"), bEnabled ? 1 : 0);

    if (bHasCellSize) {
      SetCVarFloat(TEXT("wp.Runtime.RuntimeCellSize"), (float)CellSize);
    }

    if (bHasLoadingRange) {
      SetCVarFloat(TEXT("wp.Runtime.RuntimeStreamingRange"),
                   (float)LoadingRange);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("streamingEnabled"), bEnabled);
    if (bHasCellSize) {
      Resp->SetNumberField(TEXT("cellSize"), CellSize);
    }
    if (bHasLoadingRange) {
      Resp->SetNumberField(TEXT("loadingRange"), LoadingRange);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("World Partition settings configured"), Resp);
    return true;
}

#else

// RequiresEditor on every performance action means Execute() rejects before
// Run() in non-editor builds; these exist only so the module links.
#define MCP_PERF_HANDLER_STUB(Fn)                                             \
  bool UMcpAutomationBridgeSubsystem::Fn(const FString &,                     \
                                         const TSharedPtr<FJsonObject> &,     \
                                         FMcpResponseHandle) {                \
    return false;                                                             \
  }

MCP_PERF_HANDLER_STUB(HandlePerfGenerateMemoryReport)
MCP_PERF_HANDLER_STUB(HandlePerfGetStats)
MCP_PERF_HANDLER_STUB(HandlePerfStartProfiling)
MCP_PERF_HANDLER_STUB(HandlePerfStopProfiling)
MCP_PERF_HANDLER_STUB(HandlePerfShowFps)
MCP_PERF_HANDLER_STUB(HandlePerfShowStats)
MCP_PERF_HANDLER_STUB(HandlePerfSetScalability)
MCP_PERF_HANDLER_STUB(HandlePerfSetResolutionScale)
MCP_PERF_HANDLER_STUB(HandlePerfSetVsync)
MCP_PERF_HANDLER_STUB(HandlePerfSetFrameRateLimit)
MCP_PERF_HANDLER_STUB(HandlePerfEnableGpuTiming)
MCP_PERF_HANDLER_STUB(HandlePerfConfigureNanite)
MCP_PERF_HANDLER_STUB(HandlePerfConfigureLod)
MCP_PERF_HANDLER_STUB(HandlePerfConfigureTextureStreaming)
MCP_PERF_HANDLER_STUB(HandlePerfApplyBaselineSettings)
MCP_PERF_HANDLER_STUB(HandlePerfOptimizeDrawCalls)
MCP_PERF_HANDLER_STUB(HandlePerfConfigureOcclusionCulling)
MCP_PERF_HANDLER_STUB(HandlePerfOptimizeShaders)
MCP_PERF_HANDLER_STUB(HandlePerfConfigureWorldPartition)
MCP_PERF_HANDLER_STUB(HandlePerfMergeActors)
MCP_PERF_HANDLER_STUB(HandlePerfRunBenchmark)
#undef MCP_PERF_HANDLER_STUB

#endif // WITH_EDITOR
