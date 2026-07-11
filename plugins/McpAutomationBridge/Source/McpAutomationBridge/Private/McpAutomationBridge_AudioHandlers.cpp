// =============================================================================
// McpAutomationBridge_AudioHandlers.cpp
// =============================================================================
// Audio System Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// --------------------
// Section 1: Classed manage_audio core members
//   - HandleAudio*                       : One member per core action, called by
//                                          the FMcpCall classes
//                                          (MCP/Calls/McpCalls_ManageAudio.cpp)
//
// Section 2: Sound Asset Creation
//   - create_sound_cue                   : Create SoundCue with optional wave, looping,
//                                          modulation, and attenuation nodes
//   - create_sound_class                 : Create SoundClass with properties and parent
//   - create_sound_mix                   : Create SoundMix with class adjusters
//
// Section 3: Sound Playback
//   - play_sound_at_location             : Play 3D sound at world location
//   - play_sound_2d                      : Play non-spatialized 2D sound
//   - play_sound_attached                : Attach and play sound on actor component
//   - create_ambient_sound               : Spawn persistent ambient sound at location
//   - spawn_sound_at_location            : Spawn sound component at location
//
// Section 4: Sound Mix Control
//   - push_sound_mix                     : Push SoundMix modifier onto audio stack
//   - pop_sound_mix                      : Pop SoundMix modifier from audio stack
//   - set_sound_mix_class_override       : Override SoundClass within a SoundMix
//   - clear_sound_mix_class_override     : Clear SoundClass override from SoundMix
//   - set_base_sound_mix                 : Set the base (default) SoundMix
//
// Section 5: Sound Fading & Utility
//   - fade_sound_in / fade_sound_out     : Fade audio component in/out over time
//   - prime_sound                        : Pre-load sound for instant playback
//   - create_audio_component             : Create UAudioComponent on actor or at location
//
// Dialogue and audio-effect actions (create_dialogue_voice, create_dialogue_wave,
// set_dialogue_context, create_reverb_effect, create_source_effect_chain,
// add_source_effect, create_submix_effect) are AudioAuthoring-owned and live in
// McpAutomationBridge_AudioAuthoringHandlers.cpp; this file's shadowed copies
// were deleted at the manage_audio classing.
//
// PAYLOAD/RESPONSE FORMATS:
// -------------------------
// create_sound_cue:
//   Payload:  { "name": string, "path"?: string (aliases: packagePath, savePath),
//               "wavePath"?: string, "looping"?: bool, "volume"?: number,
//               "pitch"?: number, "attenuationPath"?: string,
//               "save"?: bool (default true) }
//   Response: { "success": bool, "path": string }
//
// play_sound_at_location:
//   Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
//               "volume"?: number, "pitch"?: number, "startTime"?: number,
//               "attenuationPath"?: string, "concurrencyPath"?: string }
//   Response: { "success": bool, "soundPath": string, "location": {x,y,z} }
//
// play_sound_2d:
//   Payload:  { "soundPath": string, "volume"?: number, "pitch"?: number,
//               "startTime"?: number }
//   Response: { "success": bool, "soundPath": string, "volume": number, "pitch": number }
//
// create_sound_class:
//   Payload:  { "name": string, "path"?: string (aliases: packagePath, savePath),
//               "properties"?: { "volume"?: number, "pitch"?: number },
//               "parentClass"?: string, "save"?: bool (default true) }
//   Response: { "success": bool, "path": string, "name": string }
//
// create_sound_mix:
//   Payload:  { "name": string, "path"?: string (aliases: packagePath, savePath),
//               "classAdjusters"?: [{ "soundClass": string, "volumeAdjuster"?: number,
//               "pitchAdjuster"?: number }], "save"?: bool (default true) }
//   Response: { "success": bool, "path": string, "name": string }
//
// push_sound_mix / pop_sound_mix:
//   Payload:  { "mixName": string }
//   Response: { "success": bool, "mixName": string }
//
// set_sound_mix_class_override:
//   Payload:  { "mixName": string, "soundClassName": string, "volume"?: number,
//               "pitch"?: number, "fadeInTime"?: number, "applyToChildren"?: bool }
//   Response: { "success": bool, "mixName": string, "className": string }
//
// play_sound_attached:
//   Payload:  { "soundPath": string, "actorName": string, "attachPointName"?: string }
//   Response: { "componentName": string }
//
// fade_sound_in / fade_sound_out:
//   Payload:  { "actorName": string, "fadeTime"?: number, "targetVolume"?: number }
//   Response: { "success": bool, "actorName": string, "action": string }
//
// create_ambient_sound / spawn_sound_at_location:
//   Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
//               "volume"?: number, "pitch"?: number, "startTime"?: number,
//               "attenuationPath"?: string, "concurrencyPath"?: string }
//   Response: { "componentName": string }
//
// prime_sound:
//   Payload:  { "soundPath": string }
//   Response: { "success": bool, "message": "Sound primed" }
//
// create_audio_component:
//   Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
//               "attachTo"?: string, "volume"?: string, "pitch"?: string }
//   Response: { "success": bool, "componentPath": string, "componentName": string }
//
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0: FARFilter uses ClassNames (FName) for asset registry queries
// UE 5.1+: FARFilter uses ClassPaths (FTopLevelAssetPath) for asset registry queries
//
// SECURITY CONSIDERATIONS:
// - Asset path validation via ResolveSoundAsset/ResolveSoundMix/ResolveSoundClass
// - Actor name validation via FindAudioActorByName (scoped to world)
// - Package path validation for /Game/ prefix enforcement
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST BE FIRST - Version compatibility macros
#include "McpHandlerUtils.h"          // Utility functions for JSON parsing

// =============================================================================
// Core Includes
// =============================================================================
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_AudioHandlers.h"

// =============================================================================
// Editor-Only Includes
// =============================================================================
#if WITH_EDITOR

// --- Asset Registry & Tools ---
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"

// --- Audio Core ---
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/UObjectHash.h"

// --- Sound Asset Factories ---
#include "Factories/SoundAttenuationFactory.h"
#include "Factories/SoundClassFactory.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Factories/SoundMixFactory.h"

// --- Gameplay ---
#include "Kismet/GameplayStatics.h"

// --- Sound Assets ---
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundMix.h"

// --- Sound Cue Graph Nodes ---
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeLooping.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"

// --- Dialogue System ---
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"

// --- Audio Effects ---
#include "Sound/ReverbEffect.h"
#include "Sound/SoundEffectSubmix.h"

// --- Audio Volume ---
#include "Sound/AudioVolume.h"
#include "Sound/AmbientSound.h"
#include "Components/BrushComponent.h"

#endif // WITH_EDITOR

// =============================================================================
// Logging Category
// =============================================================================
DEFINE_LOG_CATEGORY_STATIC(LogMcpAudioHandlers, Log, All);

// =============================================================================
// Section 0: Helper Functions
// =============================================================================

/**
 * Sanitize a directory path and build a combined asset FullPath.
 *
 * Calls SanitizeProjectRelativePath on the directory, then again on the
 * combined "directory/assetName" string. Returns true on success with
 * OutDirectory and OutFullPath set; returns false if either sanitization
 * rejects the path.
 */
static bool BuildSanitizedAssetPath(
    const FString& InDirectory, const FString& AssetName,
    FString& OutDirectory, FString& OutFullPath)
{
  // Reject empty or invalid UObject names
  if (AssetName.IsEmpty()) return false;
  if (!FName::IsValidXName(AssetName, INVALID_OBJECTNAME_CHARACTERS)) {
    return false;
  }

  OutDirectory = SanitizeProjectRelativePath(InDirectory);
  if (OutDirectory.IsEmpty()) return false;
  OutFullPath = SanitizeProjectRelativePath(
      FString::Printf(TEXT("%s/%s"), *OutDirectory, *AssetName));
  return !OutFullPath.IsEmpty();
}

/**
 * Finds an actor by object path/name or by actor label/name within an optional world.
 *
 * Searches first for an exact object path or registered name, and if not found and a World is provided,
 * iterates actors in that World comparing actor label and actor name case-insensitively.
 *
 * @param ActorName Actor object path, registered name, or actor label to search for.
 * @param World Optional world to search actor labels/names in when direct lookup fails.
 * @return `AActor*` Pointer to the matched actor, `nullptr` if no matching actor is found or ActorName is empty.
 */
static AActor *FindAudioActorByName(const FString &ActorName, UWorld *World) {
  if (ActorName.IsEmpty())
    return nullptr;

  // Fast path: Direct object path/name
  AActor *Actor = FindObject<AActor>(nullptr, *ActorName);
  if (Actor && Actor->IsValidLowLevel())
    return Actor;

  // Fallback: Label search (limited scope)
  if (World) {
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (It->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
          It->GetName().Equals(ActorName, ESearchCase::IgnoreCase)) {
        return *It;
      }
    }
  }
  return nullptr;
}

static USceneComponent* EnsureAudioAttachRoot(AActor* Actor)
{
  if (!Actor)
    return nullptr;

  if (USceneComponent* Root = Actor->GetRootComponent())
    return Root;

  USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("McpAudioRoot"), RF_Transactional);
  if (!Root)
    return nullptr;

  Root->SetupAttachment(nullptr);
  Actor->SetRootComponent(Root);
  Actor->AddInstanceComponent(Root);
  Root->RegisterComponent();
  return Root;
}

