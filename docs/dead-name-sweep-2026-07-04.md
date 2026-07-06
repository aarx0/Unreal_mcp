# Dead-name sweep — 2026-07-04 (point-in-time report)

The de-alias inventory left ~150 handler-condition names no schema advertises (fork TODO
2026-07-04h). This sweep classified all of them and deleted the provably-dead class.

## Method

1. Inventory v2 (scratchpad `dead-name-inventory.ps1`): per-comparison variable attribution
   with word boundaries (fixes the v1 false-positive classes: `NodeTypeLower` matching
   `Lower`, value comparisons in compound conditions), dynamic-prefix detection
   (`sequence_*` manufacture), plus the `NativeSubAction` dispatch variable v1 missed.
   126 dead-candidates across 22 files.
2. 22 file-scoped classify agents (one per handler file), each proving per name: not in the
   published enum, not manufactured anywhere in Private (rewrites / EffectiveAction /
   concat / Printf / function-arg literals / cross-file delegation), and whether an
   advertised sibling reaches the same implementation.
3. Adversarial refuters on every `dead` verdict (uncertain = keep). 53 dead confirmed,
   0 refuted.

Verdicts: **53 dead** (deleted), **66 hidden** (implemented, no advertised name — kept,
decisions below), **1 live** (`console_command`, the Subsystem's forced internal literal),
**6 false positives**.

## Load-bearing transport fix (TODO 2026-07-04j)

`action`/`subAction` mirror bidirectionally, but a payload carrying BOTH with different
values bypassed enum validation entirely (only `action` is checked; `subAction` is an
exempt envelope key). Handlers dispatching on `subAction` would run a value no gate ever
saw. Fixed: both-present-but-different rejects INVALID_PARAMS. This is what makes
"not in the enum = externally unreachable" true for every verdict below.

## Deleted (53 names, all with a verified advertised sibling reaching the same code)

Also folded in: TODO 2026-07-04b (the EffectHandlers 30-branch Niagara-module block —
every branch both intercepted upstream by the live NiagaraAuthoring path AND a lying stub
that loaded the asset, discarded the params, and reported success without doing anything;
deleted wholesale, ~886 lines) and TODO 2026-07-04i (the shadowed create-anim-blueprint
duplicate: dead `create_animation_bp` arm, dead `create_anim_blueprint` delegation stub,
and the orphaned `HandleCreateAnimBlueprint` — all deleted).
## McpAutomationBridge_AIHandlers.cpp — set_ai_perception @ :3614
sibling: 
shape: 

## McpAutomationBridge_AIHandlers.cpp — create_nav_modifier @ :3762
sibling: 
shape: 

## McpAutomationBridge_AnimationHandlers.cpp — create_animation_bp @ :576
sibling: create_anim_blueprint
shape: Delete else-if arm lines 576-661 (`else if (LowerSub == TEXT("create_animation_bp")) {...}`) joining line 575's `}` directly to the `else if (LowerSub == TEXT("create_blend_space"))` arm at line 662. Site 2: delete the equally-dead `else if (LowerSub == TEXT("create_anim_blueprint"))` stub at lines 3785-3787 (joins line 3781's `}` to the `add_state_machine` arm at line 3788). Site 3: deleting site 2 orphans `HandleCreateAnimBlueprint` (its only caller, verified via whole-tree grep) -- delete its body at lines 4421-4565 and its declaration at McpAutomationBridgeSubsystem.h:737-739.

## McpAutomationBridge_AssetWorkflowHandlers.cpp — add_row @ :350
sibling: add_data_table_row
shape: drop `|| Lower == TEXT("add_row")` from line 350

## McpAutomationBridge_AssetWorkflowHandlers.cpp — edit_row @ :351
sibling: edit_data_table_row
shape: drop `|| Lower == TEXT("edit_row")` from line 351

## McpAutomationBridge_AssetWorkflowHandlers.cpp — update_data_table_row @ :352
sibling: edit_data_table_row
shape: drop `Lower == TEXT("update_data_table_row") ||` from line 352

## McpAutomationBridge_AssetWorkflowHandlers.cpp — update_row @ :352
sibling: edit_data_table_row
shape: drop `|| Lower == TEXT("update_row")` from line 352

## McpAutomationBridge_AssetWorkflowHandlers.cpp — remove_row @ :353
sibling: remove_data_table_row
shape: drop `|| Lower == TEXT("remove_row")` from line 353

## McpAutomationBridge_AssetWorkflowHandlers.cpp — delete_row @ :354
sibling: remove_data_table_row
shape: drop `Lower == TEXT("delete_row") ||` from line 354

## McpAutomationBridge_AssetWorkflowHandlers.cpp — get_rows @ :355
sibling: get_data_table_rows
shape: drop `Lower == TEXT("get_rows") ||` from line 355

## McpAutomationBridge_AssetWorkflowHandlers.cpp — import_rows @ :356
sibling: import_data_table
shape: drop `|| Lower == TEXT("import_rows")` from line 356 (leaving `Lower == TEXT("import_data_table"))` as the closing clause)

## McpAutomationBridge_AssetWorkflowHandlers.cpp — list_assets @ :378
sibling: list
shape: drop `|| Lower == TEXT("list_assets")` from line 378

## McpAutomationBridge_AssetWorkflowHandlers.cpp — generate_thumbnail @ :382
sibling: create_thumbnail
shape: drop `|| Lower == TEXT("generate_thumbnail")` from line 382; the inner `Lower.Equals(TEXT("generate_thumbnail"), ...)` guard at line 1289 becomes a harmless no-op check that can be trimmed in the same pass but doesn't orphan a helper

## McpAutomationBridge_AssetWorkflowHandlers.cpp — add_row @ :3689
sibling: add_data_table_row
shape: drop `|| SubAction == TEXT("add_row")` from line 3689

## McpAutomationBridge_AssetWorkflowHandlers.cpp — edit_row @ :3691
sibling: edit_data_table_row
shape: delete line 3691 (`SubAction == TEXT("edit_row") ||`) entirely

## McpAutomationBridge_AssetWorkflowHandlers.cpp — update_data_table_row @ :3692
sibling: edit_data_table_row
shape: delete line 3692 (`SubAction == TEXT("update_data_table_row") ||`) entirely

## McpAutomationBridge_AssetWorkflowHandlers.cpp — update_row @ :3693
sibling: edit_data_table_row
shape: change line 3693 from `SubAction == TEXT("update_row");` to close the bEdit expression after removing the preceding OR'd lines -- i.e. bEdit becomes `SubAction == TEXT("edit_data_table_row");` once :3691-3693 are dropped

## McpAutomationBridge_AssetWorkflowHandlers.cpp — remove_row @ :3695
sibling: remove_data_table_row
shape: delete line 3695 (`SubAction == TEXT("remove_row") ||`) entirely

## McpAutomationBridge_AssetWorkflowHandlers.cpp — delete_row @ :3696
sibling: remove_data_table_row
shape: change line 3696 from `SubAction == TEXT("delete_row");` to close the bRemove expression after :3695 is dropped -- i.e. bRemove becomes `SubAction == TEXT("remove_data_table_row");`

## McpAutomationBridge_AssetWorkflowHandlers.cpp — get_rows @ :3698
sibling: get_data_table_rows
shape: drop `|| SubAction == TEXT("get_rows")` from line 3698

## McpAutomationBridge_AssetWorkflowHandlers.cpp — import_rows @ :3700
sibling: import_data_table
shape: drop `|| SubAction == TEXT("import_rows")` from line 3700

