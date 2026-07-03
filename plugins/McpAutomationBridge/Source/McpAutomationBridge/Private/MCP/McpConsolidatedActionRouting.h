#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpConsolidatedActions
{
inline void AppendUniqueActions(TArray<FString>& Target, const TArray<FString>& Source)
{
	for (const FString& Action : Source)
	{
		Target.AddUnique(Action);
	}
}

inline FString GetPayloadSubAction(const TSharedPtr<FJsonObject>& Payload)
{
	FString SubAction;
	if (Payload.IsValid())
	{
		if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty())
		{
			Payload->TryGetStringField(TEXT("action"), SubAction);
		}
	}
	return SubAction;
}

inline TSharedPtr<FJsonObject> WithPayloadSubAction(const TSharedPtr<FJsonObject>& Payload, const FString& SubAction)
{
	if (!Payload.IsValid() || SubAction.IsEmpty())
	{
		return Payload;
	}

	FString ExistingSubAction;
	if (Payload->TryGetStringField(TEXT("subAction"), ExistingSubAction) && !ExistingSubAction.IsEmpty())
	{
		return Payload;
	}

	TSharedPtr<FJsonObject> RoutedPayload = MakeShared<FJsonObject>();
	RoutedPayload->Values = Payload->Values;
	RoutedPayload->SetStringField(TEXT("subAction"), SubAction);
	return RoutedPayload;
}

inline bool ContainsAction(const TArray<FString>& Actions, const FString& Action)
{
	return Actions.Contains(Action);
}

// Core lists name the actions owned by the tool's fallthrough handler. They must
// stay disjoint from the routed family lists below: the registration lambdas test
// families first, so a name in both silently shadows the core implementation.
// Startup validation (McpStartupValidation.cpp) enforces this at editor boot.
inline const TArray<FString>& ManageAssetCore()
{
	static const TArray<FString> Actions = {
		TEXT("list"), TEXT("import"), TEXT("duplicate"), TEXT("duplicate_asset"),
		TEXT("rename"), TEXT("rename_asset"), TEXT("move"), TEXT("move_asset"),
		TEXT("delete"), TEXT("delete_asset"), TEXT("delete_assets"),
		TEXT("create_folder"), TEXT("search_assets"), TEXT("get_dependencies"),
		TEXT("get_referencers"), TEXT("get_asset_properties"), TEXT("set_asset_property"),
		TEXT("get_source_control_state"), TEXT("analyze_graph"),
		TEXT("get_asset_graph"), TEXT("create_thumbnail"), TEXT("set_tags"),
		TEXT("get_metadata"), TEXT("set_metadata"), TEXT("validate"),
		TEXT("fixup_redirectors"), TEXT("find_by_tag"), TEXT("generate_report"),
		TEXT("create_data_table"), TEXT("add_data_table_row"),
		TEXT("edit_data_table_row"), TEXT("remove_data_table_row"),
		TEXT("get_data_table_rows"), TEXT("import_data_table"),
		TEXT("create_render_target"), TEXT("generate_lods"),
		TEXT("add_material_parameter"), TEXT("list_instances"),
		TEXT("reset_instance_parameters"), TEXT("exists"),
		TEXT("get_material_stats"), TEXT("nanite_rebuild_mesh"),
		TEXT("bulk_rename"), TEXT("bulk_delete"),
		TEXT("source_control_checkout"), TEXT("source_control_submit"),
		TEXT("save"), TEXT("save_asset"), TEXT("save_all")
	};
	return Actions;
}

inline const TArray<FString>& MaterialAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_material"), TEXT("set_blend_mode"),
		TEXT("set_shading_model"), TEXT("set_material_domain"),
		TEXT("add_texture_sample"), TEXT("add_texture_coordinate"),
		TEXT("add_scalar_parameter"), TEXT("add_vector_parameter"),
		TEXT("add_static_switch_parameter"), TEXT("add_math_node"),
		TEXT("add_world_position"), TEXT("add_vertex_normal"),
		TEXT("add_pixel_depth"), TEXT("add_fresnel"),
		TEXT("add_reflection_vector"), TEXT("add_panner"), TEXT("add_rotator"),
		TEXT("add_noise"), TEXT("add_voronoi"), TEXT("add_if"),
		TEXT("add_switch"), TEXT("add_custom_expression"),
		TEXT("connect_nodes"), TEXT("connect_material_pins"),
		TEXT("disconnect_nodes"), TEXT("break_material_connections"),
		TEXT("create_material_function"), TEXT("add_function_input"),
		TEXT("add_function_output"), TEXT("use_material_function"),
		TEXT("get_material_function_info"), TEXT("create_material_instance"),
		TEXT("set_scalar_parameter_value"), TEXT("set_vector_parameter_value"),
		TEXT("set_texture_parameter_value"), TEXT("create_landscape_material"),
		TEXT("create_decal_material"), TEXT("create_post_process_material"),
		TEXT("add_landscape_layer"), TEXT("configure_layer_blend"),
		TEXT("compile_material"), TEXT("get_material_info"), TEXT("find_node"),
		TEXT("get_node_connections"), TEXT("get_node_properties"),
		TEXT("set_node_value"),
		TEXT("set_static_switch_parameter_value"), TEXT("delete_node"),
		TEXT("update_custom_expression"), TEXT("get_node_chain"),
		TEXT("get_connected_subgraph"), TEXT("arrange_graph"),
		TEXT("add_material_node"),
		TEXT("rebuild_material"), TEXT("set_material_parameter"),
		TEXT("get_material_node_details"), TEXT("remove_material_node"),
		TEXT("set_two_sided")
	};
	return Actions;
}

inline const TArray<FString>& Texture()
{
	static const TArray<FString> Actions = {
		TEXT("create_noise_texture"), TEXT("create_gradient_texture"),
		TEXT("create_pattern_texture"), TEXT("create_normal_from_height"),
		TEXT("create_ao_from_mesh"), TEXT("resize_texture"),
		TEXT("adjust_levels"), TEXT("adjust_curves"), TEXT("blur"),
		TEXT("sharpen"), TEXT("invert"), TEXT("desaturate"),
		TEXT("channel_pack"), TEXT("channel_extract"), TEXT("combine_textures"),
		TEXT("set_compression_settings"), TEXT("set_texture_group"),
		TEXT("set_lod_bias"), TEXT("configure_virtual_texture"),
		TEXT("set_streaming_priority"), TEXT("get_texture_info")
	};
	return Actions;
}

inline TArray<FString> ManageAsset()
{
	TArray<FString> Actions = ManageAssetCore();
	AppendUniqueActions(Actions, MaterialAuthoring());
	AppendUniqueActions(Actions, Texture());
	return Actions;
}

inline const TArray<FString>& ManageBlueprintCore()
{
	static const TArray<FString> Actions = {
		TEXT("create"), TEXT("create_blueprint"), TEXT("get_blueprint"),
		TEXT("get"), TEXT("compile"), TEXT("add_component"),
		TEXT("set_default"), TEXT("get_default"), TEXT("blueprint_get_default"),
		TEXT("list_functions"), TEXT("blueprint_list_functions"),
		TEXT("modify_scs"), TEXT("get_scs"),
		TEXT("add_scs_component"),
		TEXT("remove_scs_component"), TEXT("reparent_scs_component"),
		TEXT("set_scs_transform"), TEXT("set_scs_property"),
		TEXT("ensure_exists"), TEXT("probe_handle"), TEXT("add_variable"),
		TEXT("remove_variable"), TEXT("rename_variable"), TEXT("add_function"),
		TEXT("remove_function"),
		TEXT("add_event"), TEXT("remove_event"),
		TEXT("add_construction_script"), TEXT("set_variable_metadata"),
		TEXT("set_metadata"), TEXT("add_node")
	};
	return Actions;
}