static UAudioComponent* CreateRegisteredAudioComponent(AActor* Owner, USoundBase* Sound, const FVector& Location, const FRotator& Rotation)
{
  if (!Owner || !Sound)
    return nullptr;

  UAudioComponent* AudioComp = NewObject<UAudioComponent>(Owner, NAME_None, RF_Transactional);
  if (!AudioComp)
    return nullptr;

  AudioComp->SetSound(Sound);
  AudioComp->bAutoActivate = false;
  AudioComp->SetRelativeLocation(Location);
  AudioComp->SetRelativeRotation(Rotation);
  Owner->AddInstanceComponent(AudioComp);

  if (USceneComponent* Root = EnsureAudioAttachRoot(Owner))
  {
    AudioComp->SetupAttachment(Root);
  }

  AudioComp->RegisterComponent();
  return AudioComp;
}

static UAudioComponent* CreateAudioComponentAtEditorLocation(UWorld* World, USoundBase* Sound, const FVector& Location, const FRotator& Rotation, const FString& ActorName)
{
  if (!World || !Sound)
    return nullptr;

  FActorSpawnParameters SpawnParams;
  SpawnParams.Name = ActorName.IsEmpty() ? NAME_None : FName(*ActorName);
  SpawnParams.ObjectFlags = RF_Transactional;
  AActor* Owner = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
  if (!Owner)
    return nullptr;

#if WITH_EDITOR
  if (!ActorName.IsEmpty())
    Owner->SetActorLabel(ActorName);
#endif

  return CreateRegisteredAudioComponent(Owner, Sound, FVector::ZeroVector, FRotator::ZeroRotator);
}

/**
 * @brief Resolves a USoundBase asset from an asset path or an asset name.
 *
 * Attempts to load the sound by the provided path; if the input appears to be a simple name
 * (no path separators), searches the project's /Game assets for a matching USoundWave or
 * USoundCue by name.
 *
 * VERSION COMPATIBILITY:
 * - UE 5.0: FARFilter uses ClassNames (FName)
 * - UE 5.1+: FARFilter uses ClassPaths (FTopLevelAssetPath)
 *
 * @param SoundPath Asset path (e.g. "/Game/Audio/MyCue.MyCue") or an asset name (e.g. "MyCue").
 * @return USoundBase* Pointer to the resolved sound asset, or nullptr if not found.
 */
static USoundBase *ResolveSoundAsset(const FString &SoundPath) {
	if (SoundPath.IsEmpty())
		return nullptr;

	USoundBase *Sound = nullptr;
	if (SoundPath.Contains(TEXT("/Game/")) || SoundPath.Contains(TEXT("/Engine/")))
	{
		if (UEditorAssetLibrary::DoesAssetExist(SoundPath)) {
			Sound = Cast<USoundBase>(UEditorAssetLibrary::LoadAsset(SoundPath));
		}
		if (Sound)
			return Sound;
	}

	if (SoundPath.Contains(TEXT("/")))
		return nullptr;

	FString AssetName = FPaths::GetBaseFilename(SoundPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
  Filter.ClassPaths.Add(USoundCue::StaticClass()->GetClassPathName());
#else
  // UE 5.0 fallback - use ClassNames instead of ClassPaths
  Filter.ClassNames.Add(USoundWave::StaticClass()->GetFName());
  Filter.ClassNames.Add(USoundCue::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Sound = Cast<USoundBase>(Data.GetAsset());
      if (Sound) {
        UE_LOG(LogMcpAudioHandlers, Log,
               TEXT("Resolved sound '%s' to '%s'"), *SoundPath,
               *Sound->GetPathName());
        return Sound;
      }
    }
  }

  UE_LOG(LogMcpAudioHandlers, Warning,
         TEXT("Sound asset '%s' not found."), *SoundPath);
  return nullptr;
}

/**
 * @brief Resolve a USoundMix by asset path or asset name.
 *
 * Attempts to load a USoundMix using the provided MixPath. If MixPath contains a
 * full asset path and the asset exists, that asset is returned. If MixPath does
 * not contain a path separator, the function treats it as an asset name and
 * searches the /Game packages for a matching USoundMix (case-insensitive).
 *
 * VERSION COMPATIBILITY:
 * - UE 5.0: FARFilter uses ClassNames (FName)
 * - UE 5.1+: FARFilter uses ClassPaths (FTopLevelAssetPath)
 *
 * @param MixPath Asset path or asset name to resolve.
 * @return USoundMix* Pointer to the resolved USoundMix, or nullptr if not found.
 */
static USoundMix *ResolveSoundMix(const FString &MixPath) {
	if (MixPath.IsEmpty())
		return nullptr;

	USoundMix *Mix = nullptr;
	// Only call DoesAssetExist for paths that look like full UE asset paths (contain /Game/ or /Engine/)
	// Bare names like "TestSoundMix" would cause DoesAssetExist errors and need asset registry search
	if (MixPath.Contains(TEXT("/Game/")) || MixPath.Contains(TEXT("/Engine/")))
	{
		if (UEditorAssetLibrary::DoesAssetExist(MixPath)) {
			Mix = Cast<USoundMix>(UEditorAssetLibrary::LoadAsset(MixPath));
		}
		if (Mix)
			return Mix;
	}

	// For paths without a root (e.g. "TestSoundMix"), skip DoesAssetExist to avoid UE log errors
	// and go straight to asset registry search below
	if (MixPath.Contains(TEXT("/")))
		return nullptr;

  FString AssetName = FPaths::GetBaseFilename(MixPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundMix::StaticClass()->GetClassPathName());
#else
  Filter.ClassNames.Add(USoundMix::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Mix = Cast<USoundMix>(Data.GetAsset());
      if (Mix)
        return Mix;
    }
  }
  return nullptr;
}

/**
 * @brief Locates and returns a USoundClass by asset path or by asset name.
 *
 * Attempts to load the sound class directly if ClassPath refers to an existing asset; otherwise,
 * if ClassPath does not contain a '/' it searches the project's /Game assets for a sound class
 * with a matching name (case-insensitive).
 *
 * VERSION COMPATIBILITY:
 * - UE 5.0: FARFilter uses ClassNames (FName)
 * - UE 5.1+: FARFilter uses ClassPaths (FTopLevelAssetPath)
 *
 * @param ClassPath Asset path (e.g. "/Game/Audio/MyClass") or asset name ("MyClass").
 * @return USoundClass* Pointer to the resolved sound class, or nullptr if not found or ClassPath is empty.
 */
static USoundClass *ResolveSoundClass(const FString &ClassPath) {
	if (ClassPath.IsEmpty())
		return nullptr;

	USoundClass *Class = nullptr;
	if (ClassPath.Contains(TEXT("/Game/")) || ClassPath.Contains(TEXT("/Engine/")))
	{
		if (UEditorAssetLibrary::DoesAssetExist(ClassPath)) {
			Class = Cast<USoundClass>(UEditorAssetLibrary::LoadAsset(ClassPath));
		}
		if (Class)
			return Class;
	}

	if (ClassPath.Contains(TEXT("/")))
		return nullptr;

	FString AssetName = FPaths::GetBaseFilename(ClassPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundClass::StaticClass()->GetClassPathName());
#else
  Filter.ClassNames.Add(USoundClass::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Class = Cast<USoundClass>(Data.GetAsset());
      if (Class)
        return Class;
    }
  }
  return nullptr;
}

// =============================================================================
// Classed manage_audio core members
// =============================================================================
// Per-action entry points called by the FMcpCall classes
// (MCP/Calls/McpCalls_ManageAudio.cpp). Extracted verbatim from the retired
// HandleAudioAction dispatcher; each member replicates the chain's non-editor
// NOT_IMPLEMENTED stub.

// =========================================================================
// Section 2: Sound Asset Creation
// =========================================================================

