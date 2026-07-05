# Action declarations — the server's contract

**Status: SHIPPED 2026-07-04 (Stage 1).**

The server owns a single statement of "here's what I know how to do": one
declaration per `(tool, action)` pair, listing the params that action reads and
which are required. Everything else derives from it. This replaced the
parsed-table system (a regex parser extracting truth from handler source) after
the architecture discussion of 2026-07-04: declarations are *authored*; the
parser survives only as a lint.

```
        McpDecl_*.h  (Private/MCP/Decls/ — one per tool family)
        /        |          \
   transport   published    action-decl lint
   validation  schema       (handler source vs declarations,
   (rejects    (Stage 3:    both directions)
   undeclared  derive and
   params)     delete the hand-built bags)
```

## The pieces

- **`McpCallRegistry.h`** — vocabulary (`FMcpParamDecl`, `FMcpCallDecl`,
  `EMcpCallFlags`) plus the registry. `McpRegisterAllActionDecls()` runs before
  the transport starts; duplicates are boot errors.
- **`Private/MCP/Decls/McpDecl_<Tool>.h`** — the declarations, one header per
  tool family. Bootstrapped by a 57-agent fleet reading the handler source
  (three-witness cross-check against the old parsed table and the published
  schema), **hand-maintained since**: adding an action means adding its
  declaration here.
- **Transport check** (`McpNativeTransport.cpp`) — params not in the called
  action's declaration reject with `INVALID_PARAMS`, the accepted list, and
  both fix paths. `bypassParamCheck:true` downgrades to response-visible
  `paramWarnings` for the wrong-declaration case (fixing a declaration needs a
  rebuild; a false rejection must not strand a task). `UnverifiedDecl` entries
  are skipped by validation — never enforce unattributed truth.
- **`FMcpCall`** — base class for fully-migrated actions (declaration and
  implementation co-located, Chromium-ExtensionFunction-style). New actions
  MUST be classes; legacy actions are shims (declaration + legacy family
  dispatch) that convert per-family, opportunistically.
  `Execute()` is the shared pipeline (payload-null check, `RequiresEditor`
  gate → `Run()`); `ProcessAutomationRequest` consults `FindCall(tool,
  action)` before the legacy handler map, so classed actions win per-action
  while the rest of a family stays shimmed.