// Routed to HandleBlueprintGraphAction by the manage_blueprint registration lambda.
inline const TArray<FString>& BlueprintGraph()
{
	static const TArray<FString> Actions = {
		TEXT("create_node"), TEXT("delete_node"),
		TEXT("connect_pins"), TEXT("break_pin_links"),
		TEXT("set_node_property"), TEXT("create_reroute_node"),
		TEXT("get_node_details"), TEXT("get_graph_details"),
		TEXT("get_pin_details"), TEXT("list_node_types"),
		TEXT("set_pin_default_value"), TEXT("arrange_graph"),
		TEXT("list_animbp_graphs"), TEXT("get_transition_rule_graph")
	};
	return Actions;
}

inline const TArray<FString>& WidgetAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_widget_blueprint"), TEXT("set_widget_parent_class"),
		TEXT("add_canvas_panel"), TEXT("add_horizontal_box"),
		TEXT("add_vertical_box"), TEXT("add_overlay"), TEXT("add_grid_panel"),
		TEXT("add_uniform_grid"), TEXT("add_wrap_box"), TEXT("add_scroll_box"),
		TEXT("add_size_box"), TEXT("add_scale_box"), TEXT("add_border"),
		TEXT("add_text_block"), TEXT("add_rich_text_block"), TEXT("add_image"),
		TEXT("add_button"), TEXT("add_check_box"), TEXT("add_slider"),
		TEXT("add_progress_bar"), TEXT("add_text_input"),
		TEXT("add_combo_box"), TEXT("add_spin_box"), TEXT("add_list_view"),
		TEXT("add_tree_view"), TEXT("set_anchor"), TEXT("set_alignment"),
		TEXT("set_position"), TEXT("set_size"), TEXT("set_padding"),
		TEXT("set_z_order"), TEXT("set_render_transform"),
		TEXT("set_visibility"), TEXT("set_style"), TEXT("set_clipping"),
		TEXT("create_property_binding"), TEXT("bind_text"),
		TEXT("bind_visibility"), TEXT("bind_color"), TEXT("bind_enabled"),
		TEXT("bind_on_clicked"), TEXT("bind_on_hovered"),
		TEXT("bind_on_value_changed"), TEXT("bind_event_to_delegate"),
		TEXT("create_widget_animation"),
		TEXT("add_animation_track"), TEXT("add_animation_keyframe"),
		TEXT("set_animation_loop"), TEXT("create_main_menu"),
		TEXT("create_pause_menu"), TEXT("create_settings_menu"),
		TEXT("create_loading_screen"), TEXT("create_hud_widget"),
		TEXT("add_health_bar"), TEXT("add_ammo_counter"),
		TEXT("add_minimap"), TEXT("add_crosshair"), TEXT("add_compass"),
		TEXT("add_interaction_prompt"), TEXT("add_objective_tracker"),
		TEXT("add_damage_indicator"), TEXT("create_inventory_ui"),
		TEXT("create_dialog_widget"), TEXT("create_radial_menu"),
		TEXT("get_widget_info"), TEXT("preview_widget"),
		// Tree-mutation actions: handlers exist in WidgetAuthoringHandlers.cpp
		// but were never routed here, so they returned "Unknown blueprint action".
		TEXT("remove_widget"), TEXT("reparent_widget"),
		TEXT("rename_widget"), TEXT("get_widget_slot_info"),
		// In-place class swap (the designer's "Replace With"); preserves
		// name + slot + bindings + DesiredFocusWidget via FWidgetBlueprintEditorUtils.
		TEXT("replace_widget_class"),
		// Generic add (any UWidget subclass, incl. ones without a typed add_*
		// action) + root wrapper (panel becomes root, old root its first child).
		TEXT("add_widget"), TEXT("wrap_root"),
		// Handlers existed in WidgetAuthoringHandlers.cpp but were never routed
		// here, so manage_blueprint returned "Unknown blueprint action" for them.
		TEXT("create_widget"), TEXT("show_widget"), TEXT("add_widget_component"),
		TEXT("add_safe_zone"), TEXT("add_spacer"), TEXT("add_widget_switcher"),
		TEXT("set_font"), TEXT("set_margin"),
		TEXT("create_widget_style"), TEXT("apply_style_to_widget"),
		TEXT("set_widget_binding"), TEXT("bind_localized_text"),
		TEXT("set_localization_key"),
		TEXT("get_animation_info"), TEXT("set_animation_speed"),
		TEXT("delete_animation"),
		TEXT("create_credits_screen"), TEXT("create_shop_ui"),
		TEXT("add_quest_tracker")
	};
	return Actions;
}

inline const TArray<FString>& CommonUi()
{
	static const TArray<FString> Actions = {
		TEXT("add_common_button"), TEXT("add_common_text"),
		TEXT("add_common_border"), TEXT("set_common_button_style"),
		TEXT("set_common_text_style"), TEXT("set_common_button_input_action"),
		TEXT("create_common_button_style"), TEXT("create_common_text_style")
	};
	return Actions;
}

inline TArray<FString> ManageBlueprint()
{
	TArray<FString> Actions = ManageBlueprintCore();
	AppendUniqueActions(Actions, BlueprintGraph());
	AppendUniqueActions(Actions, WidgetAuthoring());
	AppendUniqueActions(Actions, CommonUi());
	return Actions;
}

inline const TArray<FString>& BuildEnvironmentCore()
{
	static const TArray<FString> Actions = {
		TEXT("create_landscape"), TEXT("sculpt"), TEXT("sculpt_landscape"),
		TEXT("add_foliage"), TEXT("paint_foliage"),
		TEXT("create_procedural_terrain"),
		TEXT("create_procedural_foliage"),
		TEXT("add_foliage_instances"), TEXT("get_foliage_instances"),
		TEXT("remove_foliage"), TEXT("paint_landscape"),
		TEXT("paint_landscape_layer"), TEXT("modify_heightmap"),
		TEXT("set_landscape_material"), TEXT("create_landscape_grass_type"),
		TEXT("generate_lods"), TEXT("bake_lightmap"),
		TEXT("export_snapshot"), TEXT("import_snapshot"), TEXT("delete"),
		TEXT("create_sky_sphere"), TEXT("set_time_of_day"),
		TEXT("create_fog_volume")
	};
	return Actions;
}

inline const TArray<FString>& Lighting()
{
	static const TArray<FString> Actions = {
		TEXT("spawn_light"), TEXT("create_light"), TEXT("spawn_sky_light"),
		TEXT("create_sky_light"), TEXT("ensure_single_sky_light"),
		TEXT("create_lightmass_volume"),
		TEXT("create_lighting_enabled_level"), TEXT("create_dynamic_light"),
		TEXT("setup_global_illumination"), TEXT("configure_shadows"),
		TEXT("set_exposure"), TEXT("set_ambient_occlusion"),
		TEXT("setup_volumetric_fog"), TEXT("build_lighting"),
		TEXT("list_light_types")
	};
	return Actions;
}

