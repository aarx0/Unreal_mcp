// LINT-TOOL: manage_audio
// manage_audio as FMcpCall classes — nineteenth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers — HandleAudio* (AudioHandlers.cpp) for the 23 core
// actions, HandleAudioAuthoring* (AudioAuthoringHandlers.cpp) for the 27
// authoring actions — until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageAudio
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations (McpDecl_ManageAudio.h)
// and re-verified against the member bodies. 14 rows shipped with decl fixes
// at classing. The seven AudioAuthoring-owned actions the retired core
// dispatcher shadowed (create_dialogue_voice, create_dialogue_wave,
// set_dialogue_context, create_reverb_effect, create_source_effect_chain,
// add_source_effect, create_submix_effect) had rows unioned from the DEAD
// copies' reads — re-derived from the live HandleAudioAuthoringRequest
// blocks: dropped voiceName/outputPath/pluralization (create_dialogue_voice),
// waveName/soundPath/outputPath (create_dialogue_wave),
// wavePath/voicePath/contextIndex (set_dialogue_context),
// effectName/outputPath/reflectionsGain/lateGain (create_reverb_effect),
// chainName/outputPath (create_source_effect_chain), chainPath/effectName
// (add_source_effect — its effectType flipped optional: the live block only
// consults it when effectPresetPath is absent), and effectName/outputPath
// (create_submix_effect). play_sound_2d and play_sound_at_location dropped
// the functionName/params/path spellings no body reads (bootstrap residue).
// Required flags flipped optional where the body resolves a
// first-present-wins alias chain and rejects only at resolution: the
// mixName→mix→name (+soundClassName→soundClass) chains on
// set_sound_mix_class_override, clear_sound_mix_class_override, and
// set_base_sound_mix; fade_sound's soundName→actorName pair;
// create_audio_component's soundPath→path pair (joint reject only when both
// are absent — and its String-typed volume/pitch are handler-true: the body
// reads them with TryGetStringField + Atof). fade_sound_out keeps its
// declared-optional targetVolume although only the fade_in path reads it, so
// payloads the family accepts today keep validating.
inline const FMcpParamDecl P_CreateSoundCue[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("packagePath"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("wavePath"), EMcpParamKind::String, false }, { TEXT("looping"), EMcpParamKind::Bool, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("attenuationPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateSoundClass[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("packagePath"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("parentClass"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateSoundMix[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("packagePath"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("classAdjusters"), EMcpParamKind::Array, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_PlaySoundAtLocation[] = { { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("rotation"), EMcpParamKind::Array, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("startTime"), EMcpParamKind::Number, false }, { TEXT("attenuationPath"), EMcpParamKind::String, false }, { TEXT("concurrencyPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_PlaySound2D[] = { { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("startTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_PlaySoundAttached[] = { { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("attachPointName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateAmbientSound[] = { { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("startTime"), EMcpParamKind::Number, false }, { TEXT("attenuationPath"), EMcpParamKind::String, false }, { TEXT("concurrencyPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SpawnSoundAtLocation[] = { { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("rotation"), EMcpParamKind::Array, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("startTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_PushSoundMix[] = { { TEXT("mixName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_PopSoundMix[] = { { TEXT("mixName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetSoundMixClassOverride[] = { { TEXT("mixName"), EMcpParamKind::String, false }, { TEXT("mix"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("soundClassName"), EMcpParamKind::String, false }, { TEXT("soundClass"), EMcpParamKind::String, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("fadeInTime"), EMcpParamKind::Number, false }, { TEXT("applyToChildren"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ClearSoundMixClassOverride[] = { { TEXT("mixName"), EMcpParamKind::String, false }, { TEXT("mix"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("soundClassName"), EMcpParamKind::String, false }, { TEXT("soundClass"), EMcpParamKind::String, false }, { TEXT("fadeOutTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetBaseSoundMix[] = { { TEXT("mixName"), EMcpParamKind::String, false }, { TEXT("mix"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_FadeSoundOut[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("fadeTime"), EMcpParamKind::Number, false }, { TEXT("targetVolume"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_FadeSoundIn[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("fadeTime"), EMcpParamKind::Number, false }, { TEXT("targetVolume"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_PrimeSound[] = { { TEXT("soundPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateAudioComponent[] = { { TEXT("soundPath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("rotation"), EMcpParamKind::Array, false }, { TEXT("attachTo"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("volume"), EMcpParamKind::String, false }, { TEXT("pitch"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_EnableAudioAnalysis[] = { { TEXT("enable"), EMcpParamKind::Bool, false }, { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("analysisType"), EMcpParamKind::String, false }, { TEXT("windowSize"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetDopplerEffect[] = { { TEXT("soundPath"), EMcpParamKind::String, false }, { TEXT("dopplerIntensity"), EMcpParamKind::Number, false }, { TEXT("velocityScale"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetAudioOcclusion[] = { { TEXT("soundPath"), EMcpParamKind::String, false }, { TEXT("enable"), EMcpParamKind::Bool, false }, { TEXT("occlusionVolumeScale"), EMcpParamKind::Number, false }, { TEXT("occlusionFilterScale"), EMcpParamKind::Number, false }, { TEXT("occlusionInterpolationTime"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetSoundAttenuation[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("falloffDistance"), EMcpParamKind::Number, false }, { TEXT("attenuationShape"), EMcpParamKind::String, false }, { TEXT("falloffMode"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_FadeSound[] = { { TEXT("soundName"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("fadeTime"), EMcpParamKind::Number, false }, { TEXT("targetVolume"), EMcpParamKind::Number, false }, { TEXT("fadeType"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateReverbZone[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("size"), EMcpParamKind::Array, false }, { TEXT("reverbEffect"), EMcpParamKind::String, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("fadeTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddCueNode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("nodeType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("wavePath"), EMcpParamKind::String, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("indefinite"), EMcpParamKind::Bool, false }, { TEXT("loopCount"), EMcpParamKind::Number, false }, { TEXT("attenuationPath"), EMcpParamKind::String, false }, { TEXT("delay"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConnectCueNodes[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("sourceNodeId"), EMcpParamKind::String, true }, { TEXT("targetNodeId"), EMcpParamKind::String, true }, { TEXT("childIndex"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetCueAttenuation[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("attenuationPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetCueConcurrency[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("concurrencyPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateMetasound[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddMetasoundNode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("nodeClassName"), EMcpParamKind::String, false }, { TEXT("nodeType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConnectMetasoundNodes[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("sourceNodeId"), EMcpParamKind::String, true }, { TEXT("sourceOutputName"), EMcpParamKind::String, false }, { TEXT("targetNodeId"), EMcpParamKind::String, true }, { TEXT("targetInputName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddMetasoundInput[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("inputName"), EMcpParamKind::String, true }, { TEXT("inputType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddMetasoundOutput[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("outputName"), EMcpParamKind::String, true }, { TEXT("outputType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetMetasoundDefault[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("inputName"), EMcpParamKind::String, true }, { TEXT("floatValue"), EMcpParamKind::Number, false }, { TEXT("intValue"), EMcpParamKind::Number, false }, { TEXT("boolValue"), EMcpParamKind::Bool, false }, { TEXT("stringValue"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetClassProperties[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("lowPassFilterFrequency"), EMcpParamKind::Number, false }, { TEXT("lfeBleed"), EMcpParamKind::Number, false }, { TEXT("voiceCenterChannelVolume"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetClassParent[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("parentPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddMixModifier[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("soundClassPath"), EMcpParamKind::String, true }, { TEXT("volumeAdjuster"), EMcpParamKind::Number, false }, { TEXT("pitchAdjuster"), EMcpParamKind::Number, false }, { TEXT("fadeInTime"), EMcpParamKind::Number, false }, { TEXT("fadeOutTime"), EMcpParamKind::Number, false }, { TEXT("applyToChildren"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureMixEq[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("applyEQ"), EMcpParamKind::Bool, false }, { TEXT("eqPriority"), EMcpParamKind::Number, false }, { TEXT("eqSettings"), EMcpParamKind::Object, false }, { TEXT("lowFrequency"), EMcpParamKind::Number, false }, { TEXT("lowGain"), EMcpParamKind::Number, false }, { TEXT("midFrequency"), EMcpParamKind::Number, false }, { TEXT("midGain"), EMcpParamKind::Number, false }, { TEXT("highMidFrequency"), EMcpParamKind::Number, false }, { TEXT("highMidGain"), EMcpParamKind::Number, false }, { TEXT("highFrequency"), EMcpParamKind::Number, false }, { TEXT("highGain"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateAttenuationSettings[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("falloffDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureDistanceAttenuation[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("falloffDistance"), EMcpParamKind::Number, false }, { TEXT("distanceAlgorithm"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureSpatialization[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("spatializationAlgorithm"), EMcpParamKind::String, false }, { TEXT("spatialization"), EMcpParamKind::String, false }, { TEXT("spatialize"), EMcpParamKind::Bool, false }, { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ConfigureOcclusion[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("enableOcclusion"), EMcpParamKind::Bool, false }, { TEXT("occlusionLowPassFilterFrequency"), EMcpParamKind::Number, false }, { TEXT("occlusionVolumeAttenuation"), EMcpParamKind::Number, false }, { TEXT("occlusionInterpolationTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureReverbSend[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("enableReverbSend"), EMcpParamKind::Bool, false }, { TEXT("reverbWetLevelMin"), EMcpParamKind::Number, false }, { TEXT("reverbWetLevelMax"), EMcpParamKind::Number, false }, { TEXT("reverbDistanceMin"), EMcpParamKind::Number, false }, { TEXT("reverbDistanceMax"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateDialogueVoice[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("gender"), EMcpParamKind::String, false }, { TEXT("plurality"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateDialogueWave[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("spokenText"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetDialogueContext[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("speakerPath"), EMcpParamKind::String, false }, { TEXT("soundWavePath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("targetVoices"), EMcpParamKind::Array, false }, { TEXT("localizationKeyFormat"), EMcpParamKind::String, false }, { TEXT("replace"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateReverbEffect[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("density"), EMcpParamKind::Number, false }, { TEXT("diffusion"), EMcpParamKind::Number, false }, { TEXT("gain"), EMcpParamKind::Number, false }, { TEXT("gainHF"), EMcpParamKind::Number, false }, { TEXT("decayTime"), EMcpParamKind::Number, false }, { TEXT("decayHFRatio"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateSourceEffectChain[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddSourceEffect[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("effectPresetPath"), EMcpParamKind::String, false }, { TEXT("effectType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("bypass"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_CreateSubmixEffect[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("effectType"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_GetAudioInfo[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row: the retired core dispatcher was
// whole-body #if WITH_EDITOR (each member replicates its non-editor
// NOT_IMPLEMENTED stub) and the retired authoring wrapper answered
// EDITOR_REQUIRED outside editor builds (each authoring member replicates
// that stub), so the members answer exactly as the retired chains did in
// every build flavor. Mutating on everything except the one reader,
// get_audio_info.

#define MCP_AU_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)      \
class FMcpCall_ManageAudio_##ClassSuffix final : public FMcpCall                         \
{                                                                                        \
	const FMcpCallDecl& GetDecl() const override                                         \
	{                                                                                    \
		static const FMcpCallDecl Decl{ TEXT("manage_audio"), TEXT(ActionLiteral),       \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                 \
		return Decl;                                                                     \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, Payload, Socket);                                  \
	}                                                                                    \
};

// Core (AudioHandlers.cpp)
MCP_AU_CALL(CreateSoundCue, "create_sound_cue", P_CreateSoundCue, HandleAudioCreateSoundCue, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateSoundClass, "create_sound_class", P_CreateSoundClass, HandleAudioCreateSoundClass, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateSoundMix, "create_sound_mix", P_CreateSoundMix, HandleAudioCreateSoundMix, EMcpCallFlags::Mutating)
MCP_AU_CALL(PlaySoundAtLocation, "play_sound_at_location", P_PlaySoundAtLocation, HandleAudioPlaySoundAtLocation, EMcpCallFlags::Mutating)
MCP_AU_CALL(PlaySound2D, "play_sound_2d", P_PlaySound2D, HandleAudioPlaySound2D, EMcpCallFlags::Mutating)
MCP_AU_CALL(PlaySoundAttached, "play_sound_attached", P_PlaySoundAttached, HandleAudioPlaySoundAttached, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateAmbientSound, "create_ambient_sound", P_CreateAmbientSound, HandleAudioCreateAmbientSound, EMcpCallFlags::Mutating)
MCP_AU_CALL(SpawnSoundAtLocation, "spawn_sound_at_location", P_SpawnSoundAtLocation, HandleAudioSpawnSoundAtLocation, EMcpCallFlags::Mutating)
MCP_AU_CALL(PushSoundMix, "push_sound_mix", P_PushSoundMix, HandleAudioPushSoundMix, EMcpCallFlags::Mutating)
MCP_AU_CALL(PopSoundMix, "pop_sound_mix", P_PopSoundMix, HandleAudioPopSoundMix, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetSoundMixClassOverride, "set_sound_mix_class_override", P_SetSoundMixClassOverride, HandleAudioSetSoundMixClassOverride, EMcpCallFlags::Mutating)
MCP_AU_CALL(ClearSoundMixClassOverride, "clear_sound_mix_class_override", P_ClearSoundMixClassOverride, HandleAudioClearSoundMixClassOverride, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetBaseSoundMix, "set_base_sound_mix", P_SetBaseSoundMix, HandleAudioSetBaseSoundMix, EMcpCallFlags::Mutating)
MCP_AU_CALL(FadeSoundOut, "fade_sound_out", P_FadeSoundOut, HandleAudioFadeSoundOut, EMcpCallFlags::Mutating)
MCP_AU_CALL(FadeSoundIn, "fade_sound_in", P_FadeSoundIn, HandleAudioFadeSoundIn, EMcpCallFlags::Mutating)
MCP_AU_CALL(PrimeSound, "prime_sound", P_PrimeSound, HandleAudioPrimeSound, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateAudioComponent, "create_audio_component", P_CreateAudioComponent, HandleAudioCreateAudioComponent, EMcpCallFlags::Mutating)
MCP_AU_CALL(EnableAudioAnalysis, "enable_audio_analysis", P_EnableAudioAnalysis, HandleAudioEnableAudioAnalysis, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetDopplerEffect, "set_doppler_effect", P_SetDopplerEffect, HandleAudioSetDopplerEffect, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetAudioOcclusion, "set_audio_occlusion", P_SetAudioOcclusion, HandleAudioSetAudioOcclusion, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetSoundAttenuation, "set_sound_attenuation", P_SetSoundAttenuation, HandleAudioSetSoundAttenuation, EMcpCallFlags::Mutating)
MCP_AU_CALL(FadeSound, "fade_sound", P_FadeSound, HandleAudioFadeSound, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateReverbZone, "create_reverb_zone", P_CreateReverbZone, HandleAudioCreateReverbZone, EMcpCallFlags::Mutating)

// Audio authoring (AudioAuthoringHandlers.cpp)
MCP_AU_CALL(AddCueNode, "add_cue_node", P_AddCueNode, HandleAudioAuthoringAddCueNode, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConnectCueNodes, "connect_cue_nodes", P_ConnectCueNodes, HandleAudioAuthoringConnectCueNodes, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetCueAttenuation, "set_cue_attenuation", P_SetCueAttenuation, HandleAudioAuthoringSetCueAttenuation, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetCueConcurrency, "set_cue_concurrency", P_SetCueConcurrency, HandleAudioAuthoringSetCueConcurrency, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateMetasound, "create_metasound", P_CreateMetasound, HandleAudioAuthoringCreateMetasound, EMcpCallFlags::Mutating)
MCP_AU_CALL(AddMetasoundNode, "add_metasound_node", P_AddMetasoundNode, HandleAudioAuthoringAddMetasoundNode, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConnectMetasoundNodes, "connect_metasound_nodes", P_ConnectMetasoundNodes, HandleAudioAuthoringConnectMetasoundNodes, EMcpCallFlags::Mutating)
MCP_AU_CALL(AddMetasoundInput, "add_metasound_input", P_AddMetasoundInput, HandleAudioAuthoringAddMetasoundInput, EMcpCallFlags::Mutating)
MCP_AU_CALL(AddMetasoundOutput, "add_metasound_output", P_AddMetasoundOutput, HandleAudioAuthoringAddMetasoundOutput, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetMetasoundDefault, "set_metasound_default", P_SetMetasoundDefault, HandleAudioAuthoringSetMetasoundDefault, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetClassProperties, "set_class_properties", P_SetClassProperties, HandleAudioAuthoringSetClassProperties, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetClassParent, "set_class_parent", P_SetClassParent, HandleAudioAuthoringSetClassParent, EMcpCallFlags::Mutating)
MCP_AU_CALL(AddMixModifier, "add_mix_modifier", P_AddMixModifier, HandleAudioAuthoringAddMixModifier, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConfigureMixEq, "configure_mix_eq", P_ConfigureMixEq, HandleAudioAuthoringConfigureMixEq, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateAttenuationSettings, "create_attenuation_settings", P_CreateAttenuationSettings, HandleAudioAuthoringCreateAttenuationSettings, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConfigureDistanceAttenuation, "configure_distance_attenuation", P_ConfigureDistanceAttenuation, HandleAudioAuthoringConfigureDistanceAttenuation, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConfigureSpatialization, "configure_spatialization", P_ConfigureSpatialization, HandleAudioAuthoringConfigureSpatialization, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConfigureOcclusion, "configure_occlusion", P_ConfigureOcclusion, HandleAudioAuthoringConfigureOcclusion, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConfigureReverbSend, "configure_reverb_send", P_ConfigureReverbSend, HandleAudioAuthoringConfigureReverbSend, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateDialogueVoice, "create_dialogue_voice", P_CreateDialogueVoice, HandleAudioAuthoringCreateDialogueVoice, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateDialogueWave, "create_dialogue_wave", P_CreateDialogueWave, HandleAudioAuthoringCreateDialogueWave, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetDialogueContext, "set_dialogue_context", P_SetDialogueContext, HandleAudioAuthoringSetDialogueContext, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateReverbEffect, "create_reverb_effect", P_CreateReverbEffect, HandleAudioAuthoringCreateReverbEffect, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateSourceEffectChain, "create_source_effect_chain", P_CreateSourceEffectChain, HandleAudioAuthoringCreateSourceEffectChain, EMcpCallFlags::Mutating)
MCP_AU_CALL(AddSourceEffect, "add_source_effect", P_AddSourceEffect, HandleAudioAuthoringAddSourceEffect, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateSubmixEffect, "create_submix_effect", P_CreateSubmixEffect, HandleAudioAuthoringCreateSubmixEffect, EMcpCallFlags::Mutating)
MCP_AU_CALL(GetAudioInfo, "get_audio_info", P_GetAudioInfo, HandleAudioAuthoringGetAudioInfo, EMcpCallFlags::None)

#undef MCP_AU_CALL

} // namespace McpCalls::ManageAudio

void McpRegisterManageAudioCalls()
{
	using namespace McpCalls::ManageAudio;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateSoundCue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateSoundClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateSoundMix>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_PlaySoundAtLocation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_PlaySound2D>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_PlaySoundAttached>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateAmbientSound>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SpawnSoundAtLocation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_PushSoundMix>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_PopSoundMix>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SetSoundMixClassOverride>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_ClearSoundMixClassOverride>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SetBaseSoundMix>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_FadeSoundOut>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_FadeSoundIn>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_PrimeSound>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateAudioComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_EnableAudioAnalysis>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SetDopplerEffect>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SetAudioOcclusion>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SetSoundAttenuation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_FadeSound>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateReverbZone>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_AddCueNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_ConnectCueNodes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SetCueAttenuation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SetCueConcurrency>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateMetasound>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_AddMetasoundNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_ConnectMetasoundNodes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_AddMetasoundInput>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_AddMetasoundOutput>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SetMetasoundDefault>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SetClassProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SetClassParent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_AddMixModifier>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_ConfigureMixEq>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateAttenuationSettings>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_ConfigureDistanceAttenuation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_ConfigureSpatialization>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_ConfigureOcclusion>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_ConfigureReverbSend>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateDialogueVoice>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateDialogueWave>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_SetDialogueContext>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateReverbEffect>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateSourceEffectChain>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_AddSourceEffect>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_CreateSubmixEffect>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageAudio_GetAudioInfo>());
}