- **Classed families** (each deleted its string-dispatch chain, shim decl
  header, and handler-map registration in its landing commit; classed decls
  are lint-visible via the `// LINT-TOOL:` marker convention in `MCP/Calls/`):
  - **`control_actor` (2026-07-04, the pilot,
    `Private/MCP/Calls/McpCalls_ControlActor.cpp`).** 27 classes; each carries
    its true per-action contract (the bootstrap's 33-param mega-bag unions died
    here — TODO 04e resolved for this family); `Run()` delegates to the
    subsystem's dedicated handlers until the module split de-members those
    bodies. `inspect`'s actor-action forwarding converted from payload
    re-dispatch to typed direct calls (migration rule 5 in practice).
  - **`manage_sequence` (2026-07-04,
    `Private/MCP/Calls/McpCalls_ManageSequence.cpp`).** 32 classes; the
    dispatcher's 32 internally-manufactured `sequence_*` names died with the
    chain (the last of that prefix family), and its four inline-only bodies
    (`list_track_types`, `add_track`, `list_tracks`, `set_work_range`) were
    extracted to dedicated members. Flags are authored per action here —
    `RequiresEditor` everywhere except pure-reflection `list_track_types`,
    `Mutating` on writers — where the shim decls carried `None`.
  - **`control_editor` (2026-07-04,
    `Private/MCP/Calls/McpCalls_ControlEditor.cpp`).** 35 classes (34 handlers —
    `step_frame`/`single_frame_step` share one); this closed TODO 04e for good:
    the shim decls carried two identical 30-param mega-bag unions
    (`execute_command`, whose true contract is `command*` — a param the bag
    didn't even contain — and `single_frame_step`, whose true contract is
    empty) plus four contaminated rows (`possess`, `screenshot`,
    `simulate_input`, `simulate_nav`). All 35 contracts re-authored from the
    handler bodies' actual reads. `RequiresEditor` on all; `Mutating` on
    `undo`/`redo`/`save_all`/`execute_command`. PIE-only preconditions
    (pause/possess/simulate_nav/…) stay handler-enforced — the flag only
    covers GEditor existing.
  - **`manage_level` (2026-07-04,
    `Private/MCP/Calls/McpCalls_ManageLevel.cpp`).** 19 classes; this family
    had no dedicated handler functions at all — all 18 live implementations
    were inline branches in a 2,960-line dispatcher monolith, extracted to
    `HandleLevel*` members (stream/unload share `HandleLevelStreamInternal`,
    replicating unload's force-override semantics). The `set_metadata`
    declaration's `metadata`-object omission — a live false-reject that
    blocked the action's own payload — is fixed, and `create_light` is
    deduped onto the full LightingHandlers implementation via a typed call
    (migration rule 5; the SPAWN_ACTOR_AT_LOCATION payload rewrite that
    dropped `properties`/`name`/SkyLight support died with the chain). 13
    transport-dead hidden branches (`set_level_visibility`,
    `get_level_bounds`, `build_level_navigation`, …) were deleted and
    ledgered in `docs/dead-name-sweep-2026-07-04.md`.
  - **`system_control` (2026-07-04,
    `Private/MCP/Calls/McpCalls_SystemControl.cpp`).** 44 classes spanning six
    implementation TUs (Performance/SystemControl/Ui/Log/Debug/Render
    handlers) — the first classed family whose routing-table entry keeps a
    non-empty routed list: `Performance()` and `SystemControlCore()` survive
    in `GetToolRoutingTable()` solely so boot validation can prove the schema
    union still matches the published enum (no live lambda remains). Flags
    are authored per action and deliberately mixed: `RequiresEditor` on the
    33 actions whose implementing TUs are editor-gated; NOT on the five log
    actions (LogHandlers.cpp is not editor-gated — flagging them would
    regress headless use), `screenshot`, `execute_command`, `set_cvar`,
    `get_project_settings`, `set_project_setting`, `spawn_category`
    (handler-enforced preconditions). Two decls drop mega-bag residue: a
    bogus required `functionName` on `configure_texture_streaming` and
    `generate_memory_report` that neither handler reads. `set_cvar`'s inline
    lambda synthesis became `HandleSysSetCvar`; it and `execute_command`
    call `HandleConsoleCommandAction` directly — the `console_command`
    internal literal SURVIVES this classing (the console handler has other
    owners: ControlHandlers.cpp, EditorFunctionHandlers.cpp). Hidden bodies
    deleted and ledgered: `export_asset` (~180 lines, advertise-candidate),
    nine unreachable UiHandlers branches (`play_in_editor`, `stop_play`,
    `save_all`, `simulate_input`, `create_hud`, `set_widget_text`,
    `set_widget_image`, `set_widget_visibility`,
    `remove_widget_from_viewport`), and three unreachable RenderHandlers
    subactions (`create_render_target`, `attach_render_target_to_volume`,
    `nanite_rebuild_mesh` — the first and last are duplicates of live
    manage_asset implementations). The vestigial `test_progress`/`test_stale`
    gate names (no branches ever existed) died with the gate.
  - **`inspect` (2026-07-04,
    `Private/MCP/Calls/McpCalls_Inspect.cpp`).** 38 classes — the cleanest
    family yet: zero hidden and zero dead names (the sweep ledger has no
    inspect entries). The dispatcher's 13 inline global bodies extracted to
    `HandleInspect*` members (EnvironmentHandlers.cpp); `runtime_report` and
    `pie_report` share one member like they shared one branch; the eight
    detail actions (`inspect_object` + the seven `get_*_details`) are
    advertised aliases of one generic body (`HandleInspectObjectGeneric`) with
    byte-identical output — collapsing them to one name is a product decision,
    deferred. The twelve actor actions keep their typed direct calls into the
    shared control_actor handlers (rule 5 was already delivered at the pilot);
    each class now applies the actorName/name/objectPath alias normalization
    itself. `set_property`/`get_property` delegate to the 4-arg property
    handlers under their internal `set_object_property`/`get_object_property`
    keys, which survive (the handlers dispatch on them and have other owners).
    Decl burn-down: `list_objects` shed a 33-param mega-bag with seven bogus
    required fields (the body reads nothing — the contract is now `{}`);
    `pie_report` shed six unread params by adopting runtime_report's contract;
    the target aliases went uniformly optional (`add_tag`/`create_snapshot`/
    `get_component_property` *required* `actorName`, `inspect_object`
    *required* `objectPath` — each false-rejected the other spellings the
    prologue accepted); `get_component_property.propertyName` is optional
    (`propertyPath` is the alternative, matching control_actor's own row).
    `RequiresEditor` on all 38; `Mutating` on the seven writers
    (`set_property`, `set_component_property`, `add_tag`, `create_snapshot`,
    `restore_snapshot`, `delete_object`, `export`).
  - **`manage_effect` (2026-07-04,
    `Private/MCP/Calls/McpCalls_ManageEffect.cpp`).** 58 classes — the largest
    family yet, spanning three implementation TUs. The dispatcher's 16 inline
    bodies extracted verbatim to `HandleEffect*` members (EffectHandlers.cpp);
    the 36 Niagara authoring actions delegate to
    `HandleManageNiagaraAuthoringAction` (NiagaraAuthoringHandlers.cpp) and
    the three graph actions to `HandleNiagaraGraphAction`
    (NiagaraGraphHandlers.cpp, after the same subAction rewrite the chain
    did) — the manufactured `manage_niagara_authoring`/`manage_niagara_graph`
    gate literals SURVIVE inside those delegate classes (the live handlers
    gate on them), while the chain's internal `create_effect` self-recursion
    key is dead. `activate`/`activate_effect` are advertised aliases of
    `activate_niagara`'s member and `deactivate` of `deactivate_niagara`'s
    (the extracted members ignore the action spelling, so the chain's payload
    action/subAction rewrites died too). Decl burn-down:
    `create_niagara_system`'s shim row was mis-authored with a
    `set_niagara_parameter`-shaped contract (required `parameterName` plus
    `systemName`/`parameterType`/`value`); the live branch requires `name`
    and reads the same common param block as `create_niagara_emitter`, so
    both now share one contract. The other 35 authoring rows and both graph
    rows verified against their live branches unchanged. Zero hidden and
    zero dead names remained on this family's surface (the lying
    numbered-stub block was already deleted at the 2026-07-04 sweep).
    `RequiresEditor` on all 58; `Mutating` on the 52 writers — the readers
    are `get_niagara_info`, `validate_niagara_system`, `list_debug_shapes`,
    and the transient debug-draw trio (`debug_shape`, `particle`,
    `clear_debug_shapes`).
  - **`manage_interaction` (2026-07-05,
    `Private/MCP/Calls/McpCalls_ManageInteraction.cpp`).** 22 classes; the
    dispatcher had zero dedicated handler functions — all 22 bodies were
    inline branches, extracted verbatim to `HandleInteraction*` members
    (InteractionHandlers.cpp; the five column-0-indented branches from an
    earlier sweep were re-indented to member style, whitespace only). No
    cross-file delegation in either direction, no hidden or dead names. Five
    actions are shallow scaffolds preserved as-is at classing: the
    `configure_destruction_levels`/`effects`/`damage` trio (plus
    `setup_destructible_mesh`) only writes an `MCP_Destruction*Configured`
    marker tag on the actor, and `configure_trigger_filter`/`response` only
    scaffold filter/response variables without writing defaults — deepening
    or retiring them is a product decision logged in TODO.md. Contracts
    ported verbatim from the shim rows (all re-verified against the extracted
    bodies), with one decl fix shipped at classing: the configure
    door/switch/chest property rows had required their primary path spelling
    although each handler also accepts the `blueprintPath` fallback — both
    spellings are optional now, with the at-least-one requirement
    handler-enforced. `RequiresEditor` on all 22; `Mutating`
    on everything except the reader, `get_interaction_info`.
  - **`manage_character` (2026-07-05,
    `Private/MCP/Calls/McpCalls_ManageCharacter.cpp`).** 27 classes; the
    dispatcher had zero dedicated handler functions — all 27 bodies were
    inline branches, extracted verbatim to `HandleCharacter*` members
    (CharacterHandlers.cpp). The chain's whole-handler `#if !WITH_EDITOR`
    stub is replicated per member (same message and code), and the prologue's
    common name/path/blueprintPath reads moved into the members that use
    them: `create_character_blueprint` keeps `name`/`path` (default
    `/Game`), the other 26 keep `blueprintPath`. No cross-file delegation in
    either direction, no hidden or dead names (no sweep-ledger entries).
    Contracts ported verbatim from the shim rows — the cleanest of the small
    families: required flags faithful (`blueprintPath` on the 26
    configure/set/setup rows, `name` on `create_character_blueprint`), no
    mega-bags, no one-of collisions. The prologue spellings a body does not
    read (`name`/`path` on the 26, `blueprintPath` on create) stay
    declared-optional so payloads the family accepts today keep validating.
    `RequiresEditor` on all 27; `Mutating` on everything except the reader,
    `get_character_info`.
  - **`manage_combat` (2026-07-05,
    `Private/MCP/Calls/McpCalls_ManageCombat.cpp`).** 39 classes; the
    dispatcher had zero dedicated handler functions — all 39 bodies were
    inline branches, extracted verbatim to `HandleCombat*` members
    (CombatHandlers.cpp). The chain's whole-handler `#if !WITH_EDITOR`
    stub is replicated per member (same message and code), and the
    prologue's common name/path/blueprintPath reads moved into the members
    that use them: the five creators (`create_weapon_blueprint`,
    `create_projectile_blueprint`, `create_damage_type`,
    `setup_damage_type`, `create_damage_effect`) keep `name`/`path`
    (default `/Game`), `setup_hitbox_component` keeps `name` alongside
    `blueprintPath`, and the other 33 keep `blueprintPath`. No
    cross-file delegation in either direction, no hidden or dead names (no
    sweep-ledger entries). Contracts ported verbatim from the shim rows —
    all 39 re-verified against the extracted bodies, no decl fixes needed:
    required flags faithful (`blueprintPath` on the 34 rows that resolve
    a Blueprint, `name` on the five creators), no mega-bags, no one-of
    collisions, and no declared-optional-but-unread artifact — unlike
    manage_character, the shim rows never declared the prologue spellings
    a body does not read. `RequiresEditor` on all 39; `Mutating` on
    everything except the readers, `get_combat_info` and
    `get_combat_stats`.
  - **`manage_inventory` (2026-07-05,
    `Private/MCP/Calls/McpCalls_ManageInventory.cpp`).** 33 classes; the
    dispatcher had zero dedicated handler functions — all 33 bodies were
    inline branches, extracted verbatim to `HandleInventory*` members
    (InventoryHandlers.cpp). Unlike combat/character there was no
    whole-handler editor gate to replicate (the TU carries no
    `#if WITH_EDITOR` at all) and no common-parameter prologue to move —
    the chain's only shared read was `subAction`, which died with it.
    Every save/compile tail is preserved verbatim (`McpFinalizeBlueprint`
    on the Blueprint-scaffold actions, save-gated `McpSafeAssetSave` on
    the data-asset actions, compile-then-write-CDO sequencing where
    defaults are set). No cross-file delegation in either direction, no
    hidden or dead names (no sweep-ledger entries). Contracts ported
    verbatim from the shim rows — all 33 re-verified against the extracted
    bodies, no decl fixes needed: required flags faithful, no mega-bags,
    no one-of collisions, and no declared-optional-but-unread artifact
    (`get_inventory_info`'s five resolver spellings and
    `remove_loot_entry`'s entryIndex-or-itemPath one-of stay
    handler-enforced; `configure_inventory_weight` declares both
    encumbrance spellings because the body reads the historical
    'encumberance' (sic) fallbacks). First classed family with NO
    `RequiresEditor` anywhere — the TU is not editor-gated, and flagging
    would newly reject GEditor-less runs the shim served; `Mutating` on
    everything except the reader, `get_inventory_info`.
  - **`manage_gas` (2026-07-05,
    `Private/MCP/Calls/McpCalls_ManageGas.cpp`).** 31 classes; the
    dispatcher had zero dedicated handler functions — all 31 advertised
    bodies were inline branches, extracted verbatim to `HandleGas*`
    members (GASHandlers.cpp). The chain's whole-handler gate is richer
    than combat/character's and is replicated per member: the three-way
    `#if !WITH_EDITOR` (EDITOR_ONLY) / `#elif !MCP_HAS_GAS`
    (GAS_NOT_AVAILABLE) preprocessor stub pair plus the runtime
    GameplayAbilities module-loaded check (GAS_PLUGIN_NOT_ENABLED) —
    the TU's seven MCP_HAS_GAS occurrences resolve to one helpers block
    and one whole-dispatcher gate, so the per-member gate is uniform by
    evidence. Prologue reads moved into the members that use them: the
    five creators keep `name`/`path` (default `/Game`), 21 members keep
    the `blueprintPath` read with its
    attributeSetPath/effectPath/abilityPath/cuePath alias-fallback loop,
    `add_tag_to_asset`/`get_gas_info` keep `assetPath`, and
    `get_attribute`/`create_ability_set`/`add_ability` read no prologue
    spelling. One hidden name deleted and ledgered: `grant_ability`
    (transport-dead honest-error branch; advertise candidate parked for
    Aaron — see the dead-name-sweep ledger); the sweep's
    `get_attribute_value` dead disjunct was already gone at HEAD
    (deleted 2874d066). Decl burn-down — 24 of 31 rows fixed: the 21
    rows that required `blueprintPath` were contaminated (the
    dispatcher resolves it through the four alias spellings, so all
    five are optional now, at-least-one handler-enforced), and three
    one-of contracts were declared all-required — `add_tag_to_asset`'s
    tag/tagName, `add_ability`'s abilityPath/abilityClass (`setPath`
    stays required), `create_ability_set`'s setPath/assetPath — each
    now both-optional with the one-of handler-enforced. The prologue
    spellings a body does not read stay declared-optional so payloads
    the family accepts today keep validating. `RequiresEditor` on all
    31 (the TU is whole-handler editor-gated); `Mutating` on everything
    except the readers, `get_attribute` and `get_gas_info`.
  - **`manage_level_structure` (2026-07-05,
    `Private/MCP/Calls/McpCalls_ManageLevelStructure.cpp`).** 45 classes —
    the second classed family with a non-empty routed list (after
    system_control): the registration lambda that split `Volumes()` actions
    off to `HandleManageVolumesAction` was deleted with both string
    dispatchers, the `IsVolumeAction` predicate died with its only caller,
    and the
    `Volumes`/`ManageLevelStructureCore` routing lists survive only so boot
    validation can prove the schema union still matches the published enum.
    Unlike the inline-branch families, both dispatchers already delegated
    every branch to a dedicated static free function in its TU, so no body
    moved: the 45 members are thin wrappers — 17 `HandleLevelStructure*`
    (LevelStructureHandlers.cpp) + 28 `HandleVolume*` (VolumeHandlers.cpp) —
    each replicating its chain's whole-dispatcher `#if WITH_EDITOR` stub
    (same message and code), the two post-process members also replicating
    the per-branch `MCP_HAS_POSTPROCESS_VOLUME` UNSUPPORTED_VERSION stub.
    The inbound edge converted per rule 5: manage_level's classed
    `create_level` re-entered this family's dispatcher with a synthesized
    `subAction` payload; it now calls `HandleLevelStructureCreateLevel`
    directly and the `subAction` write died with the chain. One decl fix:
    `set_volume_extent` declared BOTH `volumeExtent` AND `extent` required
    although the handler resolves them as a one-of alias pair and rejects
    only when neither is present — both optional now, `volumeName` stays
    required. The other 44 rows verified handler-true, including the
    pervasive optional volumeLocation/location + volumeExtent/extent alias
    pairs (VolumeHelpers first-present-wins reads). Zero hidden and zero
    dead names (the ledger's three manage_level_structure mentions are
    WorldPartitionHandlers entries citing this family as the live sibling).
    `RequiresEditor` baked on all 45 (both TUs are whole-editor-gated);
    `Mutating` on everything except the readers, `get_level_structure_info`
    and `get_volumes_info` (`open_level_blueprint` stays a writer — it
    lazily creates the Level Script Blueprint).
  - **`manage_networking` (2026-07-05,
    `Private/MCP/Calls/McpCalls_ManageNetworking.cpp`).** 72 classes — the
    largest classed family and the third with a non-empty routed list: the
    registration lambda that split `Input()`/`GameFramework()`/`Sessions()`
    actions off to their dispatchers died with all FOUR string dispatchers,
    the `IsInputAction`/`IsGameFrameworkAction`/`IsSessionAction`
    predicates died with their only caller, and the routed lists plus
    `ManageNetworkingCore` survive only so boot validation can prove the
    schema union still matches the published enum. Three dispatchers were
    inline-branch monoliths, extracted verbatim to members in their home
    TUs: 27 `HandleNetworking*` (NetworkingHandlers.cpp; each member keeps
    the chain's `using namespace NetworkingHelpers` + `ResultJson`
    prologue), 9 `HandleInput*` (InputHandlers.cpp; each replicates the
    whole-dispatcher `#if WITH_EDITOR` gate WITH its EnhancedInput runtime
    module-load check and the NOT_AVAILABLE stub), 20 `HandleGameFramework*`
    (GameFrameworkHandlers.cpp; each replicates the `#if !WITH_EDITOR`
    EDITOR_ONLY stub, and the chain's prologue reads moved into the members
    that use them — the six creators keep `name`/`path`/`save` + the path
    sanitize, the thirteen GameMode-targeting members keep `save` + the
    gameModeBlueprint→blueprintPath first-present-wins alias resolution +
    its sanitize, `configure_player_start` also keeps the `BlueprintPath`
    sync line, `get_game_framework_info` keeps only the alias resolution).
    Sessions already delegated every branch to a dedicated static free
    function, so its 16 members are thin wrappers (level-structure
    precedent) replicating the chain's editor-build stub — the
    MCP_HAS_VOICECHAT/MCP_HAS_ONLINE_SUBSYSTEM ladders live inside the
    untouched free functions, not the dispatcher, so no per-member ladder
    exists. Decl burn-down — 13 of 72 rows fixed: twelve GameFramework rows
    declared the gameModeBlueprint/blueprintPath alias pair BOTH-required
    although each body joint-rejects only when neither is present (the nine
    configure/setup/respawn rows plus the three set_*_class rows, whose
    genuinely-required single class param stays required), and
    `set_default_pawn_class` declared BOTH of its one-of pairs all-required
    (gameModeBlueprint/blueprintPath AND pawnClass/defaultPawnClass) — all
    flipped optional with the at-least-one requirements handler-enforced.
    The other 59 rows are byte-identical to the retired shim rows and
    re-verified handler-true (`get_sessions_info` reads nothing — its
    contract stays `{}`). Zero hidden and zero dead names across all four
    dispatchers (the sweep ledger has no NetworkingHandlers, InputHandlers,
    GameFrameworkHandlers, or SessionsHandlers entries). Flags are
    deliberately mixed: `RequiresEditor` on the 45 Input/GameFramework/
    Sessions actions (their chains were whole-editor-gated); NOT on the 27
    core actions (NetworkingHandlers.cpp carries no editor gate — flagging
    would newly reject GEditor-less runs the shim served); `Mutating` on
    writers only — eleven no-op acknowledgment actions (nine Sessions
    echoes + Input's enable_input_mapping/disable_input_action) stay
    unflagged alongside the six readers, with deepen-or-retire TODO'd for
    Aaron.
  - **`build_environment` (2026-07-05,
    `Private/MCP/Calls/McpCalls_BuildEnvironment.cpp`).** 55 classes — the
    fourth classed family with a non-empty routed list: the registration
    lambda that split `Lighting()`/`Splines()` actions off to their
    dispatchers died with all three string dispatchers — deleted:
    `HandleBuildEnvironmentAction`, `HandleLightingAction`, and
    `HandleManageSplinesAction` — the `IsLightingAction`/`IsSplineAction`
    predicates are gone with their only caller, and the routed lists plus
    `BuildEnvironmentCore` survive only so boot validation can prove the
    schema union still matches the published enum. The 21 core actions
    became `HandleEnvironment*` members (EnvironmentHandlers.cpp): six
    inline bodies extracted verbatim (each replicating the chain's
    editor-build stub, with the chain's shared `Resp`/`bSuccess`/`Message`
    prologue and response tail moved into each member and the lowercased
    `LowerSub` echoes inlined to the action literal), 15 wrappers
    forwarding to the family's dedicated 4-arg members with the same
    internal action literals and payload re-shaping the retired branches
    used (foliage alias/`removeAll` re-shaping included). The 12 lighting
    actions extracted verbatim to `HandleLighting*` members, each
    replicating the chain's whole-dispatcher editor stub and
    EditorActorSubsystem precheck (the chain's payload-null precheck is
    now `FMcpCall::Execute`'s envelope check); the 22 spline actions are
    `HandleSpline*` thin wrappers (level-structure precedent) replicating
    the chain's EDITOR_ONLY stub. The old dispatchers matched
    case-insensitively (env/lighting lowercased the payload action; the
    splines chain used FString's IgnoreCase ==) — the registry's FindCall
    lookup is case-insensitive too, so dispatch behavior is preserved.
    The inbound edge converted per rule 5: manage_level's classed
    `create_light` called `HandleLightingAction(RequestId,
    TEXT("create_light"), ...)`; it now calls `HandleLightingCreateLight`
    directly. Hidden names deleted and ledgered: the Lighting chain's
    `spawn_light` disjunct (rule 6's parked internal literal — its last
    payload-rewrite caller converted to the typed call at manage_level's
    classing) and its `bake_lightmap` disjunct (that name routes to the
    env dispatcher's own `HandleBakeLightmap` implementation, never to the
    Lighting chain), plus SplineHandlers' `create_spline_mesh_actor`
    (~116-line hidden implementation, already ledgered) and the
    transport-dead `HandleControlEnvironmentAction` legacy dispatcher
    (set_time_of_day/set_sun_intensity/set_skylight_intensity; never
    registered, no callers). Decl burn-down — 2 of 55 rows fixed:
    `create_landscape_grass_type` declared BOTH meshPath AND staticMesh
    required although the body resolves them first-present-wins and
    joint-rejects only when neither is present; `modify_heightmap`
    declared heightData required although the body requires it only for
    the `set` operation and serves raise/lower/flatten without it. The
    scoping sweep's other two suspects verified handler-true: the
    configure_mesh_spacing/randomization rows keep `actorName` optional
    because an empty spelling falls back to the WorldSettings config
    target rather than rejecting. Flags deliberately mixed:
    `RequiresEditor` on the 12 Lighting + 22 Spline actions (whole-gated
    chains) and the six core inline extractions (editor-gated region of
    the retired chain); NOT on the 15 delegating core actions (their
    dispatch was ungated; the 4-arg members self-gate). `Mutating` on
    everything except the four readers — `get_foliage_instances`,
    `import_snapshot`, `list_light_types`, `get_splines_info`
    (bake_lightmap and build_lighting kick a lighting build and stay
    writers).
  - **`manage_ai` (2026-07-05,
    `Private/MCP/Calls/McpCalls_ManageAi.cpp`).** 61 classes — the family
    whose dispatcher was itself a router: `HandleManageAIAction` tested
    BehaviorTree/Navigation membership at the top of the function (the only
    caller of the `IsBehaviorTreeAction`/`IsNavigationAction` predicates,
    which died with its file-static wrappers) and forwarded to two sibling
    dispatchers, so all THREE string dispatchers are deleted —
    `HandleManageAIAction`, `HandleBehaviorTreeAction`, and
    `HandleManageNavigationAction` are gone. The 37 advertised inline
    bodies extracted verbatim to `HandleAi*` members (AIHandlers.cpp), each
    replicating the chain's whole-handler `#if !WITH_EDITOR` EDITOR_ONLY
    stub; the members that answer through the chain's shared `Result`
    object carry its creation line, and the per-branch feature ladders
    (MCP_HAS_STATE_TREE ×4, MCP_HAS_SMART_OBJECTS ×4, MCP_HAS_MASS_AI ×3,
    MCP_HAS_ENVQUERY_TESTS, MCP_AI_HAS_BEHAVIOR_TREE_GRAPH) ride inside the
    bodies unchanged. The five manufactured-rewrite wrappers became typed
    calls per rule 5: `create_behavior_tree` keeps its savePath-defaulting
    glue and calls `HandleBehaviorTreeCreate`;
    `add_composite_node`/`add_task_node` keep the enum→node-class mapping
    verbatim and call `HandleBehaviorTreeAddNode`;
    `add_decorator`/`add_service` split the shared block with the
    SubAction/bDecorator ternaries resolved per member and call
    `HandleBehaviorTreeAddSubnode` — the internal `create`/`add_node`/
    `add_subnode` subAction rewrites died with the chains (`create`
    survives only as the advertised action's own literal). The 7 Behavior
    Tree graph actions became `HandleBehaviorTree*` members
    (BehaviorTreeHandlers.cpp), each carrying the chain's
    BehaviorTreeEditor module check (its `SubAction != "create"` disjunct
    resolved to the member's constant) and the six graph members
    replicating the chain's shared asset-load prologue
    (assetPath→behaviorTreePath→path fallback, graph null-check, and the
    UpdateBehaviorTreeAsset/FindGraphNodeByIdOrName lambdas); the chain's
    payload-null check died (`FMcpCall::Execute` owns it). The 12
    navigation actions are `HandleNavigation*` thin wrappers
    (level-structure precedent) over the untouched static free functions,
    replicating the chain's EDITOR_ONLY stub. Hidden/dead deleted and
    ledgered: `set_ai_movement` (hidden CharacterMovement tuner; advertise
    candidate parked) and the dispatcher's shadowed inline
    `create_nav_link_proxy` copy (the Navigation router always won the
    name — NavigationHandlers' implementation is the live one). Decl
    burn-down — 7 of 61 rows fixed: the six BT graph rows (`add_node`,
    `add_subnode`, `break_connections`, `connect_nodes`, `remove_node`,
    `set_node_properties`) declared `assetPath` required although the
    prologue serves the behaviorTreePath/path fallbacks and joint-rejects
    only when all three are absent, and `create_nav_link_proxy` dropped
    the dead copy's blueprintPath/name/path spellings — its row now
    matches `create_smart_link`'s (the scoping sweep's
    location/startPoint/endPoint one-of suspicion did not survive the body
    evidence: all three are genuinely required, on both rows). The stale
    `McpTool_ManageAI.cpp` "60 actions" header reads 61 now (the schema
    union adds `add_subnode` to the 60-name core list — the decl count was
    right). `RequiresEditor` on all 61 (three whole-editor-gated chains);
    `Mutating` on everything except the readers — `get_ai_info`,
    `get_blackboard_value`, `get_navigation_info` — and the honest
    NOT_IMPLEMENTED `add_eqs_context`.

## Bootstrap state (2026-07-04, complete)

**1169 declarations registered at boot; all 1169 verified and enforced (100%).**
Reached in three same-day passes:

1. **Fleet bootstrap** — 57 file-scoped agents + three-witness cross-check:
   1171 decls, 962 verified (82%).
2. **Attribution burn-down** — alias-group synthesis (handlers comparing
   several names in one condition — `delete || delete_asset || delete_assets`),
   any-high-confidence-contributor merge, filename→tool affinity seeding,
   `manage_tools` hand-authored (it lived in the transport, which the fleet
   never read; the whole tool was removed 2026-07-04): 1064 verified (91%). A live argless probe of the remaining 107
   found exactly 2 dead and 105 alive.
3. **Deep attribution** — 13 *action-scoped* agents (the inverse of pass 1:
   given the action list plus the live probe responses as ground truth, trace
   dispatch to the implementing function across files): all 105 verified. The
   2 dead actions (`control_editor.set_viewport_resolution`,
   `manage_sequence.set_metadata` — advertised, no code path) were **deleted**
   from the routing enums and declarations. The decl lint acted as second
   witness and caught one agent trace pointed at a *shadowed duplicate
   implementation* of `bind_parameter_to_source` (TODO bug 2026-07-04b) plus
   two alt-group required-flag slips.

The lint allowlist pins 187 non-gap findings (parser brace-scope
mis-attributions, shared-dispatcher prologue reads, dead-branch reads).
False-positive sweeps after each pass: material battery + full ui-nav suite +
assorted valid traffic = zero per-action rejections.

## Migration rules (agreed 2026-07-04)

1. **New actions are `FMcpCall` classes.** No new string branches in family
   handlers.
2. **Deletion is completion proof.** A shim dies the commit its class lands; a
   family's if/else dispatch chain dies the commit its last action is classed;
   the parsed-table system died the commit declarations landed.
3. **Required-param enforcement is staged.** Declarations carry `bRequired`,
   but the transport does not reject missing-required yet — that flip gets its
   own evidence pass (the fleet's required flags are the least-verified part
   of the bootstrap).
4. **Client never defines the contract**; it receives it via `tools/list`.
5. **Classes never re-enter the dispatcher** (agreed 2026-07-04). Cross-action
   reuse is a typed shared function both classes call — never a cloned payload
   with a rewritten `subAction` handed back to string matching. When a family
   is classed, its payload-rewriting delegation sites convert to direct calls
   and the internal action names they targeted die with the chain.
6. **One name per action — no advertised aliases** (agreed 2026-07-04, shipped
   same day). The de-alias pass deleted 45 alias spellings (34 fleet-verified
   groups; `delete`/`delete_asset`/`delete_assets` → `delete`, bare verb wins
   when the tool supplies the noun). Internal canonical literals that rewrites
   still target (`play_anim_montage`, `save_level_as`, `console_command`,
   `spawn_light`, `stream_level`) survive inside handlers only and die at
   classing. `bake_lightmap` vs `build_lighting` turned out to be two different
   implementations, not aliases — both kept, decls corrected.
   Declarations do not attempt runtime-outcome truth (that is the apply-receipt
   work — see the dogfood TODO).