inline const TArray<FString>& Splines()
{
	static const TArray<FString> Actions = {
		TEXT("create_spline_actor"), TEXT("add_spline_point"),
		TEXT("remove_spline_point"), TEXT("set_spline_point_position"),
		TEXT("set_spline_point_tangents"), TEXT("set_spline_point_rotation"),
		TEXT("set_spline_point_scale"), TEXT("set_spline_type"),
		TEXT("create_spline_mesh_component"), TEXT("set_spline_mesh_asset"),
		TEXT("configure_spline_mesh_axis"),
		TEXT("set_spline_mesh_material"),
		TEXT("scatter_meshes_along_spline"),
		TEXT("configure_mesh_spacing"),
		TEXT("configure_mesh_randomization"), TEXT("create_road_spline"),
		TEXT("create_river_spline"), TEXT("create_fence_spline"),
		TEXT("create_wall_spline"), TEXT("create_cable_spline"),
		TEXT("create_pipe_spline"), TEXT("get_splines_info")
	};
	return Actions;
}

inline TArray<FString> BuildEnvironment()
{
	TArray<FString> Actions = BuildEnvironmentCore();
	AppendUniqueActions(Actions, Lighting());
	AppendUniqueActions(Actions, Splines());
	return Actions;
}

inline const TArray<FString>& AnimationPhysicsCore()
{
	static const TArray<FString> Actions = {
		TEXT("create_blend_space"), TEXT("create_blend_tree"),
		TEXT("create_procedural_anim"), TEXT("create_state_machine"),
		TEXT("setup_ik"), TEXT("create_pose_library"),
		TEXT("create_animation_asset"), TEXT("play_montage"),
		TEXT("play_anim_montage"), TEXT("setup_ragdoll"),
		TEXT("activate_ragdoll"), TEXT("configure_vehicle"),
		TEXT("setup_physics_simulation"), TEXT("setup_retargeting"),
		TEXT("cleanup")
	};
	return Actions;
}

inline const TArray<FString>& AnimationAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_animation_sequence"), TEXT("set_sequence_length"),
		TEXT("add_bone_track"), TEXT("set_bone_key"), TEXT("set_curve_key"),
		TEXT("add_notify"), TEXT("add_notify_state"), TEXT("add_sync_marker"),
		TEXT("set_root_motion_settings"), TEXT("set_additive_settings"),
		TEXT("create_montage"), TEXT("add_montage_section"),
		TEXT("add_montage_slot"), TEXT("set_section_timing"),
		TEXT("add_montage_notify"), TEXT("bind_anim_notify"), TEXT("set_blend_in"),
		TEXT("set_blend_out"), TEXT("link_sections"),
		TEXT("create_blend_space_1d"), TEXT("create_blend_space_2d"),
		TEXT("add_blend_sample"), TEXT("force_rebuild_blend_space"),
		TEXT("set_axis_settings"), TEXT("set_interpolation_settings"),
		TEXT("create_aim_offset"), TEXT("add_aim_offset_sample"),
		TEXT("create_anim_blueprint"), TEXT("create_animation_bp"),
		TEXT("create_animation_blueprint"), TEXT("add_state_machine"),
		TEXT("add_state"), TEXT("add_transition"),
		TEXT("set_transition_rules"), TEXT("add_blend_node"),
		TEXT("add_cached_pose"), TEXT("add_slot_node"),
		TEXT("add_layered_blend_per_bone"),
		TEXT("set_anim_graph_node_value"), TEXT("create_control_rig"),
		TEXT("create_ik_rig"), TEXT("create_ik_retargeter"),
		TEXT("set_retarget_chain_mapping"), TEXT("get_animation_info")
	};
	return Actions;
}

inline const TArray<FString>& Skeleton()
{
	static const TArray<FString> Actions = {
		TEXT("create_skeleton"), TEXT("add_bone"), TEXT("remove_bone"),
		TEXT("rename_bone"), TEXT("set_bone_transform"),
		TEXT("set_bone_parent"), TEXT("create_virtual_bone"),
		TEXT("create_socket"), TEXT("configure_socket"),
		TEXT("auto_skin_weights"), TEXT("set_vertex_weights"),
		TEXT("normalize_weights"), TEXT("prune_weights"), TEXT("copy_weights"),
		TEXT("mirror_weights"), TEXT("create_physics_asset"),
		TEXT("add_physics_body"), TEXT("configure_physics_body"),
		TEXT("add_physics_constraint"), TEXT("configure_constraint_limits"),
		TEXT("bind_cloth_to_skeletal_mesh"),
		TEXT("assign_cloth_asset_to_mesh"), TEXT("create_morph_target"),
		TEXT("set_morph_target_deltas"), TEXT("import_morph_targets"),
		TEXT("get_skeleton_info"), TEXT("list_bones"), TEXT("list_sockets"),
		TEXT("list_physics_bodies")
	};
	return Actions;
}

inline TArray<FString> AnimationPhysics()
{
	TArray<FString> Actions = AnimationPhysicsCore();
	AppendUniqueActions(Actions, AnimationAuthoring());
	AppendUniqueActions(Actions, Skeleton());
	return Actions;
}

inline const TArray<FString>& AudioAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("add_cue_node"), TEXT("connect_cue_nodes"),
		TEXT("set_cue_attenuation"), TEXT("set_cue_concurrency"),
		TEXT("create_metasound"), TEXT("add_metasound_node"),
		TEXT("connect_metasound_nodes"), TEXT("add_metasound_input"),
		TEXT("add_metasound_output"), TEXT("set_metasound_default"),
		TEXT("set_class_properties"), TEXT("set_class_parent"),
		TEXT("add_mix_modifier"), TEXT("configure_mix_eq"),
		TEXT("create_attenuation_settings"),
		TEXT("configure_distance_attenuation"),
		TEXT("configure_spatialization"), TEXT("configure_occlusion"),
		TEXT("configure_reverb_send"), TEXT("create_dialogue_voice"),
		TEXT("create_dialogue_wave"), TEXT("set_dialogue_context"),
		TEXT("create_reverb_effect"), TEXT("create_source_effect_chain"),
		TEXT("add_source_effect"), TEXT("create_submix_effect"),
		TEXT("get_audio_info")
	};
	return Actions;
}

inline const TArray<FString>& ManageAudioCore()
{
	static const TArray<FString> Actions = {
		TEXT("create_sound_cue"), TEXT("play_sound_at_location"),
		TEXT("play_sound_2d"), TEXT("create_audio_component"),
		TEXT("create_sound_mix"), TEXT("push_sound_mix"),
		TEXT("pop_sound_mix"), TEXT("set_sound_mix_class_override"),
		TEXT("clear_sound_mix_class_override"), TEXT("set_base_sound_mix"),
		TEXT("prime_sound"), TEXT("play_sound_attached"),
		TEXT("spawn_sound_at_location"), TEXT("fade_sound_in"),
		TEXT("fade_sound_out"), TEXT("create_ambient_sound"),
		TEXT("create_sound_class"), TEXT("set_sound_attenuation"),
		TEXT("create_reverb_zone"), TEXT("enable_audio_analysis"),
		TEXT("fade_sound"), TEXT("set_doppler_effect"),
		TEXT("set_audio_occlusion")
	};
	return Actions;
}

inline TArray<FString> ManageAudio()
{
	TArray<FString> Actions = ManageAudioCore();
	AppendUniqueActions(Actions, AudioAuthoring());
	return Actions;
}

