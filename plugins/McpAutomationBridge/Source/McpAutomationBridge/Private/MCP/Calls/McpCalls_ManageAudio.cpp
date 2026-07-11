// LINT-TOOL: manage_audio
// LINT-SCHEMA-DERIVED
// manage_audio as FMcpCall classes — nineteenth classed family, now adopting
// schema-from-decls (docs/action-declarations.md). Each class AUTHORS its schema
// fragment in an S_<Suffix>() function; the published facade schema folds those
// fragments and GetDecl() derives the validation decl from the same fragment via
// McpDeriveDecl(), so schema and decl are one source and cannot drift. Run()
// delegates to the namespaced free handlers: McpHandlers::Audio::HandleAudio*
// (AudioHandlers.cpp) for the 23 core actions and HandleAudioAuthoring*
// (AudioAuthoringHandlers.cpp) for the 27 authoring actions.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_AudioHandlers.h"
#include "McpAutomationBridge_AudioAuthoringHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageAudio
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads.
// Descriptions are the tool's authored help text; McpDeriveDecl() reads the param
// kinds + required-set back out of these to build the transport validation decl.
// Several actions accept a first-present-wins alias chain the decl vocabulary
// cannot express, so their members author each alias optional and the handler
// rejects only at resolution: the mixName→mix→name (+soundClassName→soundClass)
// chains on set_sound_mix_class_override / clear_sound_mix_class_override /
// set_base_sound_mix; fade_sound's soundName→actorName pair; and
// create_audio_component's soundPath→path pair (joint reject only when both are
// absent).

// Core (AudioHandlers.cpp)