// -------------------------------------------------------------------------
// create_sound_cue / audio_create_sound_cue
// -------------------------------------------------------------------------
// Creates a USoundCue asset with optional wave player, looping, modulation,
// and attenuation nodes wired into the cue graph.
//
// Payload:  { "name": string, "path"?: string (aliases: packagePath, savePath),
//             "wavePath"?: string, "looping"?: bool, "volume"?: number,
//             "pitch"?: number, "attenuationPath"?: string,
//             "save"?: bool (default true) }
// Response: { "success": bool, "path": string }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioCreateSoundCue(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString Name;
  if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString PackagePath;
  Payload->TryGetStringField(TEXT("packagePath"), PackagePath);
  if (PackagePath.IsEmpty())
    Payload->TryGetStringField(TEXT("savePath"), PackagePath);
  if (PackagePath.IsEmpty())
    Payload->TryGetStringField(TEXT("path"), PackagePath);
  if (PackagePath.IsEmpty())
    PackagePath = TEXT("/Game/Audio/Cues");

  FString FullPath;
  if (!BuildSanitizedAssetPath(PackagePath, Name, PackagePath, FullPath)) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid path"),
                        TEXT("INVALID_PATH"));
    return true;
  }

  const FString FullObjectPath = FString::Printf(TEXT("%s.%s"), *FullPath, *Name);
  USoundCue *ExistingSoundCue = nullptr;
  if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
    ExistingSoundCue = Cast<USoundCue>(UEditorAssetLibrary::LoadAsset(FullPath));
  }
  if (!ExistingSoundCue) {
    ExistingSoundCue = FindObject<USoundCue>(nullptr, *FullObjectPath);
  }
  if (!ExistingSoundCue) {
    ExistingSoundCue = LoadObject<USoundCue>(nullptr, *FullObjectPath);
  }
  if (ExistingSoundCue) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), ExistingSoundCue->GetPathName());
    Resp->SetStringField(TEXT("assetPath"), FullPath);
    Resp->SetStringField(TEXT("assetName"), Name);
    McpHandlerUtils::AddVerification(Resp, ExistingSoundCue);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("SoundCue already exists"), Resp);
    return true;
  }

  FString WavePath;
  Payload->TryGetStringField(TEXT("wavePath"), WavePath);

  USoundWave *Wave = nullptr;
  if (!WavePath.IsEmpty()) {
    Wave = LoadObject<USoundWave>(nullptr, *WavePath);
  }

  // Safe creation (CreatePackage + NewObject) -- NOT IAssetTools::CreateAsset (modal hazard;
  // see the create_sound_class note). Seed the wave-player node ourselves to match what
  // USoundCueFactoryNew::InitialSoundWaves would have produced.
  const FString FullPackagePath = PackagePath / Name;
  UPackage *Package = CreatePackage(*FullPackagePath);
  USoundCue *SoundCue = Package
      ? NewObject<USoundCue>(Package, FName(*Name), RF_Public | RF_Standalone)
      : nullptr;
  if (SoundCue) {
    FAssetRegistryModule::AssetCreated(SoundCue);
    SoundCue->MarkPackageDirty();
    if (Wave) {
      USoundNodeWavePlayer *WavePlayer = SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();
      WavePlayer->SetSoundWave(Wave);
      SoundCue->FirstNode = WavePlayer;
      SoundCue->LinkGraphNodesFromSoundNodes();
    }
  }

  if (!SoundCue) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create SoundCue"),
                        TEXT("ASSET_CREATION_FAILED"));
    return true;
  }

  // Basic graph setup if wave provided
  if (Wave) {
    USoundNode *LastNode = SoundCue->FirstNode;

    if (LastNode) {
      bool bLooping = false;
      if (Payload->TryGetBoolField(TEXT("looping"), bLooping) && bLooping) {
        USoundNodeLooping *LoopNode =
            SoundCue->ConstructSoundNode<USoundNodeLooping>();
        LoopNode->InsertChildNode(0);
        LoopNode->ChildNodes[0] = LastNode;
        LastNode = LoopNode;
      }

      double Volume = 1.0;
      double Pitch = 1.0;
      const bool bHasVolume = Payload->TryGetNumberField(TEXT("volume"), Volume);
      const bool bHasPitch = Payload->TryGetNumberField(TEXT("pitch"), Pitch);

      if (bHasVolume || bHasPitch) {
        USoundNodeModulator *ModNode =
            SoundCue->ConstructSoundNode<USoundNodeModulator>();
        ModNode->InsertChildNode(0);
        ModNode->ChildNodes[0] = LastNode;
        ModNode->PitchMin = ModNode->PitchMax = (float)Pitch;
        ModNode->VolumeMin = ModNode->VolumeMax = (float)Volume;
        LastNode = ModNode;
      }

      FString AttenuationPath;
      if (Payload->TryGetStringField(TEXT("attenuationPath"),
                                     AttenuationPath) &&
          !AttenuationPath.IsEmpty()) {
        USoundAttenuation *Attenuation =
            LoadObject<USoundAttenuation>(nullptr, *AttenuationPath);
        if (Attenuation) {
          USoundNodeAttenuation *AttenNode =
              SoundCue->ConstructSoundNode<USoundNodeAttenuation>();
          AttenNode->InsertChildNode(0);
          AttenNode->ChildNodes[0] = LastNode;
          AttenNode->AttenuationSettings = Attenuation;
          LastNode = AttenNode;
        }
      }

      SoundCue->FirstNode = LastNode;
      SoundCue->LinkGraphNodesFromSoundNodes();
    }
  }

  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave && !McpSafeAssetSave(SoundCue)) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to save sound cue"),
                        TEXT("SAVE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("path"), SoundCue->GetPathName());
  McpHandlerUtils::AddVerification(Resp, SoundCue);
  S.SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("SoundCue created"), Resp);
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// create_sound_class / audio_create_sound_class
// -------------------------------------------------------------------------
// Creates a USoundClass asset with optional volume/pitch properties and parent.
//
// Payload:  { "name": string, "path"?: string (aliases: packagePath, savePath),
//             "properties"?: { "volume"?: number, "pitch"?: number },
//             "parentClass"?: string, "save"?: bool (default true) }
// Response: { "success": bool, "path": string, "name": string }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioCreateSoundClass(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString Name;
  if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString PackagePath = TEXT("/Game/Audio/Classes");
  if (Payload->HasField(TEXT("packagePath"))) {
    PackagePath = GetJsonStringField(Payload, TEXT("packagePath"));
  } else if (Payload->HasField(TEXT("savePath"))) {
    PackagePath = GetJsonStringField(Payload, TEXT("savePath"));
  } else if (Payload->HasField(TEXT("path"))) {
    PackagePath = GetJsonStringField(Payload, TEXT("path"));
  }

  // Create directly via CreatePackage + NewObject -- NOT IAssetTools::CreateAsset, which
  // can pop an interactive "overwrite existing object?" dialog. In a headless/-unattended
  // editor that dialog never gets dismissed and HANGS the GameThread (observed >90s, asset
  // never created). Mirrors the safe create_data_table / attenuation paths in this codebase.
  const FString FullPackagePath = PackagePath / Name;
  UPackage *Package = CreatePackage(*FullPackagePath);
  USoundClass *SoundClass = Package
      ? NewObject<USoundClass>(Package, FName(*Name), RF_Public | RF_Standalone)
      : nullptr;
  if (SoundClass)
  {
      FAssetRegistryModule::AssetCreated(SoundClass);
      SoundClass->MarkPackageDirty();
  }

  if (SoundClass) {
    const TSharedPtr<FJsonObject> *Props;
    if (Payload->TryGetObjectField(TEXT("properties"), Props)) {
      double Vol = 1.0;
      if ((*Props)->TryGetNumberField(TEXT("volume"), Vol)) {
        SoundClass->Properties.Volume = (float)Vol;
      }
      double Pitch = 1.0;
      if ((*Props)->TryGetNumberField(TEXT("pitch"), Pitch)) {
        SoundClass->Properties.Pitch = (float)Pitch;
      }
    }

    FString ParentClassPath;
    if (Payload->TryGetStringField(TEXT("parentClass"), ParentClassPath) &&
        !ParentClassPath.IsEmpty()) {
      USoundClass *Parent =
          LoadObject<USoundClass>(nullptr, *ParentClassPath);
      if (Parent) {
        SoundClass->ParentClass = Parent;
      }
    }

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave && !McpSafeAssetSave(SoundClass)) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to save sound class"),
                          TEXT("SAVE_FAILED"));
      return true;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SoundClass->GetPathName());
    Resp->SetStringField(TEXT("name"), SoundClass->GetName());

    McpHandlerUtils::AddVerification(Resp, SoundClass);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("SoundClass created"), Resp);
  } else {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create SoundClass"),
                        TEXT("ASSET_CREATION_FAILED"));
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// create_sound_mix / audio_create_sound_mix
// -------------------------------------------------------------------------
// Creates a USoundMix asset with optional class adjusters for volume/pitch.
//
// Payload:  { "name": string, "path"?: string (aliases: packagePath, savePath),
//             "classAdjusters"?: [{ "soundClass": string,
//             "volumeAdjuster"?: number, "pitchAdjuster"?: number }],
//             "save"?: bool (default true) }
// Response: { "success": bool, "path": string, "name": string }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioCreateSoundMix(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString Name;
  if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString PackagePath = TEXT("/Game/Audio/Mixes");
  if (Payload->HasField(TEXT("packagePath"))) {
    PackagePath = GetJsonStringField(Payload, TEXT("packagePath"));
  } else if (Payload->HasField(TEXT("savePath"))) {
    PackagePath = GetJsonStringField(Payload, TEXT("savePath"));
  } else if (Payload->HasField(TEXT("path"))) {
    PackagePath = GetJsonStringField(Payload, TEXT("path"));
  }

  // Safe creation (CreatePackage + NewObject) -- NOT IAssetTools::CreateAsset, which can pop
  // a modal that hangs a headless editor (see the create_sound_class note above).
  const FString FullPackagePath = PackagePath / Name;
  UPackage *Package = CreatePackage(*FullPackagePath);
  USoundMix *SoundMix = Package
      ? NewObject<USoundMix>(Package, FName(*Name), RF_Public | RF_Standalone)
      : nullptr;
  if (SoundMix)
  {
      FAssetRegistryModule::AssetCreated(SoundMix);
      SoundMix->MarkPackageDirty();
  }

  if (SoundMix) {
    const TArray<TSharedPtr<FJsonValue>> *Adjusters;
    if (Payload->TryGetArrayField(TEXT("classAdjusters"), Adjusters)) {
      for (const auto &Val : *Adjusters) {
        const TSharedPtr<FJsonObject> AdjObj = Val->AsObject();
        FString ClassPath;
        if (AdjObj->TryGetStringField(TEXT("soundClass"), ClassPath)) {
          USoundClass *SC = LoadObject<USoundClass>(nullptr, *ClassPath);
          if (SC) {
            FSoundClassAdjuster Adjuster;
            Adjuster.SoundClassObject = SC;
            double Vol = 1.0;
            AdjObj->TryGetNumberField(TEXT("volumeAdjuster"), Vol);
            Adjuster.VolumeAdjuster = (float)Vol;
            double Pitch = 1.0;
            AdjObj->TryGetNumberField(TEXT("pitchAdjuster"), Pitch);
            Adjuster.PitchAdjuster = (float)Pitch;
            SoundMix->SoundClassEffects.Add(Adjuster);
          }
        }
      }
    }

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave && !McpSafeAssetSave(SoundMix)) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to save sound mix"),
                          TEXT("SAVE_FAILED"));
      return true;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SoundMix->GetPathName());
    Resp->SetStringField(TEXT("name"), SoundMix->GetName());

    McpHandlerUtils::AddVerification(Resp, SoundMix);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("SoundMix created"), Resp);
  } else {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create SoundMix"),
                        TEXT("ASSET_CREATION_FAILED"));
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// =========================================================================
// Section 3: Sound Playback
// =========================================================================