inline const TArray<FString>& SystemControlCore()
{
	// Truth in advertising (2026-06-11 drift sweep): removed actions that had
	// NO handler anywhere — profile, set_quality, set_resolution,
	// set_fullscreen, play_sound, show_widget, validate_assets. They died at
	// HandleUiAction's NOT_IMPLEMENTED catch-all; re-add only together with a
	// real implementation. console_command/execute_command/set_cvar/
	// subscribe/unsubscribe/spawn_category/lumen_update_scene are now routed
	// in the system_control registry lambda (McpAutomationBridgeSubsystem.cpp).
	static const TArray<FString> Actions = {
		TEXT("screenshot"),
		TEXT("execute_command"), TEXT("console_command"), TEXT("run_ubt"),
		TEXT("run_tests"), TEXT("list_tests"), TEXT("get_test_results"),
		TEXT("generate_test_stub"),
		TEXT("subscribe"), TEXT("unsubscribe"),
		TEXT("get_log"), TEXT("tail_log"), TEXT("clear_log"),
		TEXT("spawn_category"), TEXT("start_session"),
		TEXT("lumen_update_scene"), TEXT("create_widget"),
		TEXT("add_widget_child"), TEXT("set_cvar"),
		TEXT("get_project_settings"),
		TEXT("set_project_setting"), TEXT("execute_python"),
		TEXT("live_coding_compile")
	};
	return Actions;
}

inline const TArray<FString>& Performance()
{
	static const TArray<FString> Actions = {
		TEXT("start_profiling"), TEXT("stop_profiling"),
		TEXT("get_profile"), TEXT("capture_stats"), TEXT("get_perf_stats"),
		TEXT("run_benchmark"), TEXT("show_fps"), TEXT("show_stats"),
		TEXT("generate_memory_report"), TEXT("set_scalability"),
		TEXT("set_resolution_scale"), TEXT("set_vsync"),
		TEXT("set_frame_rate_limit"), TEXT("enable_gpu_timing"),
		TEXT("configure_texture_streaming"), TEXT("configure_lod"),
		TEXT("apply_baseline_settings"), TEXT("optimize_draw_calls"),
		TEXT("merge_actors"), TEXT("configure_occlusion_culling"),
		TEXT("optimize_shaders"), TEXT("configure_nanite"),
		TEXT("configure_world_partition")
	};
	return Actions;
}

inline TArray<FString> SystemControl()
{
	TArray<FString> Actions = SystemControlCore();
	AppendUniqueActions(Actions, Performance());
	return Actions;
}

inline const TArray<FString>& ManageNetworkingCore()
{
	static const TArray<FString> Actions = {
		TEXT("set_property_replicated"), TEXT("set_replication_condition"),
		TEXT("configure_net_update_frequency"), TEXT("configure_net_priority"),
		TEXT("set_net_dormancy"), TEXT("configure_replication_graph"),
		TEXT("create_rpc_function"), TEXT("configure_rpc_validation"),
		TEXT("set_rpc_reliability"), TEXT("set_owner"),
		TEXT("set_autonomous_proxy"), TEXT("check_has_authority"),
		TEXT("check_is_locally_controlled"),
		TEXT("configure_net_cull_distance"), TEXT("set_always_relevant"),
		TEXT("set_only_relevant_to_owner"),
		TEXT("configure_net_serialization"), TEXT("set_replicated_using"),
		TEXT("configure_push_model"), TEXT("configure_client_prediction"),
		TEXT("configure_server_correction"),
		TEXT("add_network_prediction_data"),
		TEXT("configure_movement_prediction"), TEXT("configure_net_driver"),
		TEXT("set_net_role"), TEXT("configure_replicated_movement"),
		TEXT("get_networking_info")
	};
	return Actions;
}

inline const TArray<FString>& Input()
{
	static const TArray<FString> Actions = {
		TEXT("create_input_action"), TEXT("create_input_mapping_context"),
		TEXT("add_mapping"), TEXT("remove_mapping"), TEXT("map_input_action"),
		TEXT("set_input_trigger"), TEXT("set_input_modifier"),
		TEXT("enable_input_mapping"), TEXT("disable_input_action"),
		TEXT("get_input_info")
	};
	return Actions;
}

inline const TArray<FString>& GameFramework()
{
	static const TArray<FString> Actions = {
		TEXT("create_game_mode"), TEXT("create_game_state"),
		TEXT("create_player_controller"), TEXT("create_player_state"),
		TEXT("create_game_instance"), TEXT("create_hud_class"),
		TEXT("set_default_pawn_class"), TEXT("set_player_controller_class"),
		TEXT("set_game_state_class"), TEXT("set_player_state_class"),
		TEXT("configure_game_rules"), TEXT("setup_match_states"),
		TEXT("configure_round_system"), TEXT("configure_team_system"),
		TEXT("configure_scoring_system"), TEXT("configure_spawn_system"),
		TEXT("configure_player_start"), TEXT("set_respawn_rules"),
		TEXT("configure_spectating"), TEXT("get_game_framework_info")
	};
	return Actions;
}

inline const TArray<FString>& Sessions()
{
	static const TArray<FString> Actions = {
		TEXT("configure_local_session_settings"),
		TEXT("configure_session_interface"), TEXT("configure_split_screen"),
		TEXT("set_split_screen_type"), TEXT("add_local_player"),
		TEXT("remove_local_player"), TEXT("configure_lan_play"),
		TEXT("host_lan_server"), TEXT("join_lan_server"),
		TEXT("enable_voice_chat"), TEXT("configure_voice_settings"),
		TEXT("set_voice_channel"), TEXT("mute_player"),
		TEXT("set_voice_attenuation"), TEXT("configure_push_to_talk"),
		TEXT("get_sessions_info")
	};
	return Actions;
}

inline TArray<FString> ManageNetworking()
{
	TArray<FString> Actions = ManageNetworkingCore();
	AppendUniqueActions(Actions, Input());
	AppendUniqueActions(Actions, GameFramework());
	AppendUniqueActions(Actions, Sessions());
	return Actions;
}

inline const TArray<FString>& ManageLevelStructureCore()
{
	static const TArray<FString> Actions = {
		TEXT("create_level"), TEXT("create_sublevel"),
		TEXT("configure_level_streaming"), TEXT("set_streaming_distance"),
		TEXT("configure_level_bounds"), TEXT("enable_world_partition"),
		TEXT("configure_grid_size"), TEXT("create_data_layer"),
		TEXT("assign_actor_to_data_layer"), TEXT("configure_hlod_layer"),
		TEXT("create_minimap_volume"), TEXT("open_level_blueprint"),
		TEXT("add_level_blueprint_node"),
		TEXT("connect_level_blueprint_nodes"), TEXT("create_level_instance"),
		TEXT("create_packed_level_actor"), TEXT("get_level_structure_info")
	};
	return Actions;
}

inline const TArray<FString>& Volumes()
{
	static const TArray<FString> Actions = {
		TEXT("create_trigger_volume"), TEXT("add_trigger_volume"),
		TEXT("create_trigger_box"), TEXT("create_trigger_sphere"),
		TEXT("create_trigger_capsule"), TEXT("create_blocking_volume"),
		TEXT("add_blocking_volume"), TEXT("create_kill_z_volume"),
		TEXT("add_kill_z_volume"), TEXT("create_pain_causing_volume"),
		TEXT("create_physics_volume"), TEXT("add_physics_volume"),
		TEXT("create_audio_volume"), TEXT("create_reverb_volume"),
		TEXT("create_cull_distance_volume"),
		TEXT("add_cull_distance_volume"),
		TEXT("create_precomputed_visibility_volume"),
		TEXT("create_lightmass_importance_volume"),
		TEXT("create_nav_mesh_bounds_volume"),
		TEXT("create_nav_modifier_volume"),
		TEXT("create_camera_blocking_volume"),
		TEXT("create_post_process_volume"),
		TEXT("add_post_process_volume"), TEXT("set_volume_extent"),
		TEXT("set_volume_bounds"), TEXT("set_volume_properties"),
		TEXT("remove_volume"), TEXT("get_volumes_info")
	};
	return Actions;
}

