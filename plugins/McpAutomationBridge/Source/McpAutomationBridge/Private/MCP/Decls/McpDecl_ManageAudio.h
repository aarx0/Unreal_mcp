// Action declarations for manage_audio — the server's contract: which params
// each action reads, and which are required. Fleet-authored from handler
// source (three-witness cross-check, 2026-07-04), hand-maintained since:
// adding an action = adding its declaration here (the boot validation and
// tests/schema/action-decl-lint.ps1 enforce both directions).
// UnverifiedDecl = no reachable read path was attributable; validation skips
// those actions and the lint nags until someone verifies or removes them.
#pragma once

#include "MCP/McpCallRegistry.h"

namespace McpDecls
{
inline const FMcpParamDecl P_ManageAudio_0[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("nodeType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("wavePath"), EMcpParamKind::String, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("indefinite"), EMcpParamKind::Bool, false }, { TEXT("loopCount"), EMcpParamKind::Number, false }, { TEXT("attenuationPath"), EMcpParamKind::String, false }, { TEXT("delay"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAudio_1[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("inputName"), EMcpParamKind::String, true }, { TEXT("inputType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_2[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("nodeClassName"), EMcpParamKind::String, false }, { TEXT("nodeType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_3[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("outputName"), EMcpParamKind::String, true }, { TEXT("outputType"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_4[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("soundClassPath"), EMcpParamKind::String, true }, { TEXT("volumeAdjuster"), EMcpParamKind::Number, false }, { TEXT("pitchAdjuster"), EMcpParamKind::Number, false }, { TEXT("fadeInTime"), EMcpParamKind::Number, false }, { TEXT("fadeOutTime"), EMcpParamKind::Number, false }, { TEXT("applyToChildren"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_5[] = { { TEXT("chainPath"), EMcpParamKind::String, true }, { TEXT("effectType"), EMcpParamKind::String, true }, { TEXT("effectName"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("effectPresetPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("bypass"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_6[] = { { TEXT("mixName"), EMcpParamKind::String, true }, { TEXT("mix"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("soundClassName"), EMcpParamKind::String, true }, { TEXT("soundClass"), EMcpParamKind::String, true }, { TEXT("fadeOutTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAudio_7[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("falloffDistance"), EMcpParamKind::Number, false }, { TEXT("distanceAlgorithm"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAudio_8[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("applyEQ"), EMcpParamKind::Bool, false }, { TEXT("eqPriority"), EMcpParamKind::Number, false }, { TEXT("eqSettings"), EMcpParamKind::Object, false }, { TEXT("lowFrequency"), EMcpParamKind::Number, false }, { TEXT("lowGain"), EMcpParamKind::Number, false }, { TEXT("midFrequency"), EMcpParamKind::Number, false }, { TEXT("midGain"), EMcpParamKind::Number, false }, { TEXT("highMidFrequency"), EMcpParamKind::Number, false }, { TEXT("highMidGain"), EMcpParamKind::Number, false }, { TEXT("highFrequency"), EMcpParamKind::Number, false }, { TEXT("highGain"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAudio_9[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("enableOcclusion"), EMcpParamKind::Bool, false }, { TEXT("occlusionLowPassFilterFrequency"), EMcpParamKind::Number, false }, { TEXT("occlusionVolumeAttenuation"), EMcpParamKind::Number, false }, { TEXT("occlusionInterpolationTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAudio_10[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("enableReverbSend"), EMcpParamKind::Bool, false }, { TEXT("reverbWetLevelMin"), EMcpParamKind::Number, false }, { TEXT("reverbWetLevelMax"), EMcpParamKind::Number, false }, { TEXT("reverbDistanceMin"), EMcpParamKind::Number, false }, { TEXT("reverbDistanceMax"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAudio_11[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("spatializationAlgorithm"), EMcpParamKind::String, false }, { TEXT("spatialization"), EMcpParamKind::String, false }, { TEXT("spatialize"), EMcpParamKind::Bool, false }, { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_12[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("sourceNodeId"), EMcpParamKind::String, true }, { TEXT("targetNodeId"), EMcpParamKind::String, true }, { TEXT("childIndex"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_13[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("sourceNodeId"), EMcpParamKind::String, true }, { TEXT("sourceOutputName"), EMcpParamKind::String, false }, { TEXT("targetNodeId"), EMcpParamKind::String, true }, { TEXT("targetInputName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_14[] = { { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("startTime"), EMcpParamKind::Number, false }, { TEXT("attenuationPath"), EMcpParamKind::String, false }, { TEXT("concurrencyPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAudio_15[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("falloffDistance"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAudio_16[] = { { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("rotation"), EMcpParamKind::Array, false }, { TEXT("attachTo"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("volume"), EMcpParamKind::String, false }, { TEXT("pitch"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAudio_17[] = { { TEXT("voiceName"), EMcpParamKind::String, true }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("gender"), EMcpParamKind::String, false }, { TEXT("pluralization"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("plurality"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_18[] = { { TEXT("waveName"), EMcpParamKind::String, true }, { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("spokenText"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_19[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_20[] = { { TEXT("effectName"), EMcpParamKind::String, true }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("density"), EMcpParamKind::Number, false }, { TEXT("diffusion"), EMcpParamKind::Number, false }, { TEXT("gain"), EMcpParamKind::Number, false }, { TEXT("gainHF"), EMcpParamKind::Number, false }, { TEXT("decayTime"), EMcpParamKind::Number, false }, { TEXT("decayHFRatio"), EMcpParamKind::Number, false }, { TEXT("reflectionsGain"), EMcpParamKind::Number, false }, { TEXT("lateGain"), EMcpParamKind::Number, false }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_21[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("size"), EMcpParamKind::Array, false }, { TEXT("reverbEffect"), EMcpParamKind::String, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("fadeTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAudio_22[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("packagePath"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("properties"), EMcpParamKind::Object, false }, { TEXT("parentClass"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_23[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("packagePath"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("wavePath"), EMcpParamKind::String, false }, { TEXT("looping"), EMcpParamKind::Bool, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("attenuationPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_24[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("packagePath"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("classAdjusters"), EMcpParamKind::Array, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_25[] = { { TEXT("chainName"), EMcpParamKind::String, true }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_26[] = { { TEXT("effectName"), EMcpParamKind::String, true }, { TEXT("outputPath"), EMcpParamKind::String, false }, { TEXT("effectType"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_27[] = { { TEXT("enable"), EMcpParamKind::Bool, false }, { TEXT("enabled"), EMcpParamKind::Bool, false }, { TEXT("analysisType"), EMcpParamKind::String, false }, { TEXT("windowSize"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAudio_28[] = { { TEXT("soundName"), EMcpParamKind::String, true }, { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("fadeTime"), EMcpParamKind::Number, false }, { TEXT("targetVolume"), EMcpParamKind::Number, false }, { TEXT("fadeType"), EMcpParamKind::String, false }, { TEXT("componentName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAudio_29[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("fadeTime"), EMcpParamKind::Number, false }, { TEXT("targetVolume"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAudio_30[] = { { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("componentName"), EMcpParamKind::String, false }, { TEXT("fadeTime"), EMcpParamKind::Number, false }, { TEXT("targetVolume"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ManageAudio_31[] = { { TEXT("assetPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAudio_32[] = { { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("params"), EMcpParamKind::Object, false }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("startTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageAudio_33[] = { { TEXT("functionName"), EMcpParamKind::String, true }, { TEXT("params"), EMcpParamKind::Object, false }, { TEXT("path"), EMcpParamKind::String, true }, { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("rotation"), EMcpParamKind::Array, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("startTime"), EMcpParamKind::Number, false }, { TEXT("attenuationPath"), EMcpParamKind::String, false }, { TEXT("concurrencyPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAudio_34[] = { { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("actorName"), EMcpParamKind::String, true }, { TEXT("attachPointName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageAudio_35[] = { { TEXT("mixName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAudio_36[] = { { TEXT("soundPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAudio_37[] = { { TEXT("mixName"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAudio_38[] = { { TEXT("soundPath"), EMcpParamKind::String, false }, { TEXT("enable"), EMcpParamKind::Bool, false }, { TEXT("occlusionVolumeScale"), EMcpParamKind::Number, false }, { TEXT("occlusionFilterScale"), EMcpParamKind::Number, false }, { TEXT("occlusionInterpolationTime"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_39[] = { { TEXT("mixName"), EMcpParamKind::String, true }, { TEXT("mix"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageAudio_40[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("parentPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_41[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("lowPassFilterFrequency"), EMcpParamKind::Number, false }, { TEXT("lfeBleed"), EMcpParamKind::Number, false }, { TEXT("voiceCenterChannelVolume"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_42[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("attenuationPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_43[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("concurrencyPath"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_44[] = { { TEXT("wavePath"), EMcpParamKind::String, true }, { TEXT("voicePath"), EMcpParamKind::String, true }, { TEXT("contextIndex"), EMcpParamKind::Number, false }, { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("speakerPath"), EMcpParamKind::String, false }, { TEXT("soundWavePath"), EMcpParamKind::String, false }, { TEXT("targetVoices"), EMcpParamKind::Array, false }, { TEXT("localizationKeyFormat"), EMcpParamKind::String, false }, { TEXT("replace"), EMcpParamKind::Bool, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_45[] = { { TEXT("soundPath"), EMcpParamKind::String, false }, { TEXT("dopplerIntensity"), EMcpParamKind::Number, false }, { TEXT("velocityScale"), EMcpParamKind::Number, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_46[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("inputName"), EMcpParamKind::String, true }, { TEXT("floatValue"), EMcpParamKind::Number, false }, { TEXT("intValue"), EMcpParamKind::Number, false }, { TEXT("boolValue"), EMcpParamKind::Bool, false }, { TEXT("stringValue"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_47[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("innerRadius"), EMcpParamKind::Number, false }, { TEXT("falloffDistance"), EMcpParamKind::Number, false }, { TEXT("attenuationShape"), EMcpParamKind::String, false }, { TEXT("falloffMode"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_48[] = { { TEXT("mixName"), EMcpParamKind::String, true }, { TEXT("mix"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, true }, { TEXT("soundClassName"), EMcpParamKind::String, true }, { TEXT("soundClass"), EMcpParamKind::String, true }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("fadeInTime"), EMcpParamKind::Number, false }, { TEXT("applyToChildren"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageAudio_49[] = { { TEXT("soundPath"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("rotation"), EMcpParamKind::Array, false }, { TEXT("volume"), EMcpParamKind::Number, false }, { TEXT("pitch"), EMcpParamKind::Number, false }, { TEXT("startTime"), EMcpParamKind::Number, false } };

inline const FMcpCallDecl GManageAudio[] =
{
	{ TEXT("manage_audio"), TEXT("add_cue_node"), P_ManageAudio_0, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("add_metasound_input"), P_ManageAudio_1, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("add_metasound_node"), P_ManageAudio_2, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("add_metasound_output"), P_ManageAudio_3, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("add_mix_modifier"), P_ManageAudio_4, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("add_source_effect"), P_ManageAudio_5, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("clear_sound_mix_class_override"), P_ManageAudio_6, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("configure_distance_attenuation"), P_ManageAudio_7, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("configure_mix_eq"), P_ManageAudio_8, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("configure_occlusion"), P_ManageAudio_9, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("configure_reverb_send"), P_ManageAudio_10, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("configure_spatialization"), P_ManageAudio_11, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("connect_cue_nodes"), P_ManageAudio_12, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("connect_metasound_nodes"), P_ManageAudio_13, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_ambient_sound"), P_ManageAudio_14, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_attenuation_settings"), P_ManageAudio_15, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_audio_component"), P_ManageAudio_16, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_dialogue_voice"), P_ManageAudio_17, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_dialogue_wave"), P_ManageAudio_18, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_metasound"), P_ManageAudio_19, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_reverb_effect"), P_ManageAudio_20, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_reverb_zone"), P_ManageAudio_21, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_sound_class"), P_ManageAudio_22, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_sound_cue"), P_ManageAudio_23, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_sound_mix"), P_ManageAudio_24, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_source_effect_chain"), P_ManageAudio_25, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("create_submix_effect"), P_ManageAudio_26, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("enable_audio_analysis"), P_ManageAudio_27, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("fade_sound"), P_ManageAudio_28, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("fade_sound_in"), P_ManageAudio_29, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("fade_sound_out"), P_ManageAudio_30, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("get_audio_info"), P_ManageAudio_31, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("play_sound_2d"), P_ManageAudio_32, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("play_sound_at_location"), P_ManageAudio_33, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("play_sound_attached"), P_ManageAudio_34, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("pop_sound_mix"), P_ManageAudio_35, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("prime_sound"), P_ManageAudio_36, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("push_sound_mix"), P_ManageAudio_37, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("set_audio_occlusion"), P_ManageAudio_38, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("set_base_sound_mix"), P_ManageAudio_39, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("set_class_parent"), P_ManageAudio_40, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("set_class_properties"), P_ManageAudio_41, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("set_cue_attenuation"), P_ManageAudio_42, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("set_cue_concurrency"), P_ManageAudio_43, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("set_dialogue_context"), P_ManageAudio_44, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("set_doppler_effect"), P_ManageAudio_45, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("set_metasound_default"), P_ManageAudio_46, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("set_sound_attenuation"), P_ManageAudio_47, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("set_sound_mix_class_override"), P_ManageAudio_48, EMcpCallFlags::None },
	{ TEXT("manage_audio"), TEXT("spawn_sound_at_location"), P_ManageAudio_49, EMcpCallFlags::None },
};
}