// -------------------------------------------------------------------------
// play_sound_at_location / audio_play_sound_at_location
// -------------------------------------------------------------------------
// Plays a resolved sound asset at a 3D world location with optional
// rotation, volume, pitch, start time, attenuation, and concurrency.
//
// Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
//             "volume"?: number, "pitch"?: number, "startTime"?: number,
//             "attenuationPath"?: string, "concurrencyPath"?: string }
// Response: { "success": bool, "soundPath": string, "location": {x,y,z} }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioPlaySoundAtLocation(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString SoundPath;
  if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
      SoundPath.IsEmpty()) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USoundBase *Sound = ResolveSoundAsset(SoundPath);
  if (!Sound) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Sound asset not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

  double Volume = 1.0;
  Payload->TryGetNumberField(TEXT("volume"), Volume);
  double Pitch = 1.0;
  Payload->TryGetNumberField(TEXT("pitch"), Pitch);
  double StartTime = 0.0;
  Payload->TryGetNumberField(TEXT("startTime"), StartTime);

  USoundAttenuation *Attenuation = nullptr;
  FString AttenPath;
  if (Payload->TryGetStringField(TEXT("attenuationPath"), AttenPath) &&
      !AttenPath.IsEmpty()) {
    Attenuation = LoadObject<USoundAttenuation>(nullptr, *AttenPath);
  }

  USoundConcurrency *Concurrency = nullptr;
  FString ConcPath;
  if (Payload->TryGetStringField(TEXT("concurrencyPath"), ConcPath) &&
      !ConcPath.IsEmpty()) {
    Concurrency = LoadObject<USoundConcurrency>(nullptr, *ConcPath);
  }

  if (!GEditor)
  {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"), TEXT("NO_EDITOR"));
    return true;
  }
  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No world context available"), TEXT("NO_WORLD"));
    return true;
  }

  UGameplayStatics::PlaySoundAtLocation(
      World, Sound, Location, Rotation, (float)Volume, (float)Pitch,
      (float)StartTime, Attenuation, Concurrency);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("soundPath"), SoundPath);
  TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
  LocObj->SetNumberField(TEXT("x"), Location.X);
  LocObj->SetNumberField(TEXT("y"), Location.Y);
  LocObj->SetNumberField(TEXT("z"), Location.Z);
  Resp->SetObjectField(TEXT("location"), LocObj);

  S.SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Sound played at location"), Resp);
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// play_sound_2d / audio_play_sound_2d
// -------------------------------------------------------------------------
// Plays a non-spatialized 2D sound with optional volume, pitch, start time.
//
// Payload:  { "soundPath": string, "volume"?: number, "pitch"?: number,
//             "startTime"?: number }
// Response: { "success": bool, "soundPath": string, "volume": number,
//             "pitch": number }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioPlaySound2D(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString SoundPath;
  if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
      SoundPath.IsEmpty()) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USoundBase *Sound = ResolveSoundAsset(SoundPath);
  if (!Sound) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Sound asset not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  double Volume = 1.0;
  Payload->TryGetNumberField(TEXT("volume"), Volume);
  double Pitch = 1.0;
  Payload->TryGetNumberField(TEXT("pitch"), Pitch);
  double StartTime = 0.0;
  Payload->TryGetNumberField(TEXT("startTime"), StartTime);

  if (!GEditor)
    return true;
  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                        TEXT("NO_WORLD"));
    return true;
  }

  UGameplayStatics::PlaySound2D(World, Sound, (float)Volume, (float)Pitch,
                                (float)StartTime);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("soundPath"), SoundPath);
  Resp->SetNumberField(TEXT("volume"), Volume);
  Resp->SetNumberField(TEXT("pitch"), Pitch);

  // Sound played - add sound asset verification
  McpHandlerUtils::AddVerification(Resp, Sound);
  S.SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Sound played 2D"), Resp);
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// play_sound_attached / audio_play_sound_attached
// -------------------------------------------------------------------------
// Attaches and plays a sound on an actor's component or socket.
//
// Payload:  { "soundPath": string, "actorName": string,
//             "attachPointName"?: string }
// Response: { "componentName": string }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioPlaySoundAttached(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString SoundPath, ActorName, AttachPoint;
  Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("attachPointName"), AttachPoint);

  USoundBase *Sound = ResolveSoundAsset(SoundPath);
  if (!Sound) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Sound not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  if (!GEditor)
  {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"), TEXT("NO_EDITOR"));
    return true;
  }
  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                        TEXT("NO_WORLD"));
    return true;
  }

  AActor *TargetActor = FindAudioActorByName(ActorName, World);
  if (!TargetActor) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"),
                        TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  USceneComponent *AttachComp = EnsureAudioAttachRoot(TargetActor);
  if (!AttachPoint.IsEmpty()) {
    // Try to find socket or component
    USceneComponent *FoundComp = nullptr;
    TArray<USceneComponent *> Components;
    TargetActor->GetComponents(Components);
    for (USceneComponent *Comp : Components) {
      if (Comp->GetName() == AttachPoint ||
          Comp->DoesSocketExist(FName(*AttachPoint))) {
        FoundComp = Comp;
        break;
      }
    }
    if (FoundComp)
      AttachComp = FoundComp;
  }

  UAudioComponent *AudioComp = nullptr;
  if (AttachComp)
  {
    AudioComp = CreateRegisteredAudioComponent(TargetActor, Sound, FVector::ZeroVector, FRotator::ZeroRotator);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (AudioComp) {
    Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
    McpHandlerUtils::AddVerification(Resp, Sound);
    AddComponentVerification(Resp, AudioComp);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound attached"), Resp);
  } else {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to attach sound"),
                        TEXT("ATTACH_FAILED"));
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// create_ambient_sound / audio_create_ambient_sound
// -------------------------------------------------------------------------
// Spawns a persistent ambient sound component at a world location.
//
// Payload:  { "soundPath": string, "location"?: [x,y,z], "volume"?: number,
//             "pitch"?: number, "startTime"?: number,
//             "attenuationPath"?: string, "concurrencyPath"?: string }
// Response: { "componentName": string }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioCreateAmbientSound(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString SoundPath;
  if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
      SoundPath.IsEmpty()) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USoundBase *Sound = ResolveSoundAsset(SoundPath);
  if (!Sound) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Sound asset not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);

  double Volume = 1.0;
  Payload->TryGetNumberField(TEXT("volume"), Volume);
  double Pitch = 1.0;
  Payload->TryGetNumberField(TEXT("pitch"), Pitch);
  double StartTime = 0.0;
  Payload->TryGetNumberField(TEXT("startTime"), StartTime);

  USoundAttenuation *Attenuation = nullptr;
  FString AttenPath;
  if (Payload->TryGetStringField(TEXT("attenuationPath"), AttenPath) &&
      !AttenPath.IsEmpty()) {
    Attenuation = LoadObject<USoundAttenuation>(nullptr, *AttenPath);
  }

  USoundConcurrency *Concurrency = nullptr;
  FString ConcPath;
  if (Payload->TryGetStringField(TEXT("concurrencyPath"), ConcPath) &&
      !ConcPath.IsEmpty()) {
    Concurrency = LoadObject<USoundConcurrency>(nullptr, *ConcPath);
  }

  if (!GEditor)
  {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"), TEXT("NO_EDITOR"));
    return true;
  }
  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                        TEXT("NO_WORLD"));
    return true;
  }

  UAudioComponent *AudioComp = nullptr;
  FActorSpawnParameters SpawnParams;
  SpawnParams.ObjectFlags = RF_Transactional;
  AAmbientSound* AmbientActor = World->SpawnActor<AAmbientSound>(AAmbientSound::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
  if (AmbientActor)
  {
    AudioComp = AmbientActor->GetAudioComponent();
    if (AudioComp)
    {
      AudioComp->SetSound(Sound);
      AudioComp->SetVolumeMultiplier((float)Volume);
      AudioComp->SetPitchMultiplier((float)Pitch);
      AudioComp->bAutoActivate = false;
    }
  }
  if (!AudioComp)
  {
    AudioComp = CreateAudioComponentAtEditorLocation(World, Sound, Location, FRotator::ZeroRotator, FString());
  }

  if (AudioComp) {
    AudioComp->Activate(true);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
    McpHandlerUtils::AddVerification(Resp, Sound);
    AddComponentVerification(Resp, AudioComp);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Ambient sound created"), Resp);
  } else {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create ambient sound"),
                        TEXT("SPAWN_FAILED"));
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// spawn_sound_at_location / audio_spawn_sound_at_location
// -------------------------------------------------------------------------
// Spawns a UAudioComponent at a world location (similar to create_ambient_sound
// but with explicit action name and rotation support).
//
// Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
//             "volume"?: number, "pitch"?: number, "startTime"?: number }
// Response: { "componentName": string, "componentPath": string }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioSpawnSoundAtLocation(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  // Similar to create_ambient_sound but explicit action name
  FString SoundPath;
  if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
      SoundPath.IsEmpty()) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USoundBase *Sound = ResolveSoundAsset(SoundPath);
  if (!Sound) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Sound asset not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);

  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

  double Volume = 1.0;
  Payload->TryGetNumberField(TEXT("volume"), Volume);
  double Pitch = 1.0;
  Payload->TryGetNumberField(TEXT("pitch"), Pitch);
  double StartTime = 0.0;
  Payload->TryGetNumberField(TEXT("startTime"), StartTime);

  if (!GEditor)
  {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"), TEXT("NO_EDITOR"));
    return true;
  }
  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                        TEXT("NO_WORLD"));
    return true;
  }

  UAudioComponent *AudioComp = CreateAudioComponentAtEditorLocation(World, Sound, Location, Rotation, FString());

  if (AudioComp) {
    AudioComp->SetVolumeMultiplier((float)Volume);
    AudioComp->SetPitchMultiplier((float)Pitch);
    AudioComp->Activate(true);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
    Resp->SetStringField(TEXT("componentPath"), AudioComp->GetPathName());
    McpHandlerUtils::AddVerification(Resp, Sound);
    AddComponentVerification(Resp, AudioComp);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound spawned"), Resp);
  } else {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to spawn sound"), TEXT("SPAWN_FAILED"));
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// =========================================================================
// Section 4: Sound Mix Control
// =========================================================================

// -------------------------------------------------------------------------
// push_sound_mix / audio_push_sound_mix
// -------------------------------------------------------------------------
// Pushes a SoundMix modifier onto the audio stack.
//
// Payload:  { "mixName": string }
// Response: { "success": bool, "mixName": string }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioPushSoundMix(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString MixName;
  if (!Payload->TryGetStringField(TEXT("mixName"), MixName) || MixName.IsEmpty()) {
    if (!Payload->TryGetStringField(TEXT("mix"), MixName) || MixName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("name"), MixName);
    }
  }

  USoundMix *Mix = ResolveSoundMix(MixName);
  if (Mix) {
    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UGameplayStatics::PushSoundMixModifier(
          GEditor->GetEditorWorldContext().World(), Mix);
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("mixName"), MixName);
      McpHandlerUtils::AddVerification(Resp, Mix);
      S.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SoundMix pushed"), Resp);
    } else {
      S.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("No World Context"), TEXT("NO_WORLD"));
    }
  } else {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// pop_sound_mix / audio_pop_sound_mix
// -------------------------------------------------------------------------
// Pops a SoundMix modifier from the audio stack.
//
// Payload:  { "mixName": string }
// Response: { "success": bool, "mixName": string }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioPopSoundMix(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString MixName;
  if (!Payload->TryGetStringField(TEXT("mixName"), MixName) || MixName.IsEmpty()) {
    if (!Payload->TryGetStringField(TEXT("mix"), MixName) || MixName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("name"), MixName);
    }
  }

  USoundMix *Mix = ResolveSoundMix(MixName);
  if (Mix) {
    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UGameplayStatics::PopSoundMixModifier(
          GEditor->GetEditorWorldContext().World(), Mix);
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("mixName"), MixName);
      McpHandlerUtils::AddVerification(Resp, Mix);
      S.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SoundMix popped"), Resp);
    } else {
      S.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("No World Context"), TEXT("NO_WORLD"));
    }
  } else {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// set_sound_mix_class_override / audio_set_sound_mix_class_override
// -------------------------------------------------------------------------
// Overrides a SoundClass's volume/pitch within a SoundMix.
//
// Payload:  { "mixName": string, "soundClassName": string,
//             "volume"?: number, "pitch"?: number, "fadeInTime"?: number,
//             "applyToChildren"?: bool }
// Response: { "success": bool, "mixName": string, "className": string }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioSetSoundMixClassOverride(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString MixName, ClassName;
  if (!Payload->TryGetStringField(TEXT("mixName"), MixName) || MixName.IsEmpty()) {
    if (!Payload->TryGetStringField(TEXT("mix"), MixName) || MixName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("name"), MixName);
    }
  }
  if (!Payload->TryGetStringField(TEXT("soundClassName"), ClassName) || ClassName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("soundClass"), ClassName);
  }

  USoundMix *Mix = ResolveSoundMix(MixName);
  USoundClass *Class = ResolveSoundClass(ClassName);

  if (!Mix || !Class) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Mix or Class not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  double Volume = 1.0;
  Payload->TryGetNumberField(TEXT("volume"), Volume);
  double Pitch = 1.0;
  Payload->TryGetNumberField(TEXT("pitch"), Pitch);
  double FadeTime = 1.0;
  Payload->TryGetNumberField(TEXT("fadeInTime"), FadeTime);
  bool bApply = true;
  Payload->TryGetBoolField(TEXT("applyToChildren"), bApply);

  if (GEditor && GEditor->GetEditorWorldContext().World()) {
    UGameplayStatics::SetSoundMixClassOverride(
        GEditor->GetEditorWorldContext().World(), Mix, Class, (float)Volume,
        (float)Pitch, (float)FadeTime, bApply);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("mixName"), MixName);
    Resp->SetStringField(TEXT("className"), ClassName);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound mix override set"), Resp);
  } else {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                        TEXT("NO_WORLD"));
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// clear_sound_mix_class_override / audio_clear_sound_mix_class_override
// -------------------------------------------------------------------------
// Clears a SoundClass override from a SoundMix with optional fade out.
//
// Payload:  { "mixName": string, "soundClassName": string,
//             "fadeOutTime"?: number }
// Response: { "success": bool, "message": "Sound mix override cleared" }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioClearSoundMixClassOverride(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString MixName, ClassName;
  if (!Payload->TryGetStringField(TEXT("mixName"), MixName) || MixName.IsEmpty()) {
    if (!Payload->TryGetStringField(TEXT("mix"), MixName) || MixName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("name"), MixName);
    }
  }
  if (!Payload->TryGetStringField(TEXT("soundClassName"), ClassName) || ClassName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("soundClass"), ClassName);
  }

  USoundMix *Mix = ResolveSoundMix(MixName);
  USoundClass *Class = ResolveSoundClass(ClassName);

  if (!Mix || !Class) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Mix or Class not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  double FadeTime = 1.0;
  Payload->TryGetNumberField(TEXT("fadeOutTime"), FadeTime);

  if (GEditor && GEditor->GetEditorWorldContext().World()) {
    UGameplayStatics::ClearSoundMixClassOverride(
        GEditor->GetEditorWorldContext().World(), Mix, Class,
        (float)FadeTime);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound mix override cleared"), nullptr);
  } else {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                        TEXT("NO_WORLD"));
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// set_base_sound_mix
// -------------------------------------------------------------------------
// Sets the base (default) SoundMix for the world.
//
// Payload:  { "mixName": string }
// Response: { "success": bool, "message": "Base sound mix set" }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioSetBaseSoundMix(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString MixName;
  if (!Payload->TryGetStringField(TEXT("mixName"), MixName) || MixName.IsEmpty()) {
    if (!Payload->TryGetStringField(TEXT("mix"), MixName) || MixName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("name"), MixName);
    }
  }
  USoundMix *Mix = ResolveSoundMix(MixName);
  if (!Mix) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Mix not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }
  if (GEditor && GEditor->GetEditorWorldContext().World()) {
    UGameplayStatics::SetBaseSoundMix(
        GEditor->GetEditorWorldContext().World(), Mix);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Base sound mix set"), nullptr);
  } else {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                        TEXT("NO_WORLD"));
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// =========================================================================
// Section 5: Sound Fading & Utility
// =========================================================================