inline TArray<FString> ManageLevelStructure()
{
	TArray<FString> Actions = ManageLevelStructureCore();
	AppendUniqueActions(Actions, Volumes());
	return Actions;
}

inline const TArray<FString>& ManageAICore()
{
	static const TArray<FString> Actions = {
		TEXT("create_ai_controller"), TEXT("assign_behavior_tree"),
		TEXT("assign_blackboard"), TEXT("create_blackboard_asset"),
		TEXT("add_blackboard_key"), TEXT("set_key_instance_synced"),
		TEXT("create_behavior_tree"), TEXT("add_composite_node"),
		TEXT("add_task_node"), TEXT("add_decorator"), TEXT("add_service"),
		TEXT("configure_bt_node"), TEXT("create_eqs_query"),
		TEXT("add_eqs_generator"), TEXT("add_eqs_context"),
		TEXT("add_eqs_test"), TEXT("configure_test_scoring"),
		TEXT("add_ai_perception_component"), TEXT("configure_sight_config"),
		TEXT("configure_hearing_config"),
		TEXT("configure_damage_sense_config"), TEXT("set_perception_team"),
		TEXT("create_state_tree"), TEXT("add_state_tree_state"),
		TEXT("add_state_tree_transition"), TEXT("configure_state_tree_task"),
		TEXT("create_smart_object_definition"),
		TEXT("add_smart_object_slot"), TEXT("configure_slot_behavior"),
		TEXT("add_smart_object_component"),
		TEXT("create_mass_entity_config"), TEXT("configure_mass_entity"),
		TEXT("add_mass_spawner"), TEXT("get_ai_info"),
		TEXT("create_blackboard"), TEXT("setup_perception"),
		TEXT("create_nav_link_proxy"), TEXT("set_focus"), TEXT("clear_focus"),
		TEXT("set_blackboard_value"), TEXT("get_blackboard_value"),
		TEXT("run_behavior_tree"), TEXT("stop_behavior_tree"),
		TEXT("create"), TEXT("add_node"), TEXT("connect_nodes"),
		TEXT("remove_node"), TEXT("break_connections"),
		TEXT("set_node_properties"), TEXT("configure_nav_mesh_settings"),
		TEXT("set_nav_agent_properties"), TEXT("rebuild_navigation"),
		TEXT("create_nav_modifier_component"), TEXT("set_nav_area_class"),
		TEXT("configure_nav_area_cost"), TEXT("configure_nav_link"),
		TEXT("set_nav_link_type"), TEXT("create_smart_link"),
		TEXT("configure_smart_link_behavior"), TEXT("get_navigation_info")
	};
	return Actions;
}

inline const TArray<FString>& BehaviorTree()
{
	static const TArray<FString> Actions = {
		TEXT("create"), TEXT("add_node"), TEXT("connect_nodes"),
		TEXT("remove_node"), TEXT("break_connections"),
		TEXT("set_node_properties"), TEXT("add_subnode")
	};
	return Actions;
}

inline const TArray<FString>& Navigation()
{
	static const TArray<FString> Actions = {
		TEXT("configure_nav_mesh_settings"),
		TEXT("set_nav_agent_properties"), TEXT("rebuild_navigation"),
		TEXT("create_nav_modifier_component"), TEXT("set_nav_area_class"),
		TEXT("configure_nav_area_cost"), TEXT("create_nav_link_proxy"),
		TEXT("configure_nav_link"), TEXT("set_nav_link_type"),
		TEXT("create_smart_link"), TEXT("configure_smart_link_behavior"),
		TEXT("get_navigation_info")
	};
	return Actions;
}

inline TArray<FString> ManageAI()
{
	TArray<FString> Actions = ManageAICore();
	AppendUniqueActions(Actions, BehaviorTree());
	AppendUniqueActions(Actions, Navigation());
	return Actions;
}

// Single-handler tools: no routed families, so the whole enum is the core list.
// The *Union wrappers exist only because FMcpToolRouting::SchemaUnion returns by
// value while these lists return by reference.
inline const TArray<FString>& ControlActor()
{
	static const TArray<FString> Actions = {
		TEXT("spawn"), TEXT("spawn_actor"), TEXT("spawn_blueprint"),
		TEXT("delete"), TEXT("destroy_actor"), TEXT("delete_by_tag"),
		TEXT("duplicate"), TEXT("apply_force"), TEXT("set_transform"),
		TEXT("teleport_actor"), TEXT("set_actor_location"),
		TEXT("set_actor_rotation"), TEXT("set_actor_scale"),
		TEXT("set_actor_transform"), TEXT("get_transform"),
		TEXT("get_actor_transform"), TEXT("set_visibility"),
		TEXT("set_actor_visible"), TEXT("add_component"),
		TEXT("remove_component"), TEXT("set_component_properties"),
		TEXT("set_component_property"), TEXT("get_component_property"),
		TEXT("get_components"), TEXT("get_actor_components"),
		TEXT("get_actor_bounds"), TEXT("add_tag"), TEXT("remove_tag"),
		TEXT("find_by_tag"), TEXT("find_actors_by_tag"), TEXT("find_by_name"),
		TEXT("find_actors_by_name"), TEXT("find_by_class"),
		TEXT("find_actors_by_class"), TEXT("list"),
		TEXT("set_blueprint_variables"), TEXT("create_snapshot"),
		TEXT("attach"), TEXT("attach_actor"), TEXT("detach"),
		TEXT("detach_actor"), TEXT("set_actor_collision"),
		TEXT("call_actor_function")
	};
	return Actions;
}
inline TArray<FString> ControlActorUnion() { return ControlActor(); }

inline const TArray<FString>& ControlEditor()
{
	static const TArray<FString> Actions = {
		TEXT("play"), TEXT("stop"), TEXT("stop_pie"), TEXT("pause"),
		TEXT("resume"), TEXT("eject"), TEXT("possess"),
		TEXT("set_game_speed"), TEXT("set_fixed_delta_time"),
		TEXT("set_camera"), TEXT("set_camera_position"),
		TEXT("set_viewport_camera"), TEXT("set_camera_fov"),
		TEXT("set_view_mode"), TEXT("set_viewport_resolution"),
		TEXT("console_command"), TEXT("execute_command"),
		TEXT("screenshot"), TEXT("take_screenshot"), TEXT("step_frame"),
		TEXT("single_frame_step"), TEXT("start_recording"),
		TEXT("stop_recording"), TEXT("create_bookmark"),
		TEXT("jump_to_bookmark"), TEXT("set_preferences"),
		TEXT("set_viewport_realtime"), TEXT("open_asset"),
		TEXT("close_asset"), TEXT("simulate_input"), TEXT("simulate_nav"),
		TEXT("open_level"), TEXT("focus_actor"), TEXT("show_stats"),
		TEXT("hide_stats"), TEXT("set_editor_mode"),
		TEXT("set_immersive_mode"), TEXT("set_game_view"), TEXT("undo"),
		TEXT("redo"), TEXT("save_all")
	};
	return Actions;
}
inline TArray<FString> ControlEditorUnion() { return ControlEditor(); }

