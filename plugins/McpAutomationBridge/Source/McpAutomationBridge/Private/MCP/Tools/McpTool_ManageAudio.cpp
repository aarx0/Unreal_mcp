// McpTool_ManageAudio.cpp — manage_audio tool definition

#include "McpVersionCompatibility.h"  // IWYU pragma: keep
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageAudio : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_audio"); }

	FString GetDescription() const override
	{
		return TEXT("Play/stop sounds, add audio components, configure mixes, "
			"attenuation, spatial audio, and author Sound Cues/MetaSounds.");
	}

	FString GetCategory() const override { return TEXT("utility"); }


	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ManageAudio(), TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("soundPath"), TEXT("Sound asset path."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Number(TEXT("volume"), TEXT(""))
			.Number(TEXT("pitch"), TEXT(""))
			.Number(TEXT("startTime"), TEXT(""))
			.String(TEXT("attenuationPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("concurrencyPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("mixName"), TEXT(""))
			.String(TEXT("mix"), TEXT("Alias of mixName (set_sound_mix_class_override/clear_sound_mix_class_override/set_base_sound_mix)."))
			.String(TEXT("soundClassName"), TEXT(""))
			.String(TEXT("soundClass"), TEXT("Alias of soundClassName (set_sound_mix_class_override/clear_sound_mix_class_override)."))
			.Number(TEXT("fadeInTime"), TEXT(""))
			.Number(TEXT("fadeOutTime"), TEXT(""))
			.Number(TEXT("fadeTime"), TEXT(""))
			.Number(TEXT("targetVolume"), TEXT(""))
			.Bool(TEXT("applyToChildren"), TEXT("set_sound_mix_class_override/add_mix_modifier: apply the override to child sound classes."))
			.ArrayOfObjects(TEXT("classAdjusters"), TEXT("create_sound_mix: per-class volume/pitch adjusters."),
				[](FMcpSchemaBuilder& S) {
				S.String(TEXT("soundClass"), TEXT("Sound class asset path."))
				 .Number(TEXT("volumeAdjuster"), TEXT("Volume multiplier."))
				 .Number(TEXT("pitchAdjuster"), TEXT("Pitch multiplier."));
			})
			.String(TEXT("attachPointName"), TEXT("Name of the socket."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("attachTo"), TEXT("create_audio_component: actor to attach the component to (alias of actorName)."))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.String(TEXT("parentClass"), TEXT(""))
			.String(TEXT("parentPath"), TEXT("set_class_parent: parent SoundClass asset path."))
			.FreeformObject(TEXT("properties"), TEXT(""))
			.Number(TEXT("innerRadius"), TEXT(""))
			.Number(TEXT("falloffDistance"), TEXT(""))
			.String(TEXT("attenuationShape"), TEXT(""))
			.String(TEXT("falloffMode"), TEXT(""))
			.String(TEXT("distanceAlgorithm"), TEXT("configure_distance_attenuation: linear|logarithmic|inverse|naturalsound."))
			.String(TEXT("reverbEffect"), TEXT(""))
			.Number(TEXT("density"), TEXT("create_reverb_effect: reverb density."))
			.Number(TEXT("diffusion"), TEXT("create_reverb_effect: reverb diffusion."))
			.Number(TEXT("gain"), TEXT("create_reverb_effect: overall gain."))
			.Number(TEXT("gainHF"), TEXT("create_reverb_effect: high-frequency gain."))
			.Number(TEXT("decayTime"), TEXT("create_reverb_effect: decay time in seconds."))
			.Number(TEXT("decayHFRatio"), TEXT("create_reverb_effect: high-frequency decay ratio."))
			.Object(TEXT("size"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("analysisType"), TEXT("Audio analysis type."))
			.Number(TEXT("windowSize"), TEXT("Audio analysis window size."))
			.String(TEXT("outputType"), TEXT(""))
			.String(TEXT("soundName"), TEXT(""))
			.String(TEXT("fadeType"), TEXT(""))
			.Number(TEXT("lowPassFilterFrequency"), TEXT(""))
			.Number(TEXT("lfeBleed"), TEXT("set_class_properties: LFE channel bleed amount."))
			.Number(TEXT("voiceCenterChannelVolume"), TEXT("set_class_properties: center-channel volume for dialogue."))
			.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
			.Bool(TEXT("enable"), TEXT("Whether the item/feature is enabled."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("packagePath"), TEXT("Directory path for asset creation (alias of path)."))
			.String(TEXT("savePath"), TEXT("Directory path for asset creation (alias of path)."))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation. Default true."))
			.String(TEXT("wavePath"), TEXT("Path to SoundWave asset."))
			.String(TEXT("nodeType"), TEXT(""))
			.String(TEXT("sourceNodeId"), TEXT("ID of the consuming (parent) node; pass 'Root' in connect_cue_nodes to wire the cue output."))
			.String(TEXT("targetNodeId"), TEXT("ID of the target (feeding child) node."))
			.Number(TEXT("childIndex"), TEXT("Input pin index on the source node for connect_cue_nodes."))
			.Bool(TEXT("looping"), TEXT("Whether to loop."))
			.Bool(TEXT("indefinite"), TEXT("add_cue_node (looping node): loop indefinitely."))
			.Number(TEXT("loopCount"), TEXT("add_cue_node (looping node): fixed loop count when not indefinite."))
			.Number(TEXT("delay"), TEXT("add_cue_node (delay node): delay in seconds."))
			.String(TEXT("nodeClassName"), TEXT("add_metasound_node: explicit 3-part MetaSound class name (Namespace.Name.Variant)."))
			.String(TEXT("inputName"), TEXT("Name of the input."))
			.String(TEXT("inputType"), TEXT(""))
			.String(TEXT("outputName"), TEXT("Name of the output."))
			.String(TEXT("sourcePin"), TEXT("Name of the source pin."))
			.String(TEXT("targetPin"), TEXT("Name of the target pin."))
			.FreeformObject(TEXT("defaultValue"), TEXT(""))
			.Number(TEXT("floatValue"), TEXT("set_metasound_default: float literal value."))
			.Integer(TEXT("intValue"), TEXT("set_metasound_default: int literal value."))
			.Bool(TEXT("boolValue"), TEXT("set_metasound_default: bool literal value."))
			.String(TEXT("stringValue"), TEXT("set_metasound_default: string literal value."))
			.String(TEXT("soundClassPath"), TEXT("Sound class path."))
			.Number(TEXT("dopplerIntensity"), TEXT("Doppler intensity."))
			.String(TEXT("effectType"), TEXT("Effect type."))
			.String(TEXT("effectPresetPath"), TEXT("add_source_effect: existing effect preset asset path."))
			.Bool(TEXT("bypass"), TEXT("add_source_effect: bypass the effect entry."))
			.Bool(TEXT("enableReverbSend"), TEXT("Whether reverb send is enabled."))
			.Bool(TEXT("enableOcclusion"), TEXT("configure_occlusion: enable occlusion."))
			.Number(TEXT("occlusionFilterScale"), TEXT("Occlusion filter scale."))
			.Number(TEXT("occlusionInterpolationTime"), TEXT("Occlusion interpolation time."))
			.Number(TEXT("occlusionLowPassFilterFrequency"), TEXT("configure_occlusion: low-pass filter frequency in Hz."))
			.Number(TEXT("occlusionVolumeAttenuation"), TEXT("configure_occlusion: volume attenuation."))
			.Number(TEXT("occlusionVolumeScale"), TEXT("Occlusion volume scale."))
			.Number(TEXT("reverbDistanceMax"), TEXT("Maximum reverb send distance."))
			.Number(TEXT("reverbDistanceMin"), TEXT("Minimum reverb send distance."))
			.Number(TEXT("reverbWetLevelMax"), TEXT("Maximum reverb wet level."))
			.Number(TEXT("reverbWetLevelMin"), TEXT("Minimum reverb wet level."))
			.String(TEXT("sourceOutputName"), TEXT("Source MetaSound output name."))
			.String(TEXT("spatialization"), TEXT("Spatialization algorithm: 'panner' or 'hrtf' (alias of spatializationAlgorithm)."))
			.String(TEXT("spatializationAlgorithm"), TEXT("Spatialization algorithm: 'panner' or 'hrtf'."))
			.Bool(TEXT("spatialize"), TEXT("Enable spatialization on the attenuation asset (alias of enabled)."))
			.String(TEXT("speakerPath"), TEXT("Dialogue speaker path."))
			.String(TEXT("soundWavePath"), TEXT("set_dialogue_context: SoundWave asset path for the context mapping."))
			.Array(TEXT("targetVoices"), TEXT("set_dialogue_context: target DialogueVoice asset paths."), TEXT("string"))
			.String(TEXT("localizationKeyFormat"), TEXT("set_dialogue_context: localization key format string."))
			.Bool(TEXT("replace"), TEXT("set_dialogue_context: replace an existing mapping for the same speaker."))
			.String(TEXT("gender"), TEXT("create_dialogue_voice: Masculine|Feminine|Neuter."))
			.String(TEXT("plurality"), TEXT("create_dialogue_voice: Singular|Plural."))
			.String(TEXT("spokenText"), TEXT("create_dialogue_wave: the line's spoken text."))
			.String(TEXT("targetInputName"), TEXT("Target MetaSound input name."))
			.Number(TEXT("velocityScale"), TEXT("Velocity scale."))
			.Number(TEXT("volumeAdjuster"), TEXT("Sound mix volume adjuster."))
			.Number(TEXT("pitchAdjuster"), TEXT("add_mix_modifier: pitch multiplier."))
			.Number(TEXT("eqPriority"), TEXT("configure_mix_eq: EQ effect priority."))
			.Bool(TEXT("applyEQ"), TEXT("configure_mix_eq: enable the mix's EQ effect."))
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
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageAudio);