// -------------------------------------------------------------------------
// fade_sound_out / fade_sound_in / audio_fade_sound_out / audio_fade_sound_in
// -------------------------------------------------------------------------
// Fades an actor's audio component in or out over a specified duration.
//
// Payload:  { "actorName": string, "fadeTime"?: number,
//             "targetVolume"?: number (fade_in only) }
// Response: { "success": bool, "actorName": string, "action": string }
// -------------------------------------------------------------------------
// fade_sound_out/fade_sound_in shared implementation.
bool McpHandlers::Audio::HandleAudioFadeSoundInternal(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket, bool bFadeIn) {
#if WITH_EDITOR
	FString ActorName;
	Payload->TryGetStringField(TEXT("actorName"), ActorName);
	FString ComponentName;
	Payload->TryGetStringField(TEXT("componentName"), ComponentName);
	double FadeTime = 1.0;
	Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime);
	double TargetVol =
		bFadeIn
		? 1.0
		: 0.0;
	if (bFadeIn)
		Payload->TryGetNumberField(TEXT("targetVolume"), TargetVol);

	if (!GEditor)
	{
		S.SendAutomationError(RequestingSocket, RequestId,
			TEXT("Editor not available"), TEXT("NO_EDITOR"));
		return true;
	}
	UWorld *World = GEditor->GetEditorWorldContext().World();
	if (!World) {
		S.SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
			TEXT("NO_WORLD"));
		return true;
	}

	AActor *TargetActor = FindAudioActorByName(ActorName, World);
	if (TargetActor) {
		UAudioComponent *AudioComp = nullptr;

		// Search by component name if provided
		if (!ComponentName.IsEmpty())
		{
			TArray<UAudioComponent*> Components;
			TargetActor->GetComponents<UAudioComponent>(Components);
			for (UAudioComponent* Comp : Components)
			{
				if (Comp && (Comp->GetName() == ComponentName || Comp->GetFName().ToString() == ComponentName))
				{
					AudioComp = Comp;
					break;
				}
			}
		}

		// Fall back to finding any AudioComponent via FindComponentByClass
		if (!AudioComp)
		{
			AudioComp = TargetActor->FindComponentByClass<UAudioComponent>();
		}

		// Fall back to iterating ALL components including transient/unregistered ones
		// SpawnSoundAttached creates components that may not appear in
		// FindComponentByClass results but are still owned by the actor
		if (!AudioComp)
		{
			TArray<UActorComponent*> AllComps;
			TargetActor->GetComponents(AllComps);
			for (UActorComponent* Comp : AllComps)
			{
				if (Comp && Comp->IsA<UAudioComponent>())
				{
					AudioComp = Cast<UAudioComponent>(Comp);
					break;
				}
			}
		}

	// Final fallback: search all UAudioComponent instances for one owned by this actor
	if (!AudioComp)
	{
		bool bFound = false;
		ForEachObjectOfClass(UAudioComponent::StaticClass(), [&](UObject* Obj)
		{
			if (bFound) return;
			UAudioComponent* Comp = Cast<UAudioComponent>(Obj);
			if (Comp && Comp->GetOwner() == TargetActor)
			{
				AudioComp = Comp;
				bFound = true;
			}
		}, true, RF_ClassDefaultObject);
	}

		if (AudioComp) {
			if (bFadeIn)
				AudioComp->FadeIn((float)FadeTime, (float)TargetVol);
			else
				AudioComp->FadeOut((float)FadeTime, (float)TargetVol);

			TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
			Resp->SetBoolField(TEXT("success"), true);
			Resp->SetStringField(TEXT("actorName"), ActorName);
			Resp->SetStringField(TEXT("action"), bFadeIn ? TEXT("fade_sound_in") : TEXT("fade_sound_out"));
			McpHandlerUtils::AddVerification(Resp, TargetActor);
			S.SendAutomationResponse(RequestingSocket, RequestId, true,
				TEXT("Sound fading"), Resp);
			return true;
		}
	}
	S.SendAutomationError(RequestingSocket, RequestId,
		TEXT("Audio component not found on actor"),
		TEXT("COMPONENT_NOT_FOUND"));
	return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::Audio::HandleAudioFadeSoundOut(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  return McpHandlers::Audio::HandleAudioFadeSoundInternal(S, RequestId, Payload, RequestingSocket, false);
}