inline const TArray<FString>& Inspect()
{
	static const TArray<FString> Actions = {
		TEXT("inspect_object"), TEXT("get_actor_details"),
		TEXT("get_blueprint_details"), TEXT("get_mesh_details"),
		TEXT("get_texture_details"), TEXT("get_material_details"),
		TEXT("get_level_details"), TEXT("get_component_details"),
		TEXT("set_property"), TEXT("get_property"), TEXT("get_components"),
		TEXT("get_component_property"), TEXT("set_component_property"),
		TEXT("inspect_class"), TEXT("inspect_cdo"), TEXT("diff_asset"),
		TEXT("runtime_report"), TEXT("pie_report"), TEXT("ui_focus"),
		TEXT("find_objects"), TEXT("list_objects"), TEXT("get_metadata"),
		TEXT("add_tag"), TEXT("find_by_tag"), TEXT("create_snapshot"),
		TEXT("restore_snapshot"), TEXT("export"), TEXT("delete_object"),
		TEXT("find_by_class"), TEXT("get_bounding_box"),
		TEXT("get_project_settings"), TEXT("get_world_settings"),
		TEXT("get_viewport_info"), TEXT("get_selected_actors"),
		TEXT("get_scene_stats"), TEXT("get_performance_stats"),
		TEXT("get_memory_stats"), TEXT("get_editor_settings")
	};
	return Actions;
}
inline TArray<FString> InspectUnion() { return Inspect(); }

inline const TArray<FString>& ManageLevel()
{
	static const TArray<FString> Actions = {
		TEXT("load"), TEXT("load_level"), TEXT("save"), TEXT("save_level"),
		TEXT("save_as"), TEXT("save_level_as"), TEXT("stream"),
		TEXT("unload"), TEXT("unload_level"), TEXT("create_level"),
		TEXT("create_light"), TEXT("build_lighting"), TEXT("set_metadata"),
		TEXT("export_level"), TEXT("import_level"), TEXT("list_levels"),
		TEXT("get_summary"), TEXT("delete"), TEXT("delete_level"),
		TEXT("validate_level"), TEXT("add_sublevel"), TEXT("rename_level"),
		TEXT("duplicate_level"), TEXT("get_current_level")
	};
	return Actions;
}
inline TArray<FString> ManageLevelUnion() { return ManageLevel(); }

inline const TArray<FString>& ManageSequence()
{
	static const TArray<FString> Actions = {
		TEXT("create"), TEXT("open"), TEXT("add_camera"), TEXT("add_actor"),
		TEXT("add_actors"), TEXT("remove_actors"), TEXT("get_bindings"),
		TEXT("play"), TEXT("pause"), TEXT("stop"),
		TEXT("set_playback_speed"), TEXT("add_keyframe"),
		TEXT("get_properties"), TEXT("set_properties"), TEXT("duplicate"),
		TEXT("rename"), TEXT("delete"), TEXT("list"), TEXT("get_metadata"),
		TEXT("set_metadata"), TEXT("add_spawnable_from_class"),
		TEXT("add_track"), TEXT("add_section"), TEXT("set_display_rate"),
		TEXT("set_tick_resolution"), TEXT("set_work_range"),
		TEXT("set_view_range"), TEXT("set_track_muted"),
		TEXT("set_track_solo"), TEXT("set_track_locked"),
		TEXT("list_tracks"), TEXT("remove_track"), TEXT("list_track_types")
	};
	return Actions;
}
inline TArray<FString> ManageSequenceUnion() { return ManageSequence(); }

inline const TArray<FString>& ManageGeometry()
{
	static const TArray<FString> Actions = {
		TEXT("create_box"), TEXT("create_sphere"), TEXT("create_cylinder"),
		TEXT("create_cone"), TEXT("create_capsule"), TEXT("create_torus"),
		TEXT("create_plane"), TEXT("create_disc"), TEXT("create_stairs"),
		TEXT("create_spiral_stairs"), TEXT("create_ring"),
		TEXT("create_arch"), TEXT("create_pipe"), TEXT("create_ramp"),
		TEXT("boolean_union"), TEXT("boolean_subtract"),
		TEXT("boolean_intersection"), TEXT("boolean_trim"),
		TEXT("self_union"), TEXT("extrude"), TEXT("inset"), TEXT("outset"),
		TEXT("bevel"), TEXT("offset_faces"), TEXT("shell"), TEXT("revolve"),
		TEXT("chamfer"), TEXT("extrude_along_spline"), TEXT("bridge"),
		TEXT("loft"), TEXT("sweep"), TEXT("duplicate_along_spline"),
		TEXT("loop_cut"), TEXT("edge_split"), TEXT("quadrangulate"),
		TEXT("bend"), TEXT("twist"), TEXT("taper"), TEXT("noise_deform"),
		TEXT("smooth"), TEXT("relax"), TEXT("stretch"), TEXT("spherify"),
		TEXT("cylindrify"), TEXT("lattice_deform"),
		TEXT("displace_by_texture"), TEXT("triangulate"), TEXT("poke"),
		TEXT("mirror"), TEXT("array_linear"), TEXT("array_radial"),
		TEXT("simplify_mesh"), TEXT("subdivide"), TEXT("remesh_uniform"),
		TEXT("merge_vertices"), TEXT("remesh_voxel"), TEXT("weld_vertices"),
		TEXT("fill_holes"), TEXT("remove_degenerates"), TEXT("auto_uv"),
		TEXT("project_uv"), TEXT("transform_uvs"), TEXT("unwrap_uv"),
		TEXT("pack_uv_islands"), TEXT("recalculate_normals"),
		TEXT("flip_normals"), TEXT("recompute_tangents"),
		TEXT("generate_collision"), TEXT("generate_complex_collision"),
		TEXT("simplify_collision"), TEXT("generate_lods"),
		TEXT("set_lod_settings"), TEXT("set_lod_screen_sizes"),
		TEXT("convert_to_nanite"), TEXT("convert_to_static_mesh"),
		TEXT("get_mesh_info")
	};
	return Actions;
}
inline TArray<FString> ManageGeometryUnion() { return ManageGeometry(); }