## McpAutomationBridge_AudioHandlers.cpp — audio_create_sound_cue @ :618
sibling: create_sound_cue
shape: drop `|| Lower == TEXT("audio_create_sound_cue")` arm at :618, collapsing :617-618 to `if (Lower == TEXT("create_sound_cue"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_create_sound_class @ :778
sibling: create_sound_class
shape: drop `|| Lower == TEXT("audio_create_sound_class")` arm at :778, collapsing :777-778 to `else if (Lower == TEXT("create_sound_class"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_create_sound_mix @ :870
sibling: create_sound_mix
shape: drop `|| Lower == TEXT("audio_create_sound_mix")` arm at :870, collapsing :869-870 to `else if (Lower == TEXT("create_sound_mix"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_play_sound_at_location @ :964
sibling: play_sound_at_location
shape: drop `|| Lower == TEXT("audio_play_sound_at_location")` arm at :964, collapsing :963-964 to `else if (Lower == TEXT("play_sound_at_location"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_play_sound_2d @ :1059
sibling: play_sound_2d
shape: drop `|| Lower == TEXT("audio_play_sound_2d")` arm at :1059, collapsing :1058-1059 to `else if (Lower == TEXT("play_sound_2d"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_play_sound_attached @ :1118
sibling: play_sound_attached
shape: drop `|| Lower == TEXT("audio_play_sound_attached")` arm at :1118, collapsing :1117-1118 to `else if (Lower == TEXT("play_sound_attached"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_create_ambient_sound @ :1200
sibling: create_ambient_sound
shape: drop `|| Lower == TEXT("audio_create_ambient_sound")` arm at :1200, collapsing :1199-1200 to `else if (Lower == TEXT("create_ambient_sound"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_spawn_sound_at_location @ :1307
sibling: spawn_sound_at_location
shape: drop `|| Lower == TEXT("audio_spawn_sound_at_location")` arm at :1307, collapsing :1306-1307 to `else if (Lower == TEXT("spawn_sound_at_location"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_push_sound_mix @ :1394
sibling: push_sound_mix
shape: drop `|| Lower == TEXT("audio_push_sound_mix")` arm at :1394, collapsing :1393-1394 to `else if (Lower == TEXT("push_sound_mix"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_pop_sound_mix @ :1434
sibling: pop_sound_mix
shape: drop `|| Lower == TEXT("audio_pop_sound_mix")` arm at :1434, collapsing :1433-1434 to `else if (Lower == TEXT("pop_sound_mix"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_set_sound_mix_class_override @ :1476
sibling: set_sound_mix_class_override
shape: drop `|| Lower == TEXT("audio_set_sound_mix_class_override")` arm at :1476, collapsing :1475-1476 to `else if (Lower == TEXT("set_sound_mix_class_override"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_clear_sound_mix_class_override @ :1533
sibling: clear_sound_mix_class_override
shape: drop `|| Lower == TEXT("audio_clear_sound_mix_class_override")` arm at :1533, collapsing :1532-1533 to `else if (Lower == TEXT("clear_sound_mix_class_override"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_fade_sound_out @ :1618
sibling: fade_sound_out
shape: drop `|| Lower == TEXT("audio_fade_sound_out")` line at :1618 from the 4-arm entry condition (:1616-1619)

## McpAutomationBridge_AudioHandlers.cpp — audio_fade_sound_in @ :1619
sibling: fade_sound_in
shape: drop `|| Lower == TEXT("audio_fade_sound_in")` line at :1619 from the 4-arm entry condition (:1616-1619)

## McpAutomationBridge_AudioHandlers.cpp — audio_set_sound_attenuation @ :2129
sibling: set_sound_attenuation
shape: drop `|| Lower == TEXT("audio_set_sound_attenuation")` from the single-line condition at :2129, leaving `if (Lower == TEXT("set_sound_attenuation"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_fade_sound @ :2242
sibling: fade_sound
shape: drop `|| Lower == TEXT("audio_fade_sound")` from the single-line condition at :2242, leaving `if (Lower == TEXT("fade_sound"))`

## McpAutomationBridge_AudioHandlers.cpp — audio_create_reverb_zone @ :2363
sibling: create_reverb_zone
shape: drop `|| Lower == TEXT("audio_create_reverb_zone")` from the single-line condition at :2363, leaving `if (Lower == TEXT("create_reverb_zone"))`
(All seventeen audio_* arms above were executed with this sweep, 2874d066 — the
coordinates are pre-deletion, and the whole HandleAudioAction dispatcher died at
the manage_audio classing, 2026-07-05. Noted so a scoping pass does not hunt for
live alias arms.)

## McpAutomationBridge_ControlHandlers.cpp — remove @ :2741
sibling: delete
shape: Drop the `|| LowerSub == TEXT("remove")` OR-arm at :2741, leaving `if (LowerSub == TEXT("delete"))`. HandleControlActorDelete is untouched (still reached via "delete"); no helper becomes orphaned.

## McpAutomationBridge_ControlHandlers.cpp — apply_force_to_actor @ :2744
sibling: apply_force
shape: Drop the `||\n      LowerSub == TEXT("apply_force_to_actor")` arm spanning :2743-2744, leaving `if (LowerSub == TEXT("apply_force"))`. HandleControlActorApplyForce remains reachable via "apply_force"; no orphaned helper.

## McpAutomationBridge_ControlHandlers.cpp — set_actor_visibility @ :2751
sibling: set_visibility
shape: Drop the `||\n      LowerSub == TEXT("set_actor_visibility")` arm at :2750-2751, leaving `if (LowerSub == TEXT("set_visibility"))`. HandleControlActorSetVisibility stays reachable via "set_visibility"; no orphaned helper.

## McpAutomationBridge_ControlHandlers.cpp — list_actors @ :2794
sibling: list
shape: Drop the `|| LowerSub == TEXT("list_actors")` arm at :2794, leaving `if (LowerSub == TEXT("list") || LowerSub == TEXT("list_objects"))`. HandleControlActorList remains reachable via "list"; no orphaned helper.

## McpAutomationBridge_ControlHandlers.cpp — set_collision @ :2809
sibling: set_actor_collision
shape: Drop the `LowerSub == TEXT("set_collision") ||` arm at :2809, leaving `if (LowerSub == TEXT("set_actor_collision"))`. HandleControlActorSetCollision remains reachable via "set_actor_collision"; no orphaned helper.

## McpAutomationBridge_ControlHandlers.cpp — call_function @ :2811
sibling: call_actor_function
shape: Drop the `LowerSub == TEXT("call_function") ||` arm at :2811, leaving `if (LowerSub == TEXT("call_actor_function"))`. HandleControlActorCallFunction remains reachable via "call_actor_function"; no orphaned helper.

## McpAutomationBridge_EffectHandlers.cpp — deactivate_effect @ :283
sibling: deactivate
shape: Drop the `|| NativeSubAction == TEXT("deactivate_effect")` arm at line 283, leaving `if (NativeSubAction == TEXT("deactivate"))` (line 282) as the sole condition for that else-if block (lines 282-287). Both arms of the || currently do the identical rewrite (SetStringField action/subAction to "deactivate_niagara", lines 284-286) and fall into the same RoutedAction="create_effect" recursion (lines 288-296), which reaches the live deactivate_niagara implementation at line 1189 (LowerSub=="deactivate_niagara", sourced from the payload's rewritten "action" field per line 377). No helper function is orphaned — this is a bare string comparison inside an existing else-if, not a separate function; its advertised sibling "deactivate" keeps the branch alive.

## McpAutomationBridge_EnvironmentHandlers.cpp — add_foliage_type @ :493
sibling: add_foliage
shape: At line 493, drop the `LowerSub == TEXT("add_foliage_type") ||` clause, leaving `else if (LowerSub == TEXT("add_foliage"))`. HandleAddFoliageType (McpAutomationBridge_FoliageHandlers.cpp) is not orphaned — it stays reachable via the remaining 'add_foliage' arm, which is its only other caller.

## McpAutomationBridge_GASHandlers.cpp — get_attribute_value @ :3010
sibling: get_attribute
shape: Drop the `|| SubAction == TEXT("get_attribute_value")` disjunct at line 3010, leaving `if (SubAction == TEXT("get_attribute"))`. No helper function is introduced by this arm (it's a plain condition), so nothing becomes orphaned.

## McpAutomationBridge_GeometryHandlers.cpp — difference @ :7869
sibling: boolean_subtract
shape: Delete the single-line if-arm `if (SubAction == TEXT("difference")) return HandleBooleanSubtract(this, RequestId, Payload, RequestingSocket);` at :7869 (and its now-empty `// Aliases` comment at :7868 if no other alias arms remain in that block). HandleBooleanSubtract has another caller (boolean_subtract at :7776), so it does not become orphaned.
(Executed with this sweep, 2874d066 — the :7869 coordinate is pre-deletion; the slot
held edge_split at later HEADs and the whole dispatcher died at the manage_geometry
classing, 2026-07-05. Noted so a scoping pass does not hunt for a live alias arm.)

## McpAutomationBridge_LevelHandlers.cpp — delete_levels @ :1407
sibling: delete
shape: Delete the `else if (LowerSub == TEXT("delete_levels")) { EffectiveAction = TEXT("delete_level"); }` arm at lines 1407-1408 from the LowerSub if/else-if chain; no helper function is orphaned (the assignment is inline, not a call to a shared helper), and the advertised "delete" arm (1399-1400) already reaches the identical delete_level implementation at line 2783 with zero functionality loss.

## McpAutomationBridge_SkeletonHandlers.cpp — add_socket @ :3142
sibling: create_socket
shape: Drop the `|| SubAction == TEXT("add_socket")` disjunct at line 3142, leaving `else if (SubAction == TEXT("create_socket"))`. HandleCreateSocket is unaffected (still called by create_socket).

## McpAutomationBridge_SkeletonHandlers.cpp — modify_socket @ :3146
sibling: configure_socket
shape: Drop the `|| SubAction == TEXT("modify_socket")` disjunct at line 3146, leaving `else if (SubAction == TEXT("configure_socket"))`. HandleConfigureSocket keeps its other caller (configure_socket).

## McpAutomationBridge_SkeletonHandlers.cpp — modify_physics_body @ :3167
sibling: configure_physics_body
shape: Drop the `|| SubAction == TEXT("modify_physics_body")` disjunct at line 3167, leaving `else if (SubAction == TEXT("configure_physics_body"))`. HandleConfigurePhysicsBody keeps its other caller (configure_physics_body).

## McpAutomationBridge_SkeletonHandlers.cpp — set_physics_constraint @ :3902
sibling: add_physics_constraint
shape: Delete the entire else-if block at lines 3901-3906 (comment + `else if (SubAction == TEXT("set_physics_constraint")) { return HandleAddPhysicsConstraint(...); }`). HandleAddPhysicsConstraint keeps its other caller (the add_physics_constraint arm at 3171-3174); no helper becomes orphaned.



## Hidden functionality (66 names — kept; advertise-or-delete per family at classing)

Implemented behind unreachable names. Each family needs one decision; recommendations:
**advertise** = real capability worth a decl + enum entry when its family is classed;
**delete** = permanent stub or superseded by an advertised sibling.

Quick dispositions:
- **Delete at classing (stubs/lies):** AnimationAuthoring rig stubs (circular "call
  animation_physics with action=X" errors for names nothing advertises — DELETED at the
  animation_physics classing), AnimationHandlers NOT_SUPPORTED rig family (DELETED at
  the animation_physics classing), texture create_cube/volume/array
  (honest UNSUPPORTED stubs — DELETED at the manage_asset classing),
  `import_texture` (**fake-success "import queued"** — a lying stub — DELETED at the
  manage_asset classing), `set_texture_filter`/`set_texture_wrap` (setters behind
  unrouted names — DELETED at the manage_asset classing),
  `preview_physics` (echo stub — DELETED at the animation_physics classing),
  `set_cast_shadows` (UNSUPPORTED stub — DELETED at the manage_asset classing),
  MaterialAuthoring per-node conveniences (generic `add_material_node` covers all five —
  add_component_mask/add_dot_product/add_cross_product/add_desaturation/add_append
  DELETED at the manage_asset classing, recover from git),
  WorldPartition `create_datalayer`/`set_datalayer` (live differently-spelled siblings
  exist in manage_level_structure).
- **Advertise candidates (real, no advertised equivalent):** geometry raw DynamicMesh CRUD
  family (11 actions — DELETED at the manage_geometry classing, recover from git),
  skeleton read/delete family (12: delete_socket, get_bone_transform,
  list/delete morph targets + virtual bones, set_physics_asset, remove_physics_body... —
  DELETED at the animation_physics classing, recover from git),
  `export_asset` (~180-line generic exporter — DELETED at the system_control classing,
  recover from git), `batch_console_commands` (blocklist-aware),
  `get_nodes` (graph node dump — DELETED at the manage_blueprint classing,
  recover from git), `grant_ability` (DELETED at the manage_gas
  classing, recover from git), sun/skylight intensity pair,
  `set_ai_movement` (DELETED at the manage_ai classing, recover from git),
  `cleanup_invalid_datalayers`, `attach_render_target_to_volume`
  (DELETED at the system_control classing, recover from git),
  `create_spline_mesh_actor`, Niagara `set_parameter`,
  `source_control_enable` (DELETED at the manage_asset classing, recover from git),
  UiHandlers PIE/widget pokes (DELETED at the system_control classing — raw
  TObjectIterator scans duplicated by newer widget actions; recover from git).

Full per-name evidence:
### AIHandlers (updated at the manage_ai classing, 2026-07-05)
- **set_ai_movement** @:3845 - Standalone `if (SubAction == TEXT("set_ai_movement"))` block (:3845-3998) in HandleManageAIAction. Not in manage_ai's action enum and no code manufactures the literal `set_ai_movement` anywhere in Private (grep confirms only the comment/condition), so the whole branch is unreachable both externally … Disposition: **deleted at the manage_ai classing (2026-07-05)** (the branch sat at :3614-:3767 at HEAD); advertise candidate parked for Aaron — recover from git.
- **create_nav_link_proxy (shadowed inline copy)** @:3976 - Below-radar dead duplicate found at the manage_ai classing: the advertised name belongs to the Navigation family, and HandleManageAIAction's top-of-function Navigation router forwarded it to NavigationHandlers' `HandleCreateNavLinkProxy` (the live point-link actor spawner) before the inline branch could ever match, so the dispatcher's own copy — a NavLinkProxy *blueprint* creator reading blueprintPath/name/path, a different capability from the live spawner — was unreachable since the router landed. **Deleted at the manage_ai classing (2026-07-05)**; recover from git if the blueprint-creator variant is ever wanted (its blueprintPath/name/path spellings left the decl row with it).

### AnimationAuthoringHandlers (updated at the animation_physics classing, 2026-07-05)
All four rig stubs below were **deleted at the animation_physics classing (2026-07-05)**
— circular "call animation_physics with action=X" errors for names nothing advertises
(transport-dead); recover from git history if advertising is wanted. Deleted with them,
below the sweep inventory's radar: the chain's dead `create_pose_library` copy (:3751,
MCP_HAS_POSEASSET-guarded WRONG_HANDLER_ROUTE stub after name/skeleton validation) —
the advertised name sits in `AnimationPhysicsCore()`, so the registration lambda always
routed it to the primary dispatcher's live UMcpGenericDataAsset implementation (now
`HandleAnimPhysCreatePoseLibrary`) and this authoring copy could never match; its
`save` read left the decl row with it.
- **add_control** @:3703 - 3-line stub (3703-3721, guarded by #if MCP_HAS_CONTROLRIG): if ControlName param is empty it returns MISSING_CONTROL_NAME, otherwise it unconditionally falls through to ANIM_ERROR_RESPONSE("add_control is handled by the animation_physics runtime authoring route; call animation_physics with action=ad…
- **add_rig_unit** @:3724 - 13-line stub (3724-3736, guarded by #if MCP_HAS_CONTROLRIG): reads assetPath/unitType into local variables that are then unused, and unconditionally returns ANIM_ERROR_RESPONSE("add_rig_unit is handled by the animation_physics runtime authoring route; call animation_physics with action=add_rig_unit.…
- **connect_rig_elements** @:3739 - 10-line stub (3739-3748, guarded by #if MCP_HAS_CONTROLRIG): does no field extraction at all, unconditionally returns ANIM_ERROR_RESPONSE("connect_rig_elements is handled by the animation_physics runtime authoring route; call animation_physics with action=connect_rig_elements.", WRONG_HANDLER_ROUTE)…
- **add_ik_chain** @:3862 - 18-line stub (3862-3879, guarded by #if MCP_HAS_IKRIG): validates ChainName is non-empty (else MISSING_CHAIN_NAME) then unconditionally falls through to ANIM_ERROR_RESPONSE("add_ik_chain is handled by the animation_physics runtime authoring route; call animation_physics with action=add_ik_chain.", W…

### AnimationHandlers (updated at the animation_physics classing, 2026-07-05)
All four NOT_SUPPORTED rig stubs below were **deleted at the animation_physics classing
(2026-07-05)** — permanent stubs, transport-dead (no decl row, not in any routing list,
schema enum rejects the names); recover from git history if advertising is wanted.
Deleted with them, below the sweep inventory's radar: the dispatcher's THIRTY
unreachable shadowed copies of routed authoring actions (create_animation_sequence,
set_sequence_length, add_bone_track, set_bone_key, set_curve_key, add_notify,
create_montage, add_montage_section, add_montage_slot, set_section_timing,
add_montage_notify, set_blend_in, set_blend_out, link_sections, create_blend_space_1d,
create_blend_space_2d, add_blend_sample, set_axis_settings, set_interpolation_settings,
create_aim_offset, add_aim_offset_sample, add_state_machine, add_state, add_transition,
set_transition_rules, add_blend_node, add_cached_pose, add_slot_node,
create_control_rig, create_ik_rig) — every one of those advertised names is in
`AnimationAuthoring()`, so the registration lambda always routed it to
AnimationAuthoringHandlers' implementation first and the primary's own copy could never
match (same class as manage_ai's shadowed create_nav_link_proxy). The live authoring
implementations are unchanged (now the `HandleAnimAuthoring*` members); the spellings
only the shadowed copies read left the decl rows with them (see the classing's decl
burn-down). Recover any copy from git — several were older, differently-shaped
implementations of the same action name (e.g. the aim-offset/blend-space creators
without axis parameters, the sampleX/sampleY blend-sample variant).
- **add_control** @:4110 - Control Rig graph-editing family (add_control/add_rig_unit/connect_rig_elements): always returns NOT_SUPPORTED, no actual Control Rig graph mutation implemented behind any of the three. Extent: lines 4110-4135.
- **add_rig_unit** @:4136 - Control Rig graph-editing family (same family as add_control/connect_rig_elements): always returns NOT_SUPPORTED, no rig-unit insertion logic implemented. Extent: lines 4136-4158.
- **connect_rig_elements** @:4159 - Control Rig graph-editing family (same family as add_control/add_rig_unit): always returns NOT_SUPPORTED, no pin-connection logic implemented. Extent: lines 4159-4189.
- **add_ik_chain** @:4337 - IK Rig authoring family: create_ik_rig (line 4275) is real/advertised, but its natural follow-on add_ik_chain is a permanent NOT_SUPPORTED stub with no chain-editing logic, and is additionally unreachable by any name. Extent: lines 4337-4366.

### AssetWorkflowHandlers
- **source_control_enable** @:418 - Source Control family (McpAutomationBridge_AssetWorkflowHandlers.cpp :838-891, ~54 lines, WITH_EDITOR-gated). HandleSourceControlEnable wraps ISourceControlModule::Get(): if source control is already enabled it returns success with the active provider's name; otherwise it optionally sets the provide…

  Disposition: **deleted at the manage_asset classing (2026-07-05)** — arm +
  member (HandleSourceControlEnable) removed; advertise candidate parked for
  Aaron (recover from git).

### AssetWorkflowHandlers — shadowed material copies (found at the manage_asset classing, 2026-07-05)
The eight entries below are the below-radar shadowed-dead class found at the
manage_asset classing (audio/animation_physics precedent): each name is in
`MaterialAuthoring()`, so the registration lambda always routed it to the live
`HandleManageMaterialAuthoringAction` branch first, and HandleAssetAction's own
arm + dedicated member could never match. **Deleted at the manage_asset classing
(2026-07-05)**; the live MaterialAuthoring implementations are unchanged (now the
`HandleMaterial*` members); the spellings only the shadowed copies read left the
decl rows with them (see the classing's decl burn-down — 5 own-branch rows + 3
alias rows re-derived from the live bodies). Recover any copy from git.
- **create_material** arm @:344 → HandleCreateMaterial @:3448 - dead copy read `properties` the live body never reads.
- **create_material_instance** arm @:346 → HandleCreateMaterialInstance @:3939 - dead copy read `parameters`.
- **add_material_node** arm @:425 → HandleAddMaterialNode @:5349 - dead copy read materialPath/posX/posY/value/color/texturePath.
- **connect_material_pins** arm @:427 → HandleConnectMaterialPins @:5539 - dead copy read materialPath/fromExpression/toExpression/targetPin (live target connect_nodes reads sourceNodeId/targetNodeId/inputName/sourcePin).
- **remove_material_node** arm @:429 → HandleRemoveMaterialNode @:5758 - dead copy read materialPath/expressionIndex.
- **break_material_connections** arm @:431 → HandleBreakMaterialConnections @:5913 - dead copy read materialPath/expressionIndex/inputName (live target disconnect_nodes reads nodeId/pinName).
- **get_material_node_details** arm @:433 → HandleGetMaterialNodeDetails @:6082 - dead copy read materialPath/expressionIndex.
- **rebuild_material** arm @:435 → HandleRebuildMaterial @:6688 - dead copy read materialPath (live target compile_material reads waitForShaders/save).

### AudioHandlers (added at the manage_audio classing, 2026-07-05)
All seven entries below are the below-radar shadowed-dead class found at the
manage_audio classing (same class as animation_physics's thirty): each name is in
`AudioAuthoring()`, so the registration lambda always routed it to
AudioAuthoringHandlers' live block first and the core dispatcher's own arm +
dedicated member could never match. **Deleted at the manage_audio classing
(2026-07-05)**; the live authoring implementations are unchanged (now the
`HandleAudioAuthoring*` members); the spellings only the shadowed members read
left the decl rows with them (see the classing's decl burn-down). Recover any
copy from git — several were differently-shaped implementations of the same
action name (the UE 5.7-aware NewObject dialogue creators, the
reflectionsGain/lateGain reverb params, add_source_effect's typed
EQ/Reverb/Delay entries).
- **create_dialogue_voice** arm @:1827 → HandleCreateDialogueVoice @:2484 - read voiceName/outputPath/gender/pluralization; NewObject-based UDialogueVoice creator (the live block reads name/path/gender/plurality/save and creates via UDialogueVoiceFactory).
- **create_dialogue_wave** arm @:1830 → HandleCreateDialogueWave @:2576 - read waveName/soundPath/outputPath; required a USoundWave and seeded one context mapping (the live block reads name/path/spokenText/save).
- **set_dialogue_context** arm @:1833 → HandleSetDialogueContext @:2667 - read wavePath/voicePath/contextIndex; set the speaker on an existing mapping by index (the live block appends/replaces full mappings from speakerPath/soundWavePath/targetVoices).
- **create_reverb_effect** arm @:1841 → HandleCreateReverbEffect @:2743 - read effectName/outputPath plus eight reverb params incl. reflectionsGain/lateGain (the live block reads name/path/save + six params).
- **create_source_effect_chain** arm @:1844 → HandleCreateSourceEffectChain @:2834 - read chainName/outputPath (the live block reads name/path/save).
- **add_source_effect** arm @:1847 → HandleAddSourceEffect @:2900 - read chainPath/effectType/effectName with typed EQ/Reverb/Delay entries (the live block loads a preset by effectPresetPath or synthesizes one from effectType).
- **create_submix_effect** arm @:1850 → HandleCreateSubmixEffect @:2980 - read effectName/outputPath/effectType and created a USoundEffectSubmixPreset (the live block creates a USoundSubmix from name/path/save).

### BlueprintGraphHandlers (updated at the manage_blueprint classing, 2026-07-05)
- **get_nodes** @:1790 - Family: "list all nodes in a graph" node-dump. Body (lines 1790-1859) iterates TargetGraph->Nodes and, per node, emits nodeId/nodeName/nodeType/nodeTitle/comment/x/y plus a nested pins array (pinName/pinType/direction/pinSubType-for-object-class-struct/linkedTo as {nodeId,pinName} objects), wrapped … Not in `manage_blueprint`'s action enum, no decl row, and nothing manufactures the literal `get_nodes` (grep confirms only the else-if condition + doc/comment mentions), so the BlueprintGraph dispatcher's else-if was transport-dead. Disposition: **deleted at the manage_blueprint classing (2026-07-05)** — the else-if branch (:1790-1859) removed and the chain re-spliced (connect_pins → break_pin_links, brace-balance net 0); the only handler-body edit of that classing. Advertise candidate parked for Aaron — recover from git.

### ConsoleCommandHandlers
- **batch_console_commands** @:189 - Batch console-command family: a fully implemented handler (lines 189-334, ~146 lines) that reads a client-supplied JSON 'commands' array (accepting either raw strings or {command|cmd} objects), runs each command through the same ConsoleCommandSecurity::IsBlockedCommand blocklist used by the single-c…

### ControlHandlers
- **get_actor** @:2796 - HandleControlActorGet (impl at ControlHandlers.cpp:4888-4938, ~50 lines) is a single-actor lookup-by-name: resolves actorName via FindActorByName, then returns name/label/path/class/tags[]/location[]/scale[] as JSON. It is a distinct, complete feature (not a stub) that duplicates none of the adverti…
- **get_actor_by_name** @:2797 - Same family/branch as "get_actor" above — HandleControlActorGet (ControlHandlers.cpp:4888-4938): single-actor lookup-by-name returning name/label/path/class/tags/location/scale. The whole 3-way OR (get / get_actor / get_actor_by_name) is unreachable externally and unmanufactured internally; treat as…

### EnvironmentHandlers (updated at the build_environment classing)
The whole `HandleControlEnvironmentAction` legacy dispatcher (EnvironmentHandlers.cpp
:1064-1290) was **deleted at the build_environment classing (2026-07-05)**: it gated on
the `control_environment` action name, which `InitializeHandlers` never registers (only
the 21 canonical tool names are) and which no code calls — repo-wide, the only mentions
were its own definition and header declaration. Disposition for both entries below plus
the below-radar hidden `set_time_of_day` sibling branch (:1160, a duplicate of the
advertised build_environment action): **deleted (transport-dead); recover from git
history if advertising is wanted.**
- **set_sun_intensity** @:1208 - Belongs to the HandleControlEnvironmentAction family (lines 1064-1290): a small time/lighting-control handler with three sibling branches — set_time_of_day (rotates the first ADirectionalLight to simulate sun position from an 'hour' param), set_sun_intensity (this one: sets the first ADirectionalLig…
- **set_skylight_intensity** @:1243 - Same HandleControlEnvironmentAction family as set_sun_intensity (lines 1064-1290) — see that finding's familyNote. This branch specifically: locates the first ASkyLight actor in the editor world (FindFirstSkyLight lambda) and applies USkyLightComponent::SetIntensity from the 'intensity' payload fiel…

### GASHandlers
- **grant_ability** @:3641 - Section 6 'Ability Sets' family (create_ability_set / add_ability / grant_ability per the file's own header taxonomy). grant_ability's implementation (lines 3641-3741, ~101 lines) validates actorPath/blueprintPath and abilityPath/abilityClass params, loads the actor Blueprint and the ability class, … Disposition: **deleted at manage_gas classing** (2026-07-05); advertise-candidate parked for Aaron — recover from git.

### GeometryHandlers (updated at the manage_geometry classing, 2026-07-05)
All eleven entries below were **deleted at the manage_geometry classing (2026-07-05)**
— the dispatcher arms died with the retired chain and the eleven file-static
implementations (HandleCreateProceduralMesh, HandleAppendTriangle, HandleAppendVertex,
HandleDeleteVertex, HandleDeleteTriangle, HandleGetVertexPosition,
HandleSetVertexPosition, HandleSetUVs, HandleSetVertexColor, HandleSplitNormals,
HandleTranslateMesh; each file-local, with exactly its definition and its dispatcher
arm as its two references) died as their single callers vanished. Disposition per the
sweep's quick-dispositions: the raw-mesh CRUD family stays an **advertise candidate
parked for Aaron** (real, distinct capability — direct FDynamicMesh3 vertex/triangle
editing that no advertised action covers — recover from git).
- **create_procedural_mesh** @:7771 - Part of a 'raw DynamicMesh CRUD' family (create_procedural_mesh, append_triangle, append_vertex, delete_vertex, delete_triangle, get_vertex_position, set_vertex_position, translate_mesh, set_vertex_color, set_uvs, split_normals) that manipulates FDynamicMesh3 directly on an ADynamicMeshActor, distin…
- **append_triangle** @:7772 - Same raw-mesh family as create_procedural_mesh. HandleAppendTriangle (:5831-5902, ~72 lines) locates the target ADynamicMeshActor by label then calls FDynamicMesh3::AppendVertex three times (for v0/v1/v2) followed by AppendTriangle(idx0,idx1,idx2,groupID) directly on the mesh's edit ref, notifying t…
- **set_uvs** @:7834 - Same raw-mesh family, UV subset. HandleSetUVs (:6005-6119, ~115 lines) ensures a UV overlay/layer exists for the requested channel (growing NumUVLayers if needed) and sets the UV coordinate on every UV-overlay element whose parent vertex matches the given vertexIndex.
- **set_vertex_color** @:7835 - Same raw-mesh family. HandleSetVertexColor (:5908-5998, ~91 lines) lazily enables vertex colors on the FDynamicMesh3 if absent, then either sets one vertex's RGBA (vertexIndex) or all vertices' color (setAll flag).
- **split_normals** @:7841 - Same raw-mesh family, normals subset. HandleSplitNormals (:5725-5784, ~60 lines) looks up the ADynamicMeshActor, then calls UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals with a hard opening-angle threshold (payload 'splitAngle', default 60 degrees) to hard-split normals along shar…
- **append_vertex** @:7851 - Same raw-mesh family. HandleAppendVertex (:6126-6182, ~57 lines) appends one vertex at the given 'position' via FDynamicMesh3::AppendVertex, returning the new vertex index and updated vertex count.
- **delete_vertex** @:7852 - Same raw-mesh family. HandleDeleteVertex (:6188-6253, ~66 lines) validates the vertex index then calls FDynamicMesh3::RemoveVertex (which also removes incident triangles), reporting success and the new vertex count.
- **delete_triangle** @:7853 - Same raw-mesh family. HandleDeleteTriangle (:6259-6323, ~65 lines) validates the triangle index then calls FDynamicMesh3::RemoveTriangle, reporting success and the new triangle count.
- **get_vertex_position** @:7854 - Same raw-mesh family, read-only query. HandleGetVertexPosition (:6329-6390, ~62 lines) calls UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition for the given vertexIndex and returns {x,y,z} or an INVALID_VERTEX error.
- **set_vertex_position** @:7855 - Same raw-mesh family. HandleSetVertexPosition (:6396-6459, ~64 lines) validates the vertex index then calls FDynamicMesh3::SetVertex to move the vertex to the given 'position'.
- **translate_mesh** @:7856 - Same raw-mesh family, whole-mesh transform. HandleTranslateMesh (:6465-6521, ~57 lines) calls UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(Mesh, Translation) to shift every vertex of the mesh by the given offset.

### MaterialAuthoringHandlers
- **add_component_mask** @:1295 - ComponentMask node family: reads optional r/g/b/a booleans from the payload (default R=G=B=true, A=false), creates a UMaterialExpressionComponentMask, positions it, adds it to the material/function, and returns its nodeId. Extent: lines 1292-1324. The advertised generic 'add_material_node' action (l…
- **add_dot_product** @:1329 - DotProduct node family: creates a UMaterialExpressionDotProduct with no extra payload-driven configuration, positions it, adds it to the host, returns nodeId. Extent: lines 1326-1348. The advertised 'add_material_node' (nodeType="DotProduct", line 4716) reaches a separate generic creation path (line…
- **add_cross_product** @:1353 - CrossProduct node family: creates a UMaterialExpressionCrossProduct, positions it, adds it to the host, returns nodeId. Extent: lines 1350-1372. The advertised 'add_material_node' (nodeType="CrossProduct", line 4718) reaches the separate generic creation path at line 4776 instead of this branch, so …
- **add_desaturation** @:1377 - Desaturation node family: optionally reads a {r,g,b} 'luminanceFactors' object from the payload (default 0.3/0.59/0.11) into an FLinearColor, creates a UMaterialExpressionDesaturation, positions it, adds it to the host, returns nodeId. Extent: lines 1374-1407. The advertised 'add_material_node' (nod…
- **add_append** @:1412 - AppendVector node family ("dedicated handler for convenience" per its own comment): creates a UMaterialExpressionAppendVector, positions it, adds it to the host, returns nodeId. Extent: lines 1409-1431. Two advertised actions can independently produce the same node class through separate code paths …
- **set_cast_shadows** @:5048 - Stub family: validates/sanitizes an 'assetPath' string and then unconditionally sends back an UNSUPPORTED_OPERATION error stating shadow casting can't be set on a material asset and should be configured on a mesh/light component instead. Extent: lines 5045-5069. It performs no material mutation at a…

Disposition for all six MaterialAuthoring names above (add_component_mask,
add_dot_product, add_cross_product, add_desaturation, add_append,
set_cast_shadows): **deleted at the manage_asset classing (2026-07-05)** —
`IsMaterialAuthoringAction` never routed them (not in `MaterialAuthoring()`), so
they were transport-dead; recover from git. `add_desaturation` was the only
reader of the advertised `luminanceFactors` param — that param is KEPT in
McpTool_ManageAsset.cpp (so the classing commit holds the golden byte-identical)
and exempted in the param-reconciliation allowlist; it is now a dead advertised
param, an advertise-or-remove candidate parked for Aaron. Same for `targetPin`
(McpTool_ManageAudio.cpp), whose only reader was the shadowed
HandleConnectMaterialPins deleted alongside the eight material arms.

### NiagaraGraphHandlers
- **set_parameter** @:436 - Dead/unreachable 'set exposed Niagara System user parameter' family (lines 436-500): given a parameterName and a numeric/bool value, it looks up the System's exposed user-parameter store (FNiagaraUserRedirectionParameterStore) and sets it if the parameter is typed Float or Bool, returning PARAM_FAIL…

### RenderHandlers (updated at the system_control classing)
All three unreachable HandleRenderAction subactions were deleted when `lumen_update_scene`
was extracted to `HandleRenderLumenUpdateScene`; disposition for each: **deleted at
system_control classing (transport-dead); recover from git history if advertising is
wanted.**
- **attach_render_target_to_volume** @:215 - Family: 'attach a render target to a PostProcessVolume via a dynamic material instance'. Reads volumePath (finds an APostProcessVolume actor via FindObject), targetPath (LoadObject a UTextureRenderTarget2D), materialPath+parameterName (LoadObject a base UMaterialInterface), then creates a UMaterialI… Advertise-candidate (no advertised equivalent anywhere).
- **create_render_target** @:87 - Hidden duplicate: `manage_asset.create_render_target` is advertised and reaches its own live implementation (AssetWorkflowHandlers.cpp:355). This copy was unreachable (below the sweep inventory's radar because the advertised sibling name exists on another tool).
- **nanite_rebuild_mesh** @:291 - Hidden duplicate: `manage_asset.nanite_rebuild_mesh` is advertised and reaches `HandleNaniteRebuildMesh` (AssetWorkflowHandlers.cpp:5054). This copy was unreachable (same below-radar class as create_render_target).

### SkeletonHandlers (updated at the animation_physics classing, 2026-07-05)
All twelve entries below were **deleted at the animation_physics classing (2026-07-05)**
— the dispatcher arms died with the chain and the ten dedicated member implementations
(HandleSetPhysicsAsset, HandleRemovePhysicsBody, HandleGetPhysicsAssetInfo,
HandleSetMorphTargetValue, HandleListMorphTargets, HandleDeleteMorphTarget,
HandleDeleteSocket, HandleGetBoneTransform, HandleListVirtualBones,
HandleDeleteVirtualBone; each had exactly the branch, its definition, and its header
declaration as its three references) died as their single callers vanished;
preview_physics was an inline echo stub. Disposition per the sweep's quick-dispositions:
the read/delete family stays an **advertise candidate parked for Aaron** (real,
distinct capabilities with no advertised equivalent — recover from git);
preview_physics was a stub, delete-class.
- **set_physics_asset** @:3179 - HandleSetPhysicsAsset (McpAutomationBridge_SkeletonHandlers.cpp:2266-2323, ~58 lines) assigns an EXISTING UPhysicsAsset to a USkeletalMesh via Mesh->SetPhysicsAsset(PhysAsset) and saves the mesh. Distinct from the advertised create_physics_asset (which builds a brand-new physics asset via HandleCrea…
- **remove_physics_body** @:3183 - HandleRemovePhysicsBody (SkeletonHandlers.cpp:2330-2414, ~85 lines) finds the SkeletalBodySetup for a given boneName in a UPhysicsAsset, removes any ConstraintSetup entries referencing that bone, removes the body setup, and rebuilds the bounds/index maps before saving. add_physics_body (advertised) …
- **get_physics_asset_info** @:3187 - HandleGetPhysicsAssetInfo (SkeletonHandlers.cpp:3014-3097, ~84 lines) loads a UPhysicsAsset (by physicsAssetPath or via a skeletal mesh's assigned asset) and reports per-body geometry counts (spheres/boxes/capsules/convex, physics type) plus per-constraint bone pairs. list_physics_bodies (advertised…
- **set_morph_target_value** @:3213 - HandleSetMorphTargetValue (SkeletonHandlers.cpp:2421-2534, ~114 lines) finds a live actor by name in the editor world, gets its USkeletalMeshComponent, and calls SetMorphTarget(name, value) at runtime (a live per-instance weight, optionally auto-adding curves via addMissing) - distinct from the adve…
- **list_morph_targets** @:3217 - HandleListMorphTargets (SkeletonHandlers.cpp:2643-2694, ~52 lines) loads a USkeletalMesh and enumerates Mesh->GetMorphTargets(), reporting each target's name and vertex-delta count. create_morph_target/set_morph_target_deltas/import_morph_targets are advertised but write-only operations on different…
- **delete_morph_target** @:3221 - HandleDeleteMorphTarget (SkeletonHandlers.cpp:2701-2763, ~63 lines) finds a named UMorphTarget on a skeletal mesh and removes it via Mesh->UnregisterMorphTarget, then saves. No advertised action performs deletion of a morph target.
- **delete_socket** @:3225 - HandleDeleteSocket (SkeletonHandlers.cpp:2541-2636, ~96 lines) removes a named socket from a USkeleton's Sockets array, resolving the skeleton either via a skeletal mesh's GetSkeleton() or directly via skeletonPath. No advertised action deletes a socket - create_socket/configure_socket only create o…
- **remove_socket** @:3225 - Same HandleDeleteSocket family described under delete_socket (SkeletonHandlers.cpp:2541-2636). delete_socket and remove_socket are two unadvertised spellings for the same unreachable delete-socket operation - both would need the same advertise-or-delete decision together.
- **get_bone_transform** @:3229 - HandleGetBoneTransform (SkeletonHandlers.cpp:2770-2870, ~101 lines) resolves a FReferenceSkeleton (from a skeletal mesh or skeleton asset), looks up a bone by name, and reports its ref-pose location/rotation/scale plus parent bone name. set_bone_transform is advertised but is a write-only sibling (H…
- **list_virtual_bones** @:3233 - HandleListVirtualBones (SkeletonHandlers.cpp:2877-2939, ~63 lines) resolves a USkeleton (directly or via a skeletal mesh) and enumerates Skeleton->GetVirtualBones(), reporting each virtual bone's name/source bone/target bone. create_virtual_bone is advertised but only creates (HandleCreateVirtualBon…
- **delete_virtual_bone** @:3237 - HandleDeleteVirtualBone (SkeletonHandlers.cpp:2946-3007, ~62 lines) finds a named virtual bone on a USkeleton and removes it via Skeleton->RemoveVirtualBones. No advertised action performs deletion of a virtual bone - create_virtual_bone only creates.
- **preview_physics** @:3908 - Inline stub at SkeletonHandlers.cpp:3907-3942 (~35 lines): loads skeletalMeshPath (or legacy skeletonPath) via LoadObject, errors if missing, and otherwise just echoes back assetPath/previewRequested/mode:"editor_asset_verification" - it does not toggle any real physics-simulation preview state, jus…

### SplineHandlers (updated at the build_environment classing)
- **create_spline_mesh_actor** @:1992 - HandleCreateSplineMeshActor (SplineHandlers.cpp :1316-1431, ~116 lines) spawns a brand-new plain AActor at a given location/rotation, creates a USplineMeshComponent as its root component (distinct from the advertised create_spline_mesh_component path, HandleCreateSplineMeshComponent, which adds a sp… Disposition: **deleted at the build_environment classing (2026-07-05)** — the name is not in the `Splines()` routing list, so it was transport-dead (the dispatcher arm and the free function both died); advertise candidate parked for Aaron, recover from git.

### LightingHandlers (added at the build_environment classing, 2026-07-05)
Two dead disjuncts inside the retired `HandleLightingAction` chain, below the sweep
inventory's radar (compound `||` arms of advertised branches). Both **deleted at the
build_environment classing; recover from git history if advertising is wanted.**
- **spawn_light** @:217 - `if (Lower == TEXT("spawn_light") || Lower == TEXT("create_light"))` — rule 6's parked internal literal: the manage_level create_light payload rewrite that targeted it became a typed `HandleLightingAction(..., TEXT("create_light"), ...)` call at manage_level's classing (2026-07-04), and `spawn_light` is not in the `Lighting()` routing list, so no path could produce the spelling. The body lives on as `HandleLightingCreateLight` (its UE_LOG strings keep the historical `spawn_light:` prefix).
- **bake_lightmap** @:736 - `else if (Lower == TEXT("build_lighting") || Lower == TEXT("bake_lightmap"))` — `bake_lightmap` sits in `BuildEnvironmentCore()`, not `Lighting()`, so the registration lambda always routed it to the env dispatcher's `HandleBakeLightmap` (the BUILD_LIGHTING editor-function implementation); this Lighting-chain copy (console `BuildLighting <quality>` exec) was unreachable. Deliberately-kept sibling implementation per the de-alias pass ("two different implementations, not aliases"): `build_lighting` keeps this body as `HandleLightingBuildLighting`; `bake_lightmap` keeps `HandleEnvironmentBakeLightmap`.

### SystemControlHandlers (updated at the system_control classing)
- **export_asset** @:1161 - Complete, fully-implemented generic asset-export feature (SystemControlHandlers.cpp:1161-1340, ~180 lines) living inside HandleSystemControlAction. Given assetPath + exportPath params, it: sanitizes both paths (SanitizeProjectRelativePath/SanitizeProjectFilePath), enforces the resolved export path s… Disposition: **deleted at system_control classing**; advertise-candidate — generic asset exporter, recover from git.

### TextureHandlers
- **import_texture** @:2830 - TEXTURE CREATION family, 'import_texture' arm (lines 2830-2860, ~31 lines): loads an existing UTexture2D via UEditorAssetLibrary::LoadAsset(sourcePath); if not found but the sourcePath file exists on disk, returns a fake-success 'import queued' stub noting real AssetTools import isn't implemented; o…
- **set_texture_filter** @:2862 - TEXTURE SETTINGS family, 'set_texture_filter' arm (lines 2862-2901, ~40 lines): loads a UTexture2D by assetPath, maps a 'filter' string (Nearest/Bilinear/Trilinear/Default) to TextureFilter enum, applies via PreEditChange/PostEditChange + UpdateResource, optionally saves.
- **set_texture_wrap** @:2903 - TEXTURE SETTINGS family, 'set_texture_wrap' arm (lines 2903-2942, ~40 lines): loads a UTexture2D by assetPath, maps a 'wrapMode' string (Wrap/Clamp/Mirror) to AddressX/AddressY TextureAddress enums, applies via PreEditChange/PostEditChange + UpdateResource, optionally saves.
- **create_cube_texture** @:3158 - TEXTURE CREATION family, 'create_cube_texture' arm (lines 3158-3174, ~17 lines): validates 'name' is present then unconditionally returns an UNSUPPORTED_OPERATION error stating cube maps aren't implemented for generated assets -- a permanent stub, does no actual work regardless of inputs.
- **create_volume_texture** @:3176 - TEXTURE CREATION family, 'create_volume_texture' arm (lines 3176-3194, ~19 lines): validates 'name' is present then unconditionally returns an UNSUPPORTED_OPERATION error stating volume textures aren't implemented for generated assets -- a permanent stub.
- **create_texture_array** @:3196 - TEXTURE CREATION family, 'create_texture_array' arm (lines 3196-3214, ~19 lines): validates 'name' is present then unconditionally returns an UNSUPPORTED_OPERATION error stating texture arrays aren't implemented for generated assets -- a permanent stub.

### UiHandlers (updated at the system_control classing)
All nine unreachable HandleUiAction branches were deleted when the five advertised bodies
were extracted to `HandleUi*` members; disposition for each: **deleted at system_control
classing (transport-dead); recover from git history if advertising is wanted.**
- **play_in_editor** @:500 - PIE-control pair (with stop_play): GEditor->Exec(TEXT("Play In Editor")) guarded by a GEditor->PlayWorld already-playing check; returns status "playing". Lines 500-519 (20 lines).
- **stop_play** @:523 - PIE-control pair (with play_in_editor): GEditor->Exec(TEXT("Stop Play In Editor")) guarded by a GEditor->PlayWorld check (errors NOT_PLAYING if not currently playing). Lines 523-543 (21 lines).
- **save_all** @:547 - GEditor->Exec(TEXT("Asset Save All")). Not in the sweep's original inventory: `control_editor.save_all` advertises the name against a different implementation (`HandleControlEditorSaveAll`), which stays live.
- **simulate_input** @:563 - Raw FSlateApplication ProcessKeyDown/UpEvent by key name. Not in the sweep's original inventory: `control_editor.simulate_input` advertises the name against a different implementation (`HandleControlEditorSimulateInput`), which stays live.
- **create_hud** @:615 - Loads a UUserWidget subclass from a widgetPath payload field via LoadClass, instantiates it in the GameViewport world via CreateWidget, and calls AddToViewport — a minimal HUD-spawn helper distinct from the advertised create_hud_widget/create_hud_class blueprint-authoring actions. Lines 615-641 (27 …
- **set_widget_text** @:645 - Text-setter for TextBlock widgets by name: first walks all UUserWidget instances in the editor world and the game-viewport world via UWidgetBlueprintLibrary::GetAllWidgetsOfClass, calling GetWidgetFromName and casting to UTextBlock; if not found, falls back to a raw TObjectIterator<UTextBlock> globa…
- **set_widget_image** @:704 - Image-setter for Image widgets by name: loads a UTexture2D from a texturePath field, then does a raw TObjectIterator<UImage> global scan matching GetName()==key and calls SetBrushFromTexture. Lines 704-728 (25 lines).
- **set_widget_visibility** @:732 - Visibility toggle by widget name: raw TObjectIterator<UUserWidget> scan matching GetName()==key first, then falls back to a broader TObjectIterator<UWidget> scan; sets ESlateVisibility::Visible or Collapsed based on a 'visible' bool field. Lines 732-769 (38 lines).
- **remove_widget_from_viewport** @:855 - Widget removal by optional 'key': if key is empty, enumerates all UUserWidgets in the game-viewport world via UWidgetBlueprintLibrary::GetAllWidgetsOfClass and calls RemoveFromParent on each (a dead TempWidgets query against the editor world is computed but unused beforehand); if key is given, does …

### LevelHandlers (manage_level — appended at its classing)
The manage_level dispatcher carried 13 unadvertised branches the schema enum gate
rejected before dispatch (transport-dead; they sat below the sweep's inventory because
the dispatcher's own prologue manufactured their `EffectiveAction` literals). All 13
were deleted at the manage_level classing; disposition for each: **deleted at
manage_level classing (transport-dead); recover from git history if advertising is
wanted.**
- **get_level_info** - direct-name arm of the `get_summary` implementation (level
  info + GameMode/default-pawn readback); the capability stays advertised as
  `get_summary`.
- **set_level_world_settings** - applies a JSON property map to the current level's
  AWorldSettings via reflection (`worldSettings`/`settings`/`properties` object or
  flat payload fields).
- **set_level_lighting** - same AWorldSettings reflection write aimed at lighting
  state (LightmassSettings etc.) via a `lighting`/`lightingSettings` object.
- **add_level_to_world** - adds a level package as a ULevelStreamingDynamic sublevel;
  thinner sibling of the advertised `add_sublevel` (no duplicate/broken-reference
  recovery, no streaming-load verification).
- **remove_level_from_world** - removes a loaded level from the world via
  UEditorLevelUtils::RemoveLevelFromWorld.
- **set_level_visibility** - toggles a loaded level's editor visibility via
  UEditorLevelUtils::SetLevelVisibility.
- **set_level_locked** - locks/unlocks a loaded level via
  FLevelUtils::ToggleLevelLock.
- **get_level_actors** - lists actor names (+count) in a loaded level.
- **get_level_bounds** - reads the LevelBounds actor's bounding box min/max.
- **get_level_lighting_scenarios** - lists levels flagged bIsLightingScenario.
- **build_level_lighting** - kicks FEditorBuildUtils BuildLighting; subset of the
  advertised `build_lighting` (no quality echo).
- **build_level_navigation** - kicks FEditorBuildUtils BuildAIPaths; no advertised
  navigation-build sibling on manage_level.
- **build_all_level** - kicks FEditorBuildUtils BuildAll.

### WorldPartitionHandlers
- **create_datalayer** @:347 - manage_level_structure (live, advertised, dispatched at McpAutomationBridgeSubsystem.cpp:1222 -> HandleManageLevelStructureAction) has a genuinely-working, differently-spelled sibling `create_data_layer` (with underscore) at McpAutomationBridge_LevelStructureHandlers.cpp:3128-3131 -> HandleCreateDat…
- **set_datalayer** @:446 - The live, advertised sibling doing the analogous operation is manage_level_structure's `assign_actor_to_data_layer` (tools-schema.golden.json:8503, dispatched McpAutomationBridge_LevelStructureHandlers.cpp:3132-3135 -> HandleAssignActorToDataLayer, impl :1906-2093). It looks up the actor by name/lab…
- **cleanup_invalid_datalayers** @:563 - No analogous cleanup subaction exists at all in the live manage_level_structure world-partition family (enable_world_partition, configure_grid_size, create_data_layer, assign_actor_to_data_layer, configure_hlod_layer, create_minimap_volume — checked schema + dispatcher at LevelStructureHandlers.cpp:…