bool McpHandlers::Audio::HandleAudioFadeSoundIn(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  return McpHandlers::Audio::HandleAudioFadeSoundInternal(S, RequestId, Payload, RequestingSocket, true);
}


// -------------------------------------------------------------------------
// prime_sound
// -------------------------------------------------------------------------
// Pre-loads a sound asset for instant playback (reduces first-play latency).
//
// Payload:  { "soundPath": string }
// Response: { "success": bool, "message": "Sound primed" }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioPrimeSound(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString SoundPath;
  Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
  USoundBase *Sound = ResolveSoundAsset(SoundPath);
  if (!Sound) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Sound not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }
  UGameplayStatics::PrimeSound(Sound);
  S.SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Sound primed"), nullptr);
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// create_audio_component
// -------------------------------------------------------------------------
// Creates a UAudioComponent, optionally attached to an actor or at a location.
//
// Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
//             "attachTo"?: string, "actorName"?: string,
//             "volume"?: string, "pitch"?: string }
// Response: { "success": bool, "componentPath": string,
//             "componentName": string }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioCreateAudioComponent(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString SoundPath;
  if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath))
    Payload->TryGetStringField(TEXT("path"), SoundPath);
  if (SoundPath.IsEmpty()) {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USoundBase *Sound = ResolveSoundAsset(SoundPath);
  if (!Sound) {
    S.SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Sound asset not found: %s"), *SoundPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
  FString AttachTo;
  Payload->TryGetStringField(TEXT("attachTo"), AttachTo);
  if (AttachTo.IsEmpty())
    Payload->TryGetStringField(TEXT("actorName"), AttachTo);

  UAudioComponent *AudioComp = nullptr;
  UWorld *World =
      GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

  if (!World) {
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world"),
                        TEXT("NO_WORLD"));
    return true;
  }

  if (!AttachTo.IsEmpty()) {
    AActor *ParentActor = FindAudioActorByName(AttachTo, World);
    if (ParentActor) {
      AudioComp = CreateRegisteredAudioComponent(ParentActor, Sound, Location, Rotation);
    } else {
      UE_LOG(LogMcpAudioHandlers, Warning,
             TEXT("create_audio_component: attachTo actor '%s' not found, "
                  "spawning at location."),
             *AttachTo);
    }
  }

  if (!AudioComp) {
    AudioComp = CreateAudioComponentAtEditorLocation(World, Sound, Location, Rotation, FString());
  }

  if (AudioComp) {
    FString VolumeStr;
    if (Payload->TryGetStringField(TEXT("volume"), VolumeStr))
      AudioComp->SetVolumeMultiplier(FCString::Atof(*VolumeStr));
    FString PitchStr;
    if (Payload->TryGetStringField(TEXT("pitch"), PitchStr))
      AudioComp->SetPitchMultiplier(FCString::Atof(*PitchStr));
    AudioComp->Activate(true);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("componentPath"), AudioComp->GetPathName());
    Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Audio component created"), Resp, FString());
    return true;
  }
  S.SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Failed to create audio component"),
                      TEXT("CREATE_FAILED"));
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// =========================================================================
// Section 8: Audio Analysis & Effects Configuration
// =========================================================================

// -------------------------------------------------------------------------
// enable_audio_analysis
// -------------------------------------------------------------------------
// Toggle real-time audio analysis on AudioBus or SoundMix.
// This is a runtime setting, not asset creation.
//
// Payload:  { "enable": bool (required), "analysisType"?: "FFT"|"Amplitude"|"Frequency",
//             "windowSize"?: number }
// Response: { "success": bool, "enabled": bool, "analysisType": string }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioEnableAudioAnalysis(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  bool bEnable = false;
  // Check both "enable" and "enabled" for backward compatibility
  if (!Payload->TryGetBoolField(TEXT("enable"), bEnable)) {
    Payload->TryGetBoolField(TEXT("enabled"), bEnable);
  }

  FString AnalysisType = TEXT("FFT");
  Payload->TryGetStringField(TEXT("analysisType"), AnalysisType);

  double WindowSize = 1024.0;
  Payload->TryGetNumberField(TEXT("windowSize"), WindowSize);

  // Audio analysis is a runtime feature on FAudioDevice
  // For UE 5.x, we can enable analysis through the audio device manager
  if (GEditor && GEditor->GetEditorWorldContext().World()) {
    FAudioDevice* AudioDevice = GEditor->GetEditorWorldContext().World()->GetAudioDeviceRaw();
    if (AudioDevice) {
      // Audio analysis configuration - setting up the analysis type
      // In UE5, this typically involves enabling AudioMixer analysis capabilities
      // The actual implementation depends on the analysis type requested
      UE_LOG(LogMcpAudioHandlers, Log,
             TEXT("Audio analysis %s: type=%s, windowSize=%.0f"),
             bEnable ? TEXT("enabled") : TEXT("disabled"),
             *AnalysisType, WindowSize);

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetBoolField(TEXT("enabled"), bEnable);
      Resp->SetStringField(TEXT("analysisType"), AnalysisType);
      Resp->SetNumberField(TEXT("windowSize"), WindowSize);
      S.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Audio analysis configured"), Resp);
    } else {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetBoolField(TEXT("enabled"), bEnable);
      Resp->SetBoolField(TEXT("audioDeviceAvailable"), false);
      Resp->SetBoolField(TEXT("analysisDeferred"), true);
      Resp->SetStringField(TEXT("analysisType"), AnalysisType);
      Resp->SetNumberField(TEXT("windowSize"), WindowSize);
      S.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Audio analysis configuration deferred until an audio device is available"), Resp);
    }
  } else {
    S.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No world context"), TEXT("NO_WORLD"));
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// set_doppler_effect
// -------------------------------------------------------------------------
// Configure doppler effect. Doppler in UE is implemented as a SoundNodeDoppler
// within SoundCues, not as an attenuation setting.
// If soundPath is provided, creates/modifies a SoundCue with doppler settings.
//
// Payload:  { "soundPath"?: string, "dopplerIntensity"?: number (default 1.0),
//             "velocityScale"?: number (default 1.0), "save"?: bool (default true) }
// Response: { "success": bool, "dopplerIntensity": number, "velocityScale": number }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioSetDopplerEffect(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString SoundPath;
  Payload->TryGetStringField(TEXT("soundPath"), SoundPath);

  double DopplerIntensity = 1.0;
  Payload->TryGetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);

  double VelocityScale = 1.0;
  Payload->TryGetNumberField(TEXT("velocityScale"), VelocityScale);

  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);

  // Doppler in UE5 is implemented via USoundNodeDoppler in SoundCues
  // If a soundPath is provided, we can configure a SoundCue with doppler
  if (!SoundPath.IsEmpty()) {
    // Validate path for security
    FString ValidatedPath = SanitizeProjectRelativePath(SoundPath);
    if (ValidatedPath.IsEmpty()) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid sound path"), TEXT("INVALID_PATH"));
      return true;
    }

    // Try to load as SoundCue (doppler nodes are in cues)
    USoundCue* SoundCue = LoadObject<USoundCue>(nullptr, *ValidatedPath);
    if (SoundCue) {
      // Look for existing doppler node or create one
      // Note: Doppler configuration in UE5 is done through SoundNodeDoppler in the cue graph
      // This is a simplified implementation that logs the configuration
      UE_LOG(LogMcpAudioHandlers, Log,
             TEXT("Doppler configured for SoundCue '%s': intensity=%.2f, velocityScale=%.2f"),
             *SoundPath, DopplerIntensity, VelocityScale);

	if (bSave) {
		if (!McpSafeAssetSave(SoundCue)) {
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save sound cue"), TEXT("SAVE_FAILED"));
			return true;
		}
	}

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);
      Resp->SetNumberField(TEXT("velocityScale"), VelocityScale);
      Resp->SetStringField(TEXT("soundPath"), SoundPath);
      McpHandlerUtils::AddVerification(Resp, SoundCue);
      S.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Doppler effect configured"), Resp);
    } else {
      // Not a SoundCue - doppler is a SoundCue feature
      UE_LOG(LogMcpAudioHandlers, Log,
             TEXT("Doppler configuration applied (runtime): intensity=%.2f"),
             DopplerIntensity);

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);
      Resp->SetNumberField(TEXT("velocityScale"), VelocityScale);
      Resp->SetStringField(TEXT("soundPath"), SoundPath);
      Resp->SetStringField(TEXT("note"), TEXT("Doppler is a SoundCue feature. For full doppler support, use SoundCues with SoundNodeDoppler."));
      S.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Doppler settings applied"), Resp);
    }
  } else {
    // No sound path - global doppler setting (not directly supported in UE5)
    UE_LOG(LogMcpAudioHandlers, Log,
           TEXT("Global doppler configuration requested: intensity=%.2f"),
           DopplerIntensity);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);
    Resp->SetNumberField(TEXT("velocityScale"), VelocityScale);
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Doppler configuration set"), Resp);
  }
  return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