inline const TArray<FString>& ManageEffect()
{
	static const TArray<FString> Actions = {
		TEXT("particle"), TEXT("niagara"), TEXT("debug_shape"),
		TEXT("spawn_niagara"), TEXT("create_dynamic_light"),
		TEXT("create_niagara_system"), TEXT("create_niagara_emitter"),
		TEXT("create_volumetric_fog"), TEXT("create_particle_trail"),
		TEXT("create_environment_effect"), TEXT("create_impact_effect"),
		TEXT("create_niagara_ribbon"), TEXT("activate"),
		TEXT("activate_effect"), TEXT("activate_niagara"),
		TEXT("deactivate"), TEXT("deactivate_niagara"),
		// "reset" removed (2026-06-11): never had a handler — it is a
		// bool payload FIELD of activate_niagara, not an action.
		TEXT("advance_simulation"), TEXT("add_niagara_module"),
		TEXT("connect_niagara_pins"), TEXT("remove_niagara_node"),
		TEXT("set_niagara_parameter"), TEXT("clear_debug_shapes"),
		TEXT("cleanup"), TEXT("list_debug_shapes"),
		TEXT("add_emitter_to_system"), TEXT("set_emitter_properties"),
		TEXT("add_spawn_rate_module"), TEXT("add_spawn_burst_module"),
		TEXT("add_spawn_per_unit_module"),
		TEXT("add_initialize_particle_module"),
		TEXT("add_particle_state_module"), TEXT("add_force_module"),
		TEXT("add_velocity_module"), TEXT("add_acceleration_module"),
		TEXT("add_size_module"), TEXT("add_color_module"),
		TEXT("add_sprite_renderer_module"), TEXT("add_mesh_renderer_module"),
		TEXT("add_ribbon_renderer_module"),
		TEXT("add_light_renderer_module"), TEXT("add_collision_module"),
		TEXT("add_kill_particles_module"), TEXT("add_camera_offset_module"),
		TEXT("add_user_parameter"), TEXT("set_parameter_value"),
		TEXT("bind_parameter_to_source"),
		TEXT("add_skeletal_mesh_data_interface"),
		TEXT("add_static_mesh_data_interface"),
		TEXT("add_spline_data_interface"),
		TEXT("add_audio_spectrum_data_interface"),
		TEXT("add_collision_query_data_interface"),
		TEXT("add_event_generator"), TEXT("add_event_receiver"),
		TEXT("configure_event_payload"), TEXT("enable_gpu_simulation"),
		TEXT("add_simulation_stage"), TEXT("get_niagara_info"),
		TEXT("validate_niagara_system")
	};
	return Actions;
}
inline TArray<FString> ManageEffectUnion() { return ManageEffect(); }

inline const TArray<FString>& ManageGAS()
{
	static const TArray<FString> Actions = {
		TEXT("add_ability_system_component"), TEXT("configure_asc"),
		TEXT("create_attribute_set"), TEXT("add_attribute"),
		TEXT("set_attribute_base_value"), TEXT("set_attribute_clamping"),
		TEXT("create_gameplay_ability"), TEXT("set_ability_tags"),
		TEXT("set_ability_costs"), TEXT("set_ability_cooldown"),
		TEXT("set_ability_targeting"), TEXT("add_ability_task"),
		TEXT("set_activation_policy"), TEXT("set_instancing_policy"),
		TEXT("create_gameplay_effect"), TEXT("set_effect_duration"),
		TEXT("add_effect_modifier"), TEXT("set_modifier_magnitude"),
		TEXT("add_effect_execution_calculation"), TEXT("add_effect_cue"),
		TEXT("set_effect_stacking"), TEXT("set_effect_tags"),
		TEXT("create_gameplay_cue_notify"), TEXT("configure_cue_trigger"),
		TEXT("set_cue_effects"), TEXT("add_tag_to_asset"),
		TEXT("get_gas_info"), TEXT("get_attribute"),
		TEXT("create_ability_set"), TEXT("add_ability"),
		TEXT("create_execution_calculation")
	};
	return Actions;
}
inline TArray<FString> ManageGASUnion() { return ManageGAS(); }

inline const TArray<FString>& ManageCharacter()
{
	static const TArray<FString> Actions = {
		TEXT("create_character_blueprint"),
		TEXT("configure_capsule_component"),
		TEXT("configure_mesh_component"),
		TEXT("configure_camera_component"),
		TEXT("configure_movement_speeds"), TEXT("configure_jump"),
		TEXT("configure_rotation"), TEXT("add_custom_movement_mode"),
		TEXT("configure_nav_movement"), TEXT("setup_mantling"),
		TEXT("setup_vaulting"), TEXT("setup_climbing"),
		TEXT("setup_sliding"), TEXT("setup_wall_running"),
		TEXT("setup_grappling"), TEXT("setup_footstep_system"),
		TEXT("map_surface_to_sound"), TEXT("configure_footstep_fx"),
		TEXT("get_character_info"), TEXT("setup_movement"),
		TEXT("set_walk_speed"), TEXT("set_jump_height"),
		TEXT("set_gravity_scale"), TEXT("set_ground_friction"),
		TEXT("set_braking_deceleration"), TEXT("configure_crouch"),
		TEXT("configure_sprint")
	};
	return Actions;
}
inline TArray<FString> ManageCharacterUnion() { return ManageCharacter(); }

inline const TArray<FString>& ManageCombat()
{
	static const TArray<FString> Actions = {
		TEXT("create_weapon_blueprint"), TEXT("configure_weapon_mesh"),
		TEXT("configure_weapon_sockets"), TEXT("set_weapon_stats"),
		TEXT("configure_hitscan"), TEXT("configure_projectile"),
		TEXT("configure_spread_pattern"), TEXT("configure_recoil_pattern"),
		TEXT("configure_aim_down_sights"),
		TEXT("create_projectile_blueprint"),
		TEXT("configure_projectile_movement"),
		TEXT("configure_projectile_collision"),
		TEXT("configure_projectile_homing"), TEXT("create_damage_type"),
		TEXT("configure_damage_execution"), TEXT("setup_hitbox_component"),
		TEXT("setup_reload_system"), TEXT("setup_ammo_system"),
		TEXT("setup_attachment_system"), TEXT("setup_weapon_switching"),
		TEXT("configure_muzzle_flash"), TEXT("configure_tracer"),
		TEXT("configure_impact_effects"), TEXT("configure_shell_ejection"),
		TEXT("create_melee_trace"), TEXT("configure_combo_system"),
		TEXT("create_hit_pause"), TEXT("configure_hit_reaction"),
		TEXT("setup_parry_block_system"), TEXT("configure_weapon_trails"),
		TEXT("get_combat_info"), TEXT("setup_damage_type"),
		TEXT("configure_hit_detection"), TEXT("get_combat_stats"),
		TEXT("create_damage_effect"), TEXT("apply_damage"), TEXT("heal"),
		TEXT("create_shield"), TEXT("modify_armor")
	};
	return Actions;
}
inline TArray<FString> ManageCombatUnion() { return ManageCombat(); }

inline const TArray<FString>& ManageInventory()
{
	static const TArray<FString> Actions = {
		TEXT("create_item_data_asset"), TEXT("set_item_properties"),
		TEXT("create_item_category"), TEXT("assign_item_category"),
		TEXT("create_inventory_component"),
		TEXT("configure_inventory_slots"), TEXT("add_inventory_functions"),
		TEXT("configure_inventory_events"),
		TEXT("set_inventory_replication"), TEXT("create_pickup_actor"),
		TEXT("configure_pickup_interaction"),
		TEXT("configure_pickup_respawn"), TEXT("configure_pickup_effects"),
		TEXT("create_equipment_component"), TEXT("define_equipment_slots"),
		TEXT("configure_equipment_effects"), TEXT("add_equipment_functions"),
		TEXT("configure_equipment_visuals"), TEXT("create_loot_table"),
		TEXT("add_loot_entry"), TEXT("configure_loot_drop"),
		TEXT("set_loot_quality_tiers"), TEXT("create_crafting_recipe"),
		TEXT("configure_recipe_requirements"),
		TEXT("create_crafting_station"), TEXT("add_crafting_component"),
		TEXT("configure_item_stacking"), TEXT("set_item_icon"),
		TEXT("add_recipe_ingredient"), TEXT("remove_loot_entry"),
		TEXT("configure_inventory_weight"),
		TEXT("configure_station_recipes"), TEXT("get_inventory_info")
	};
	return Actions;
}
inline TArray<FString> ManageInventoryUnion() { return ManageInventory(); }