static void S_CreateSoundCue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("packagePath"), TEXT("Directory path for asset creation (alias of path)."))
	 .String(TEXT("savePath"), TEXT("Directory path for asset creation (alias of path)."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("wavePath"), TEXT("Path to SoundWave asset."))
	 .Bool(TEXT("looping"), TEXT("Whether to loop."))
	 .Number(TEXT("volume"), TEXT(""))
	 .Number(TEXT("pitch"), TEXT(""))
	 .String(TEXT("attenuationPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("name")});
}

static void S_CreateSoundClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("packagePath"), TEXT("Directory path for asset creation (alias of path)."))
	 .String(TEXT("savePath"), TEXT("Directory path for asset creation (alias of path)."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Object(TEXT("properties"), TEXT(""))
	 .String(TEXT("parentClass"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("name")});
}

static void S_CreateSoundMix(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("packagePath"), TEXT("Directory path for asset creation (alias of path)."))
	 .String(TEXT("savePath"), TEXT("Directory path for asset creation (alias of path)."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .ArrayOfObjects(TEXT("classAdjusters"), TEXT("create_sound_mix: per-class volume/pitch adjusters."),
		[](FMcpSchemaBuilder& S) {
			S.String(TEXT("soundClass"), TEXT("Sound class asset path."))
			 .Number(TEXT("volumeAdjuster"), TEXT("Volume multiplier."))
			 .Number(TEXT("pitchAdjuster"), TEXT("Pitch multiplier."));
		})
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("name")});
}

static void S_PlaySoundAtLocation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("soundPath"), TEXT("Sound asset path."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Number(TEXT("volume"), TEXT(""))
	 .Number(TEXT("pitch"), TEXT(""))
	 .Number(TEXT("startTime"), TEXT(""))
	 .String(TEXT("attenuationPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("concurrencyPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("soundPath")});
}

static void S_PlaySound2D(FMcpSchemaBuilder& B)
{
	B.String(TEXT("soundPath"), TEXT("Sound asset path."))
	 .Number(TEXT("volume"), TEXT(""))
	 .Number(TEXT("pitch"), TEXT(""))
	 .Number(TEXT("startTime"), TEXT(""))
	 .Required({TEXT("soundPath")});
}

static void S_PlaySoundAttached(FMcpSchemaBuilder& B)
{
	B.String(TEXT("soundPath"), TEXT("Sound asset path."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("attachPointName"), TEXT("Name of the socket."))
	 .Required({TEXT("soundPath"), TEXT("actorName")});
}

static void S_CreateAmbientSound(FMcpSchemaBuilder& B)
{
	B.String(TEXT("soundPath"), TEXT("Sound asset path."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Number(TEXT("volume"), TEXT(""))
	 .Number(TEXT("pitch"), TEXT(""))
	 .Number(TEXT("startTime"), TEXT(""))
	 .String(TEXT("attenuationPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("concurrencyPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("soundPath")});
}

static void S_SpawnSoundAtLocation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("soundPath"), TEXT("Sound asset path."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .Number(TEXT("volume"), TEXT(""))
	 .Number(TEXT("pitch"), TEXT(""))
	 .Number(TEXT("startTime"), TEXT(""))
	 .Required({TEXT("soundPath")});
}

static void S_PushSoundMix(FMcpSchemaBuilder& B)
{
	B.String(TEXT("mixName"), TEXT(""))
	 .String(TEXT("mix"), TEXT("Alias of mixName (set_sound_mix_class_override/clear_sound_mix_class_override/set_base_sound_mix)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .RequiredAnyOf({TEXT("mixName"), TEXT("mix"), TEXT("name")});
}

static void S_PopSoundMix(FMcpSchemaBuilder& B)
{
	B.String(TEXT("mixName"), TEXT(""))
	 .String(TEXT("mix"), TEXT("Alias of mixName (set_sound_mix_class_override/clear_sound_mix_class_override/set_base_sound_mix)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .RequiredAnyOf({TEXT("mixName"), TEXT("mix"), TEXT("name")});
}

static void S_SetSoundMixClassOverride(FMcpSchemaBuilder& B)
{
	B.String(TEXT("mixName"), TEXT(""))
	 .String(TEXT("mix"), TEXT("Alias of mixName (set_sound_mix_class_override/clear_sound_mix_class_override/set_base_sound_mix)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("soundClassName"), TEXT(""))
	 .String(TEXT("soundClass"), TEXT("Alias of soundClassName (set_sound_mix_class_override/clear_sound_mix_class_override)."))
	 .Number(TEXT("volume"), TEXT(""))
	 .Number(TEXT("pitch"), TEXT(""))
	 .Number(TEXT("fadeInTime"), TEXT(""))
	 .Bool(TEXT("applyToChildren"), TEXT("set_sound_mix_class_override/add_mix_modifier: apply the override to child sound classes."))
	 .RequiredAnyOf({TEXT("mixName"), TEXT("mix"), TEXT("name")})
	 .RequiredAnyOf({TEXT("soundClassName"), TEXT("soundClass")});
}

static void S_ClearSoundMixClassOverride(FMcpSchemaBuilder& B)
{
	B.String(TEXT("mixName"), TEXT(""))
	 .String(TEXT("mix"), TEXT("Alias of mixName (set_sound_mix_class_override/clear_sound_mix_class_override/set_base_sound_mix)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("soundClassName"), TEXT(""))
	 .String(TEXT("soundClass"), TEXT("Alias of soundClassName (set_sound_mix_class_override/clear_sound_mix_class_override)."))
	 .Number(TEXT("fadeOutTime"), TEXT(""))
	 .RequiredAnyOf({TEXT("mixName"), TEXT("mix"), TEXT("name")})
	 .RequiredAnyOf({TEXT("soundClassName"), TEXT("soundClass")});
}

static void S_SetBaseSoundMix(FMcpSchemaBuilder& B)
{
	B.String(TEXT("mixName"), TEXT(""))
	 .String(TEXT("mix"), TEXT("Alias of mixName (set_sound_mix_class_override/clear_sound_mix_class_override/set_base_sound_mix)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .RequiredAnyOf({TEXT("mixName"), TEXT("mix"), TEXT("name")});
}

static void S_FadeSoundOut(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Number(TEXT("fadeTime"), TEXT(""))
	 .Number(TEXT("targetVolume"), TEXT(""))
	 .Required({TEXT("actorName")});
}

static void S_FadeSoundIn(FMcpSchemaBuilder& B)
{
	B.String(TEXT("actorName"), TEXT("Name of the actor."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Number(TEXT("fadeTime"), TEXT(""))
	 .Number(TEXT("targetVolume"), TEXT(""))
	 .Required({TEXT("actorName")});
}

static void S_PrimeSound(FMcpSchemaBuilder& B)
{
	B.String(TEXT("soundPath"), TEXT("Sound asset path."))
	 .Required({TEXT("soundPath")});
}

static void S_CreateAudioComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("soundPath"), TEXT("Sound asset path."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll")); })
	 .String(TEXT("attachTo"), TEXT("create_audio_component: actor to attach the component to (alias of actorName)."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Number(TEXT("volume"), TEXT(""))
	 .Number(TEXT("pitch"), TEXT(""))
	 .RequiredAnyOf({TEXT("soundPath"), TEXT("path")});
}

static void S_EnableAudioAnalysis(FMcpSchemaBuilder& B)
{
	B.Bool(TEXT("enable"), TEXT("Whether the item/feature is enabled."))
	 .Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
	 .String(TEXT("analysisType"), TEXT("Audio analysis type."))
	 .Number(TEXT("windowSize"), TEXT("Audio analysis window size."));
}

static void S_SetDopplerEffect(FMcpSchemaBuilder& B)
{
	B.String(TEXT("soundPath"), TEXT("Sound asset path."))
	 .Number(TEXT("dopplerIntensity"), TEXT("Doppler intensity."))
	 .Number(TEXT("velocityScale"), TEXT("Velocity scale."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."));
}

static void S_SetAudioOcclusion(FMcpSchemaBuilder& B)
{
	B.String(TEXT("soundPath"), TEXT("Sound asset path."))
	 .Bool(TEXT("enable"), TEXT("Whether the item/feature is enabled."))
	 .Number(TEXT("occlusionVolumeScale"), TEXT("Occlusion volume scale."))
	 .Number(TEXT("occlusionFilterScale"), TEXT("Occlusion filter scale."))
	 .Number(TEXT("occlusionInterpolationTime"), TEXT("Occlusion interpolation time."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."));
}

static void S_SetSoundAttenuation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Number(TEXT("innerRadius"), TEXT(""))
	 .Number(TEXT("falloffDistance"), TEXT(""))
	 .String(TEXT("attenuationShape"), TEXT(""))
	 .String(TEXT("falloffMode"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("name")});
}

static void S_FadeSound(FMcpSchemaBuilder& B)
{
	B.String(TEXT("soundName"), TEXT(""))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Number(TEXT("fadeTime"), TEXT(""))
	 .Number(TEXT("targetVolume"), TEXT(""))
	 .String(TEXT("fadeType"), TEXT(""))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .RequiredAnyOf({TEXT("soundName"), TEXT("actorName")});
}

static void S_CreateReverbZone(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("size"), TEXT("3D scale (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .String(TEXT("reverbEffect"), TEXT(""))
	 .Number(TEXT("volume"), TEXT(""))
	 .Number(TEXT("fadeTime"), TEXT(""))
	 .Required({TEXT("name")});
}

// Audio authoring (AudioAuthoringHandlers.cpp)

static void S_AddCueNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("nodeType"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .String(TEXT("wavePath"), TEXT("Path to SoundWave asset."))
	 .Number(TEXT("volume"), TEXT(""))
	 .Number(TEXT("pitch"), TEXT(""))
	 .Bool(TEXT("indefinite"), TEXT("add_cue_node (looping node): loop indefinitely."))
	 .Integer(TEXT("loopCount"), TEXT("add_cue_node (looping node): fixed loop count when not indefinite."))
	 .String(TEXT("attenuationPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("delay"), TEXT("add_cue_node (delay node): delay in seconds."))
	 .Required({TEXT("assetPath")});
}

static void S_ConnectCueNodes(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("sourceNodeId"), TEXT("ID of the consuming (parent) node; pass 'Root' in connect_cue_nodes to wire the cue output."))
	 .String(TEXT("targetNodeId"), TEXT("ID of the target (feeding child) node."))
	 .Integer(TEXT("childIndex"), TEXT("Input pin index on the source node for connect_cue_nodes."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("assetPath"), TEXT("sourceNodeId"), TEXT("targetNodeId")});
}

static void S_SetCueAttenuation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("attenuationPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("assetPath")});
}

static void S_SetCueConcurrency(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("concurrencyPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("assetPath")});
}

static void S_CreateMetasound(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("name")});
}

static void S_AddMetasoundNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("nodeClassName"), TEXT("add_metasound_node: explicit 3-part MetaSound class name (Namespace.Name.Variant)."))
	 .String(TEXT("nodeType"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("assetPath")})
	 .RequiredAnyOf({TEXT("nodeClassName"), TEXT("nodeType")});
}

static void S_ConnectMetasoundNodes(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("sourceNodeId"), TEXT("ID of the consuming (parent) node; pass 'Root' in connect_cue_nodes to wire the cue output."))
	 .String(TEXT("sourceOutputName"), TEXT("Source MetaSound output name."))
	 .String(TEXT("targetNodeId"), TEXT("ID of the target (feeding child) node."))
	 .String(TEXT("targetInputName"), TEXT("Target MetaSound input name."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("assetPath"), TEXT("sourceNodeId"), TEXT("targetNodeId")});
}

static void S_AddMetasoundInput(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("inputName"), TEXT("Name of the input."))
	 .String(TEXT("inputType"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("assetPath"), TEXT("inputName")});
}

static void S_AddMetasoundOutput(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("outputName"), TEXT("Name of the output."))
	 .String(TEXT("outputType"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("assetPath"), TEXT("outputName")});
}

static void S_SetMetasoundDefault(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("inputName"), TEXT("Name of the input."))
	 .Number(TEXT("floatValue"), TEXT("set_metasound_default: float literal value."))
	 .Integer(TEXT("intValue"), TEXT("set_metasound_default: int literal value."))
	 .Bool(TEXT("boolValue"), TEXT("set_metasound_default: bool literal value."))
	 .String(TEXT("stringValue"), TEXT("set_metasound_default: string literal value."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("assetPath"), TEXT("inputName")});
}

static void S_SetClassProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Number(TEXT("volume"), TEXT(""))
	 .Number(TEXT("pitch"), TEXT(""))
	 .Number(TEXT("lowPassFilterFrequency"), TEXT(""))
	 .Number(TEXT("lfeBleed"), TEXT("set_class_properties: LFE channel bleed amount."))
	 .Number(TEXT("voiceCenterChannelVolume"), TEXT("set_class_properties: center-channel volume for dialogue."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("assetPath")});
}

static void S_SetClassParent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("parentPath"), TEXT("set_class_parent: parent SoundClass asset path."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("assetPath")});
}

static void S_AddMixModifier(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("soundClassPath"), TEXT("Sound class path."))
	 .Number(TEXT("volumeAdjuster"), TEXT("Sound mix volume adjuster."))
	 .Number(TEXT("pitchAdjuster"), TEXT("add_mix_modifier: pitch multiplier."))
	 .Number(TEXT("fadeInTime"), TEXT(""))
	 .Number(TEXT("fadeOutTime"), TEXT(""))
	 .Bool(TEXT("applyToChildren"), TEXT("set_sound_mix_class_override/add_mix_modifier: apply the override to child sound classes."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("assetPath"), TEXT("soundClassPath")});
}

static void S_ConfigureMixEq(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Bool(TEXT("applyEQ"), TEXT("configure_mix_eq: enable the mix's EQ effect."))
	 .Number(TEXT("eqPriority"), TEXT("configure_mix_eq: EQ effect priority."))
	 .Object(TEXT("eqSettings"), TEXT("configure_mix_eq: 4-band EQ (frequencyCenter0-3, gain0-3, bandwidth0-3)."),
		[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("frequencyCenter0")).Number(TEXT("gain0")).Number(TEXT("bandwidth0"))
			 .Number(TEXT("frequencyCenter1")).Number(TEXT("gain1")).Number(TEXT("bandwidth1"))
			 .Number(TEXT("frequencyCenter2")).Number(TEXT("gain2")).Number(TEXT("bandwidth2"))
			 .Number(TEXT("frequencyCenter3")).Number(TEXT("gain3")).Number(TEXT("bandwidth3"));
		})
	 .Number(TEXT("lowFrequency"), TEXT("configure_mix_eq: flat alias for eqSettings band-0 frequency."))
	 .Number(TEXT("lowGain"), TEXT("configure_mix_eq: flat alias for eqSettings band-0 gain."))
	 .Number(TEXT("midFrequency"), TEXT("configure_mix_eq: flat alias for eqSettings band-1 frequency."))
	 .Number(TEXT("midGain"), TEXT("configure_mix_eq: flat alias for eqSettings band-1 gain."))
	 .Number(TEXT("highMidFrequency"), TEXT("configure_mix_eq: flat alias for eqSettings band-2 frequency."))
	 .Number(TEXT("highMidGain"), TEXT("configure_mix_eq: flat alias for eqSettings band-2 gain."))
	 .Number(TEXT("highFrequency"), TEXT("configure_mix_eq: flat alias for eqSettings band-3 frequency."))
	 .Number(TEXT("highGain"), TEXT("configure_mix_eq: flat alias for eqSettings band-3 gain."))
	 .Required({TEXT("assetPath")});
}

static void S_CreateAttenuationSettings(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Number(TEXT("innerRadius"), TEXT(""))
	 .Number(TEXT("falloffDistance"), TEXT(""))
	 .Required({TEXT("name")});
}

static void S_ConfigureDistanceAttenuation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Number(TEXT("innerRadius"), TEXT(""))
	 .Number(TEXT("falloffDistance"), TEXT(""))
	 .String(TEXT("distanceAlgorithm"), TEXT("configure_distance_attenuation: linear|logarithmic|inverse|naturalsound."))
	 .Required({TEXT("assetPath")});
}

static void S_ConfigureSpatialization(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .String(TEXT("spatializationAlgorithm"), TEXT("Spatialization algorithm: 'panner' or 'hrtf'."))
	 .String(TEXT("spatialization"), TEXT("Spatialization algorithm: 'panner' or 'hrtf' (alias of spatializationAlgorithm)."))
	 .Bool(TEXT("spatialize"), TEXT("Enable spatialization on the attenuation asset (alias of enabled)."))
	 .Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
	 .Required({TEXT("assetPath")});
}

static void S_ConfigureOcclusion(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Bool(TEXT("enableOcclusion"), TEXT("configure_occlusion: enable occlusion."))
	 .Number(TEXT("occlusionLowPassFilterFrequency"), TEXT("configure_occlusion: low-pass filter frequency in Hz."))
	 .Number(TEXT("occlusionVolumeAttenuation"), TEXT("configure_occlusion: volume attenuation."))
	 .Number(TEXT("occlusionInterpolationTime"), TEXT("Occlusion interpolation time."))
	 .Required({TEXT("assetPath")});
}

static void S_ConfigureReverbSend(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Bool(TEXT("enableReverbSend"), TEXT("Whether reverb send is enabled."))
	 .Number(TEXT("reverbWetLevelMin"), TEXT("Minimum reverb wet level."))
	 .Number(TEXT("reverbWetLevelMax"), TEXT("Maximum reverb wet level."))
	 .Number(TEXT("reverbDistanceMin"), TEXT("Minimum reverb send distance."))
	 .Number(TEXT("reverbDistanceMax"), TEXT("Maximum reverb send distance."))
	 .Required({TEXT("assetPath")});
}

static void S_CreateDialogueVoice(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("gender"), TEXT("create_dialogue_voice: Masculine|Feminine|Neuter."))
	 .String(TEXT("plurality"), TEXT("create_dialogue_voice: Singular|Plural."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("name")});
}

static void S_CreateDialogueWave(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("spokenText"), TEXT("create_dialogue_wave: the line's spoken text."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("name")});
}

static void S_SetDialogueContext(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("speakerPath"), TEXT("Dialogue speaker path."))
	 .String(TEXT("soundWavePath"), TEXT("set_dialogue_context: SoundWave asset path for the context mapping."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Array(TEXT("targetVoices"), TEXT("set_dialogue_context: target DialogueVoice asset paths."), TEXT("string"))
	 .String(TEXT("localizationKeyFormat"), TEXT("set_dialogue_context: localization key format string."))
	 .Bool(TEXT("replace"), TEXT("set_dialogue_context: replace an existing mapping for the same speaker."))
	 .Required({TEXT("assetPath")});
}

static void S_CreateReverbEffect(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Number(TEXT("density"), TEXT("create_reverb_effect: reverb density."))
	 .Number(TEXT("diffusion"), TEXT("create_reverb_effect: reverb diffusion."))
	 .Number(TEXT("gain"), TEXT("create_reverb_effect: overall gain."))
	 .Number(TEXT("gainHF"), TEXT("create_reverb_effect: high-frequency gain."))
	 .Number(TEXT("decayTime"), TEXT("create_reverb_effect: decay time in seconds."))
	 .Number(TEXT("decayHFRatio"), TEXT("create_reverb_effect: high-frequency decay ratio."))
	 .Required({TEXT("name")});
}

static void S_CreateSourceEffectChain(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("name")});
}

static void S_AddSourceEffect(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .String(TEXT("effectPresetPath"), TEXT("add_source_effect: existing effect preset asset path."))
	 .String(TEXT("effectType"), TEXT("Effect type."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Bool(TEXT("bypass"), TEXT("add_source_effect: bypass the effect entry."))
	 .Required({TEXT("assetPath")});
}

static void S_CreateSubmixEffect(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("effectType"), TEXT("Effect type."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
	 .Required({TEXT("name")});
}

static void S_GetAudioInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
	 .Required({TEXT("assetPath")});
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row: the retired core dispatcher was
// whole-body #if WITH_EDITOR (each member replicates its non-editor
// NOT_IMPLEMENTED stub) and the retired authoring wrapper answered
// EDITOR_REQUIRED outside editor builds (each authoring member replicates
// that stub), so the members answer exactly as the retired chains did in
// every build flavor. Mutating on everything except the one reader,
// get_info.

#define MCP_AU_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                    \
class FMcpCall_ManageAudio_##ClassSuffix final : public FMcpCall                          \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_audio"),                \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),            \
			&S_##ClassSuffix);                                                            \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return HandlerFn(S, RequestId, Payload, Socket);                                  \
	}                                                                                     \
};

// Core (AudioHandlers.cpp)
MCP_AU_CALL(CreateSoundCue, "create_sound_cue", McpHandlers::Audio::HandleAudioCreateSoundCue, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateSoundClass, "create_sound_class", McpHandlers::Audio::HandleAudioCreateSoundClass, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateSoundMix, "create_sound_mix", McpHandlers::Audio::HandleAudioCreateSoundMix, EMcpCallFlags::Mutating)
MCP_AU_CALL(PlaySoundAtLocation, "play_sound_at_location", McpHandlers::Audio::HandleAudioPlaySoundAtLocation, EMcpCallFlags::Mutating)
MCP_AU_CALL(PlaySound2D, "play_sound_2d", McpHandlers::Audio::HandleAudioPlaySound2D, EMcpCallFlags::Mutating)
MCP_AU_CALL(PlaySoundAttached, "play_sound_attached", McpHandlers::Audio::HandleAudioPlaySoundAttached, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateAmbientSound, "create_ambient_sound", McpHandlers::Audio::HandleAudioCreateAmbientSound, EMcpCallFlags::Mutating)
MCP_AU_CALL(SpawnSoundAtLocation, "spawn_sound_at_location", McpHandlers::Audio::HandleAudioSpawnSoundAtLocation, EMcpCallFlags::Mutating)
MCP_AU_CALL(PushSoundMix, "push_sound_mix", McpHandlers::Audio::HandleAudioPushSoundMix, EMcpCallFlags::Mutating)
MCP_AU_CALL(PopSoundMix, "pop_sound_mix", McpHandlers::Audio::HandleAudioPopSoundMix, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetSoundMixClassOverride, "set_sound_mix_class_override", McpHandlers::Audio::HandleAudioSetSoundMixClassOverride, EMcpCallFlags::Mutating)
MCP_AU_CALL(ClearSoundMixClassOverride, "clear_sound_mix_class_override", McpHandlers::Audio::HandleAudioClearSoundMixClassOverride, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetBaseSoundMix, "set_base_sound_mix", McpHandlers::Audio::HandleAudioSetBaseSoundMix, EMcpCallFlags::Mutating)
MCP_AU_CALL(FadeSoundOut, "fade_sound_out", McpHandlers::Audio::HandleAudioFadeSoundOut, EMcpCallFlags::Mutating)
MCP_AU_CALL(FadeSoundIn, "fade_sound_in", McpHandlers::Audio::HandleAudioFadeSoundIn, EMcpCallFlags::Mutating)
MCP_AU_CALL(PrimeSound, "prime_sound", McpHandlers::Audio::HandleAudioPrimeSound, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateAudioComponent, "create_audio_component", McpHandlers::Audio::HandleAudioCreateAudioComponent, EMcpCallFlags::Mutating)
MCP_AU_CALL(EnableAudioAnalysis, "enable_audio_analysis", McpHandlers::Audio::HandleAudioEnableAudioAnalysis, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetDopplerEffect, "set_doppler_effect", McpHandlers::Audio::HandleAudioSetDopplerEffect, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetAudioOcclusion, "set_audio_occlusion", McpHandlers::Audio::HandleAudioSetAudioOcclusion, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetSoundAttenuation, "set_sound_attenuation", McpHandlers::Audio::HandleAudioSetSoundAttenuation, EMcpCallFlags::Mutating)
MCP_AU_CALL(FadeSound, "fade_sound", McpHandlers::Audio::HandleAudioFadeSound, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateReverbZone, "create_reverb_zone", McpHandlers::Audio::HandleAudioCreateReverbZone, EMcpCallFlags::Mutating)

// Audio authoring (AudioAuthoringHandlers.cpp)
MCP_AU_CALL(AddCueNode, "add_cue_node", McpHandlers::Audio::HandleAudioAuthoringAddCueNode, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConnectCueNodes, "connect_cue_nodes", McpHandlers::Audio::HandleAudioAuthoringConnectCueNodes, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetCueAttenuation, "set_cue_attenuation", McpHandlers::Audio::HandleAudioAuthoringSetCueAttenuation, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetCueConcurrency, "set_cue_concurrency", McpHandlers::Audio::HandleAudioAuthoringSetCueConcurrency, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateMetasound, "create_metasound", McpHandlers::Audio::HandleAudioAuthoringCreateMetasound, EMcpCallFlags::Mutating)
MCP_AU_CALL(AddMetasoundNode, "add_metasound_node", McpHandlers::Audio::HandleAudioAuthoringAddMetasoundNode, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConnectMetasoundNodes, "connect_metasound_nodes", McpHandlers::Audio::HandleAudioAuthoringConnectMetasoundNodes, EMcpCallFlags::Mutating)
MCP_AU_CALL(AddMetasoundInput, "add_metasound_input", McpHandlers::Audio::HandleAudioAuthoringAddMetasoundInput, EMcpCallFlags::Mutating)
MCP_AU_CALL(AddMetasoundOutput, "add_metasound_output", McpHandlers::Audio::HandleAudioAuthoringAddMetasoundOutput, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetMetasoundDefault, "set_metasound_default", McpHandlers::Audio::HandleAudioAuthoringSetMetasoundDefault, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetClassProperties, "set_class_properties", McpHandlers::Audio::HandleAudioAuthoringSetClassProperties, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetClassParent, "set_class_parent", McpHandlers::Audio::HandleAudioAuthoringSetClassParent, EMcpCallFlags::Mutating)
MCP_AU_CALL(AddMixModifier, "add_mix_modifier", McpHandlers::Audio::HandleAudioAuthoringAddMixModifier, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConfigureMixEq, "configure_mix_eq", McpHandlers::Audio::HandleAudioAuthoringConfigureMixEq, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateAttenuationSettings, "create_attenuation_settings", McpHandlers::Audio::HandleAudioAuthoringCreateAttenuationSettings, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConfigureDistanceAttenuation, "configure_distance_attenuation", McpHandlers::Audio::HandleAudioAuthoringConfigureDistanceAttenuation, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConfigureSpatialization, "configure_spatialization", McpHandlers::Audio::HandleAudioAuthoringConfigureSpatialization, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConfigureOcclusion, "configure_occlusion", McpHandlers::Audio::HandleAudioAuthoringConfigureOcclusion, EMcpCallFlags::Mutating)
MCP_AU_CALL(ConfigureReverbSend, "configure_reverb_send", McpHandlers::Audio::HandleAudioAuthoringConfigureReverbSend, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateDialogueVoice, "create_dialogue_voice", McpHandlers::Audio::HandleAudioAuthoringCreateDialogueVoice, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateDialogueWave, "create_dialogue_wave", McpHandlers::Audio::HandleAudioAuthoringCreateDialogueWave, EMcpCallFlags::Mutating)
MCP_AU_CALL(SetDialogueContext, "set_dialogue_context", McpHandlers::Audio::HandleAudioAuthoringSetDialogueContext, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateReverbEffect, "create_reverb_effect", McpHandlers::Audio::HandleAudioAuthoringCreateReverbEffect, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateSourceEffectChain, "create_source_effect_chain", McpHandlers::Audio::HandleAudioAuthoringCreateSourceEffectChain, EMcpCallFlags::Mutating)
MCP_AU_CALL(AddSourceEffect, "add_source_effect", McpHandlers::Audio::HandleAudioAuthoringAddSourceEffect, EMcpCallFlags::Mutating)
MCP_AU_CALL(CreateSubmixEffect, "create_submix_effect", McpHandlers::Audio::HandleAudioAuthoringCreateSubmixEffect, EMcpCallFlags::Mutating)
MCP_AU_CALL(GetAudioInfo, "get_info", McpHandlers::Audio::HandleAudioAuthoringGetAudioInfo, EMcpCallFlags::None)

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