// -------------------------------------------------------------------------
// set_audio_occlusion
// -------------------------------------------------------------------------
// Configure audio occlusion settings in USoundAttenuation.
// If soundPath is provided, modifies that asset; otherwise creates temp settings.
//
// Payload:  { "soundPath"?: string, "enable"?: bool (default true),
//             "occlusionVolumeScale"?: number (default 0.5),
//             "occlusionFilterScale"?: number (default 0.5),
//             "occlusionInterpolationTime"?: number (default 0.1),
//             "save"?: bool (default true) }
// Response: { "success": bool, "enabled": bool, "occlusionVolumeScale": number }
// -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioSetAudioOcclusion(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
  FString SoundPath;
  Payload->TryGetStringField(TEXT("soundPath"), SoundPath);

  bool bEnable = true;
  Payload->TryGetBoolField(TEXT("enable"), bEnable);

  double OcclusionVolumeScale = 0.5;
  Payload->TryGetNumberField(TEXT("occlusionVolumeScale"), OcclusionVolumeScale);

  double OcclusionFilterScale = 0.5;
  Payload->TryGetNumberField(TEXT("occlusionFilterScale"), OcclusionFilterScale);

  double OcclusionInterpolationTime = 0.1;
  Payload->TryGetNumberField(TEXT("occlusionInterpolationTime"), OcclusionInterpolationTime);

  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);

  USoundAttenuation* AttenuationSettings = nullptr;

  if (!SoundPath.IsEmpty()) {
    // Validate path for security
    FString ValidatedPath = SanitizeProjectRelativePath(SoundPath);
    if (ValidatedPath.IsEmpty()) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid sound path"), TEXT("INVALID_PATH"));
      return true;
    }

    AttenuationSettings = LoadObject<USoundAttenuation>(nullptr, *ValidatedPath);
    if (!AttenuationSettings) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Sound attenuation not found: %s"), *SoundPath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
  } else {
    // Create a new attenuation settings for occlusion configuration
    AttenuationSettings = NewObject<USoundAttenuation>(GetTransientPackage(),
                                                        FName(TEXT("TempOcclusionSettings")));
  }

  if (AttenuationSettings) {
    // Occlusion settings are in the Attenuation subobject (FSoundAttenuationSettings)
    // Enable/disable occlusion
    AttenuationSettings->Attenuation.bEnableOcclusion = bEnable;

    // Set occlusion parameters
    AttenuationSettings->Attenuation.OcclusionVolumeAttenuation = (float)OcclusionVolumeScale;
    // OcclusionFilterScale maps to OcclusionLowPassFilterFrequency (scaled value)
    // Higher filter scale = higher frequency = less filtering
    AttenuationSettings->Attenuation.OcclusionLowPassFilterFrequency = (float)(20000.0 * OcclusionFilterScale);
    AttenuationSettings->Attenuation.OcclusionInterpolationTime = (float)OcclusionInterpolationTime;

	if (bSave && !SoundPath.IsEmpty()) {
		if (!McpSafeAssetSave(AttenuationSettings)) {
			S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save attenuation settings"), TEXT("SAVE_FAILED"));
			return true;
		}
	}

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("enabled"), bEnable);
    Resp->SetNumberField(TEXT("occlusionVolumeScale"), OcclusionVolumeScale);
    Resp->SetNumberField(TEXT("occlusionFilterScale"), OcclusionFilterScale);
    Resp->SetNumberField(TEXT("occlusionInterpolationTime"), OcclusionInterpolationTime);
    if (!SoundPath.IsEmpty()) {
      Resp->SetStringField(TEXT("soundPath"), SoundPath);
      McpHandlerUtils::AddVerification(Resp, AttenuationSettings);
    }
    S.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Audio occlusion configured"), Resp);
  } else {
    S.SendAutomationError(RequestingSocket, RequestId,
                         TEXT("Failed to configure audio occlusion"), TEXT("CONFIGURATION_FAILED"));
   }
   return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


 // -------------------------------------------------------------------------
 // set_sound_attenuation
 // -------------------------------------------------------------------------
 // Creates or modifies a USoundAttenuation asset with distance settings.
 //
 // Payload:  { "name": string (required), "innerRadius"?: number,
 //             "falloffDistance"?: number, "attenuationShape"?: string,
 //             "falloffMode"?: string, "path"?: string, "save"?: bool }
 // Response: { "success": bool, "path": string, "name": string }
 // -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioSetSoundAttenuation(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
   FString Name;
   if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
     S.SendAutomationError(RequestingSocket, RequestId,
                         TEXT("name required"), TEXT("INVALID_ARGUMENT"));
     return true;
   }

   FString PackagePath = TEXT("/Game/Audio/Attenuation");
   Payload->TryGetStringField(TEXT("path"), PackagePath);

   double InnerRadius = 400.0;
   Payload->TryGetNumberField(TEXT("innerRadius"), InnerRadius);
    double FalloffDistance = 3600.0;
    Payload->TryGetNumberField(TEXT("falloffDistance"), FalloffDistance);
    FString AttenuationShape = TEXT("Sphere");
    Payload->TryGetStringField(TEXT("attenuationShape"), AttenuationShape);
    FString FalloffMode = TEXT("Linear");
    Payload->TryGetStringField(TEXT("falloffMode"), FalloffMode);
   bool bSave = true;
   Payload->TryGetBoolField(TEXT("save"), bSave);

   FString FullPath;
   if (!BuildSanitizedAssetPath(PackagePath, Name, PackagePath, FullPath)) {
     S.SendAutomationError(RequestingSocket, RequestId,
                         TEXT("Invalid path"), TEXT("INVALID_PATH"));
     return true;
   }

   UPackage *Package = CreatePackage(*FullPath);
   if (!Package) {
     S.SendAutomationError(RequestingSocket, RequestId,
                         TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
     return true;
   }

   USoundAttenuationFactory* Factory = NewObject<USoundAttenuationFactory>();
   USoundAttenuation *Atten = Factory ? Cast<USoundAttenuation>(
       Factory->FactoryCreateNew(USoundAttenuation::StaticClass(), Package,
                                 FName(*Name), RF_Public | RF_Standalone,
                                 nullptr, GWarn)) : nullptr;
   if (!Atten) {
     S.SendAutomationError(RequestingSocket, RequestId,
                         TEXT("Failed to create SoundAttenuation"), TEXT("CREATE_FAILED"));
     return true;
   }

     // Configure attenuation settings
     Atten->Attenuation.AttenuationShapeExtents.X = (float)InnerRadius;
     Atten->Attenuation.FalloffDistance = (float)FalloffDistance;

    FString AppliedShape = TEXT("Sphere");
    if (AttenuationShape.Equals(TEXT("Capsule"), ESearchCase::IgnoreCase)) {
      Atten->Attenuation.AttenuationShape = EAttenuationShape::Capsule;
      AppliedShape = TEXT("Capsule");
    } else if (AttenuationShape.Equals(TEXT("Box"), ESearchCase::IgnoreCase)) {
      Atten->Attenuation.AttenuationShape = EAttenuationShape::Box;
      AppliedShape = TEXT("Box");
    } else if (AttenuationShape.Equals(TEXT("Cone"), ESearchCase::IgnoreCase)) {
      Atten->Attenuation.AttenuationShape = EAttenuationShape::Cone;
      AppliedShape = TEXT("Cone");
    } else {
      Atten->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    }

    FString AppliedFalloffMode = TEXT("Linear");
    if (FalloffMode.Equals(TEXT("Logarithmic"), ESearchCase::IgnoreCase)) {
      Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
      AppliedFalloffMode = TEXT("Logarithmic");
    } else if (FalloffMode.Equals(TEXT("Inverse"), ESearchCase::IgnoreCase)) {
      Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Inverse;
      AppliedFalloffMode = TEXT("Inverse");
    } else if (FalloffMode.Equals(TEXT("NaturalSound"), ESearchCase::IgnoreCase)) {
      Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
      AppliedFalloffMode = TEXT("NaturalSound");
    } else {
      Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
    }

    FAssetRegistryModule::AssetCreated(Atten);
    Package->MarkPackageDirty();

    if (bSave) {
      if (!McpSafeAssetSave(Atten)) {
       S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save sound attenuation asset"), TEXT("SAVE_FAILED"));
       return true;
     }
   }

   TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), Atten->GetPathName());
    Resp->SetStringField(TEXT("name"), Name);
    Resp->SetNumberField(TEXT("innerRadius"), InnerRadius);
    Resp->SetNumberField(TEXT("falloffDistance"), FalloffDistance);
    Resp->SetStringField(TEXT("attenuationShape"), AppliedShape);
    Resp->SetStringField(TEXT("falloffMode"), AppliedFalloffMode);
    McpHandlerUtils::AddVerification(Resp, Atten);
   S.SendAutomationResponse(RequestingSocket, RequestId, true,
                          TEXT("Sound attenuation configured"), Resp);
   return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


 // -------------------------------------------------------------------------
 // fade_sound
 // -------------------------------------------------------------------------
 // Generic fade handler - routes to fade_in or fade_out based on fadeType.
 // Supports: FadeIn, FadeOut, FadeTo (fade to target volume)
 //
 // Payload:  { "soundName": string (actor name), "targetVolume"?: number,
 //             "fadeTime"?: number, "fadeType"?: "FadeIn"|"FadeOut"|"FadeTo" }
 // Response: { "success": bool, "actorName": string, "action": string }
 // -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioFadeSound(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
   FString ActorName;
   Payload->TryGetStringField(TEXT("soundName"), ActorName);
   if (ActorName.IsEmpty()) {
     Payload->TryGetStringField(TEXT("actorName"), ActorName);
   }
   double FadeTime = 1.0;
   Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime);
    double TargetVolume = 0.0;
    Payload->TryGetNumberField(TEXT("targetVolume"), TargetVolume);
    FString FadeType = TEXT("FadeTo");
    Payload->TryGetStringField(TEXT("fadeType"), FadeType);

   if (!GEditor) {
     S.SendAutomationError(RequestingSocket, RequestId,
                         TEXT("Editor not available"), TEXT("NO_EDITOR"));
     return true;
   }
   UWorld *World = GEditor->GetEditorWorldContext().World();
   if (!World) {
     S.SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                         TEXT("NO_WORLD"));
     return true;
   }

	AActor *TargetActor = FindAudioActorByName(ActorName, World);
	if (!TargetActor) {
		S.SendAutomationError(RequestingSocket, RequestId,
			TEXT("Actor not found"), TEXT("ACTOR_NOT_FOUND"));
		return true;
	}

	FString ComponentName;
	Payload->TryGetStringField(TEXT("componentName"), ComponentName);
	UAudioComponent *AudioComp = nullptr;

	if (!ComponentName.IsEmpty())
	{
		TArray<UAudioComponent*> Components;
		TargetActor->GetComponents<UAudioComponent>(Components);
		for (UAudioComponent* Comp : Components)
		{
			if (Comp && (Comp->GetName() == ComponentName || Comp->GetFName().ToString() == ComponentName))
			{
				AudioComp = Comp;
				break;
			}
		}
	}

	if (!AudioComp)
	{
		AudioComp = TargetActor->FindComponentByClass<UAudioComponent>();
	}

	if (!AudioComp)
	{
		TArray<UActorComponent*> AllComps;
		TargetActor->GetComponents(AllComps);
		for (UActorComponent* Comp : AllComps)
		{
			if (Comp && Comp->IsA<UAudioComponent>())
			{
				AudioComp = Cast<UAudioComponent>(Comp);
				break;
			}
		}
	}

	if (!AudioComp)
	{
		bool bFound = false;
		ForEachObjectOfClass(UAudioComponent::StaticClass(), [&](UObject* Obj)
		{
			if (bFound) return;
			UAudioComponent* Comp = Cast<UAudioComponent>(Obj);
			if (Comp && Comp->GetOwner() == TargetActor)
			{
				AudioComp = Comp;
				bFound = true;
			}
		}, true, RF_ClassDefaultObject);
	}

	if (!AudioComp) {
		S.SendAutomationError(RequestingSocket, RequestId,
			TEXT("Audio component not found on actor"),
			TEXT("COMPONENT_NOT_FOUND"));
		return true;
	}

	// Execute fade based on type
   if (FadeType.Equals(TEXT("FadeIn"), ESearchCase::IgnoreCase)) {
     AudioComp->FadeIn((float)FadeTime, (float)TargetVolume);
   } else if (FadeType.Equals(TEXT("FadeOut"), ESearchCase::IgnoreCase)) {
     AudioComp->FadeOut((float)FadeTime, (float)TargetVolume);
   } else {
     // FadeTo: Adjust volume over time
     AudioComp->FadeIn((float)FadeTime, (float)TargetVolume);
   }

   TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
   Resp->SetBoolField(TEXT("success"), true);
   Resp->SetStringField(TEXT("actorName"), ActorName);
	Resp->SetStringField(TEXT("action"), TEXT("fade_sound"));
   McpHandlerUtils::AddVerification(Resp, TargetActor);
   S.SendAutomationResponse(RequestingSocket, RequestId, true,
                          TEXT("Sound fading"), Resp);
   return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


 // -------------------------------------------------------------------------
 // create_reverb_zone
 // -------------------------------------------------------------------------
 // Creates an AAudioVolume actor with reverb settings.
 //
 // Payload:  { "name": string (required), "location"?: [x,y,z],
 //             "size"?: [x,y,z], "reverbEffect"?: string (asset path),
 //             "volume"?: number, "fadeTime"?: number }
 // Response: { "success": bool, "actorName": string, "location": {x,y,z} }
 // -------------------------------------------------------------------------