inline const TArray<FString>& ManageInteraction()
{
	static const TArray<FString> Actions = {
		TEXT("create_interaction_component"),
		TEXT("configure_interaction_trace"),
		TEXT("configure_interaction_widget"),
		TEXT("add_interaction_events"),
		TEXT("create_interactable_interface"), TEXT("create_door_actor"),
		TEXT("configure_door_properties"), TEXT("create_switch_actor"),
		TEXT("configure_switch_properties"), TEXT("create_chest_actor"),
		TEXT("configure_chest_properties"), TEXT("create_lever_actor"),
		TEXT("setup_destructible_mesh"),
		TEXT("configure_destruction_levels"),
		TEXT("configure_destruction_effects"),
		TEXT("configure_destruction_damage"),
		TEXT("add_destruction_component"), TEXT("create_trigger_actor"),
		TEXT("configure_trigger_events"), TEXT("configure_trigger_filter"),
		TEXT("configure_trigger_response"), TEXT("get_interaction_info")
	};
	return Actions;
}
inline TArray<FString> ManageInteractionUnion() { return ManageInteraction(); }

// Named *Actions to avoid reading as the manage_tools tool itself.
inline const TArray<FString>& ManageToolsActions()
{
	static const TArray<FString> Actions = {
		TEXT("list_tools"), TEXT("list_categories"), TEXT("enable_tools"),
		TEXT("disable_tools"), TEXT("enable_category"),
		TEXT("disable_category"), TEXT("get_status"), TEXT("reset")
	};
	return Actions;
}
inline TArray<FString> ManageToolsUnion() { return ManageToolsActions(); }

inline bool IsMaterialAuthoringAction(const FString& Action) { return ContainsAction(MaterialAuthoring(), Action); }
inline bool IsTextureAction(const FString& Action) { return ContainsAction(Texture(), Action); }
inline bool IsBlueprintGraphAction(const FString& Action) { return ContainsAction(BlueprintGraph(), Action); }
inline bool IsWidgetAuthoringAction(const FString& Action) { return ContainsAction(WidgetAuthoring(), Action); }
inline bool IsCommonUiAction(const FString& Action) { return ContainsAction(CommonUi(), Action); }
inline bool IsAnimationAuthoringAction(const FString& Action) { return ContainsAction(AnimationAuthoring(), Action); }
inline bool IsAudioAuthoringAction(const FString& Action) { return ContainsAction(AudioAuthoring(), Action); }
inline bool IsLightingAction(const FString& Action) { return ContainsAction(Lighting(), Action); }
inline bool IsSplineAction(const FString& Action) { return ContainsAction(Splines(), Action); }
inline bool IsSkeletonAction(const FString& Action) { return ContainsAction(Skeleton(), Action); }
inline bool IsPerformanceAction(const FString& Action) { return ContainsAction(Performance(), Action); }
inline bool IsInputAction(const FString& Action) { return ContainsAction(Input(), Action); }
inline bool IsGameFrameworkAction(const FString& Action) { return ContainsAction(GameFramework(), Action); }
inline bool IsSessionAction(const FString& Action) { return ContainsAction(Sessions(), Action); }
inline bool IsVolumeAction(const FString& Action) { return ContainsAction(Volumes(), Action); }
inline bool IsBehaviorTreeAction(const FString& Action) { return ContainsAction(BehaviorTree(), Action); }
inline bool IsNavigationAction(const FString& Action) { return ContainsAction(Navigation(), Action); }

// ─── Routing introspection ───────────────────────────────────────────────────
// Mirrors what each tool's registration lambda tests, in test order, so startup
// validation can prove: no action is claimed by two routed families, no routed
// family shadows the core (fallthrough) list, and shared schema enums match the
// published schema. When a registration lambda in InitializeHandlers gains or
// loses a family test, this table must change with it.

struct FMcpRoutedFamily
{
	const TCHAR* Name;
	const TArray<FString>& (*List)();
};

struct FMcpToolRouting
{
	const TCHAR* ToolName;
	TArray<FMcpRoutedFamily> RoutedFamilies;   // membership tests, in lambda order
	const TArray<FString>& (*CoreActions)();   // fallthrough handler's actions (null = not listed)
	TArray<FString> (*SchemaUnion)();          // shared schema enum (null = facade declares inline)
};

inline const TArray<FMcpToolRouting>& GetToolRoutingTable()
{
	static const TArray<FMcpToolRouting> Table = {
		{ TEXT("manage_asset"),
		  { { TEXT("MaterialAuthoring"), &MaterialAuthoring }, { TEXT("Texture"), &Texture } },
		  &ManageAssetCore, &ManageAsset },
		{ TEXT("manage_blueprint"),
		  { { TEXT("CommonUi"), &CommonUi }, { TEXT("WidgetAuthoring"), &WidgetAuthoring },
		    { TEXT("BlueprintGraph"), &BlueprintGraph } },
		  &ManageBlueprintCore, &ManageBlueprint },
		{ TEXT("build_environment"),
		  { { TEXT("Lighting"), &Lighting }, { TEXT("Splines"), &Splines } },
		  &BuildEnvironmentCore, &BuildEnvironment },
		{ TEXT("animation_physics"),
		  { { TEXT("AnimationAuthoring"), &AnimationAuthoring }, { TEXT("Skeleton"), &Skeleton } },
		  &AnimationPhysicsCore, &AnimationPhysics },
		{ TEXT("system_control"),
		  { { TEXT("Performance"), &Performance } },
		  &SystemControlCore, &SystemControl },
		{ TEXT("manage_networking"),
		  { { TEXT("Input"), &Input }, { TEXT("GameFramework"), &GameFramework },
		    { TEXT("Sessions"), &Sessions } },
		  &ManageNetworkingCore, &ManageNetworking },
		{ TEXT("manage_level_structure"),
		  { { TEXT("Volumes"), &Volumes } },
		  &ManageLevelStructureCore, &ManageLevelStructure },
		{ TEXT("manage_ai"), {}, &ManageAICore, &ManageAI },
		{ TEXT("manage_audio"),
		  { { TEXT("AudioAuthoring"), &AudioAuthoring } },
		  &ManageAudioCore, &ManageAudio },
		{ TEXT("control_actor"), {}, &ControlActor, &ControlActorUnion },
		{ TEXT("control_editor"), {}, &ControlEditor, &ControlEditorUnion },
		{ TEXT("inspect"), {}, &Inspect, &InspectUnion },
		{ TEXT("manage_level"), {}, &ManageLevel, &ManageLevelUnion },
		{ TEXT("manage_sequence"), {}, &ManageSequence, &ManageSequenceUnion },
		{ TEXT("manage_geometry"), {}, &ManageGeometry, &ManageGeometryUnion },
		{ TEXT("manage_effect"), {}, &ManageEffect, &ManageEffectUnion },
		{ TEXT("manage_gas"), {}, &ManageGAS, &ManageGASUnion },
		{ TEXT("manage_character"), {}, &ManageCharacter, &ManageCharacterUnion },
		{ TEXT("manage_combat"), {}, &ManageCombat, &ManageCombatUnion },
		{ TEXT("manage_inventory"), {}, &ManageInventory, &ManageInventoryUnion },
		{ TEXT("manage_interaction"), {}, &ManageInteraction, &ManageInteractionUnion },
		{ TEXT("manage_tools"), {}, &ManageToolsActions, &ManageToolsUnion },
	};
	return Table;
}
}