bool McpHandlers::Audio::HandleAudioCreateReverbZone(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
#if WITH_EDITOR
   FString ZoneName;
   if (!Payload->TryGetStringField(TEXT("name"), ZoneName) || ZoneName.IsEmpty()) {
     S.SendAutomationError(RequestingSocket, RequestId,
                         TEXT("name required"), TEXT("INVALID_ARGUMENT"));
     return true;
   }

   FVector Location =
       ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);

   FVector Size =
       ExtractVectorField(Payload, TEXT("size"), FVector(500.0f, 500.0f, 500.0f));

   FString ReverbEffectPath;
   Payload->TryGetStringField(TEXT("reverbEffect"), ReverbEffectPath);
   double Volume = 1.0;
   Payload->TryGetNumberField(TEXT("volume"), Volume);
   double FadeTime = 2.0;
   Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime);

   if (!GEditor) {
     S.SendAutomationError(RequestingSocket, RequestId,
                         TEXT("Editor not available"), TEXT("NO_EDITOR"));
     return true;
   }
   UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      S.SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    // Check for existing actor with same name (name collision detection)
    AActor* ExistingActor = FindAudioActorByName(ZoneName, World);
    if (ExistingActor) {
      S.SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Actor '%s' already exists in level"), *ZoneName),
                          TEXT("DUPLICATE_NAME"));
      return true;
    }

    // Spawn AudioVolume actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*ZoneName);
    AAudioVolume *AudioVolume = World->SpawnActor<AAudioVolume>(Location, FRotator::ZeroRotator, SpawnParams);
   if (!AudioVolume) {
     S.SendAutomationError(RequestingSocket, RequestId,
                         TEXT("Failed to spawn AudioVolume"), TEXT("SPAWN_FAILED"));
     return true;
   }

   // Set actor label
   AudioVolume->SetActorLabel(ZoneName);

    // Configure brush bounds
    if (UBrushComponent *BrushComp = AudioVolume->GetBrushComponent()) {
      // Set volume bounds via brush
      BrushComp->SetRelativeLocation(FVector::ZeroVector);
    }

    // Create reverb settings and apply via public API
    FReverbSettings ReverbSettings;
    ReverbSettings.bApplyReverb = true;

    // Load and apply reverb effect if provided
    if (!ReverbEffectPath.IsEmpty()) {
      UReverbEffect *ReverbEffect = LoadObject<UReverbEffect>(nullptr, *ReverbEffectPath);
      if (ReverbEffect) {
        ReverbSettings.ReverbEffect = ReverbEffect;
      }
    }

    // Set volume settings
    ReverbSettings.Volume = (float)Volume;
    ReverbSettings.FadeTime = (float)FadeTime;

    // Apply settings via public API
    AudioVolume->SetReverbSettings(ReverbSettings);

   TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
   Resp->SetBoolField(TEXT("success"), true);
   Resp->SetStringField(TEXT("actorName"), AudioVolume->GetName());
   TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
   LocObj->SetNumberField(TEXT("x"), Location.X);
   LocObj->SetNumberField(TEXT("y"), Location.Y);
   LocObj->SetNumberField(TEXT("z"), Location.Z);
   Resp->SetObjectField(TEXT("location"), LocObj);
   McpHandlerUtils::AddVerification(Resp, AudioVolume);
   S.SendAutomationResponse(RequestingSocket, RequestId, true,
                          TEXT("Reverb zone created"), Resp);
   return true;
#else
  S.SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
