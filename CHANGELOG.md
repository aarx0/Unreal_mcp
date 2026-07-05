# 📋 Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### build_environment classed — fifteenth FMcpCall family (2026-07-05)

- **55 classes in `MCP/Calls/McpCalls_BuildEnvironment.cpp`; all three string dispatchers are gone** — `HandleBuildEnvironmentAction` (EnvironmentHandlers.cpp, 21 branches: 15 delegating + 6 inline), `HandleLightingAction` (LightingHandlers.cpp, 12 inline branches), `HandleManageSplinesAction` (SplineHandlers.cpp, 22 delegating branches), plus the registration lambda that split them on `IsLightingAction`/`IsSplineAction` (both predicates died with their only caller). The 21 core actions became `HandleEnvironment*` members: the six inline bodies (export_snapshot, import_snapshot, delete, create_sky_sphere, set_time_of_day, create_fog_volume) extracted verbatim (whitespace-only dedent), each replicating the chain's editor-gated region stub ("Environment building actions require editor build.", NOT_IMPLEMENTED) and carrying the chain's shared `Resp`/`bSuccess`/`Message`/`ErrorCode` prologue and `SendAutomationResponse` tail per member, with the lowercased `LowerSub` echoes inlined to the member's action literal (the only non-dedent body delta); the other 15 branches already forwarded to the family's dedicated 4-arg members (Foliage/Landscape/AssetWorkflow/EnvironmentHandlers), so their wrappers reproduce the forwards verbatim — same internal action literals (`add_foliage_type`, `paint_landscape_layer`, `sculpt_landscape`, ...) and the same foliage payload re-shaping (foliageType→foliageTypePath alias copy, transforms/locations passthrough, `removeAll` default) — and carry no gate (the retired dispatch was ungated there; each 4-arg member self-gates). The 12 lighting branches extracted verbatim to `HandleLighting*` members, each replicating the chain's whole-dispatcher `#if WITH_EDITOR` / NOT_IMPLEMENTED stub and its `EditorActorSubsystem` precheck (EDITOR_ACTOR_SUBSYSTEM_MISSING); the chain's payload-null precheck died — `FMcpCall::Execute` performs it. The 22 spline branches already delegated to static free functions, so `HandleSpline*` members are thin wrappers (level-structure precedent) replicating the chain's "Spline operations require editor build" EDITOR_ONLY stub. Case behavior preserved: env/lighting lowercased the payload action and splines compared with FString's default IgnoreCase — the call registry's FindCall is case-insensitive the same way. The chains' action/subAction prologues (env/lighting read `action`, splines read `subAction` — the transport mirrors the two fields before dispatch), the three dispatcher declarations, the handler-map registration, and `McpDecl_BuildEnvironment.h` all died with the conversion.
- **Fourth classed family with a non-empty routed list (system_control precedent):** `GetToolRoutingTable()` keeps the `build_environment` row with its `Lighting`/`Splines` routed lists and `BuildEnvironmentCore` core list so boot validation can prove the schema union still matches the published enum; the row now documents the per-class delegation split rather than a live lambda.
- **Inbound edge converted (migration rule 5):** manage_level's classed `create_light` (`HandleLevelCreateLight`, LevelHandlers.cpp) re-entered the Lighting dispatcher as `HandleLightingAction(RequestId, TEXT("create_light"), Payload, Socket)`; it now calls `HandleLightingCreateLight(RequestId, Payload, Socket)` directly.
- **Hidden names deleted and ledgered (docs/dead-name-sweep-2026-07-04.md):** the Lighting chain's `spawn_light` disjunct (rule 6's parked internal literal — its last payload-rewrite caller became a typed call at manage_level's classing, leaving the disjunct unreachable: `spawn_light` is not in the `Lighting()` routing list and the only remaining caller passed the `create_light` literal) and its `bake_lightmap` disjunct (`bake_lightmap` sits in `BuildEnvironmentCore()`, so the name always routed to the env dispatcher's own `HandleBakeLightmap` BUILD_LIGHTING implementation — the Lighting-chain copy was dead); SplineHandlers' `create_spline_mesh_actor` arm + its ~116-line `HandleCreateSplineMeshActor` free function (already ledgered as a hidden implementation; not in `Splines()`, transport-dead); and the whole `HandleControlEnvironmentAction` legacy dispatcher (EnvironmentHandlers.cpp:1064-1290 — never registered in `InitializeHandlers`, no callers anywhere; its set_sun_intensity/set_skylight_intensity branches were already ledgered, its set_time_of_day sibling was a hidden duplicate of the advertised env action).
- **Decl burn-down — 2 of 55 rows fixed, backed by the bodies' own rejects:** `create_landscape_grass_type` declared BOTH `meshPath` AND `staticMesh` required, but the handler resolves them first-present-wins and joint-rejects only when neither is present ("meshPath or staticMesh required", LandscapeHandlers.cpp) — both optional now, `name` stays required; `modify_heightmap` declared `heightData` required, but the handler requires it only for the `set` operation ("heightData array required for 'set' operation") and serves raise/lower/flatten without it — optional now. The scoping sweep's other two suspects verified handler-true, no fix: configure_mesh_spacing/configure_mesh_randomization keep `actorName` optional because an empty spelling falls back to the WorldSettings config target (`ResolveSplineConfigTarget`) instead of rejecting. The other 53 rows are byte-identical to the retired shim rows (`list_light_types` stays `{}`).
- Flags authored, deliberately mixed: `RequiresEditor` on the 12 Lighting + 22 Spline actions (whole-editor-gated chains; the members answer their stubs in non-editor builds) and on the six core inline extractions (their bodies sat inside the retired dispatcher's editor-gated region); NOT on the 15 delegating core actions — their dispatch was ungated and each 4-arg member self-gates, so flagging would newly reject GEditor-less runs the shim served. `Mutating` on everything except the four readers — `get_foliage_instances`, `import_snapshot` (reads a snapshot file back, writes nothing), `list_light_types`, `get_splines_info`; `bake_lightmap` and `build_lighting` both kick a lighting build and stay writers. Gate battery: decl lint PASS (1116 decls), param reconciliation PASS, docs-reference PASS, per-file brace/paren/#if-#endif balance net 0, param-row byte-compare vs the retired shim rows (52 identical, 2 flips-only, 1 empty), member-body inverse byte-compare vs the retired branches (18 extracted + 37 wrapper delegations). Build + live smoke pending the next editor session.

### manage_networking classed — fourteenth FMcpCall family (2026-07-05)

- **72 classes in `MCP/Calls/McpCalls_ManageNetworking.cpp`; all four string dispatchers are gone** — `HandleManageNetworkingAction` (NetworkingHandlers.cpp, 27 inline branches), `HandleInputAction` (InputHandlers.cpp, 9 inline branches), `HandleManageGameFrameworkAction` (GameFrameworkHandlers.cpp, 20 inline branches), `HandleManageSessionsAction` (SessionsHandlers.cpp, 16 delegating branches), plus the registration lambda that split them on `IsInputAction`/`IsGameFrameworkAction`/`IsSessionAction` (all three predicates died with their only caller). The three inline-branch chains extracted verbatim to members in their home TUs (whitespace-only dedent; `RequestingSocket` → `Socket`): each `HandleNetworking*` member keeps the chain's `using namespace NetworkingHelpers` + `ResultJson` prologue; each `HandleInput*` member replicates the chain's whole-dispatcher `#if WITH_EDITOR` gate WITH the EnhancedInput runtime module-load check (ENHANCEDINPUT_PLUGIN_NOT_ENABLED) and the `#else` NOT_AVAILABLE stub; each `HandleGameFramework*` member replicates the `#if !WITH_EDITOR` EDITOR_ONLY stub, with the chain's prologue reads moved into the members that use them — the six creators keep `name`/`path` (default `/Game`)/`save` plus the path SECURITY sanitize, the thirteen GameMode-targeting members keep `save` plus the gameModeBlueprint→blueprintPath first-present-wins alias resolution and its SECURITY sanitize, `configure_player_start` additionally keeps the `BlueprintPath` sync line (its dispatcher-context comment trimmed), `get_game_framework_info` keeps only the alias resolution. Sessions' branches already delegated to dedicated static free functions, so its 16 members are thin wrappers (level-structure precedent) replicating the chain's `#if WITH_EDITOR` / "manage_sessions requires editor build" stub — the MCP_HAS_VOICECHAT/MCP_HAS_ONLINE_SUBSYSTEM ladders live inside the untouched free functions (`HandleEnableVoiceChat`, `HandleMutePlayer`), not the dispatcher, so no per-member feature ladder was needed (the scoping sweep's "replicate per-member ladders" note resolved to nothing, by evidence). The chains' subAction/payload prologues (core read `subAction` with an Action-name fallback no branch matched; Input and Sessions read the payload `action` field; GameFramework hard-required `subAction`), the four dispatcher declarations, the handler-map registration, and `McpDecl_ManageNetworking.h` all died with the conversion.
- **Third classed family with a non-empty routed list (system_control precedent):** `GetToolRoutingTable()` keeps the `manage_networking` row with its `Input`/`GameFramework`/`Sessions` routed lists and `ManageNetworkingCore` core list so boot validation can prove the schema union still matches the published enum; the row now documents the per-class delegation split rather than a live lambda.
- **Decl burn-down — 13 of 72 rows fixed, backed by the bodies' own rejects (HEAD GameFrameworkHandlers.cpp lines):** the dispatcher resolved `gameModeBlueprint` through the `blueprintPath` fallback (first present spelling wins, :495-500) before any branch ran, and every consuming body rejects only when BOTH are absent, so the twelve shim rows that declared the pair all-required false-rejected every one-spelling payload the handler serves — `configure_game_rules` (:1012), `setup_match_states` (:1081), `configure_round_system` (:1180), `configure_team_system` (:1276), `configure_scoring_system` (:1368), `configure_spawn_system` (:1455), `configure_player_start` (:1564, via the synced `BlueprintPath`), `set_respawn_rules` (:1689), `configure_spectating` (:1783), and the three set-class rows `set_player_controller_class` (:862), `set_game_state_class` (:912), `set_player_state_class` (:962), whose genuinely-required single spellings (`playerControllerClass` :869, `gameStateClass` :919, `playerStateClass` :969) STAY required. `set_default_pawn_class` is the thirteenth and worst row: it declared BOTH of its one-of pairs all-required — gameModeBlueprint/blueprintPath (joint reject :807) AND pawnClass/defaultPawnClass (first-present-wins :814-818, joint reject :819-823) — all four spellings optional now. The other 59 rows are byte-identical to the retired shim rows and re-verified handler-true (`get_sessions_info` reads nothing from the payload — contract stays `{}`).
- Flags authored, deliberately mixed: `RequiresEditor` on the 45 Input/GameFramework/Sessions actions (those chains were whole-editor-gated; the members answer their stubs in non-editor builds) and NOT on the 27 core actions — NetworkingHandlers.cpp carries no editor gate, and flagging them would newly reject GEditor-less runs the shim served (manage_inventory precedent; the world-dependent bodies keep their handler-enforced NO_WORLD errors). `Mutating` on writers only: eleven actions parse and acknowledge without writing anything and stay unflagged alongside the six readers — nine Sessions echoes (`configure_local_session_settings`, `configure_session_interface`, `set_split_screen_type`, `configure_lan_play`, `join_lan_server` — builds the travel URL, never travels — `configure_voice_settings`, `set_voice_channel`, `set_voice_attenuation`, `configure_push_to_talk`) and two Input no-ops (`enable_input_mapping`, `disable_input_action` — load the asset, answer success, touch nothing); the silent-success class is TODO'd (deepen or retire, Aaron's call). Zero hidden and zero dead names across all four dispatchers (the sweep ledger has no NetworkingHandlers/InputHandlers/GameFrameworkHandlers/SessionsHandlers entries). Gate battery: decl lint PASS (1116 decls), param reconciliation PASS, docs-reference PASS, per-file brace/paren/#if-#endif balance net 0, param-row byte-compare vs the retired shim rows (58 identical, 13 flips-only, 1 empty), member-body inverse byte-compare vs the retired branches (56/56 extracted + 16/16 wrapper delegations). Build + live smoke pending the next editor session.

### manage_level_structure classed — thirteenth FMcpCall family (2026-07-05)

- **45 classes in `MCP/Calls/McpCalls_ManageLevelStructure.cpp`; both string dispatchers are gone** — `HandleManageLevelStructureAction` (LevelStructureHandlers.cpp, 17 branches) and `HandleManageVolumesAction` (VolumeHandlers.cpp, 28 branches), plus the registration lambda that split them on `IsVolumeAction(subAction)` (the predicate died with its only caller). Unlike the inline-branch families, every branch already delegated to a dedicated static free function in its TU, so no body moved: the 45 new members are thin wrappers — `HandleLevelStructure*` for the 17 core actions, `HandleVolume*` for the 28 volume actions — each forwarding `(this, RequestId, Payload, Socket)` to its free function and replicating the retired chain's whole-dispatcher `#if WITH_EDITOR` stub per member with the same message and code (core: "manage_level_structure requires editor build"; volumes: "Volume operations require editor build", EDITOR_ONLY). The two post-process members additionally replicate the dispatcher's per-branch `#if MCP_HAS_POSTPROCESS_VOLUME` / UNSUPPORTED_VERSION stub ("PostProcessVolume requires UE 5.1 or later"). The chains' `subAction` prologues, the two dispatcher declarations, the handler-map registration, and `McpDecl_ManageLevelStructure.h` all died with the conversion.
- **Second classed family with a non-empty routed list (system_control precedent):** `GetToolRoutingTable()` keeps the `manage_level_structure` row with its `Volumes` routed list and `ManageLevelStructureCore` core list so boot validation can prove the schema union still matches the published enum; the row now documents the per-class delegation split rather than a live lambda.
- **Inbound edge converted (migration rule 5):** manage_level's classed `create_level` (`HandleLevelCreate`, LevelHandlers.cpp) re-entered this family's dispatcher with a synthesized `subAction: create_level` payload; it now calls `HandleLevelStructureCreateLevel(RequestId, CreatePayload, Socket)` directly, and the synthesized `subAction` write died with the chain.
- **Decl burn-down — 1 of 45 rows fixed, backed by the body's own reject (HEAD VolumeHandlers.cpp lines):** the shim's `set_volume_extent` row declared BOTH `volumeExtent` AND `extent` required, but `HandleSetVolumeExtent` resolves them as a one-of alias pair (VolumeHelpers::GetVolumeExtent — first present spelling wins, :188-193) and rejects only when neither is present ("volumeExtent (or extent) is required", :1832-1839), so requiring both false-rejected every payload the handler serves — both optional now, `volumeName` stays required (:1823-1830). The other 44 rows are byte-identical to the retired shim rows and re-verified handler-true against the free-function bodies, including the pervasive optional volumeLocation/location + volumeExtent/extent alias pairs (first-present-wins reads with defaults) and `create_packed_level_actor`'s optional `levelAssetPath` (validated only when provided, LevelStructureHandlers.cpp:2833) vs `create_level_instance`'s required one (:2736).
- Flags authored: `RequiresEditor` baked on all 45 — both implementation TUs are whole-editor-gated (the members answer the retired chains' editor-build stubs in non-editor builds); `Mutating` on everything except the readers, `get_level_structure_info` and `get_volumes_info` — `open_level_blueprint` stays a writer (it lazily creates the Level Script Blueprint via `GetLevelScriptBlueprint(false)`). Zero hidden and zero dead names in either dispatcher (the sweep ledger has no LevelStructureHandlers/VolumeHandlers entries). Gate battery: decl lint PASS (1116 decls, allowlist 123 — no new pins, none retired), param reconciliation PASS, per-file brace/paren/#if-#endif balance net 0, param-row byte-compare vs the retired shim rows (42 identical, 1 flips-only, 2 empty), wrapper-delegation byte-compare vs the retired branches (45/45). Build + live smoke pending the next editor session.

### manage_gas classed — twelfth FMcpCall family (2026-07-05)

- **31 classes in `MCP/Calls/McpCalls_ManageGas.cpp`; `HandleManageGASAction` (the ~3,350-line dispatcher in GASHandlers.cpp) is gone.** The family had zero dedicated handler functions — all 31 advertised bodies were inline branches, extracted verbatim to `HandleGas*` members in the same TU (whitespace-only extraction dedent; `RequestingSocket` → `Socket`). The dispatcher's whole-handler gate is richer than combat/character's and is replicated per member with the same messages and codes: the three-way `#if !WITH_EDITOR` (EDITOR_ONLY) / `#elif !MCP_HAS_GAS` (GAS_NOT_AVAILABLE) / `#else` preprocessor stub pair PLUS the runtime FModuleManager GameplayAbilities module-loaded check (GAS_PLUGIN_NOT_ENABLED). Contrary to the scoping sweep's "per-section gating" note, MCP_HAS_GAS's seven textual occurrences resolve to just two regions — the helpers block (`#if WITH_EDITOR && MCP_HAS_GAS`, untouched) and the single whole-dispatcher gate — so the per-member gate is uniform by evidence, not invention. Prologue reads moved into the members that use them: the five creators (`create_attribute_set`, `create_gameplay_ability`, `create_gameplay_effect`, `create_gameplay_cue_notify`, `create_execution_calculation`) keep `name`/`path` (default `/Game`); 21 members keep the `blueprintPath` read WITH its four-alias fallback loop (attributeSetPath/effectPath/abilityPath/cuePath — HEAD GASHandlers.cpp:534-547); `add_tag_to_asset`/`get_gas_info` keep `assetPath`; `get_attribute`/`create_ability_set`/`add_ability` read no prologue spelling. Version-guarded bodies preserved byte-identical: the 5.3+ modular-GE component writes (MCP_HAS_MODULAR_GE_COMPONENTS in set_effect_tags and the McpAddGrantedTagToEffect helper) and the 5.7+ deprecation guards (AbilityTags/StackingType/ExtendDuration). The chain, its payload/subAction prologue, the handler-map registration, and `McpDecl_ManageGas.h` all died with the conversion.
- **One hidden name deleted and ledgered: `grant_ability`** (HEAD GASHandlers.cpp:3641-3741) — transport-dead (no decl row, not in the schema enum) and already an honest NOT_IMPLEMENTED error since the dogfood fail-fast wave; deleted with the chain and ledgered in docs/dead-name-sweep-2026-07-04.md as deleted-at-classing, advertise candidate parked for Aaron (recover from git). The only comment-level edits beyond the mechanical transform: the file-header action list drops its grant_ability line and the 13.6 banner count reads "(2 actions)". The sweep's other flagged item — a `get_attribute_value` dead disjunct — does not exist at HEAD (already deleted in the 2874d066 dead-name sweep); confirmed absent, nothing carried.
- **Decl burn-down — 24 of 31 rows fixed (27 required-flags flipped), every fix backed by the body's own reject (HEAD GASHandlers.cpp lines):** the 21 rows that declared `blueprintPath` required were contaminated — the dispatcher resolves BlueprintPath through the four alias spellings before any body runs (:534-547), so a payload passing only an alias satisfied the handler while failing the declared contract; all five spellings are optional now with the at-least-one requirement handler-enforced. Per-row empty-reject evidence: add_ability_system_component :557 and configure_asc :586 (via ResolveBlueprintOrError's Missing-blueprintPath reject, BlueprintHandlers.cpp:1086-1090), add_attribute :680, set_attribute_base_value :746, set_attribute_clamping :841, set_ability_tags :979, set_ability_costs :1198, set_ability_cooldown :1251, set_ability_targeting :1304, add_ability_task :1451, set_activation_policy :1621, set_instancing_policy :1705, set_effect_duration :1831, add_effect_modifier :1918, set_modifier_magnitude :2030, add_effect_execution_calculation :2123, add_effect_cue :2179, set_effect_stacking :2229, set_effect_tags :2351, configure_cue_trigger :2603, set_cue_effects :2647. Three one-of contracts were declared all-required: `add_tag_to_asset` required both `tag` and `tagName` but the body accepts either (:2806-2811) — both optional now, `assetPath` stays required (:2800); `add_ability` required both `abilityPath` and `abilityClass` but the body falls back from one to the other (:3519-3528) — both optional now, `setPath` stays required (:3512-3517); `create_ability_set` required both `setPath` and `assetPath` but the body falls back setPath→assetPath (:3374-3383) — both optional now. The 7 untouched rows are byte-identical to the retired shim rows, each required flag backed by its own reject (creators' `name` :643/:942/:1769/:2533/:3750, get_attribute's `attribute` :3023, get_gas_info's `assetPath` :3142); the dispatcher-prologue spellings a body does not read stay declared-optional so payloads the family accepts today keep validating.
- Flags authored: `RequiresEditor` baked on all 31 — the TU is whole-handler editor-gated (the members answer the EDITOR_ONLY stub in non-editor builds); `Mutating` on everything except the readers, `get_attribute` and `get_gas_info` (the shim rows carried `None` throughout). Gate battery: decl lint PASS (1116 decls, allowlist 123 — no new pins, none retired), param reconciliation PASS (1428 declared / 1344 payload-read, allowlist 122, stale 0), schema-snapshot PASS against the live bridge (byte-identical golden, 21 tools — published schema untouched at source level), per-branch byte-verification of the extraction (31/31 branch bodies byte-identical after the inverse transform; brace/paren balance net 0; #if/#endif 51/51), and param-row byte-compare vs the retired shim rows (24 rows flips-only, 7 identical). Build + live smoke pending the next editor session.

### manage_inventory classed — eleventh FMcpCall family (2026-07-05)

- **33 classes in `MCP/Calls/McpCalls_ManageInventory.cpp`; `HandleManageInventoryAction` (the ~2,900-line dispatcher in InventoryHandlers.cpp) is gone.** The family had zero dedicated handler functions — all 33 bodies were inline branches, extracted verbatim to `HandleInventory*` members in the same TU (whitespace-only extraction dedent; `RequestingSocket` → `Socket`). Unlike combat/character there was no whole-handler `#if !WITH_EDITOR` guard to replicate (the TU carries no editor gate at all — the module is editor-only) and no common-parameter prologue to move: the chain's only shared read was `subAction`, which died with it. Every save/compile tail preserved exactly (`McpFinalizeBlueprint` with its per-action bStructural/save arguments on the Blueprint-scaffold actions; save-gated `McpSafeAssetSave` — default-true on creators, default-false on data-asset mutators — on the UMcpGenericDataAsset actions; the compile-then-write-CDO `McpSafeCompileBlueprint` sequencing wherever defaults are written after adding variables). The chain, its `subAction` prologue, the handler-map registration, and `McpDecl_ManageInventory.h` all died with the conversion. No cross-file delegation in either direction; zero hidden and zero dead names (the sweep ledger has no InventoryHandlers entries).
- **Contracts ported verbatim from the shim rows — all 33 re-verified against the extracted bodies' reads, no decl fixes needed.** Required flags faithful (each required param is backed by the branch's own empty-reject: `blueprintPath` on the component/variable scaffolds, `name` on the five creators, the path spellings — `itemPath`/`pickupPath`/`recipePath`/`lootTablePath`/`stationPath`/`actorPath` — on their loaders, and the two-param rejects on `add_loot_entry`/`add_recipe_ingredient`/`assign_item_category`/`configure_loot_drop`/`create_crafting_recipe`), no mega-bags, no one-of collisions, and no declared-optional-but-unread artifact. The handler-enforced one-ofs stay beyond the decl vocabulary (`get_inventory_info`'s five optional resolver spellings, `remove_loot_entry`'s entryIndex-or-itemPath); `configure_inventory_weight` keeps both encumbrance spellings declared because the body reads the historical 'encumberance' (sic) fallbacks.
- Flags authored: NO `RequiresEditor` — the first classed family without it anywhere: InventoryHandlers.cpp is not editor-gated (no `#if WITH_EDITOR`, no GEditor read), so baking the flag would newly reject GEditor-less commandlet runs the shim family served. `Mutating` on everything except the reader, `get_inventory_info` (the shim rows carried `None` throughout). Published schema untouched at the source level (no schema-feeding file in the diff; tool definition and routing tables unchanged). Gate battery: decl lint, param reconciliation, per-branch byte-verification of the extraction, and the schema-snapshot byte-compare against the live bridge (byte-identical golden, 21 tools); live smoke is pending the next session.
- Also makes `McpDeclRegistration.h` self-contained (`MCP/McpCallRegistry.h` for `FMcpCallRegistry`): its inline registration function calls `FMcpCallRegistry::Get()` but the header only reached the type transitively through the shim decl headers it includes — the exact fragility class that broke the tenth conversion (McpStartupValidation.h), and each classing commit deletes one more of those transitive carriers. Found by the post-delete header audit; one-line include, no behavior change.

### manage_combat classed — tenth FMcpCall family (2026-07-05)

- **39 classes in `MCP/Calls/McpCalls_ManageCombat.cpp`; `HandleManageCombatAction` (the ~2,300-line dispatcher in CombatHandlers.cpp) is gone.** The family had zero dedicated handler functions — all 39 bodies were inline branches, extracted verbatim to `HandleCombat*` members in the same TU (whitespace-only extraction dedent; `RequestingSocket` → `Socket`). The dispatcher's whole-handler `#if !WITH_EDITOR` guard is replicated per member (same `EDITOR_ONLY` message), and its common-parameter prologue moved into the members that read it: the five creators (`create_weapon_blueprint`, `create_projectile_blueprint`, `create_damage_type`, `setup_damage_type`, `create_damage_effect`) keep the `name`/`path` (default `/Game`) reads, `setup_hitbox_component` keeps `name` (its optional component-name override) alongside `blueprintPath`, and the other 33 keep `blueprintPath`. Every compile/save tail preserved exactly (`MarkBlueprintAsStructurallyModified` + `McpSafeCompileBlueprint` + CDO reflection writes + `McpSafeAssetSave` on the variable-scaffold actions; plain compile+save on the component-only ones; `McpFinalizeBlueprint` on `setup_attachment_system`/`configure_hit_detection`). The chain, its `subAction` prologue, the handler-map registration, and `McpDecl_ManageCombat.h` all died with the conversion. No cross-file delegation in either direction; zero hidden and zero dead names (the sweep ledger has no CombatHandlers entries).
- **Contracts ported verbatim from the shim rows — all 39 re-verified against the extracted bodies' reads, no decl fixes needed.** Required flags faithful (`blueprintPath` genuinely required on the 34 rows that resolve a Blueprint — `ResolveBlueprintOrError` rejects empty — and `name` on the five creators, backed by their own empty-rejects), no mega-bags, no one-of collisions. Unlike manage_character there is no declared-optional-but-unread artifact: the shim rows never declared the prologue spellings a body does not read, so every declared param is read by its member (`set_weapon_stats`'s `criticalMultiplier`/`headshotMultiplier` and `setup_reload_system`'s `maxAmmo` are `Any`-kind presence checks that answer with an `ignoredParams` pointer to the consuming action).
- Flags authored: `RequiresEditor` on all 39 (the implementation TU is editor-gated); `Mutating` on everything except the two readers, `get_combat_info` and `get_combat_stats` (the shim rows carried `None` throughout). Published schema untouched at the source level (no schema-feeding file in the diff; the tool definition and routing tables are unchanged). Offline battery: decl lint, param reconciliation, per-branch byte-verification of the extraction; the schema-snapshot byte-compare and live smoke are pending the next editor session (the snapshot test queries the live bridge).

### manage_character classed — ninth FMcpCall family (2026-07-05)

- **27 classes in `MCP/Calls/McpCalls_ManageCharacter.cpp`; `HandleManageCharacterAction` (the ~1,840-line dispatcher in CharacterHandlers.cpp) is gone.** The family had zero dedicated handler functions — all 27 bodies were inline branches, extracted verbatim to `HandleCharacter*` members in the same TU (whitespace-only extraction dedent; `RequestingSocket` → `Socket`). The dispatcher's whole-handler `#if !WITH_EDITOR` guard is replicated per member (same `EDITOR_ONLY` message), and its common-parameter prologue moved into the members that read it: `create_character_blueprint` keeps the `name`/`path` (default `/Game`) reads, the other 26 keep `blueprintPath`. Every mark/compile/save tail preserved exactly (`McpSafeAssetSave` on create; `SetBPVarDefaultValue`'s compile+CDO import and `MarkBlueprintAs*Modified` elsewhere — this family is mark-only by design outside create). The chain, its `subAction` prologue, the handler-map registration, and `McpDecl_ManageCharacter.h` all died with the conversion. No cross-file delegation in either direction; zero hidden and zero dead names (the sweep ledger has no CharacterHandlers entries).
- **Contracts ported verbatim from the shim rows — all 27 re-verified against the extracted bodies' reads, no decl fixes needed.** The cleanest of the small families: required flags faithful (`blueprintPath` genuinely required on the 26 configure/set/setup rows — `ResolveBlueprintOrError` rejects empty — and `name` on `create_character_blueprint`), no mega-bags, no one-of collisions. Deliberate porting artifact: every row keeps the dispatcher prologue's `name`/`path`/`blueprintPath` spellings declared-optional even where the extracted body doesn't read them (`name`/`path` on the 26, `blueprintPath` on create) — dropping them would turn payloads the family accepts today into undeclared-param rejects.
- Flags authored: `RequiresEditor` on all 27 (the implementation TU is editor-gated); `Mutating` on everything except the reader, `get_character_info` (the shim rows carried `None` throughout). Published schema untouched (byte-identical golden). Offline battery: decl lint, param reconciliation, schema snapshot golden, per-branch byte-verification of the extraction; live smoke pending the next editor session.

### manage_interaction classed — eighth FMcpCall family (2026-07-05)

- **22 classes in `MCP/Calls/McpCalls_ManageInteraction.cpp`; `HandleManageInteractionAction` (the ~2,100-line dispatcher in InteractionHandlers.cpp) is gone.** The family had zero dedicated handler functions — all 22 bodies were inline branches, extracted verbatim to `HandleInteraction*` members in the same TU. Two stated whitespace-only deviations: the extraction dedent (branch bodies sat one `if` level deep), and the five column-0-indented branches a prior sweep left flush-left (`configure_destruction_levels`/`effects`/`damage`, `configure_trigger_filter`/`response`) re-indented to member style. Every finalize/save tail (`McpFinalizeBlueprint`, `SaveLoadedAssetThrottled`, `McpSafeAssetSave`) is preserved exactly. The chain's `subAction` prologue, the family gate, the handler-map registration, and `McpDecl_ManageInteraction.h` all died with the conversion. No cross-file delegation existed in either direction; zero hidden and zero dead names (the file's dead Section 6/7 dispatch was already swept at the 2026-07-03 dead-code pass).
- **Five of the 22 are shallow scaffolds, preserved as-is — deepening or retiring them is a product decision (logged in TODO.md), not this commit's.** The `configure_destruction_*` trio resolves the actor, calls `Modify()`, and records an `MCP_Destruction*Configured` marker tag (same shape as `setup_destructible_mesh`); `configure_trigger_filter`/`response` scaffold their filter/response variables without writing the requested defaults.
- **Contracts ported verbatim from the shim rows — all 22 re-verified against the extracted bodies' reads.** No mega-bags in this family; one known skew ported as-authored and documented in the file: `configure_door_properties`/`configure_switch_properties`/`configure_chest_properties` *require* their primary path spelling (`doorPath`/`switchPath`/`chestPath`) while the handlers also accept the declared-optional `blueprintPath` fallback — the required flag false-rejects a fallback-only call. `get_interaction_info`'s at-least-one-resolver rule stays handler-enforced (beyond the decl vocabulary).
- Flags authored: `RequiresEditor` on all 22 (every body is editor-gated); `Mutating` on everything except the reader, `get_interaction_info` (the shim rows carried `None` throughout). Published schema untouched (byte-identical golden).

### manage_effect classed — seventh FMcpCall family, the largest yet (2026-07-04)

- **58 classes in `MCP/Calls/McpCalls_ManageEffect.cpp`; `HandleEffectAction` (the ~1,800-line dispatcher whose re-dispatch RECURSED INTO ITSELF under an internal `create_effect` key) is gone.** Its 16 inline bodies are extracted verbatim to `HandleEffect*` members in EffectHandlers.cpp. The dispatcher carried two divergent copies of the cleanup implementation — semantically identical, differing in brace style, one comment, and a line wrap; the second sat after the sub-action chain and was unreachable (the chain's `cleanup` branch returns first, and a top-level `cleanup` action never passed the family guard) — the reachable first copy is the extracted one. One stated deviation from verbatim: `create_volumetric_fog`'s spawn-failure path used to fall out of its branch into the dispatcher's catch-all (an `UNKNOWN_ACTION` reply for a spawn failure); the extracted member reports `SPAWN_FAILED`. The `create_effect` key, the normalize/rewrite prologue, both `Is*SubAction` lambdas, an unused `SendResponse` lambda, the handler-map registration, and `McpDecl_ManageEffect.h` all died with the chain.
- **39 of the 58 classes are cross-TU delegates and their gate literals SURVIVE**: the 36 Niagara authoring actions call `HandleManageNiagaraAuthoringAction` (NiagaraAuthoringHandlers.cpp) under its `manage_niagara_authoring` literal — the monolith dispatches on the payload's `subAction`, which the transport mirrors from `action`, so no payload rewrite is needed; the three graph actions call `HandleNiagaraGraphAction` (NiagaraGraphHandlers.cpp) under `manage_niagara_graph` after rewriting `subAction` to its internal `add_module`/`connect_pins`/`remove_node` spellings, exactly as the chain did. Both handlers flip public (transitional, until the module split); their bodies are untouched. `activate`/`activate_effect`/`deactivate` now delegate straight to the extracted `activate_niagara`/`deactivate_niagara` members — the payload `action`+`subAction` rewrites those aliases needed under the chain are gone.
- **Decl fix: `create_niagara_system` was mis-authored** with a `set_niagara_parameter`-shaped contract — required `parameterName` plus `systemName`/`parameterType`/`value` on a branch that reads none of them. The live branch (NiagaraAuthoringHandlers.cpp:355) requires `name` and reads the monolith's common param block (`path`/`savePath`/`save`/…), identical to `create_niagara_emitter`; both now share one contract. The other 35 authoring rows and both graph rows were re-verified against their live branches' enforcement — no other changes.
- Flags authored: `RequiresEditor` on all 58 (all three implementation TUs are editor-gated); `Mutating` on the 52 writers — readers are `get_niagara_info`, `validate_niagara_system`, `list_debug_shapes`, and the transient debug-draw trio. Published schema untouched (byte-identical golden). Zero hidden and zero dead names remained on this family's surface (the lying numbered-stub block died at the dead-name sweep).

### inspect classed — sixth FMcpCall family; the read-side workhorse converts clean (2026-07-04)

- **38 classes in `MCP/Calls/McpCalls_Inspect.cpp`; `HandleInspectAction` (the ~1,000-line dispatcher in EnvironmentHandlers.cpp) is gone.** Its 13 inline global bodies are extracted verbatim to `HandleInspect*` members in the same TU (`runtime_report`/`pie_report` share `HandleInspectRuntimeReport` like they shared one branch; the two bodies that echoed the caller's raw sub-action string now echo their fixed action name). The object-detail catch-all became `HandleInspectObjectGeneric`, carrying its own objectPath/actorName/name resolution — the eight detail actions (`inspect_object` + the seven `get_*_details`) are advertised aliases of that one body with byte-identical output; collapsing them to one name is a product decision, deferred. The chain's actor-alias normalization moved into the classes (`NormalizeActorAlias`, applied by the twelve actor actions before their typed calls into the shared control_actor handlers), and `set_property`/`get_property` delegate to the property handlers under their internal `set_object_property`/`get_object_property` dispatch keys — those literals are load-bearing and survive. `McpDecl_Inspect.h` died with the shims; nothing else did: this family had zero hidden and zero dead names (the sweep ledger has no inspect entries).
- **Decl burn-down**: `list_objects`'s declaration was a 33-param mega-bag (control_actor-flavored residue, seven bogus *required* fields — `blueprintPath`, `componentType`, `childActor`, `parentActor`, `tag`, `variables`, `snapshotName`) for a body that reads nothing; since bRequired is not transport-enforced the damage was accept-junk, not false-reject — the contract is now `{}`. `pie_report` drops six params its shared body never read by adopting runtime_report's contract. The target aliases are uniformly optional: `add_tag`/`create_snapshot`/`get_component_property` *required* `actorName` and `inspect_object` *required* `objectPath`, though the prologue accepted actorName/name/objectPath interchangeably (staged-enforcement false-rejects). `get_component_property.propertyName` is optional (`propertyPath` is the alternative, matching control_actor's own row).
- Flags authored: `RequiresEditor` on all 38; `Mutating` on the seven writers (`set_property`, `set_component_property`, `add_tag`, `create_snapshot`, `restore_snapshot`, `delete_object`, `export`). Published schema untouched (byte-identical golden); the stale decl-lint allowlist pins from the dead dispatcher's prologue reads (37 `inspect.*` entries) are pruned.

### system_control classed — fifth FMcpCall family, the first spanning multiple handler TUs (2026-07-04)

- **44 classes in `MCP/Calls/McpCalls_SystemControl.cpp`; the registration lambda — the family's real dispatcher — is gone.** system_control was the last multi-branch router still standing in `InitializeHandlers`: its lambda tested `IsPerformanceAction`, five per-action Ui re-dispatches, two console synthesis branches, five log names, `spawn_category`, and `lumen_update_scene` before falling through to `HandleSystemControlAction`'s gate. All of that is now per-class delegation to extracted members across six TUs: 21 `HandlePerf*` (PerformanceHandlers.cpp), 9 `HandleSys*` (SystemControlHandlers.cpp; `start_session` stays a one-line delegation to `HandleInsightsAction`), 5 `HandleUi*` (UiHandlers.cpp), the log family (`HandleLogQuery` shared by `get_log`/`tail_log`, `HandleLogClear`, `HandleLogSubscribe`/`HandleLogUnsubscribe` over a shared `HandleLogSetSubscribed`), `HandleDebugSpawnCategory`, and `HandleRenderLumenUpdateScene`. The dead internal dispatch names (`manage_performance`, `manage_ui`, `manage_logs`, `manage_debug`, `manage_render`) died with their gates — including the `action:"manage_logs"`/`"manage_render"` echoes in response bodies, which now say `system_control`. `McpDecl_SystemControl.h` died with the shims.
- **Flags are authored and deliberately mixed — `RequiresEditor` is NOT baked into the macro.** 33 actions whose implementing TUs are editor-gated carry it (all Performance, all build/test/python, `lumen_update_scene`, `create_widget`, `add_widget_child`); the five log actions do not (LogHandlers.cpp runs in non-editor builds — flagging them would regress headless use), nor do `screenshot`, `execute_command`, `set_cvar`, `get_project_settings`, `set_project_setting`, `spawn_category` (handler-enforced preconditions with fallbacks). Two decls shed mega-bag residue: `configure_texture_streaming` and `generate_memory_report` both *required* a `functionName` neither handler reads. `set_cvar`'s inline lambda synthesis (key required → compose command) moved verbatim into `HandleSysSetCvar`.
- **The `console_command` literal survives this classing** — correcting the control_editor entry's prediction below: `execute_command`/`set_cvar` classes call `HandleConsoleCommandAction` directly under its internal canonical name, and the console handler keeps its other owners (ControlHandlers.cpp `HandleControlEditorConsoleCommand`, EditorFunctionHandlers.cpp). It dies only if the console handler itself is ever classed away.
- **Thirteen hidden bodies deleted, all ledgered in `docs/dead-name-sweep-2026-07-04.md`**: `export_asset` (~180-line generic exporter, the SystemControlHandlers gate's only unadvertised branch — advertise-candidate, recover from git); nine unreachable UiHandlers branches (`play_in_editor`, `stop_play`, `save_all`, `simulate_input`, `create_hud`, `set_widget_text`, `set_widget_image`, `set_widget_visibility`, `remove_widget_from_viewport` — the scoping pass counted seven; `save_all`/`simulate_input` sat below the sweep radar because control_editor advertises those names against different implementations); three unreachable RenderHandlers subactions (`create_render_target`, `attach_render_target_to_volume`, `nanite_rebuild_mesh` — the first and last are hidden duplicates of live manage_asset implementations). `HandleUiAction`, `HandleLogAction`, `HandleDebugAction`, `HandleRenderAction`, `HandlePerformanceAction`, and `HandleSystemControlAction` (including its vestigial `test_progress`/`test_stale` gate names, which never had branches) all died; `HandleInsightsAction` and `HandleConsoleCommandAction` survive untouched.
- **Routing-table first**: `GetToolRoutingTable()`'s system_control row keeps its `Performance` routed-family entry — the first classed family with a non-empty routed list — because boot validation derives the schema-union check from it; the row now documents the per-class delegation rather than a live lambda. Published schema untouched (byte-identical golden).

### manage_level classed — fourth FMcpCall family; create_light dedup delivered (2026-07-04)

- **19 classes in `MCP/Calls/McpCalls_ManageLevel.cpp`; the 2,960-line `HandleLevelAction` monolith is gone.** This family had no dedicated handler functions at all — every implementation lived inline in one dispatcher whose prologue rewrote advertised names into internal literals (`save_as`→`save_level_as`, `delete`→`delete_level`, …) before a second round of string matching. The 18 live bodies are extracted verbatim to `HandleLevel*` members (`stream`/`unload` share `HandleLevelStreamInternal`, preserving unload's force-override of `shouldBeLoaded`/`shouldBeVisible` exactly); the prologue, the rewrite literals, the dead top-level alias gate, and the handler-map registration died with the chain, as did `McpDecl_ManageLevel.h`.
- **`set_metadata` was false-rejecting its own payload**: the shim decl omitted the `metadata` object — the one param that carries the values to write — so the transport rejected every real call at validation. The classed contract declares it. Remaining decl deltas from the shim header, all re-verified against the extracted bodies: one-of fallback groups declare each member optional per the classed-family convention (`add_sublevel.subLevelPath`/`levelPath`, `delete.levelPath`/`path`, `duplicate_level.sourcePath`/`levelPath`, `export_level.exportPath`/`destinationPath`, `import_level.sourcePath`/`packagePath`, `rename_level.levelPath`/`sourcePath` + `destinationPath`/`newName`, `validate_level.levelPath`/`assetPath`); `load.levelPath` and `save_as.savePath` stay hard-required, as does `duplicate_level.destinationPath`.
- **`create_light` deduped (TODO 04d's noted follow-up)**: the manage_level branch used to rewrite the payload into `execute_editor_function` SPAWN_ACTOR_AT_LOCATION — silently dropping `properties` (intensity/color/castShadows), `name`, and SkyLight support. The classed action now calls `HandleLightingAction` directly (migration rule 5), so both routes are one implementation; its contract widens to the full LightingHandlers surface with `location` still required. The `spawn_light` rewrite literal no longer exists in manage_level; its match arm in LightingHandlers survives until that family's classing.
- **13 transport-dead hidden branches deleted** (`get_level_info`'s direct arm, `set_level_world_settings`, `set_level_lighting`, `add_level_to_world`, `remove_level_from_world`, `set_level_visibility`, `set_level_locked`, `get_level_actors`, `get_level_bounds`, `get_level_lighting_scenarios`, `build_level_lighting`, `build_level_navigation`, `build_all_level`) — the schema enum gate rejected their names before dispatch; each is ledgered with a capability note in `docs/dead-name-sweep-2026-07-04.md` for a future advertise decision from history. Dead-code bonus: `create_level`'s post-delegation editor-world path (it has delegated to manage_level_structure's inactive-world create since the UE 5.7 TickTaskManager fix) and its only dependency, the ~230-line `McpSafeNewMap`, deleted.
- Flags authored: `RequiresEditor` on all 19; `Mutating` on the 15 writers (the shim rows carried `None` throughout).

### control_editor classed — third FMcpCall family; TODO 04e closed (2026-07-04)

- **35 classes in `MCP/Calls/McpCalls_ControlEditor.cpp` (34 handlers — `step_frame`/`single_frame_step` share one), and the last mega-bag declarations are gone.** The shim decls for this family were the 04e false-reject class at its worst: `execute_command` and `single_frame_step` carried byte-identical 30-param union arrays that *required* `assetPath` and `mode` (params neither handler reads) while `execute_command`'s one real required param — `command` — wasn't in the bag at all. Four more rows were contaminated (`possess` demanded `functionName`/`actor_name` copied from control_actor's `call_actor_function`; `screenshot`/`simulate_input`/`simulate_nav` declared params their handlers never read). All 35 contracts are re-authored from the handler bodies' actual payload reads, with `RequiresEditor` on every action (the shim rows said `None` throughout) and `Mutating` on `undo`/`redo`/`save_all`/`execute_command`.
- **Dispatch-side this was the cleanest family yet**: zero inline bodies to extract, no alias OR-arms, no cross-file payload re-dispatch in either direction. The chain, its handler-map registration, and `McpDecl_ControlEditor.h` died with the conversion. `manage_asset.save_all`'s direct call into `HandleControlEditorSaveAll` is unaffected (typed member call, not dispatch). The `console_command` internal literal survives inside `HandleControlEditorConsoleCommand`'s direct delegation to the console handler — it dies when system_control is classed.
- PIE-only preconditions (`pause`/`possess`/`simulate_nav`/…) remain handler-enforced; the `RequiresEditor` flag only covers GEditor existing.

### Small-bug batch: texture envelope reject, create_light fail-fast, MapCheck false-fail (2026-07-04)

- **Texture ops no longer reject their own envelope** (TODO 04c): every per-branch ValidParams allowlist admits the mirrored `action` key (they predated the action/subAction bidirectional mirror), and an empty required path now says `assetPath required` instead of blaming "traversal or invalid characters" on a string that was simply absent.
- **`manage_level create_light` requires `location`** (TODO 04d): an argless call previously spawned a default PointLight at origin silently — a fail-fast violation that mutated the level. Both implementations gate now (the manage_level rewrite in LevelHandlers and the full spawn_light in LightingHandlers — a duplicate-implementation pair noted for dedup at manage_level's classing).
- **`[MapCheck]` diagnostics are benign to the post-op guard** (TODO 04f): loading a level whose content carries MapCheck warnings no longer converts a successful load into ENGINE_ERROR; MapCheck lines describe the content, not the operation.

### manage_sequence classed — second FMcpCall family (2026-07-04)

- **32 classes in `MCP/Calls/McpCalls_ManageSequence.cpp`; the `sequence_*` manufactured-name family is extinct.** The legacy dispatcher prefixed every payload sub-action with `sequence_` and string-matched the result — 32 internal names no schema ever advertised, the largest surviving block of runtime-manufactured dispatch names from the de-alias postmortem. Chain, prefix machinery, shim decl header (`McpDecl_ManageSequence.h`), and handler-map registration all died with this conversion. The shim decls were already true per-action contracts (no mega-bags), so the classing transfers them verbatim — but now with authored flags: `RequiresEditor` on all but pure-reflection `list_track_types`, `Mutating` on the 21 writers (the shim rows carried `None` throughout).
- **Four actions had no functions — their bodies lived inline in the dispatcher** (`list_track_types`, `add_track`, `list_tracks`, `set_work_range`); extracted to dedicated `HandleSequence*` members, error texts updated from the dead internal names to the advertised ones. Scoping found zero cross-file delegation in either direction and zero hidden/dead names for this family (the sweep's per-family ledger confirms), so no rule-5 conversions were needed. Dead-code bonus: `EnsureSequenceEntry` + `GSequenceRegistry` (orphaned prologue infrastructure, no callers) deleted.
- Lint: `MCP_*_CALL` parsing now accepts `{}` as the params argument for zero-param actions.

### control_actor classed — the FMcpCall pilot family (2026-07-04)

- **The strangler-fig migration has its first fully-classed family.** `ProcessAutomationRequest` now consults the call registry before the legacy handler map (classed actions win per-action; everything else falls through unchanged), and `FMcpCall::Execute()` is the shared pipeline: payload-null check and a new `RequiresEditor` flag carry what the old chain prologue did, then `Run()` — which mirrors the legacy handler signature, so conversion is delegation, not rewrite. control_actor's 27 advertised actions each became a class co-locating implementation with its **true param contract**: the bootstrap's 33-param mega-bag unions (the false-reject class from TODO 04e, `call_actor_function`'s bag among them) are resolved for this family — `get_actor_bounds` declares one param now, not 33.
- **Deletion is the completion proof, delivered:** `HandleControlActorAction` (the string-dispatch chain), its handler-map registration, and the whole shim decl header `McpDecl_ControlActor.h` died in this commit. The conversion also surfaced and fixed a dialect-3 hazard the dead-name sweep's global filter had masked: **`inspect` forwards 12 advertised actor actions** (`get_metadata`, `restore_snapshot`, `export`, `set_property`, …) by re-dispatching a rewritten payload through control_actor's chain — those now call the shared handlers directly (migration rule 5), and `HandleControlActorGet` (unreachable single-actor lookup, capability subsumed by `inspect_object`/`get_actor_details`) was deleted outright.
- The decl lint learned classed families: `MCP/Calls/McpCalls_*.cpp` files carry a `// LINT-TOOL:` marker and the same param-array shape, so per-class declarations stay lint-visible.

### Dead-name sweep: 53 unreachable spellings + the lying Niagara-stub block deleted (2026-07-04)

- **Every handler-condition name no schema advertises is now classified** (full report: `docs/dead-name-sweep-2026-07-04.md`). Inventory v2 fixed the de-alias pass's known false-positive classes (word-boundary variable matching, per-comparison attribution, dynamic-prefix detection, the missed `NativeSubAction` dispatch variable) → 126 candidates; 22 file-scoped classify agents traced each name's reachability; adversarial refuters attacked every dead verdict. Result: **53 dead deleted** (0 refuted — each had a fleet-verified advertised sibling reaching the same implementation: the 16-name `audio_*` prefix family, data-table `add_row`/`edit_row`/`get_rows`-era aliases, control_actor `remove`/`call_function`/`list_actors` leftovers, skeleton `add_socket`/`modify_*` spellings, and friends), **66 hidden** implemented-but-unadvertised names inventoried for per-family advertise-or-delete decisions at classing (TODO 04h stays open for those), **1 live** internal literal protected (`console_command`), 6 false positives.
- **The EffectHandlers "numbered block" was 30 lying stubs — deleted wholesale (~886 lines, TODO 04b closed).** A dedicated audit proved every branch (add_spawn_rate_module … add_simulation_stage) was doubly dead: `IsNiagaraAuthoringSubAction` intercepts all 30 names upstream to the real NiagaraAuthoringHandlers implementations, and each stub body loaded the Niagara system, discarded the parsed params, and **reported success without performing the operation**. The shadowed create-anim-blueprint duplicate went the same way (TODO 04i closed: dead inline block + delegation stub + orphaned `HandleCreateAnimBlueprint`).
- **Transport: `action`/`subAction` mismatch now rejects (TODO 04j).** They mirror when one is missing, but a payload sending BOTH with different values bypassed enum validation entirely (`subAction` is an exempt envelope key, unknown keys are log-only) — handlers dispatching on `subAction` would run a value no gate ever saw. Both-present-but-different is INVALID_PARAMS ("send 'action' only"); this closes the loophole that made "unadvertised" not quite mean "unreachable".

### `manage_tools` removed (2026-07-04)

- **The runtime tool-enablement system is gone: 22 tools → 21.** Aaron's call after the dispatch-architecture discussion — months of dogfooding never once used enable/disable in practice (its sole live appearance was the 2026-07-02 audit incident, where its then-global state broke sibling sessions), and the defensible use-case (schema-payload slimming for context-tight clients) doesn't apply to any client we have. Deleted whole: `McpTool_ManageTools.cpp`, `FMcpDynamicToolManager` (+ per-session overlay machinery, both session-expiry `DropSession` hooks, protected-tool rules), the hand-authored `McpDecl_ManageTools.h` (1124 → 1116 declarations), the routing/enum entries, the transport's local intercept and `TOOL_DISABLED` gate, and the `bLoadAllToolsOnStart` setting. `tools/list` now serves the full registry unconditionally (`GetFilteredToolsResponse` → `GetToolsResponse`), and sessions carry no state beyond their activity timestamp.

### De-aliasing: one name per action (2026-07-04)

- **44 alias spellings deleted from the advertised surface** (routing enums, declarations, handler gate disjuncts, schema prose) — Aaron's call: no aliases; bare verb wins when the tool supplies the noun. `manage_asset.delete` (was also `delete_asset`/`delete_assets`), `manage_level.load`/`save`/`save_as`/`unload`/`delete`, `control_actor.spawn`/`attach`/`detach`/`find_by_tag`/`find_by_name`/`find_by_class`/`get_transform`/`set_visibility`/`get_components`/`set_component_property`, the six-name transform pile → `set_transform`, `control_editor.stop`/`set_camera`/`screenshot`, `execute_command` unified on both tools that had a `console_command` twin, `system_control.get_perf_stats` (was also `get_profile`/`capture_stats`), `animation_physics.create_anim_blueprint`/`play_montage`, `build_environment.paint_landscape`/`sculpt`/`create_light`/`create_sky_light`, `manage_effect.spawn_niagara`, `manage_networking.add_mapping`. Dropped spellings now die at the enum with did-you-mean suggestions; every group was agent-verified as a single implementation before deletion (34 verified groups; the 9-agent fleet's reference inventory drove the edits).
- **Not aliases after all — the verification caught two impostors.** `build_environment.bake_lightmap` vs `build_lighting` reach two different engine paths (LevelEditorSubsystem::BuildLightMaps vs a raw `BuildLighting` console exec) with different validation and response shapes — both kept, and their param declarations turned out to be swapped/miswritten (now corrected to the handlers' truth). The fleet also proved the `create_anim_blueprint` duplicate inside AnimationHandlers.cpp is unreachable dead code (TODO 2026-07-04i).
- **Internal canonical literals survive inside handlers only** (`play_anim_montage`, `save_level_as`, `delete_level`, `console_command`, `stream_level`, `spawn_light`, `sculpt_landscape`, `paint_landscape_layer`) — they are rewrite targets the dialect-3 delegation still needs, and they die per family at classing (migration rules 5/6 in docs/action-declarations.md).
- 1124 declarations registered (was 1169); decl lint, param reconciliation, docs test, schema snapshot (golden re-pinned on cold boot), material 18/18, ui-nav at its exact known baseline. Postmortem note: the mechanical enum edit initially deleted five *canonical* names because the verification fleet's reference lists included keep-name lines; the live probe caught all five within minutes (`paint_landscape`/`play_montage`/`spawn_niagara`/`create_light`/`create_sky_light` rejecting at the enum), restored and re-verified — token-level diff against HEAD now confirms removed == intended exactly.

### Action declarations: the server's authored contract (2026-07-04)

- **The parsed-table system is dead; declarations are authored.** After the architecture discussion (declarations should be data someone writes, not truth a regex parser scrapes from source), a 63-agent Sonnet fleet read every handler file end-to-end — including everything regexes can't see: alias loops, renamed payload variables, cross-file dispatch — and authored per-action param declarations, cross-checked three ways (fleet × old parsed table × published schema, internal dispatch names normalized to advertised actions). Result: `Private/MCP/Decls/McpDecl_*.h`, 23 headers, **1171 declarations** (1035 fleet-verified; 136 `UnverifiedDecl`, which validation skips rather than enforce unattributed truth), registered at boot into the new **`FMcpCallRegistry`** with loud duplicate detection. Hand-maintained from here: adding an action means adding its declaration.
- **`FMcpCall` base class lands** — the Chromium-ExtensionFunction-style end state: new actions MUST be classes (declaration + implementation co-located); the ~1000 legacy actions are declaration-backed shims that convert per-family, each family's dispatch chain deleted the commit its last action converts.
- **Old system deleted the same commit** (the migration rule): `McpActionParamTable.h`, its generator, and its freshness test are gone. The generator's brace-scope parser survives *demoted* as `tests/schema/action-decl-lint.ps1` — a drift detector that fails when a handler branch reads a param its declaration doesn't list (it caught 15 real fleet misses pre-ship, `control_editor.screenshot.path` among them; its bootstrap allowlist pins 151 known parser mis-attributions).
- Transport validation now consumes the registry. Pre-ship false-positive sweep: material battery 18/18 + full ui-nav suite + assorted calls = **zero** rejections on valid traffic. Required-param flags are declared but not yet enforced (staged, own evidence pass).
- **Attribution completed to 100% in two same-day follow-up passes (2026-07-04).** The burn-down pass took 82%→91%: alias-*group* synthesis (handlers compare several names in one condition — `delete || delete_asset || delete_assets` — so a group's params flow to every name in it), any-high-confidence-contributor merge for cross-file delegation, filename→tool affinity for files that dispatch only internal names, `manage_tools` hand-authored (it lives in the transport, which no agent read). A live argless probe of the remaining 107 sorted them: exactly **2 dead** — `control_editor.set_viewport_resolution` and `manage_sequence.set_metadata`, advertised with no code path, now **deleted** from routing enums and declarations — and 105 alive. The deep-attribution pass then verified those 105 with 13 *action-scoped* agents (the inverse of the bootstrap: given the action list plus live probe error lines as ground truth, trace dispatch to the implementing function wherever it lives). Final state: **1169 declarations, 0 `UnverifiedDecl`, every action's param contract enforced.** The lint earned its keep as second witness: it exposed a shadowed duplicate implementation of `manage_effect.bind_parameter_to_source` (the agent had traced the *live* path; the "obvious" handler branch turned out to be dead code — TODO 2026-07-04b) and two alt-group required-flag slips. The probe also caught two interface bugs in passing: texture ops reject their own routing envelope (`Invalid parameter: action`), and `create_light` with zero params silently spawns a default light (TODO 2026-07-04c/d).
- **Full enforcement immediately caught a bootstrap false-reject — fixed same day.** With no more `UnverifiedDecl` skips, the ui-nav pause specs went red one assertion worse than their known baseline: `control_actor.call_actor_function`'s bootstrap declaration was a 33-param brace-skew union bag that *omitted* `functionName`/`arguments` (their reads live in the dedicated `HandleControlActorCallFunction`, which single-line dispatch hides from both the bootstrap parser and the lint), so the transport rejected the driver's `TogglePause` call. Declaration corrected to the handler's true contract (`actorName*`, `functionName*`, `componentName`, `arguments`); suite back to its exact known baseline. The mega-bag false-reject class is TODO 2026-07-04e; `control_actor` is now the top pilot-classing candidate. The ui-nav driver also swallowed tool errors (`| Out-Null`) — `Invoke-Tool` now throws on `isError`, so the next false-reject fails the spec loudly instead of masquerading as a game regression. Rejection message polish: zero-param actions now say "This action takes no params besides 'action' itself" instead of an empty list.

### Per-action param registry (2026-07-03/04, rejects by default)

- **Params sent to an action that never reads them now reject** — the largest remaining silent-no-op class. Schemas declare params per *tool* (~150 in `manage_blueprint`'s flat bag), so `fromState` sent to `create_node` passed every existing check and was silently ignored. A generated table (`McpActionParamTable.h`, 1001 tool.action entries) records which params each action's handler branch actually reads; violations get `INVALID_PARAMS` naming the unread params, the action's accepted params, and both fix paths. Shipped warn-first, flipped to rejection the same day (Aaron: the caller is an LLM — a named, explained refusal gets fixed in one turn, and log-only warnings were invisible to the caller anyway).
- **Stale-table escape hatch, because fixing the table needs a rebuild:** `bypassParamCheck:true` downgrades the rejection to `paramWarnings` riding the response (text + envelope), so a false positive can't strand a task or teach the caller to drop a working param — the error text routes the miss back to the repo (regenerate script + TODO). Bypass use is logged.
- **The table is generated, not hand-written:** `scripts/generate-action-param-table.ps1` parses handler source — action-comparison branch markers segment each file by brace scope, payload reads inside a branch belong to that action, prologue reads are file-shared, reads in files without branches (shared helpers, function-per-action handlers) are global-shared. Attribution is fail-permissive by construction: a missed read can only suppress a warning, never invent one. Tool→action truth comes from the schema golden. `tests/schema/action-param-table-test.ps1` regenerates and diffs so the checked-in table cannot drift from handler source (battery member #5).
- Emitted as constant-initialized C arrays — an `initializer_list` aggregate or one 945-call initializer function trips MSVC C4883 (function too large to optimize) at this entry count.

### Blind-sleep triage (2026-07-03)

- **All 19 `FPlatformProcess::Sleep` sites classified; 14 game-thread stalls eliminated.** The core insight: a game-thread sleep runs no engine ticks, so every "let the engine settle/process/stabilize" delay was provably doing nothing — the engine *can't* settle while the handler sleeps. Survivors: 3 socket-thread polls/backoffs in the transport (legitimate cross-thread waits, one with game-thread task pumping) and 2 failure-path-only backoffs (level-save file-visibility retry ladder, folder-delete retry).
- **Converted to real waits where the intent was real:** `McpSafeLoadMap`'s PIE-stop poll loop (`RequestEndPlayMap` is serviced on the *next tick*, which never runs while the handler sleeps — the loop always spun its full second and exited with PIE still up) is now a synchronous `GEditor->EndPlayMap()`, live-verified by loading a level mid-PIE; its async-loading poll loop is a single blocking `FlushAsyncLoading()`; its pre-load cleanup uses immediate `CollectGarbage` (the deferred `ForceGarbageCollection` flag never ran before `LoadMap` needed the old world gone); `add_sublevel`'s "give it a moment to load" sleep is `FlushLevelStreaming()` (streaming advances on world tick, not wall-clock).
- **Deleted where the intent was folklore:** post-`FlushRenderingCommands` "GPU settle" delays (the flush already blocks), post-`ScanFilesSynchronous` "registry settle", `NewMap` "stability" delays, and the delete-path quiesce sleeps. The level-save verify ladder now checks *before* sleeping (save I/O is synchronous — the happy path paid 50ms for nothing).
- Measured: `add_variable`+compile 732→313ms per 5 (the quiesce sleeps); level ops shed ~250ms fixed overhead per load/new-map. Deletes barely moved (~5%) — their cost is real work (`ForceDeleteObjects`' internal GC + compilation barriers), not sleeps.
- `tests/schema/README.md` documents a `-UpdateGolden` trap found en route: tool schemas are cached at registration, so re-pinning after a Live-Coding schema patch silently pins the stale cache — re-pin only against a cold-booted full rebuild (this commit carries the correct re-pin the previous commit thought it made).

### Contract + latency quick wins (2026-07-03)

- **Schema validation promoted from warn to reject** — `tools/call` args that miss a required parameter or send an out-of-enum value now get `INVALID_PARAMS` with the violations listed (small enums enumerate their allowed values; big ones suggest near-misses — `get_editor_state` → "did you mean: get_state…"). Promotion gate was evidence, per the warn-first plan: 11 editor sessions and hundreds of calls produced exactly 5 warnings, every one a genuinely wrong call that a rejection would have surfaced faster. `action`/`subAction` now mirror bidirectionally *before* validation so legacy `subAction`-only payloads stay valid; unknown top-level keys remain log-only by design.
- **Responses serialize on the write thread** — `CompletePendingRequest` built the tool result and serialized the full JSON body on the game thread ("cheap, no I/O" said the comment — not true for multi-hundred-node graph dumps) before handing the string to the pool writer. Serialization now happens inside the pool task. Safe by audit: all ~2000 send sites pass a function-local result they never touch after the call (zero violations, no member-object results); the handoff is now a documented ownership contract on `SendAutomationResponse`.
- **The "three material handlers ignoring `save:false`" were dead code — deleted** (509 lines + header declarations). `HandleAddMaterialTextureSample`/`HandleAddMaterialExpression`/`HandleCreateMaterialNodes` had zero call sites; the *live* material authoring path (`FinalizeMaterialHost`, used by `add_texture_sample`/`add_expression`/etc.) already honors `save:false` correctly. The review item was recorded against unreachable copies — the same dead-duplicate class as the third `run_ubt` copy.

### ui-nav suite runs through the bridge (2026-07-03)

- **`run_tests suite:"ui-nav"`** runs the checked-in CommonUI focus/nav regression specs (`tests/ui-nav/*.json`, optional `spec` to pick one) from any MCP client — no shell access needed. Fire-and-poll like `run_ubt`: the bridge launches the external pwsh runner detached (a blocking in-handler runner would deadlock against its own bridge calls and PIE ticks) with output to `Saved/McpTests/<runId>.log`, and **`get_test_results`** polls the process and parses per-spec pass/fail counts, `[FAIL]` assertion lines, and a log tail on abnormal exits. Mutual busy-checks with engine automation runs (both drive the editor). Unknown specs error with the available list.
- First full run through the new path immediately caught a real nav regression: both pause specs fail (`TogglePause` pushes `WBP_PauseScreen_C` but activation-time desired focus never applies — focus stays on the game viewport) while `main_menu`/`options_roundtrip` pass 16/16 — pausing *over the HubWorld main menu* is a state that didn't exist when the suite was green (2026-06-09). Logged in the game backlog; game-or-spec fix is a design call.

### Widget property value readback (2026-07-03)

- **`get_widget_info` + `widgetName` returns property values** — the last hole in the readback-depth list: authored widget values (TextBlock text, fill percent, visibility, font) were unverifiable without hand-building `inspect` subobject paths. The response carries the widget's reflected properties (every exportable one, or just `propertyNames[]` — explicitly null when a named property doesn't exist on that class), its exact `objectPath` for `inspect get_property`/`set_property` follow-ups, and `slotClass`. Unknown `widgetName` errors with the tree's actual widget names. Verified live: `add_text_block` text + fontSize round-trip on a scratch widget, sub-widget-class null semantics, and WBP_HUD bar reads.

### AnimBP graph readback (2026-07-03)

- **`list_animbp_graphs` and `get_transition_rule_graph` implemented** — both were schema-advertised on `manage_blueprint` with no handler behind them (fell through to `INVALID_SUBACTION`). The list walks every graph in the blueprint (anim subgraphs included) with type classification (StateMachine / State / TransitionRule / Conduit / CustomTransitionBlend / AnimGraph / EventGraph / Function / Macro), node counts, parent hierarchy, and a `transition: "From -> To"` label on rule graphs — whose generated names collide (`AnimationTransitionGraph_0` ×4 in one state machine) and are otherwise indistinguishable.
- **Transition conditions are finally readable** — `get_transition_rule_graph` resolves a transition by `fromState`+`toState` (optionally `stateMachine`) or `transitionName` and returns crossfade duration, priority, automatic-rule flag, blend logic type, and the bound rule graph with full per-node pins, defaults, and `"<nodeGuid>:<pinName>"` links — the "why does this transition fire" that `get_graph_details` topology never carried. Parallel transitions between the same two states are reported honestly (`matchCount` + `transitionIndex` selector).
- **`get_graph_details` gained `includePinLinks`** — opt-in per-node pin/link detail on any graph; the node dump was factored into a shared builder used by both actions.

### Tool-result contract: failure details + structuredContent (2026-07-03)

- **Failure responses carry the handler's details object** — `BuildToolResult` serialized the result JSON into the text block only on success; failures collapsed to a bare `Error [CODE]: message`, dropping validation specifics, per-item outcomes, and the ENGINE_ERROR guard's `engineErrors` array (assembled since the guard shipped, never reached a client). Failures now append the details JSON exactly like successes; the first line is unchanged, so existing clients see strictly more.
- **`structuredContent` envelope on every tool result** — `{success, message, errorCode?, data?}` alongside `content[]`/`isError` (F7 review remainder). The handler result rides `data` verbatim rather than being spread at the top level, so handler keys can't collide with the envelope's. Typed clients get machine-readable outcomes without parsing the text block; text-only clients are unaffected.
- The `live_coding_compile`/`get_build_status` success-contract (completion states ride `success:true` with the outcome in the body) stands on its own merits — a failed *build* is a successful *status report* — but no longer exists to dodge the transport dropping details.

### Small-bug batch (2026-07-03)

- **`add_montage_notify` PlayMontageNotify NAME_None** — all three notify-creation sites now set the notify *object's* `NotifyName` (reflection; the montage On-Notify delegates broadcast the object member, not the event's). The routed `add_montage_notify` branch was a third creation site the first fix pass missed — caught by the live repro reading back `None`.
- **`delete_asset` false ENGINE_ERROR** — `[SourceControl] … paths from the index` status chatter is now classified benign in the post-op guard.
- **`get_graph_details` on pose-dataflow graphs** — graphs with no exec pins (AnimGraph, state-machine inner graphs) skip exec-liveness instead of flagging every node dead; response carries a `livenessNote`. Exec graphs unaffected (verified both ways).
- **`inspect_class` `/Script/` resolution** — verified already fixed (StaticLoadClass path branch); TODO entry closed with live evidence.

### Build visibility + latency (2026-07-03)

- **`run_ubt` is fire-and-poll** — launches UBT detached (default `-NoUBA -WaitMutex`; `noUBA:false` opts into faster UBA builds when no Live Coding patching follows) with output redirected to `Saved/McpBuilds/<buildId>.log`, returning a `buildId` in milliseconds. The previous implementation pumped a pipe **on the game thread for up to 300 seconds**, freezing the editor and every queued bridge call for the whole build. A third, unreachable `run_ubt` copy went with the dead `McpAutomationBridge_PipelineHandlers.cpp` TU (zero call sites).
- **New `system_control get_build_status`** — polls a build (default: the session's latest): `running|succeeded|failed`, returnCode, `[N/M]` progress, parsed compiler-error lines, warningCount, the `Result:` line, logTail, logPath. Verified live on a clean build and a bogus-target failure.
- **`live_coding_compile` returns real diagnostics** — captured LogLiveCoding output, parsed compiler `errors` (on Failure they only exist in the LiveCoding console and UBT's `Log.txt`; the handler scrapes the latter and echoes `ubtLogPath`), and a `reason` explaining NoChanges. Completion states now ride a *successful* response with the outcome in the body (`success`/`compileResult`) — the transport's error shape drops the details object, and a failed compile's logged errors previously tripped the post-op ENGINE_ERROR guard, which replaced the diagnostic response outright (`ClearCapturedErrors()` before responding).
- **Background-throttle latency tax removed** — the bridge disables `bThrottleCPUWhenNotForeground` (live value only, never saved) when the native server starts: every call is serviced on the game-thread tick, and a backgrounded editor throttled that to a steady ~332ms per call vs ~156ms unthrottled (measured; floor ~20ms when the game thread is idle). The client's window is almost always the foreground one, so this taxed *every* round-trip.

### Repo + docs pass (2026-07-03)

- **`main` is the default branch** — `dev` was merged into `main` (74a237db) and GitHub's default flipped; the two are identical and future work lands on `main`. The fork had inherited upstream's `dev`-as-default convention while `main` sat frozen at April upstream.
- **Docs-reference regression test** (`tests/docs/docs-reference-test.ps1`) — offline guard against the rot class the audit found: relative links must resolve, backticked `Mcp*`/`Handle*`/`.ts` file+symbol references must exist in the tree, and known-rot tripwire phrases fail (`ws://` endpoints, `npx` install steps, `src/tools/`, "TypeScript and C++", `Saved/Config` as a settings location). Lines that describe their referent as deleted/moot are exempt (±1 line for wrapping); deliberate history pins live in `tests/docs/docs-reference-allowlist.txt`. Companion convention in AGENTS.md + extending-the-bridge step 7: docs (especially design-doc Status headers) flip in the same commit as the change that outdates them — the part no test can catch.
- **Docs staleness audit** — every living doc verified claim-by-claim against the code (12 auditors, 60 findings, all fixed): root README's enable instructions pointed at the pruned `Saved/Config` file and still claimed SSE; the plugin-level README/AGENTS.md/CHANGELOG still documented the deleted WebSocket/TypeScript bridge (README rewritten native-only, AGENTS.md contracts corrected, CHANGELOG pointerized to this file); `pull-architecture.md` now describes the shipped parked-connection design and the per-session role of sessions; `Roadmap.md`/`native-automation-progress.md` dropped TS-era claims and mark the five already-implemented Phase-28 actions; CommonUI as-builts' status headers now say SHIPPED/RESOLVED instead of "no code written"; `tests/schema/` gained the README the other batteries had; `Engine-API-Reference.md` retargeted 5.6→5.7.

### Post-audit follow-ups (2026-07-03)

- **Per-session tool enablement** — `manage_tools` mutations now scope to the calling session (overlay keyed by `Mcp-Session-Id`; registry defaults are the shared immutable template). One client trimming its tool set no longer disables tools under concurrent sessions; `tools/list` is session-filtered; `TOOL_DISABLED` self-attributes the session's own last change; overlays drop on `reset`, session expiry, and `DELETE /mcp`.
- **GAS correctness:** tag writes target the modular 5.3+ GE components (granted/requirements/immunity) so they survive `PostCDOCompiled` — previously lost on compile for fresh 5.7 effects; `add_effect_modifier` requires and resolves an `attribute` (bare / `Set.Attr` / path-qualified, loading unloaded sets); all 13 mutating GAS actions now save (were dirty-only — writes vanished on editor restart).
- **Readback depth:** `get_gas_info` (duration/period/modifiers/dual tag view), `get_combat_stats` (CDO values), `get_animation_info` (BlendSpace axes/samples, AnimBP state machines), `get_character_info` (mesh/ABP/CMC), `get_mesh_info` (static-mesh asset branch: per-LOD counts, collision, Nanite), `get_networking_info` (per-property replication, RPCs).
- **Discoverability:** 637 undeclared-but-working params declared across all 22 schemas (Sonnet sweep, reviewed, reconciliation-gated); `manage_asset` gains per-asset `save` and routes `save_all` to the existing `control_editor` implementation; `UNKNOWN_ACTION` errors name the sub-action, not the tool.
- Geometry: `hullPrecision` drives `ConvexDecompositionSearchFactor`; `collisionType` enum matches the handler. Dead `MiscHandlers.cpp` TU deleted (980 lines, zero call sites).

### Dogfood audit + fix waves (2026-07-02, consolidated record in `docs/hardening-2026-07-02.md`)

- **Live audit of all 22 tools** (471 calls, 112 findings, report in `docs/dogfood-audit-2026-07-02.md`); ~95 findings fixed same day, every fix re-verified by re-running the audit's exact failing repro against the rebuilt editor.
- **Silent-success class eliminated** — interaction `configure_*` persists (values reach `FBPVariableDescription::DefaultValue` + compiled CDO, read-modify-write partial configures), `set_anchor` writes and reads back, GAS tag setters error on unregistered tags, `inspect set_property` `saved:true` means on-disk, character scaffolds seed defaults, brush volumes get real geometry (`UActorFactory::CreateBrushForVolumeActor` — creates/resize/readback verified; previously null-`UModel`, extent 0,0,0 forever).
- **Destination paths honored everywhere** — audio/animation/geometry/AI/interaction creates accept `path`/`savePath` aliases and no longer spray hardcoded folders; `manage_ai create` honors `blackboardPath` instead of silently binding the first (production) blackboard found.
- **Dead advertised actions now real** — GAS `add_ability`, BT `add_task_node`/`add_decorator`/`add_service` (enum-name wrappers over graph authoring), `set_niagara_parameter` (existed, was unroutable), SoundCue root wiring via `Root` pseudo-node, `create_behavior_tree` produces graph-authorable BTs.
- **Dead-param triage** — `param-reconciliation-test.ps1` found ~40 declared-but-never-read params beyond the audit's 20; 15 implemented for real (geometry: `weldDistance`/`hardEdgeAngle`/`computeWeightedNormals`/`targetEdgeLength`/`uvOffset`/`numSides`/`numRings`/`radialSegments`/`hullCount`/`reductionPercent`; `reductionSettings` on `generate_lods`; GAS `period` + `setByCallerTag`; sight-perception params; EQS `searchCenter`), ~28 removed as speculative or response-keys-declared-as-inputs (incl. `timeoutMs`×4, deprecated `loSHearingRange`).
- **Editor-exit crash fixed** — listen-socket use-after-free in transport shutdown (two-owner teardown race); `Stop()` now only closes, `Shutdown()` destroys once after thread join.
- **Sanitizer no longer redacts error guidance** — handler-authored error text keeps its paths/action names; the Unix-path matcher requires real path shapes even in engine-captured text.
- **`TOOL_DISABLED` demystified** — not a race: `manage_tools` state is global across sessions; errors now cite the last mutation + age and state the shared-across-sessions behavior.
- **Dead code deleted** — Interaction §6 dispatch + 6 unreachable §7 handlers, AudioAuthoring duplicate creates, MiscHandlers PPV chain; `Encumberance`→`Encumbrance` (old wire spellings still accepted).
- **New test:** `tests/schema/param-reconciliation-test.ps1` — offline static check that schema-declared params are read and payload-read params are declared (`-UpdateAllowlist` re-pins; dead pins currently 6, all documented indirect reads).

### Architecture-review hardening (2026-07-02, review in `docs/architecture-review-2026-07-02.md`)

- **Boot-time action-routing validation** (`McpStartupValidation`) — every canonical tool must register; shared list duplicates, routed-family overlaps, family/core shadowing, and schema/union drift all fail loudly at editor start. All 22 tool schemas now come from shared lists in `McpConsolidatedActionRouting.h`.
- **Loud registration + honest tool errors** — silently-dropped tool/handler registrations now error+ensure; unknown tool names return `UNKNOWN_TOOL` instead of `TOOL_DISABLED`; `manage_tools` summaries surface `notFound`/`protected`.
- **Honest persistence** — a throttled save can no longer report success while leaving a dirty package unsaved; material graph edits (FINALIZE_HOST + 12 legacy sites + 6 manage_material_graph sites) and all 32 Niagara `save:true` sites now write to disk; `AddAssetVerification` reports observed `existsOnDisk`/`unsavedChanges`; failed saves escalate to `ENGINE_ERROR` via the log capture.
- **Exactly-one-response guarantee** — the 10 `AsyncTask(GameThread)` re-queue completions were inlined; a consuming handler that neither responds nor calls `MarkRequestDeferred` gets an immediate `HANDLER_NO_RESPONSE` instead of the 300s reaper; `ERequestOrigin::WebSocket` renamed `Unspecified`; unroutable responses log at Error.
- **manage_asset sub-action dispatch fixed** — `fixup_redirectors`, `bulk_rename`/`bulk_delete`, source-control actions, `find_by_tag`, `analyze_graph`, `nanite_rebuild_mesh` had never worked through the native tool gate (gated sub-handlers received the raw tool name and declined); previously-unadvertised implemented actions added to schemas (`manage_gas` create_ability_set/add_ability/create_execution_calculation; `manage_blueprint` list_animbp_graphs/get_transition_rule_graph).
- **Dead WebSocket transport deleted** — `FMcpBridgeWebSocket` (~2,100 lines) + OpenSSL/SSL deps, 19 dead WS-era settings, Pattern B dispatch, `OnToolsChanged`, notification builders, phantom PCH, and the two game-thread-sleeping protocol self-tests are gone; handler signatures thread the inert `FMcpResponseHandle` alias; `docs/pull-architecture.md` is DONE.
- **Build-graph de-inversion** — `McpAutomationBridgeHelpers.h` no longer includes the subsystem header (envelope senders moved to `McpResponseHelpers.h`; `FMcpResponseHandle` + log category live in `McpAutomationBridgeGlobals.h`); extension guide documents the subsystem-header freeze.
- **Operational polish** — warn-first schema validation of tools/call arguments (missing required fields, out-of-enum values); one queued request drained per tick instead of the whole queue; slow handlers (>1s) log a Warning; the screenshot handler restores `bThrottleCPUWhenNotForeground`; the log-capture ring only attaches when the bridge is enabled.
- **Schema snapshot test** — `tests/schema/schema-snapshot-test.ps1` pins the published tools/list contract against a checked-in golden (`-UpdateGolden` to re-pin intentionally).

### Added

- **`manage_behavior_tree.add_subnode` action** — authors decorators and services as subnodes attached to root/composite/task graph nodes (rather than as standalone graph nodes). Supports `parentNodeId="root"` sentinel for top-level decorators that populate `UBehaviorTree::RootDecorators` after editor compile. Validates subnode UClass against `UBTDecorator` / `UBTService` and rejects Services on the root graph node (`INVALID_PARENT_FOR_SUBNODE`).
- **`manage_behavior_tree.set_node_properties` now handles `FBlackboardKeySelector` struct properties** — pass `properties: { BlackboardKey: "<key name>" }` and the handler calls `ResolveSelectedKey` against the BT's assigned blackboard. Returns `BB_KEY_NOT_FOUND` when the resolved selector ID is invalid (silent-failure guard).
- **`FindGraphNodeByIdOrName` now walks `UAIGraphNode::SubNodes`** — all four BT SubActions (`connect_nodes`, `remove_node`, `break_connections`, `set_node_properties`) gain subnode-aware lookup as a side effect, enabling `set_node_properties` on a decorator/service via its `nodeId`.
- **Native MCP Streamable HTTP Transport** — built-in HTTP/SSE MCP server directly in the C++ plugin, no TypeScript bridge or Node.js required. AI clients connect via `http://localhost:3000/mcp`. Supports SSE streaming, multiple concurrent sessions, dynamic tool management. Opt-in via `bEnableNativeMCP` project setting.
- **`execute_python` action** in `system_control` — execute Python code inline or from `.py` files with stdout/stderr capture, execution time tracking, and RAII temp file cleanup. Max code size: 1 MB.
- **Capability token authentication** for native MCP transport — validates `X-MCP-Capability-Token` header when `bRequireCapabilityToken` is enabled.
- **Native C++ self-describing tool definitions** with `FMcpSchemaBuilder` fluent API — replaces JSON schema loader. The TypeScript bridge exposes 22 canonical parent MCP tools.
- **Dynamic tool manager** — enable/disable tools and categories at runtime via `manage_tools`, with protected tools/categories.
- **Editor status bar indicator** — shows MCP port and active session count.
- **`animation_physics: force_rebuild_blend_space` action** — explicit rebuild for `UBlendSpace` / `UBlendSpace1D` assets whose `SampleData` or `BlendParameters` UPROPERTY were mutated via a path that bypasses `PostEditChangeProperty` (e.g. raw Python `set_editor_property` or scripted `sample_data` writes), which otherwise leaves the BlendSpace's cached grid stale and causes referencing `AnimBlueprint` compiles to warn `sample out of bounds`. The handler calls `ValidateSampleData()` to drop invalid/out-of-range samples, then `PostEditChangeProperty` against the `SampleData` UPROPERTY to force grid + RuntimeBuilder rebuild. Optional `rebuildBlendParameters: true` triggers an additional event for `BlendParameters` (axis min/max). Optional `compileReferencers: true` (default) cascade-compiles every `AnimBlueprint` referencing the BlendSpace via `IAssetRegistry::GetReferencers`, returning `referencersCompiled` count and `compiledAnimBlueprints` path list. Also adds a `PostEditChange()` call to the existing `add_blend_sample` handler so MCP's own sample additions can no longer leave the grid stale.
- **`animation_physics: bind_anim_notify` action** — authors the AnimNotify→`BlueprintCallable` wiring that montage-driven combat needs but the bridge previously could not do (it could place a notify, but not bind it to a function). Given an Anim Blueprint (`blueprintPath`/`assetPath`), a `notifyName`, a `functionName`, and an optional `targetClass`, it adds an `AnimNotify_<notifyName>` `UK2Node_CustomEvent` to the AnimBP **EventGraph** (the `UbergraphPages` page named `EventGraph` — never the `AnimGraph`), then wires `event.then → Cast<targetClass>.exec`, `TryGetPawnOwner → Cast.source`, `Cast.validCast → functionName.exec`, `Cast.result → functionName.self`, and compiles + safe-saves. A name-only montage notify dispatches via `FindFunction("AnimNotify_"+name)` on the AnimInstance (`UAnimInstance::TriggerSingleAnimNotify`), so the custom-event name is the contract. Uses the verified `UK2Node_DynamicCast` accessors (`GetValidCastPin`/`GetCastResultPin`/`GetCastSourcePin`), and is robust to the owner getter being pure vs impure. Idempotent: returns success without duplicating if the `AnimNotify_<name>` event already exists. Avoids the historical `add_event` pin-duplication bug (no `AllocateDefaultPins` after `FGraphNodeCreator::Finalize`) — verified live across an editor restart with zero dropped links. `targetClass` defaults to `APawn`; resolves both `/Game/...` BP paths and `/Script/...` native names.
- **`manage_blueprint: get_graph_details` now reports execution reachability / dead nodes** — each node gains an `execReachable` boolean and the result gains a top-level `deadNodeCount` + `deadNodes: [{ nodeId, nodeTitle, reason }]` summary. `reason` is `exec-unreachable` (a node with exec pins not reached from any entry along exec wires) or `feeds-nothing-live` (a pure/data node whose output feeds only dead nodes). Computed by a new `ComputeGraphLiveness` pass: (1) exec BFS from true entry nodes — an entry is a node with an exec OUTPUT pin and NO exec INPUT pin (events, custom/input events, function & macro entries), so an exec node whose exec input merely happens to be unconnected is correctly NOT treated as an entry and reads as dead; (2) a backward data-liveness fixpoint marking a pure node live iff it transitively feeds a live node. Closes the gap where `linkCount`/`isOrphan` count a node's data + `then` wires and so report an exec-unreachable node (e.g. a `Set Sound Mix Class Override` with an empty exec input but wired Volume/`then`) as "connected". Connectivity only — node enable-state is reported separately. Reuses the existing `ArrangePinIsExec`/`ArrangeNodeIsPure` helpers.
- **`animation_physics: get_animation_info` now returns montage internals** — for `UAnimMontage` assets the response gains `blendInTime`/`blendOutTime`/`autoBlendOut`, a `sections` array (`{name, startTime, nextSection}`), and a `slots` array (`{slotName, segments:[{anim, startPos, length}]}`). Previously only counts (`numSections`/`numSlots`/`numNotifies`) were exposed, so verifying montage state — blend tuning, section→section links, the slot's segment source anims — required raw `execute_python`, and slot tracks aren't even Python-reflectable. Reads `CompositeSections` (`FCompositeSection::GetTime`), `BlendIn`/`BlendOut` (`FAlphaBlend::GetBlendTime`), `bEnableAutoBlendOut`, and `SlotAnimTracks` (`FAnimSegment::GetAnimReference`/`GetLength`).
- **`get_input_info` now lists an InputMappingContext's mappings** — for a `UInputMappingContext` the response gains a `mappings` array (`{action, actionName, key, triggerCount, modifierCount}`) alongside `mappingCount`, via the canonical `GetMappings()` accessor (which reads the live storage even when the raw `Mappings` UPROPERTY reflects empty — UE5.7 keeps the data in the nested `DefaultKeyMappings`). Previously only `mappingCount` was returned, so reading action→key bindings needed raw `execute_python`. Also accepts `blueprintPath` as a fallback for `assetPath`, since the `manage_networking` tool that re-dispatches input actions exposes `blueprintPath` (not `assetPath`) — without which `get_input_info` was effectively uncallable over MCP.
- **`inspect: diff_asset` action** — structural diff between two on-disk `.uasset` versions (e.g. a git revision vs the working tree), so binary Blueprint changes can be reviewed without UE's in-editor visual diff or an editor restart to load both versions. Loads each file as an isolated package via `DiffUtils::LoadPackageForDiff` (so neither collides with the live, already-loaded asset), then compares Blueprint structure: `parentClass`, implemented `interfaces`, SCS `components` (added/removed/class-changed), `variables` (added/removed/type-changed), function/event `graphs` (added/removed + node-count delta), and — when `includeDefaults`, default true — CDO default properties (`changed`/`added`/`removed`, via reflected `ExportObjectToJson` + a deterministic key-sorted stringify for equality). A `gasSignals` array flags any added component/variable/interface/graph/default whose name or class contains GAS keywords (AbilitySystem / Attribute / GameplayAbility / GameplayEffect / GameplayTag / …). The client supplies the two versions as filesystem paths (`oldFilePath` / `newFilePath`; e.g. `git show <ref>:<path>` to a temp file for the old side) with optional `assetName` (default = `newFilePath` filename stem). Non-Blueprint assets fall back to a generic reflected property diff. Verified live: diffing a melee-dummy BP across its working-tree change reported exactly one delta (`SwingInterval 1.0 → 2.0`) with empty `gasSignals`, cutting through `+3.2 KB` of re-serialization byte-churn to the single semantic change. After comparing, the handler **releases both diff packages** (`ResetLoaders` to drop the linker and free the on-disk file handle, then clears `RF_Standalone`/`RF_Public` and requests a GC) — without this each diff leaks a package, keeps the source file locked until the next GC/editor restart, and a later diff of the same path returns a stale cached copy. The interactive editor diff tool can't do this (its package must stay loaded while the diff window is open, GC'd on close); a one-shot diff has no reason to hold it.

### Security

- **Symlink escape prevention** in `execute_python` file path validation — resolves symlinks and re-validates against project directory.
- **Code size limit** — `execute_python` enforces 1 MB maximum for inline code payloads.
- **Explicit request origin tracking** (`ERequestOrigin`) — routes HTTP vs WebSocket responses by explicit origin instead of inferring from `TargetSocket==nullptr`.
- **Tool registry thread safety** — `Register()` holds `CacheMutex` for entire body; `GetAllTools()` returns copy.
- **Dynamic tool manager protection** — `EnableCategory("all")` respects protected categories and initial state.

### Changed

- Tool categories now use four groups: `core`, `world`, `gameplay`, and `utility`. The singleton `authoring` category was removed, and `manage_blueprint` moved into `core`.
- `manage_blueprint` schema: `location`, `rotation`, `scale` changed from flat number arrays to structured objects with named sub-fields — matches TypeScript schema.
- `system_control` schema: removed `export_asset` action (not in TS) and `additionalArgs` parameter.
- `control_editor` schema: added `set_editor_mode` action.
- `ScanPathsSynchronous` removed from asset query/workflow handlers to prevent GameThread blocking. Documented limitation: newly-added assets may not appear until editor rescan.
- Screenshot handler now returns `async: true` with `expectedDelay` field and timing guidance.
- **Dispatch collapsed onto the O(1) handler registry.** `ProcessAutomationRequest` had a second, legacy linear if-chain that re-offered each action to ~50 `Handle*Action` functions as a fallback after the registry lookup — a stalled migration that, among other things, let one mis-gated handler silently swallow unrouted requests. The if-chain is deleted: every tool dispatches by name through `AutomationHandlers`, and a miss (or a handler that declines) is the single `UNKNOWN_ACTION` error. Registered the six actions that were reachable only via the if-chain, including the canonical `control_editor` tool.
- **`connect_pins` params standardized to `source`/`target`.** Blueprint `connect_pins` now reads `sourceNodeId`/`sourcePinName`/`targetNodeId`/`targetPinName` (handler + schema); Niagara `connect_pins` aligned to match. No aliases — the old `from`/`to` names return `NODE_NOT_FOUND`. Behavior-tree (`parentNodeId`/`childNodeId`) and state-machine (`fromState`/`toState`) keep their domain-correct vocabularies.
- **Consolidated two parallel JSON field-getter implementations onto one canonical.** `McpHandlerUtils::GetOptional*` and a TextureHandlers-local `TextureHandlerHelpers` namespace + `*TextAuth` macros — both behaviorally identical to `McpAutomationBridgeHelpers::GetJson*Field` — are deleted; ~360 call sites redirected to the canonical `GetJson*Field`. Deleting the losers is the forcing function: future reuse is a compile error, not a silently tolerated second path.

- **`inspect_cdo` sub-action** for the `inspect` tool – inspect any Blueprint's Class Default Object without spawning an actor. Reads CDO property values via reflection. For Actor BPs, enumerates all components: native CDO components with effective override values, plus Blueprint SCS components from node templates (full parent chain). Includes parent attachment info for SCS components. Source classified as Native, SCS, or SCS_Inherited. Key fields (mesh, animClass, transform) included in summary; full property export via detailed or propertyNames filter.

### Fixed

- **Game Feature Plugin path validation** – `SanitizeProjectRelativePath` now uses `FPackageName::IsValidLongPackageName` instead of a manual `/Content/` heuristic, correctly recognizing all registered engine mount points (game feature plugins like `/MyGameFeature/`, `/ShooterCore/`, `/ALS/`, etc.).
- **`manage_level: get_level_info` no longer requires the level to be loaded** – previously returned `LEVEL_NOT_FOUND` for any map asset path that hadn't already been `load_level`'d. Now falls back to `IAssetRegistry::GetAssetByObjectPath` and returns `objectPath`, `assetName`, `packageName`, `assetClass`, and the asset's `tagsAndValues` map alongside `loaded: false`. The loaded case is unchanged but now also includes `loaded: true`. Does not auto-load the level.
- **`inspect: set_property` refreshes stale node title cache for `K2Node_EnhancedInputAction`** – `HandleSetObjectProperty` writes any UObject's UPROPERTY by reflection, which includes K2Node subobjects when `objectPath` resolves to one. For K2Node types whose `GetNodeTitle` derives from a written property (e.g. `K2Node_EnhancedInputAction::InputAction`), the editor's `FNodeTextCache` is keyed off the schema's visualization cache ID, which `PostEditChange` does not bump. The result: the title kept rendering its prior value ("`EnhancedInputAction None`") until the user manually clicked the node, even though the binding had taken. After `PostEditChange`, if `RootObject` is a `UK2Node` whose class name is in a narrow whitelist (currently only `K2Node_EnhancedInputAction`), the handler now calls `K2Node::ReconstructNode()` and `Schema::ForceVisualizationCacheClear()` on the owning graph, followed by `NotifyGraphChanged()`, so the cached title is invalidated and the next render computes a fresh one. Other K2Node classes are unaffected (no `ReconstructNode` invoked outside the whitelist), and non-K2Node writes follow the unchanged path.
- **`manage_blueprint: create_node nodeType=VariableSet/VariableGet` now supports inherited UPROPERTY via `memberClass`** – previously the handler only consulted `Blueprint->NewVariables` and `Blueprint->GeneratedClass->FindPropertyByName`, so any attempt to reference a UPROPERTY that lives on a parent or SCS component class (e.g. `UCharacterMovementComponent::MaxWalkSpeed` from a `Character` Blueprint) returned `VARIABLE_NOT_FOUND`. Adds `McpFindPropertyRecursive` (walks the `UClass` parent chain) and accepts an optional `memberClass` payload field, supporting both short names (`"CharacterMovementComponent"`) and full paths (`"/Script/Engine.CharacterMovementComponent"`). Uses `IsChildOf` to classify self- vs external-context: properties on the Blueprint's own ancestor chain still use `SetSelfMember`; properties on an unrelated owner class use `SetFromField<FProperty>` so the K2Node exposes a `Target` pin the caller wires to a component reference. This makes generated graphs compile cleanly instead of failing with "Get CharacterMovement uses an invalid target".
- **`manage_blueprint`: AnimBP graph discovery actions now route to the correct handler** – `list_animbp_graphs` and `get_transition_rule_graph` are implemented in `HandleBlueprintGraphAction` (`McpAutomationBridge_BlueprintGraphHandlers.cpp`) but were missing from the dispatcher's `GraphSubActions` set in `McpAutomationBridgeSubsystem.cpp`, so requests fell through to `HandleBlueprintAction` and returned `UNKNOWN_ACTION`. Adds both actions to the routing set. No handler changes required.
- **`animation_physics` schema params `savePath` and `length` were silently ignored by several authoring actions.** `create_animation_sequence`, `create_aim_offset`, and `create_anim_blueprint` read only the undocumented `path` field, so a caller passing the schema's advertised `savePath` got the asset written to the default folder instead. They now prefer `savePath` and fall back to legacy `path`. `create_animation_sequence` and `set_sequence_length` read only `numFrames`/`frameRate` (neither advertised in the schema) and ignored the advertised `length` (seconds); both now honor `length`, with explicit `numFrames` as an override. (`create_montage`'s `savePath` was already fixed previously.)
- **`add_notify` is now montage-capable via the `animation_physics` tool.** It was absent from the `AnimationAuthoring()` routing list, so the `animation_physics` smart-router fell through to the weaker `McpAutomationBridge_AnimationHandlers.cpp` handler that only accepts `UAnimSequence` and rejected `UAnimMontage`. Adding `add_notify` to the routing list sends it to the authoring handler, which loads `UAnimSequenceBase` (montages included). Verified live: a notify now adds to a `UAnimMontage`.
- **300s hang on `manage_audio` `create_sound_class`/`create_sound_mix` — fixed (two root causes, neither a modal).** The `manage_audio` dispatch lambda passed the tool name to `HandleAudioAction` instead of the sub-action, so its `create_sound_` prefix gate never matched and the request parked. Compounding it, `HandleManageSkeleton`'s gate was inverted (returned "handled" for non-skeleton actions), silently swallowing the unrouted request until the stale-request watchdog. Five more handlers (sessions/level_structure/navigation/splines/volumes) read the sub-action with no Action gate; gates added. Diagnosed via minidump + cdb (GameThread idle, no handler on any stack — disproving the prior "Slate modal" theory). create_sound_class/mix now return in ~0.3s; the parked-request timeout warning now reports the active modal window + slate-active state.
- **`create_node` CustomEvent `float` params truncated to int.** The pin-type resolver set a `PC_Float` *category* (which compiles to an int — silent truncation) while the adjacent `double` case correctly used `PC_Real`. Now `PC_Real` + float sub-category. Same class as the earlier `add_variable`/`MakePinType` fix; this site had been missed.
- **`manage_asset` material-graph connect schema declared dead duplicate params.** Removed `fromNodeId`/`fromPin`/`toNodeId`/`toPin` (residue from the removed TypeScript auto-map, #50) — the handler reads `sourceNodeId`/`targetNodeId`/`inputName`.

### Tests

- Added characterization integration test for `manage_ai.get_ai_info(blackboardPath)` BB key serialization shape. Pins `assignedBlackboard`, `keyCount`, and `blackboardKeys[].{name,type,instanceSynced}` so future BB serializer refactors cannot silently regress callers.

---

## 🏷️ [0.5.21] - 2026-04-03

> [!IMPORTANT]
> ### 🔒 Security, New Features & Major Crash Fixes
> This release adds custom content mount points, full audio authoring, project settings management, vehicle physics configuration, blend tree/procedural animation/state machine creation, sequencer improvements, and critical crash prevention for deleting animation/IK assets and folders.

<details>
<summary><b>🛡️ Security</b></summary>

- **Command Injection in bump-version action** – Sanitized `release-type` input ([#327](https://github.com/ChiR24/Unreal_mcp/pull/327))
- **Command Injection in editor console commands** – Mixed-context sanitization for `start_recording`, `set_camera_fov`, `set_game_speed` ([#322](https://github.com/ChiR24/Unreal_mcp/pull/322))
- **Path Traversal in `export_level`** – Added path validation ([#305](https://github.com/ChiR24/Unreal_mcp/pull/305))
- **Path Traversal in screenshot filename** – Sanitized filenames, blocked traversal patterns ([#314](https://github.com/ChiR24/Unreal_mcp/pull/314))
- **Synchronous fs Hardening** – Replaced blocking `fs.existsSync` / `fs.readdirSync` with async versions ([#318](https://github.com/ChiR24/Unreal_mcp/pull/318))

</details>

<details>
<summary><b>✨ Added</b></summary>

- **Custom Content Mount Points** – `MCP_ADDITIONAL_PATH_PREFIXES` to whitelist plugin mount points (`/ProjectObject/`, etc.) ([#326](https://github.com/ChiR24/Unreal_mcp/pull/326) – thanks @6r0m)
- **Full Audio Authoring** – Create sound waves, sound cues, sound classes, sound mixes, attenuation settings; success flags in responses.
- **Project Settings Management** – New `manage_project_settings` tool (get/set project settings via config).
- **Animation Authoring** – `create_blend_tree`, `create_procedural_anim`, `create_state_machine` (C++ implementations, not console commands).
- **Vehicle Physics Configuration** – `configure_vehicle` with wheels, engine, transmission, mass, drag coefficient.
- **Sequencer** – `set_tick_resolution`, `set_view_range` actions.
- **Widget Authoring** – New template widgets: main menu, pause menu, HUD, crosshair, ammo counter, health bar, compass, interaction prompt, objective tracker, damage indicator, inventory grid, dialog box, radial menu, credits scroll, shop UI, quest tracker.
- **Runtime Module Checks** – Verify GameplayAbilities, EnhancedInput, BehaviorTreeEditor, LevelSequenceEditor, NiagaraEditor, StateTree, SmartObjects, MassEntity are loaded before use (clear error messages when plugins missing).

</details>

<details>
<summary><b>🛠️ Fixed</b></summary>

#### Crash Prevention (UE 5.7+)

- **Animation/Rig asset deletion** – Completely rewrote `McpSafeDeleteFolder` and added `DeleteAnimationRigClusterOrdered` to prevent 0xFFFFFFFFFFFFFFFF crashes when deleting AnimBlueprints, IKRigs, IKRetargeters, ControlRigBlueprints, and AnimSequences.
- **Folder deletion** – Replaced `UEditorAssetLibrary::DeleteDirectory` with `McpSafeDeleteFolder` (proper world switching, package unloading, compilation quiesce).
- **Blueprint creation** – Added pre‑creation checks in `CreateControlRigBlueprint` and widget blueprint creation to prevent engine assertion failures.
- **Widget creation** – Fixed widget crash ([#306](https://github.com/ChiR24/Unreal_mcp/pull/306)) by adding GUID registration (`RegisterWidgetGuid`) and safe tree replacement (`SafeAddWidgetToTree`).
- **AnimNotify/NotifyState** – Added abstract class validation and track existence checks.

#### Asset & Path Handling

- Improved asset loading reliability for newly created AI assets (removed stale `DoesAssetExist` checks).
- Resolved asset query parameter bugs and expanded `classNames` support ([#311](https://github.com/ChiR24/Unreal_mcp/pull/311)).
- Replaced custom asset directory checks with `UEditorAssetLibrary` to avoid stale cache issues.
- Fixed `searchText` filtering in `search_assets` action ([#308](https://github.com/ChiR24/Unreal_mcp/pull/308)).
- Added `offset` pagination to asset search.

#### Blueprint & Graph Editing

- Unified pin serialization across blueprint graph handlers ([#309](https://github.com/ChiR24/Unreal_mcp/pull/309)) – linked pins returned as objects with `nodeId` and `pinName`.
- Improved actor lookup to match subsystem behavior (checks both label and name).
- Aligned `get_ai_info` output with TypeScript schema ([#310](https://github.com/ChiR24/Unreal_mcp/pull/310)).

#### Performance & Console

- Delegated console command settings to C++ handler for better performance.
- Ensured successful execution of console commands (check `GEngine->Exec` return value).
- Added validation for required session parameters (interfaceType, controllerId, playerIndex, etc.).
- Removed redundant `AsyncTask` wrappers in `generate_thumbnail` and `generate_lods` (fixed 30‑second timeout).

#### Level Operations

- **`rename_level`** – Now uses `DuplicateAsset` + `DeleteAsset` to avoid modal “Find/Replace” dialog.
- **`duplicate_level`** – Validates source existence and deletes destination if already present.
- **`export_level`** – Added source level existence check before export.

#### Voice Chat & Sessions

- Improved `mute_player` – falls back to `BlockPlayers` when voice server not connected.
- Added validation for required parameters in all session actions.

#### Plugin Stability

- Used delay‑load for optional plugin modules to prevent missing dependency errors ([#317](https://github.com/ChiR24/Unreal_mcp/pull/317)).
- Refactored IK retargeter initialization using controller API (UE 5.7+) with backward compatibility fallback.
- Enhanced actor and component stability across subsystems.

#### Documentation

- Fixed rate limiting defaults and missing GraphQL heading ([#307](https://github.com/ChiR24/Unreal_mcp/pull/307)).

</details>

<details>
<summary><b>🔄 Dependencies</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `picomatch` | 4.0.3 → 4.0.4 | [#316](https://github.com/ChiR24/Unreal_mcp/pull/316) |
| Dependencies group | 9 updates | [#320](https://github.com/ChiR24/Unreal_mcp/pull/320) |
| `github/codeql-action` | 4.33.0 → 4.34.1 | [#319](https://github.com/ChiR24/Unreal_mcp/pull/319) |

</details>

<details>
<summary><b>👥 Contributors</b></summary>

- @google-labs-jules[bot] for all security fixes
- @kalihman for asset query, searchText, docs, and blueprint graph fixes
- @dependabot[bot] for dependency updates
- @6r0m for custom content mount points (first contribution)

</details>

---

## 🏷️ [0.5.20] - 2026-03-21

> [!IMPORTANT]
> ### 🛡️ Security Fix & UE 5.0 Compatibility
> This release includes a critical path traversal fix in export_asset, UE 5.0 compatibility improvements, and external actors support for World Partition.

### 🛡️ Security

<details>
<summary><b>🔒 Path Traversal in export_asset</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/5cf2a3c">5cf2a3c</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Path traversal in `export_asset` action |
| **Fix** | Added path validation to prevent directory traversal attacks |

**Files Modified:**
- `McpAutomationBridge_SystemControlHandlers.cpp`

</details>

### ✨ Added

<details>
<summary><b>🌍 External Actors Support</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/51143c3">51143c3</a>)</summary>

| Feature | Description |
|---------|-------------|
| **External Actors** | Support for World Partition external actors in level structure handlers |
| **Streaming Reference** | Streaming reference creation for external actor packages |

**Files Modified:**
- `McpAutomationBridge_LevelStructureHandlers.cpp` (+127 lines)

</details>

### 🛠️ Fixed

<details>
<summary><b>🎮 UE 5.0 Compatibility</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/1057023">1057023</a>)</summary>

| Bug | Fix |
|-----|-----|
| `bIsWorldInitialized` API not available in UE 5.0 | Direct access to `bIsWorldInitialized` for UE 5.0 compatibility |

**Files Modified:**
- `McpAutomationBridgeHelpers.h`
- `McpAutomationBridge_LevelStructureHandlers.cpp`

</details>

<details>
<summary><b>🐛 Tick Task Manager Crashes</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/8c311d7">8c311d7</a>)</summary>

| Bug | Fix |
|-----|-----|
| Crashes from tick task manager during world operations | Added safety checks and proper cleanup in world management |
| World cleanup issues | Enhanced cleanup with `FlushRenderingCommands` safety |

**Files Modified:**
- `McpAutomationBridgeHelpers.h` (+36 lines)
- `McpAutomationBridge_LevelStructureHandlers.cpp` (+65 lines)
- `McpSafeOperations.h` (+16 lines)

</details>

<details>
<summary><b>🐛 Sublevel Creation</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/bffb68c">bffb68c</a>)</summary>

| Bug | Fix |
|-----|-----|
| Sublevel creation path handling issues | Enhanced sublevel creation process with proper path handling |

**Files Modified:**
- `McpAutomationBridge_LevelStructureHandlers.cpp` (+201 lines)

</details>

<details>
<summary><b>🔧 UE 5.7 Build</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/295">#295</a>)</summary>

| Bug | Fix |
|-----|-----|
| Missing includes causing build failures on UE 5.7 | Added missing includes in `McpHandlerUtils.cpp` and `McpPropertyReflection.cpp` |

**Contributors:** @a2448825647

</details>

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| `release-drafter/release-drafter` | 7.0.0 | 7.1.1 | [#300](https://github.com/ChiR24/Unreal_mcp/pull/300) |
| `softprops/action-gh-release` | 2.5.3 | 2.6.1 | [#301](https://github.com/ChiR24/Unreal_mcp/pull/301) |
| `github/codeql-action` | 4.32.6 | 4.33.0 | [#299](https://github.com/ChiR24/Unreal_mcp/pull/299) |

</details>

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| `flatted` | 3.3.3 | 3.4.2 | [#304](https://github.com/ChiR24/Unreal_mcp/pull/304) |

</details>

### 🔌 Plugin

<details>
<summary><b>MCP Automation Bridge v0.1.3</b></summary>

Updated plugin version to 0.1.3 with all fixes and features from this release.

See [Plugin CHANGELOG](Plugins/McpAutomationBridge/CHANGELOG.md) for details.

</details>

---

## 🏷️ [0.5.19] - 2026-03-18

> [!IMPORTANT]
> ### 🛡️ Security Hardening & Major Plugin Refactoring
> This release includes critical security fixes for command injection and path traversal vulnerabilities, a complete deep-level refactoring of 57 C++ handler files with centralized utilities, and removal of the WebAssembly integration.

### 🛡️ Security

<details>
<summary><b>🔒 Command Injection Prevention</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/288">#288</a>)</summary>

| Component | Change |
|-----------|--------|
| **sanitizeCommandArgument()** | Added semicolon sanitization to prevent command chaining attacks |
| **Physics Tools** | Sanitized constraint names, actor names, vehicle names, destruction names |
| **Animation Tools** | Sanitized state machine names, state names, transition conditions |
| **System Handlers** | Sanitized vehicle type, save paths, and all user-provided strings |

**Attack Vector Blocked:** Input like `"name;quit"` can no longer execute arbitrary commands.

</details>

<details>
<summary><b>🔒 Path Traversal Fixes</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/271">#271</a>, <a href="https://github.com/ChiR24/Unreal_mcp/pull/282">#282</a>)</summary>

| Component | Change |
|-----------|--------|
| **validateSnapshotPath()** | Fixed bypass where paths equal to CWD (without trailing separator) were incorrectly rejected |
| **Asset Handlers** | Added path sanitization to prevent traversal attacks |
| **Blueprint Creation** | Added savePath sanitization |

</details>

<details>
<summary><b>🔒 GraphQL CORS Hardening</b></summary>

| Component | Change |
|-----------|--------|
| **Default CORS** | Changed from permissive `'*'` to safe loopback origins |
| **Allowed Origins** | `localhost:4000`, `127.0.0.1:4000`, `localhost:3000`, `127.0.0.1:3000` |
| **Warning** | Added security warning when `'*'` is explicitly configured |

</details>

### 🔧 Changed

<details>
<summary><b>🏗️ Complete C++ Plugin Refactoring</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/280">#280</a>)</summary>

Deep line-by-line refactoring of 57 handler files and 8 infrastructure files:

| New Infrastructure File | Purpose |
|------------------------|---------|
| `McpHandlerUtils.h/cpp` | Standardized JSON response builders (1,900 lines) |
| `McpPropertyReflection.h/cpp` | Property reflection utilities (1,356 lines) |
| `McpSafeOperations.h` | Safe asset/level save for UE 5.7 (659 lines) |
| `McpVersionCompatibility.h` | UE 5.0-5.7 API compatibility macros (225 lines) |
| `McpHandlerDeclarations.h` | Forward declarations (844 lines) |
| `McpAutomationBridge_ConsoleCommandHandlers.cpp` | Batch and single command execution (302 lines) |

**Bugs Fixed During Refactoring:**
- EditorFunctionHandlers: use-after-free bug
- EffectHandlers: truncated condition + missing braces
- InventoryHandlers: duplicate TArray with undefined variables
- MaterialAuthoringHandlers: duplicate include + missing UE 5.0 fallback
- NavigationHandlers: case-sensitivity error
- SkeletonHandlers: duplicate verification + redundant code
- WidgetAuthoringHandlers: unreachable code block

</details>

<details>
<summary><b>⚡ Performance Improvements</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/283">#283</a>)</summary>

| Component | Change |
|-----------|--------|
| **Batch Console Commands** | New batch execution API for parallel command processing |
| **validate_assets** | Changed from sequential to `Promise.all` concurrent validation |
| **State Machine Creation** | States now added in parallel instead of sequentially |

</details>

<details>
<summary><b>🗑️ WebAssembly Integration Removed</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/240">#240</a>)</summary>

Removed WebAssembly (wasm-pack/Rust) integration:
- Deleted `src/wasm/` directory (874 lines)
- Deleted `wasm/` Rust crate (1,500+ lines)
- Removed WASM dependency from all handlers
- Native C++ handlers provide equivalent functionality

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 Blueprint Inspect Crash</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/270">#270</a>)</summary>

| Bug | Fix |
|-----|-----|
| Blueprint inspect crashed when variable list exceeded buffer | Fixed truncated variable list handling |
| Function library blueprints not supported | Added function library blueprint support (#258) |

</details>

<details>
<summary><b>🐛 GAS Duplicate Effect Creation</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/251">#251</a>)</summary>

| Bug | Fix |
|-----|-----|
| `create_gameplay_effect` assertion failure on duplicates | Prevented duplicate GameplayEffect creation |

</details>

<details>
<summary><b>🐛 Volume Handler Mobility</b></summary>

| Bug | Fix |
|-----|-----|
| Volume attachment failed for movable actors | Added mobility check for target actors in volume handlers |

</details>

<details>
<summary><b>🐛 UE 5.7 Compatibility</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/274">#274</a>)</summary>

| Bug | Fix |
|-----|-----|
| GeometryScript AppendCapsule compile error on UE 5.5+ | Added version guard for segment steps parameter |

</details>

<details>
<summary><b>🐛 Action Name Alignment</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/253">#253</a>)</summary>

| Bug | Fix |
|-----|-----|
| TypeScript action names mismatched C++ handlers | Aligned all action names with C++ handler expectations |

</details>

### 🗑️ Removed

<details>
<summary><b>Deprecated Tool Files</b></summary>

Removed deprecated standalone tool files (consolidated into handlers):
- `src/tools/audio.ts` → `src/tools/handlers/audio-handlers.ts`
- `src/tools/debug.ts` → consolidated into system handlers
- `src/tools/introspection.ts` → `src/tools/handlers/inspect-handlers.ts`
- `src/tools/materials.ts` → `src/tools/handlers/material-authoring-handlers.ts`
- `src/tools/performance.ts` → `src/tools/handlers/performance-handlers.ts`
- `src/tools/ui.ts` → consolidated into widget handlers
- `src/tools/input.ts` → `src/tools/handlers/input-handlers.ts`
- `src/tools/behavior-tree.ts` → consolidated
- `src/tools/engine.ts` → consolidated

</details>

### 🔄 Dependencies

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| hono | 4.12.0 | 4.12.7 | [#261](https://github.com/ChiR24/Unreal_mcp/pull/261), [#277](https://github.com/ChiR24/Unreal_mcp/pull/277) |
| express-rate-limit | 8.2.1 | 8.3.0 | [#269](https://github.com/ChiR24/Unreal_mcp/pull/269) |
| @types/node | Various updates | | Multiple PRs |

</details>

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| release-drafter/release-drafter | 6.2.0 | 7.0.0 | [#286](https://github.com/ChiR24/Unreal_mcp/pull/286) |
| softprops/action-gh-release | 2.5.0 | 2.5.3 | [#287](https://github.com/ChiR24/Unreal_mcp/pull/287) |
| actions/setup-node | 6.2.0 | 6.3.0 | [#257](https://github.com/ChiR24/Unreal_mcp/pull/257) |
| github/codeql-action | 4.32.5 | 4.32.6 | [#266](https://github.com/ChiR24/Unreal_mcp/pull/266) |

</details>

### 📊 Statistics

- **Commits:** 55 non-merge commits
- **Files Changed:** 185 files
- **Lines Added:** ~30,280
- **Lines Removed:** ~20,440
- **New C++ Infrastructure:** 5 files (~4,900 lines)
- **Bugs Fixed:** 15+
- **Security Fixes:** 4 critical

---

## 🏷️ [0.5.18] - 2026-02-21

> [!IMPORTANT]
> ### 🔧 Installation, Documentation & Dependency Updates
> This release fixes npm install failures when downloading from GitHub releases, adds first-time project setup guidance, and updates dependencies.

### 🛠️ Fixed

<details>
<summary><b>🐛 npm install failure from release archives</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/215">#215</a>)</summary>

| Issue | Root Cause | Fix |
|-------|------------|-----|
| `npm install` fails with ESLint config error | Release archives excluded `eslint.config.mjs` and other build files | Added `-source` archives with complete build files |
| `prepare` script runs build unnecessarily | Checked only `dist/` existence, not build artifacts | Now verifies `dist/cli.js` and `dist/index.js` exist |
| Deprecated `--ext .ts` flag in lint | ESLint 9.x removed support for `--ext` flag | Removed flag, extensions configured in `eslint.config.mjs` |

**Files Modified:**
- `package.json` (prepare script, lint scripts, removed prebuild)
- `.github/workflows/release.yml` (added source archives, fixed plugin path)
- `README.md` (added Rust/wasm-pack prerequisites)

</details>

### 📚 Documentation

<details>
<summary><b>📖 First-time project open instructions</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/112df08">112df08</a>)</summary>

Added guidance for users opening Unreal projects for the first time:
- Explains UE prompt to rebuild missing modules
- Documents expected plugin load failure after first rebuild
- Recommends closing and reopening project to resolve

</details>

### ⬆️ Dependencies

| Package | From | To | PR |
|---------|------|-----|-----|
| hono | 4.11.7 | 4.12.0 | [#213](https://github.com/ChiR24/Unreal_mcp/pull/213) |
| ajv | 8.17.1 | 8.18.0 | [#210](https://github.com/ChiR24/Unreal_mcp/pull/210) |
| actions/stale | 10.1.1 | 10.2.0 | [#208](https://github.com/ChiR24/Unreal_mcp/pull/208) |
| actions/dependency-review-action | 4.8.2 | 4.8.3 | [#212](https://github.com/ChiR24/Unreal_mcp/pull/212) |

---

## 🏷️ [0.5.17] - 2026-02-16

> [!IMPORTANT]
> ### 🔧 World Tools Category Fixes & Security Hardening
> This release includes critical bug fixes, security hardening, and UE 5.7 compatibility improvements across all world-building tools (landscape, foliage, geometry, volumes, navigation).

### 🛡️ Security

<details>
<summary><b>🔒 Path Validation & Input Sanitization</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/207">#207</a>)</summary>

| Component | Change |
|-----------|--------|
| **SanitizeProjectRelativePath** | Rejects Windows absolute paths, normalizes slashes, collapses `//`, requires valid UE roots (`/Game`, `/Engine`, `/Script`) |
| **SanitizeProjectFilePath** | File operations with path traversal protection |
| **ValidateAssetCreationPath** | Combines folder + name validation for asset creation |
| **Actor/Volume Name Validation** | Blocks invalid characters, enforces length checks |
| **Snapshot Path Validation** | Prevents directory traversal attacks via snapshot paths |

**Files Modified:**
- `McpAutomationBridgeHelpers.h` (+326 lines of security helpers)
- `src/tools/environment.ts` (snapshot path security)
- `src/utils/path-security.ts` (path normalization)

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 Landscape Handler Silent Fallback Bug</b> (McpAutomationBridge_LandscapeHandlers.cpp)</summary>

| Bug | Root Cause | Fix |
|-----|------------|-----|
| False positives on non-existent landscapes | Path matching compared `GetPathName()` (internal path) with asset path | Normalized both paths with `.uasset` stripping |
| Silent fallback to single landscape | `if (!Landscape && LandscapeCount == 1)` used any available landscape | Removed fallback, now returns `LANDSCAPE_NOT_FOUND` error |
| Wrong response path | Returned requested path instead of actual path | Now returns `Landscape->GetPackage()->GetPathName()` |

**Affected Handlers:** `HandleModifyHeightmap`, `HandlePaintLandscapeLayer`, `HandleSculptLandscape`, `HandleSetLandscapeMaterial`

</details>

<details>
<summary><b>🐛 Rotation Yaw Bug</b> (McpAutomationBridge_LightingHandlers.cpp:200)</summary>

| Bug | Fix |
|-----|-----|
| `Rotation.Yaw` read from `LocPtr` instead of `RotPtr` | Changed to `GetJsonNumberField((*RotPtr), TEXT("yaw"))` |

**Impact:** Incorrect rotation when spawning lights with rotation parameters.

</details>

<details>
<summary><b>🐛 Integer Overflow in Heightmap Operations</b> (McpAutomationBridge_LandscapeHandlers.cpp:631-635)</summary>

| Bug | Fix |
|-----|-----|
| `static_cast<int16>(CurrentHeights[i])` overflows for values > 32767 | Changed to `static_cast<int32>` |

**Impact:** Heightmap raise/lower operations now produce correct results for heights above midpoint.

</details>

<details>
<summary><b>🐛 set_curve_key Success Reporting</b> (McpAutomationBridge_AnimationHandlers.cpp:2139)</summary>

| Bug | Fix |
|-----|-----|
| `bSuccess` initialized `false`, only set `true` inside `if (bSuccess)` block (unreachable) | Moved success logic before the condition check |

**Impact:** `set_curve_key` now correctly reports success.

</details>

<details>
<summary><b>🐛 CraftingSpeed Truncation</b> (McpAutomationBridge_InventoryHandlers.cpp:2716)</summary>

| Bug | Fix |
|-----|-----|
| `int32 CraftingSpeed` truncated fractional multipliers (1.5 → 1) | Changed to `double` |

</details>

<details>
<summary><b>🐛 Invalid Color Fallback Not Applied</b> (McpAutomationBridge_LightingHandlers.cpp:277)</summary>

| Bug | Fix |
|-----|-----|
| `SetLightColor()` only called when `bColorValid == true`, but `bColorValid = false` for invalid colors | Removed guard, always call `SetLightColor()` after correcting invalid colors to white |

</details>

<details>
<summary><b>🐛 Double-Validation in Snapshot Path</b> (src/tools/environment.ts:253, 322)</summary>

| Bug | Fix |
|-----|-----|
| Redundant second `validateSnapshotPath()` call on already-resolved absolute paths | Removed redundant call |

</details>

<details>
<summary><b>🐛 Intel GPU Driver Crash Prevention</b> (McpAutomationBridgeHelpers.h)</summary>

| Bug | Fix |
|-----|-----|
| `MONZA DdiThreadingContext` exceptions on Intel GPUs during level save | Added `McpSafeLevelSave` helper with `FlushRenderingCommands` and retry logic |

</details>

### ✨ Added

<details>
<summary><b>🛤️ LOD Generation Enhancements</b> (McpAutomationBridge_GeometryHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **landscapePath support** | LOD generation now accepts single `landscapePath` or array `assetPaths` |
| **lodCount parameter** | Alternative to `numLODs` for specifying LOD count |
| **Path sanitization** | All LOD operations use `SanitizeProjectRelativePath` |

</details>

<details>
<summary><b>🌿 FoliageType Auto-Creation</b> (McpAutomationBridge_FoliageHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **Auto-create FoliageType** | When painting/adding foliage, FoliageType is automatically created from StaticMesh if missing |
| **Path validation** | All foliage operations use path sanitization |

</details>

<details>
<summary><b>🏔️ Landscape Layer Auto-Creation</b> (McpAutomationBridge_LandscapeHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **Auto-create layers** | When painting, landscape layers are auto-created if they don't exist (matches UE editor behavior) |

</details>

<details>
<summary><b>📊 Handler Verification</b> (Multiple Handler Files)</summary>

| Pattern | Description |
|---------|-------------|
| **AddActorVerification** | Returns `actorPath`, `actorName`, `actorGuid`, `existsAfter`, `actorClass` |
| **AddComponentVerification** | Returns `componentName`, `componentClass`, `ownerActorPath` |
| **AddAssetVerification** | Returns `assetPath`, `assetName`, `existsAfter`, `assetClass` |
| **VerifyAssetExists** | Verifies asset exists at path |

**Files Updated:** PropertyHandlers, LevelHandlers, EffectHandlers, GASHandlers, SequenceHandlers, SkeletonHandlers, and 30+ additional handler files

</details>

### 🔧 Changed

<details>
<summary><b>🎮 UE 5.7 Compatibility</b></summary>

| Component | Change |
|-----------|--------|
| **WebSocket Protocol** | `GetProtocolType()` (FName) replaces deprecated `GetProtocolFamily()` (enum) |
| **SCS Save** | `McpSafeAssetSave` replaces `SaveLoadedAssetThrottled` to prevent recursive `FlushRenderingCommands` crashes |
| **PostProcessVolume** | Conditionally compiled (removed in UE 5.7) |
| **Niagara Graph** | Initialize `GraphSource`/`NiagaraGraph` to prevent null graph crashes |
| **Landscape Edit** | `FLandscapeEditDataInterface` for UE 5.5+, deprecation suppression for 5.0-5.4 |
| **WorldPartition** | Support `RuntimeHashSet` in addition to `RuntimeSpatialHash` for UE 5.7+ |

</details>

<details>
<summary><b>📈 Performance Improvements</b></summary>

| Component | Change |
|-----------|--------|
| **Heightmap Modification** | Pass `false` to `FLandscapeEditDataInterface` to prevent 60+ second GPU sync delays |
| **Landscape Updates** | Use `MarkPackageDirty` instead of `PostEditChange` to avoid unnecessary rebuilds |
| **Geometry Operations** | Memory pressure checks and triangle limits to prevent OOM crashes |

</details>

### 📊 Statistics

- **Files Changed:** 70 files
- **Lines Added:** ~7,200
- **Lines Removed:** ~1,400
- **Bug Fixes:** 8 critical bugs
- **New Verification Helpers:** 4

---

## 🏷️ [0.5.16] - 2026-02-12

> [!IMPORTANT]
> ### 🚀 Major Feature Release: 200+ Action Handlers
> This release adds ~200 new C++ automation sub-actions across all domains, introduces progress heartbeat protocol for long-running operations, dynamic tool management, IPv6 support, and comprehensive security hardening.

### ✨ Added

<details>
<summary><b>🎮 200+ MCP Action Handlers</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/200">#200</a>)</summary>

| Domain | New Actions |
|--------|-------------|
| **AI** | 50+ actions for EQS, Perception, State Trees, Smart Objects |
| **Combat** | Weapons, projectiles, damage, melee combat |
| **Character** | Character creation, movement, advanced locomotion |
| **Inventory** | Items, equipment, loot tables, crafting |
| **GAS** | Gameplay Ability System: abilities, effects, attributes |
| **Audio** | MetaSounds, sound classes, dialogue |
| **Materials** | Material expressions, landscape layers |
| **Textures** | Texture creation, compression, virtual texturing |
| **Levels** | 15+ new sub-actions for level management |
| **Volumes** | 18 volume types |
| **Performance** | Profiling, optimization, scalability |
| **Input** | Enhanced Input Actions & Contexts |
| **Interaction** | Interactables, destructibles, triggers |
| **Misc** | System control, tests, logs |

**New Handler Files:**
- `McpAutomationBridge_CharacterHandlers.cpp` (337 lines)
- `McpAutomationBridge_CombatHandlers.cpp` (398 lines)
- `McpAutomationBridge_SystemControlHandlers.cpp` (324 lines)
- `McpAutomationBridge_MiscHandlers.cpp` (1010 lines)
- `McpAutomationBridge_WidgetAuthoringHandlers.cpp` (2404 lines)

</details>

<details>
<summary><b>💓 Progress Heartbeat Protocol</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/201">#201</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Progress Updates** | C++ sends `progress_update` WebSocket messages during long-running operations |
| **Deadline Extensions** | TS extends request deadlines on each update with deadlock safeguards |
| **Stale Detection** | Detects same percentage for 3 consecutive updates |
| **Absolute Cap** | 5-minute maximum extension limit |
| **Max Extensions** | 10 extensions per request |

**Timeout Changes:**
- Default request timeout: 60s → 30s (extensions handle slow ops)

</details>

<details>
<summary><b>🔧 Dynamic Tool Management</b></summary>

| Feature | Description |
|---------|-------------|
| **manage_tools MCP Tool** | Enables AI to enable/disable tools at runtime |
| **Protected Tools** | `manage_tools`, `inspect`, and core category cannot be disabled |
| **list_changed Notifications** | Tool registry sends MCP notifications when tools change |
| **Category Filtering** | Filter tools by category (core, world, authoring, gameplay, utility) |

</details>

<details>
<summary><b>🌐 IPv6 Support</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/194">#194</a>)</summary>

| Feature | Description |
|---------|-------------|
| **IPv6 Addresses** | Full support for IPv6 addresses in automation bridge |
| **Hostname Resolution** | DNS resolution via `GetAddressInfo` instead of fallback to 127.0.0.1 |
| **Address Family Detection** | Auto-detect IPv6 by checking for colons in address |
| **Zone ID Handling** | Strip zone IDs from IPv6 addresses for Node.js compatibility |
| **Fallback Support** | Re-create socket as IPv4 when IPv6 not available |

</details>

### 🛡️ Security

<details>
<summary><b>🔒 Security Hardening</b></summary>

| Function | Description |
|----------|-------------|
| **SanitizeProjectRelativePath** | Rejects Windows absolute paths, normalizes slashes, collapses `//`, requires valid UE roots |
| **SanitizeAssetName** | Strips SQL injection patterns, invalid characters, enforces 64-char limit |
| **ValidateAssetCreationPath** | Combines folder + name validation |
| **IsValidAssetPath** | Rejects `:` (Windows drive letters) and consecutive slashes |

**TypeScript Security:**
- `src/utils/path-security.ts`: Collapse `//` normalization
- `src/utils/validation.ts`: SQL injection detection

</details>

<details>
<summary><b>🔒 String Escaping Fix</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/202">#202</a>)</summary>

| Issue | Fix |
|-------|-----|
| Incomplete string escaping in path handling | Added proper escaping for special characters |

</details>

### 🔧 Changed

<details>
<summary><b>🎮 UE 5.7 Compatibility Fixes</b></summary>

| Component | Change |
|-----------|--------|
| **WebSocket** | `GetProtocolType()` (FName) replaces `GetProtocolFamily()` (enum) |
| **SCS Save** | `McpSafeAssetSave` prevents recursive `FlushRenderingCommands` crashes |
| **PostProcessVolume** | Conditionally compiled (removed in 5.7) |
| **Niagara** | Initialize `GraphSource`/`NiagaraGraph` to prevent null graph crashes |

</details>

<details>
<summary><b>⚡ Performance & Infrastructure</b></summary>

| Change | Description |
|--------|-------------|
| **Memory Detection** | Windows `GlobalMemoryStatusEx` replaces heuristic detection |
| **Rate Limit** | `MaxAutomationRequestsPerMinute` raised 120 → 600 |
| **Logging** | Improved request/response logging with action name and filtered payload preview |
| **Blueprint Handler** | Variable name collision generates unique suffix, type validation before loading |

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 Various Fixes</b></summary>

| Fix | Description |
|-----|-------------|
| **~30 handlers** | Handlers that returned `nullptr` now return structured JSON |
| **Blueprint** | Unknown actions return explicit error instead of silent failure |
| **Level tools** | File existence checked before load, post-load path validation |
| **Eject handler** | Changed from stopping PIE to ejecting from possessed pawn |

</details>

### 📊 Statistics

- **Files Changed:** 83 files
- **Lines Added:** ~23,000
- **Lines Removed:** ~2,700
- **New Action Handlers:** ~200
- **New Handler Files:** 5

---

## 🏷️ [0.5.15] - 2026-02-06

> [!NOTE]
> ### 🌐 Network Configuration Release
> This release adds support for non-loopback binding in automation bridge settings, enabling LAN access configuration.

### ✨ Added

<details>
<summary><b>🌐 Non-Loopback Binding Support</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/193">#193</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Non-Loopback Binding** | Automation bridge can now bind to non-loopback addresses (e.g., `0.0.0.0`) for LAN access |
| **Allow Non-Loopback Setting** | New `bAllowNonLoopback` setting in plugin configuration |
| **TypeScript Support** | Added `MCP_AUTOMATION_ALLOW_NON_LOOPBACK` environment variable |
| **Host Validation Tests** | New test suite for bridge host validation |

**Configuration:**
```env
# Enable LAN access
MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true
MCP_AUTOMATION_HOST=0.0.0.0
```

**Security Note:** Only enable on trusted networks with appropriate firewall rules.

</details>

### 🔄 Dependencies

<details>
<summary><b>Dependabot Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.32.1 → 4.32.2 | [#189](https://github.com/ChiR24/Unreal_mcp/pull/189) |
| Dependencies group | 2 updates | [#190](https://github.com/ChiR24/Unreal_mcp/pull/190) |

</details>

### 📊 Statistics

- **Files Changed:** 8 files
- **Lines Added:** ~270
- **Lines Removed:** ~10

---

## 🏷️ [0.5.14] - 2026-02-05

> [!IMPORTANT]
> ### 🔐 TLS & Network Security Release
> This release introduces TLS/SSL support for secure WebSocket connections (`wss://`), per-connection rate limiting, loopback-only network binding enforcement, and authentication state tracking for the Automation Bridge.

### 🛡️ Security

<details>
<summary><b>🔒 Loopback-Only Binding & Handshake Enforcement</b> (<code>70c2745</code>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Loopback Binding** | Automation Bridge now only binds to loopback addresses (127.0.0.1 or ::1) |
| **Handshake Required** | Automation requests require completed `bridge_hello` handshake |

**C++ Plugin:**
- Rejects `0.0.0.0` and `::` bind attempts, falls back to `127.0.0.1` with warning
- Added `AuthenticatedSockets` tracking set in `McpConnectionManager`
- Unauthenticated sockets receive `HANDSHAKE_REQUIRED` error and connection close (code 4004)

**TypeScript Bridge:**
- Added `normalizeLoopbackHost()` to validate and enforce loopback addresses
- Non-loopback host values rejected with warning and fallback

</details>

### ✨ Added

<details>
<summary><b>🔐 TLS/SSL, Rate Limiting & Schema Validation</b> (<code>d2a94cf</code>)</summary>

| Feature | Description |
|---------|-------------|
| **TLS/SSL Support** | Full `wss://` WebSocket support with OpenSSL/TLS integration (TLS 1.2+) |
| **Rate Limiting** | Per-connection limits: configurable, defaults to disabled (0) for development |
| **Schema Validation** | New Zod schemas in `src/automation/message-schema.ts` for type-safe message parsing |

**New Plugin Settings:**
- `bEnableTls`, `TlsCertificatePath`, `TlsPrivateKeyPath` - TLS configuration
- `MaxMessagesPerMinute`, `MaxAutomationRequestsPerMinute` - Rate limit configuration

**C++ Implementation:**
- `InitializeTlsContext()`, `EstablishTls()`, `SendRaw()`, `RecvRaw()` - TLS-aware I/O
- Requires UE 5.7+ for native socket release; graceful fallback on older versions

**TypeScript Integration:**
- Added `rateLimitState` tracking with cleanup on connection close

</details>

### 🛠️ Fixed

<details>
<summary><b>🔧 TLS Memory Management</b> (<code>321206e</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Struct Initialization** | Fixed `FParsedWebSocketUrl` member initialization order (Port using uninitialized `bUseTls`) |
| **SSL Context Ownership** | Added `bOwnsSslContext` to prevent double-free of client contexts owned by `ISslManager` |

</details>

<details>
<summary><b>🔧 Thread Safety & TLS Error Handling</b> (<code>6fd1553</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Mutex Protection** | Added `SocketRateLimits` cleanup in `ForceReconnect` with proper mutex locking |
| **Declaration** | Moved `ShutdownTls()` declaration outside `WITH_SSL` guard for compilation compatibility |

</details>

<details>
<summary><b>🔧 Review Feedback Fixes</b> (<code>8987a3e</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Duplicate Call** | Fixed duplicate `ActiveSockets.Empty()` call in connection manager |
| **TypeScript Cleanup** | Added `rateLimitState` cleanup in `closeAll()` method |

</details>

### 🔄 Dependencies

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | 1.25.3 → 1.26.0 | [#187](https://github.com/ChiR24/Unreal_mcp/pull/187) |
| `mcp-client-capabilities` | Latest | [#186](https://github.com/ChiR24/Unreal_mcp/pull/186) |

</details>

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.32.0 → 4.32.1 | [#185](https://github.com/ChiR24/Unreal_mcp/pull/185) |
| `actions/github-script` | 7.0.1 → 8.0.0 | [#184](https://github.com/ChiR24/Unreal_mcp/pull/184) |

</details>

---

## 🏷️ [0.5.13] - 2026-02-02

> [!IMPORTANT]
> ### 🛡️ Security & Compatibility Release
> This release includes multiple critical security fixes for command injection and path traversal vulnerabilities, along with full Unreal Engine 5.0 backward compatibility and WebSocket stability improvements.

### 🛡️ Security

<details>
<summary><b>🔒 Command Injection in UITools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/144">#144</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection via unsanitized user input in widget creation |
| **Fix** | Added `sanitizeConsoleString()` and applied `sanitizeAssetName()` to all user-provided identifiers |
| **Contributors** | @google-labs-jules[bot] |

</details>

<details>
<summary><b>🔒 Command Injection in LevelTools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/179">#179</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection via level names, event types, and game mode parameters |
| **Fix** | Added `sanitizeCommandArgument()` and applied to all console command parameters |
| **Contributors** | @google-labs-jules[bot] |

</details>

<details>
<summary><b>🔒 Path Traversal in Asset Listing</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/163">#163</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in `listAssets` via `filter.pathStartsWith` parameter |
| **Fix** | Applied `normalizeAndSanitizePath()` to GraphQL `listAssets` and asset handler `list` action |
| **Contributors** | @google-labs-jules[bot] |

</details>

### ✨ Added

<details>
<summary><b>🎮 Unreal Engine 5.0 Compatibility</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/183">#183</a>)</summary>

| Component | Description |
|-----------|-------------|
| **API Abstractions** | Version-guarded macros for Material, Niagara, AssetRegistry, Animation, and World Partition APIs |
| **Build System** | Made plugin dependencies optional with dynamic memory-based configuration |
| **Coverage** | 41 handler files updated with UE 5.0-5.7 compatibility |

**Compatibility Macros Added:**
- `MCP_GET_MATERIAL_EXPRESSIONS()` - Abstracts material expression access
- `MCP_DATALAYER_TYPE` / `MCP_DATALAYER_ASSET_TYPE` - Data layer type abstraction
- `MCP_ASSET_FILTER_CLASS_PATHS` - Asset registry filter abstraction
- `MCP_ASSET_DATA_GET_CLASS_PATH()` - FAssetData abstraction
- `MCP_NIAGARA_EMITTER_DATA_TYPE` - Niagara emitter abstraction

</details>

### 🛠️ Fixed

<details>
<summary><b>🔌 WebSocket Stability</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/180">#180</a>, <a href="https://github.com/ChiR24/Unreal_mcp/pull/181">#181</a>)</summary>

| Fix | Description |
|-----|-------------|
| **TOCTOU Race** | Fixed Time-of-Check-Time-of-Use race condition in ListenSocket shutdown |
| **Shutdown Hang** | Fixed WebSocket server blocking cook/package builds |
| **Version Compatibility** | Fixed `PendingReceived.RemoveAt()` API differences for UE 5.4+ |

**Contributors:** @kalihman

</details>

<details>
<summary><b>🔧 Resource Handlers</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/165">#165</a>)</summary>

- Fixed broken actors and level resource handlers
- Added missing actors and level resources to MCP resource list

**Contributors:** @kalihman

</details>

<details>
<summary><b>🔧 Other Fixes</b></summary>

| Fix | Description |
|-----|-------------|
| UE 5.7 | Resolved macro handling and ControlRig dynamic loading issues |
| UE 5.5 | Fixed API compatibility issues in handlers |
| UE 5.1 | Fixed `MaterialDomain.h` inclusion path |
| JSON | Refactored JSON handling in McpAutomationBridge |

</details>

### 🧪 Testing

- Added security regression tests for UITools, LevelTools, and asset handlers

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `release-drafter/release-drafter` | 6.1.1 → 6.2.0 | [#160](https://github.com/ChiR24/Unreal_mcp/pull/160) |
| `actions/checkout` | 6.0.1 → 6.0.2 | [#161](https://github.com/ChiR24/Unreal_mcp/pull/161) |
| `github/codeql-action` | 4.31.10 → 4.32.0 | [#168](https://github.com/ChiR24/Unreal_mcp/pull/168), [#170](https://github.com/ChiR24/Unreal_mcp/pull/170) |
| `google-github-actions/run-gemini-cli` | Latest | [#177](https://github.com/ChiR24/Unreal_mcp/pull/177) |

</details>

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | Latest | [#154](https://github.com/ChiR24/Unreal_mcp/pull/154) |
| `hono` | 4.11.4 → 4.11.7 | [#173](https://github.com/ChiR24/Unreal_mcp/pull/173) |
| `@types/node` | Various updates | [#158](https://github.com/ChiR24/Unreal_mcp/pull/158), [#162](https://github.com/ChiR24/Unreal_mcp/pull/162), [#175](https://github.com/ChiR24/Unreal_mcp/pull/175) |

</details>

---

## 🏷️ [0.5.12] - 2026-01-15

> [!NOTE]
> ### 🔧 Handler Synchronization Release
> This release focuses on synchronizing TypeScript handler parameters with C++ handlers and dependency updates.

### 🛠️ Fixed

<details>
<summary><b>🔧 TS Handler Parameter Sync</b> (<code>5953232</code>)</summary>

- Synchronized TypeScript handler parameters with C++ handlers for consistency
- Fixed parameter mapping issues between TS and C++ layers

</details>

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `release-drafter/release-drafter` | 6.1.0 → 6.1.1 | [#141](https://github.com/ChiR24/Unreal_mcp/pull/141) |
| `google-github-actions/run-gemini-cli` | Latest | [#142](https://github.com/ChiR24/Unreal_mcp/pull/142) |

</details>

<details>
<summary><b>NPM Package Updates</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/143">#143</a>)</summary>

| Package | Update |
|---------|--------|
| `@types/node` | Various dev dependency updates |

</details>

---

## 🏷️ [0.5.11] - 2026-01-12

> [!IMPORTANT]
> ### 🛡️ Security Hardening & UE 5.7 Compatibility
> This release includes multiple critical security fixes for path traversal and command injection vulnerabilities, along with UE 5.7 Interchange compatibility fixes.

### 🛡️ Security

<details>
<summary><b>🔒 Path Traversal in Asset Import</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/125">#125</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Path traversal in asset import functionality |
| **Fix** | Added path sanitization and validation |

</details>

<details>
<summary><b>🔒 Command Injection Bypass</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/122">#122</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Command injection bypass via flexible whitespace |
| **Fix** | Enhanced command validation to detect and block bypass attempts |

</details>

<details>
<summary><b>🔒 Path Traversal in Screenshots</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/120">#120</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in screenshot filenames |
| **Fix** | Implemented filename sanitization and path validation |

</details>

<details>
<summary><b>🔒 Path Traversal in GraphQL</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/135">#135</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in GraphQL resolvers |
| **Fix** | Added input sanitization for GraphQL resolver paths |

</details>

<details>
<summary><b>🔒 GraphQL CORS Configuration</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/118">#118</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 MEDIUM |
| **Vulnerability** | Insecure GraphQL CORS configuration |
| **Fix** | Implemented secure CORS policy |

</details>

<details>
<summary><b>🔒 Enhanced Command Validation</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/113">#113</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection bypasses |
| **Fix** | Enhanced validation patterns to prevent injection bypasses |

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 UE 5.7 Asset Import Crash</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/138">#138</a>)</summary>

| Fix | Description |
|-----|-------------|
| **Interchange Compatibility** | Deferred asset import to next tick for UE 5.7 Interchange compatibility |
| **Name Sanitization** | Improved asset import robustness and name sanitization |

**Closes [#137](https://github.com/ChiR24/Unreal_mcp/issues/137)**

</details>

### 🔄 Dependencies

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | 1.25.1 → 1.25.2 | [#119](https://github.com/ChiR24/Unreal_mcp/pull/119) |
| `hono` | 4.11.1 → 4.11.4 | [#129](https://github.com/ChiR24/Unreal_mcp/pull/129) |
| `@types/node` | Various updates | [#130](https://github.com/ChiR24/Unreal_mcp/pull/130), [#133](https://github.com/ChiR24/Unreal_mcp/pull/133), [#134](https://github.com/ChiR24/Unreal_mcp/pull/134) |

</details>

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.31.9 → 4.31.10 | [#126](https://github.com/ChiR24/Unreal_mcp/pull/126) |
| `actions/setup-node` | 6.1.0 → 6.2.0 | [#133](https://github.com/ChiR24/Unreal_mcp/pull/133) |
| `dependabot/fetch-metadata` | 2.4.0 → 2.5.0 | [#114](https://github.com/ChiR24/Unreal_mcp/pull/114) |

</details>

---

## 🏷️ [0.5.10] - 2026-01-04

> [!IMPORTANT]
> ### 🚀 Context Reduction Initiative & Spline System
> This release implements the **Context Reduction Initiative** (Phases 48-53), reducing AI context overhead from ~78,000 to ~25,000 tokens, and adds a complete **Spline System** (Phase 26) with 21 new actions. ([#107](https://github.com/ChiR24/Unreal_mcp/pull/107), [#105](https://github.com/ChiR24/Unreal_mcp/pull/105))

### ✨ Added

<details>
<summary><b>🛤️ Spline System (Phase 26)</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/105">#105</a>)</summary>

New `manage_splines` tool with 21 actions for spline-based content creation:

| Category | Actions |
|----------|---------|
| **Creation** | `create_spline_actor`, `add_spline_point`, `remove_spline_point`, `set_spline_point` |
| **Properties** | `set_closed_loop`, `set_spline_type`, `set_tangent`, `get_spline_info` |
| **Mesh Components** | `create_spline_mesh`, `set_mesh_asset`, `set_spline_mesh_axis`, `set_spline_mesh_material` |
| **Scattering** | `create_mesh_along_spline`, `set_scatter_spacing`, `randomize_scatter` |
| **Quick Templates** | `create_road_spline`, `create_river_spline`, `create_fence_spline`, `create_wall_spline`, `create_cable_spline`, `create_pipe_spline` |
| **Utility** | `get_splines_info` |

**C++ Implementation:**
- `McpAutomationBridge_SplineHandlers.cpp` (1,512 lines)
- Full UE5 Spline API integration with `USplineComponent` and `USplineMeshComponent`

</details>

<details>
<summary><b>🔧 Pipeline Management Tool</b></summary>

New `manage_pipeline` tool for dynamic tool category management:

| Action | Description |
|--------|-------------|
| `set_categories` | Enable specific tool categories (core, world, authoring, gameplay, utility, all) |
| `list_categories` | Show available categories and their tools |
| `get_status` | View current state and tool counts |

**MCP Capability:**
- Server advertises `capabilities.tools.listChanged: true`
- Client capability detection via `mcp-client-capabilities` package
- Backward compatible: clients without `listChanged` support get ALL tools

</details>

### 🔧 Changed

<details>
<summary><b>📉 Context Reduction Initiative (Phases 48-53)</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/107">#107</a>)</summary>

| Phase | Description | Token Reduction |
|-------|-------------|-----------------|
| **Phase 48** | Schema Pruning - Condensed all 35+ tool descriptions to 1-2 sentences | ~23,000 |
| **Phase 49** | Common Schema Extraction - Shared schemas for paths, names, locations | ~8,000 |
| **Phase 50** | Dynamic Tool Loading - Category-based filtering | ~50,000 (when using filtering) |
| **Phase 53** | Strategic Tool Merging - Consolidated 4 tools | ~10,000 |

**Total Potential Reduction: ~91,000 tokens**

**Common Schemas Added:**
- `assetPath`, `actorName`, `location`, `rotation`, `scale`, `save`, `overwrite`
- `standardResponse` for consistent output formatting
- Helper functions: `createOutputSchema()`, `actionDescription()`

</details>

<details>
<summary><b>🔀 Tool Consolidation (Phase 53)</b></summary>

| Deprecated Tool | Merged Into | Actions Moved |
|-----------------|-------------|---------------|
| `manage_blueprint_graph` | `manage_blueprint` | 11 graph actions |
| `manage_audio_authoring` | `manage_audio` | 30 authoring actions |
| `manage_niagara_authoring` | `manage_effect` | 36 authoring actions |
| `manage_animation_authoring` | `animation_physics` | 45 authoring actions |

**Benefits:**
- Reduced tool count: 38 → 35
- Simplified tool discovery for AI assistants
- Backward compatible: deprecated tools still work with once-per-session warnings
- Action routing uses parameter sniffing to resolve conflicts

</details>

### ⚠️ Deprecated

- `manage_blueprint_graph` - Use `manage_blueprint` with graph actions instead
- `manage_audio_authoring` - Use `manage_audio` with authoring actions instead
- `manage_niagara_authoring` - Use `manage_effect` with authoring actions instead
- `manage_animation_authoring` - Use `animation_physics` with authoring actions instead

### 📊 Statistics

- **Files Changed:** 20
- **Lines Added:** 4,541
- **Lines Removed:** 3,555
- **Net Change:** +986 lines
- **New C++ Handler:** 1,512 lines (`McpAutomationBridge_SplineHandlers.cpp`)
- **New TS Handler:** 169 lines (`spline-handlers.ts`)
- **Common Schemas Added:** 50+ reusable schema definitions

### 🔗 Related Issues

Closes [#104](https://github.com/ChiR24/Unreal_mcp/issues/104), [#106](https://github.com/ChiR24/Unreal_mcp/issues/106), [#108](https://github.com/ChiR24/Unreal_mcp/issues/108), [#109](https://github.com/ChiR24/Unreal_mcp/issues/109), [#111](https://github.com/ChiR24/Unreal_mcp/issues/111)

---

## 🏷️ [0.5.9] - 2026-01-03

> [!IMPORTANT]
> ### 🎮 Major Feature Release
> This release introduces **15+ new automation tools** with comprehensive handlers for Navigation, Volumes, Level Structure, Sessions, Game Framework, and complete game development systems. ([#53](https://github.com/ChiR24/Unreal_mcp/pull/53))

### 🛡️ Security

<details>
<summary><b>🔒 Fix Arbitrary File Read in LogTools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/103">#103</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Arbitrary file read via `logPath` parameter |
| **Impact** | Attackers could read any file on the system by manipulating the `logPath` override |
| **Fix** | Validated that `logPath` ends with `.log` and is within `Saved/Logs` directory |

**Protections Added:**
- Enforced `.log` extension requirement
- Restricted to `Saved/Logs` directory (CWD or UE_PROJECT_PATH)
- Added path traversal and sibling directory attack protection

</details>

### ✨ Added

<details>
<summary><b>🛠️ New Automation Tools</b></summary>

| Tool | Description |
|------|-------------|
| `manage_navigation` | NavMesh configuration, Nav Modifiers, Nav Links, pathfinding control |
| `manage_volumes` | 18 volume types (Trigger, Blocking, Audio, Physics, Navigation, Streaming) |
| `manage_level_structure` | World Partition, HLOD, Data Layers, Level Blueprints |
| `manage_sessions` | Split-screen, LAN play, Voice Chat configuration |
| `manage_game_framework` | GameMode, GameState, PlayerController, match flow |
| `manage_skeleton` | Bone manipulation, sockets, physics assets |
| `manage_material_authoring` | Material expressions, landscape materials |
| `manage_texture` | Texture creation, compression, virtual texturing |
| `manage_animation_authoring` | AnimBP, Control Rig, IK Rig, Retargeter |
| `manage_niagara_authoring` | Niagara systems, modules, parameters |
| `manage_gas` | Gameplay Ability System (Abilities, Effects, Attributes) |
| `manage_character` | Character creation, movement, locomotion |
| `manage_combat` | Weapons, projectiles, damage, melee combat |
| `manage_ai` | EQS, Perception, State Trees, Smart Objects |
| `manage_inventory` | Items, equipment, loot tables, crafting |
| `manage_interaction` | Interactables, destructibles, triggers |
| `manage_widget_authoring` | UMG widgets, layout, styling |
| `manage_networking` | Replication, RPCs, network prediction |
| `manage_audio_authoring` | MetaSounds, sound classes, dialogue |

</details>

### 🔧 Changed

<details>
<summary><b>Build & Infrastructure Improvements</b></summary>

| Change | Description |
|--------|-------------|
| Bounded Directory Search | Replaced unbounded recursive search with bounded depth search (3-4 levels) |
| Property Management | Enhanced property management across all automation handlers |
| Connection Manager | Added `IsReconnectPending()` method to McpConnectionManager |
| State Machine | Improved state machine creation with enhanced error handling |

</details>

### 📊 Statistics

- **New Tools:** 15+
- **New C++ Handler Files:** 20+

---

## 🏷️ [0.5.8] - 2026-01-02

> [!IMPORTANT]
> ### 🛡️ Security Release
> Critical security fix for path traversal vulnerability and material graph parameter improvements.

### 🛡️ Security

<details>
<summary><b>🔒 Fix Path Traversal in INI Reader</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/48">#48</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Path traversal in `getProjectSetting()` |
| **Impact** | Attackers could access arbitrary files by injecting `../` sequences into the category parameter |
| **Fix** | Added strict regex validation `^[a-zA-Z0-9_-]+$` to `cleanCategory` in `src/utils/ini-reader.ts` |

</details>

### 🛠️ Fixed

<details>
<summary><b>Material Graph Parameter Mapping</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/50">#50</a>)</summary>

| Schema Parameter | C++ Handler Expected | Status |
|------------------|---------------------|--------|
| `fromNodeId` | `sourceNodeId` | ✅ Auto-mapped |
| `toNodeId` | `targetNodeId` | ✅ Auto-mapped |
| `toPin` | `inputName` | ✅ Auto-mapped |

Closes [#49](https://github.com/ChiR24/Unreal_mcp/issues/49)

</details>

---

## 🏷️ [0.5.7] - 2026-01-01

> [!IMPORTANT]
> ### 🛡️ Security Release
> Critical security fix for Python execution bypass vulnerability.

### 🛡️ Security

<details>
<summary><b>🔒 Fix Python Execution Bypass</b> (<code>e16dab0</code>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Python execution restriction bypass |
| **Impact** | Attackers could execute arbitrary Python code by using tabs instead of spaces after the `py` command |
| **Fix** | Updated `CommandValidator` to use regex `^py(?:\s|$)` which correctly matches `py` followed by any whitespace |

</details>

### 🔧 Changed

<details>
<summary><b>Release Process Improvements</b></summary>

- Removed automatic git tag creation from release workflow
- Updated release summary instructions for manual tag management

</details>

### 🔄 Dependencies

<details>
<summary><b>Package Updates</b></summary>

| Package | Update | Type |
|---------|--------|------|
| `zod` | 4.2.1 → 4.3.4 | Minor |
| `qs` | 6.14.0 → 6.14.1 | Patch (indirect) |
| `github/codeql-action` | 3.28.1 → 4.31.9 | Major |

</details>

---

## 🏷️ [0.5.6] - 2025-12-30

> [!IMPORTANT]
> ### 🛡️ Type Safety Milestone
> This release achieves **near-zero `any` type usage** across the entire codebase. All tool interfaces, handlers, automation bridge, GraphQL resolvers, and WASM integration now use strict TypeScript types with `unknown` and proper type guards.

### ✨ Added

<details>
<summary><b>📐 New Zod Schema Infrastructure</b></summary>

| File | Description |
|------|-------------|
| `src/schemas/primitives.ts` | 261 lines of Zod schemas for Vector3, Rotator, Transform, Color, etc. |
| `src/schemas/responses.ts` | 380 lines of response validation schemas |
| `src/schemas/parser.ts` | 167 lines of safe parsing utilities with type guards |
| `src/schemas/index.ts` | 173 lines of unified schema exports |

**Total:** 981 lines of new type-safe schema infrastructure

</details>

<details>
<summary><b>🔧 Type-Safe Argument Helpers</b> (<code>d5e6d1e</code>)</summary>

New extraction functions in `argument-helper.ts`:

| Function | Description |
|----------|-------------|
| `extractString(params, key)` | Extract required string with assertion |
| `extractOptionalString(params, key)` | Extract optional string |
| `extractNumber(params, key)` | Extract required number with assertion |
| `extractOptionalNumber(params, key)` | Extract optional number |
| `extractBoolean(params, key)` | Extract required boolean with assertion |
| `extractOptionalBoolean(params, key)` | Extract optional boolean |
| `extractArray<T>(params, key, validator?)` | Extract typed array with optional validation |
| `extractOptionalArray<T>(params, key, validator?)` | Extract optional array |
| `normalizeArgsTyped(args, configs)` | Returns `NormalizedArgs` interface with accessor methods |

**NormalizedArgs Interface:**
- `getString(key)`, `getOptionalString(key)`
- `getNumber(key)`, `getOptionalNumber(key)`
- `getBoolean(key)`, `getOptionalBoolean(key)`
- `get(key)` for raw `unknown` access
- `raw()` for full object access

</details>

<details>
<summary><b>🔌 WASM Module Interface</b> (<code>d5e6d1e</code>)</summary>

Defined structured `WASMModule` interface replacing `any`:

```typescript
interface WASMModule {
  PropertyParser?: new () => { parse_properties(json, maxDepth) };
  TransformCalculator?: new () => { composeTransform, decomposeMatrix };
  Vector?: new (x, y, z) => { x, y, z, add(other) };
  DependencyResolver?: new () => { analyzeDependencies, calculateDepth, ... };
}
```

</details>

<details>
<summary><b>📝 Automation Bridge Types</b> (<code>f97b008</code>)</summary>

| Type | Location | Description |
|------|----------|-------------|
| `QueuedRequestItem` | `automation/types.ts` | Typed interface for queued request items |
| `ASTFieldNode` | `graphql/resolvers.ts` | GraphQL AST node types for parseLiteral |
| `ASTNode` | `graphql/resolvers.ts` | Typed AST parsing |

</details>

### 🔧 Changed

<details>
<summary><b>🎯 Tool Interfaces Refactored</b> (<code>d5e6d1e</code>)</summary>

**ITools Interface - Replaced all `any` with concrete types:**

| Property | Before | After |
|----------|--------|-------|
| `materialTools` | `any` | `MaterialTools` |
| `niagaraTools` | `any` | `NiagaraTools` |
| `animationTools` | `any` | `AnimationTools` |
| `physicsTools` | `any` | `PhysicsTools` |
| `lightingTools` | `any` | `LightingTools` |
| `debugTools` | `any` | `DebugVisualizationTools` |
| `performanceTools` | `any` | `PerformanceTools` |
| `audioTools` | `any` | `AudioTools` |
| `uiTools` | `any` | `UITools` |
| `introspectionTools` | `any` | `IntrospectionTools` |
| `engineTools` | `any` | `EngineTools` |
| `behaviorTreeTools` | `any` | `BehaviorTreeTools` |
| `logTools` | `any` | `LogTools` |
| `inputTools` | `any` | `InputTools` |
| Index signature | `[key: string]: any` | `[key: string]: unknown` |

**StandardActionResponse:**
- Changed `StandardActionResponse<T = any>` → `StandardActionResponse<T = unknown>`

**IBlueprintTools:**
- `operations: any[]` → `operations: Array<Record<string, unknown>>`
- `defaultValue?: any` → `defaultValue?: unknown`
- `propertyValue: any` → `propertyValue: unknown`

**IAssetResources:**
- `list(): Promise<any>` → `list(): Promise<Record<string, unknown>>`

</details>

<details>
<summary><b>🔷 GraphQL Resolvers Type Safety</b> (<code>f97b008</code>, <code>fa4dddc</code>)</summary>

All scalar resolvers now use typed parameters:

| Scalar | Before | After |
|--------|--------|-------|
| `Vector.serialize` | `(value: any)` | `(value: unknown)` |
| `Rotator.serialize` | `(value: any)` | `(value: unknown)` |
| `Transform.parseLiteral` | `(ast: any)` | `(ast: ASTNode)` |
| `JSON.parseLiteral` | `(ast: any)` | `(ast: ASTNode): unknown` |

**Internal interfaces typed:**
- `Asset.metadata?: Record<string, any>` → `Record<string, unknown>`
- `Actor.properties?: Record<string, any>` → `Record<string, unknown>`
- `Blueprint.defaultValue?: any` → `unknown`

</details>

<details>
<summary><b>🌐 Automation Bridge Type Safety</b> (<code>f97b008</code>)</summary>

| Location | Before | After |
|----------|--------|-------|
| `onError` callback | `(err: any)` | `(err: unknown)` |
| `onHandshakeFail` callback | `(err: any)` | `(err: Record<string, unknown>)` |
| `catch` block | `catch (err: any)` | `catch (err: unknown)` with type guard |
| `onMessage` handler | `(data: any)` | `(data: Buffer \| string)` |
| `queuedRequestItems` | inline type with `any` | `QueuedRequestItem[]` |

</details>

<details>
<summary><b>🔌 WASM Integration Type Safety</b> (<code>d5e6d1e</code>)</summary>

| Method | Before | After |
|--------|--------|-------|
| `parseProperties()` | `Promise<any>` | `Promise<unknown>` |
| `analyzeDependencies()` | `Promise<any>` | `Promise<unknown>` |
| `fallbackParseProperties()` | `any` | `unknown` |
| `fallbackAnalyzeDependencies()` | `any` | `Record<string, unknown>` |
| `globalThis.fetch` patch | `(globalThis as any).fetch` | Typed with `GlobalThisWithFetch` |
| Error handling | `(error as any)?.code` | `(error as Record<string, unknown>)?.code` |

</details>

<details>
<summary><b>📊 Handler Types Expanded</b> (<code>d5e6d1e</code>)</summary>

`src/types/handler-types.ts` expanded with 147+ lines of new typed interfaces for all handler argument types.

</details>

### 🛠️ Fixed

<details>
<summary><b>✅ extractOptionalArray Behavior</b> (<code>f97b008</code>)</summary>

- Now returns `undefined` (instead of throwing) when value is not an array
- Documented behavior: graceful fallback for type mismatches
- Allows handlers to use default behavior when optional arrays are invalid

</details>

### 📊 Statistics

- **Files Changed:** 70 source files
- **Lines Added:** 3,806
- **Lines Removed:** 1,816
- **Net Change:** +1,990 lines (mostly type definitions)
- **New Schema Files:** 4 (981 lines total)
- **`any` → `unknown` Replacements:** 100+ occurrences

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `actions/first-interaction` | 1.3.0 → 3.1.0 | [#38](https://github.com/ChiR24/Unreal_mcp/pull/38) |
| `actions/labeler` | 5.0.0 → 6.0.1 | Dependabot |
| `github/codeql-action` | SHA update | Dependabot |
| `release-drafter/release-drafter` | SHA update | Dependabot |
| Dev dependencies group | 2 updates | Dependabot |

</details>

---

## 🏷️ [0.5.5] - 2025-12-29

> [!NOTE]
> ### 📝 Quality & Validation Release
> This release focuses on **input validation**, **structured logging**, and **developer experience** improvements. WebSocket connections now enforce message size limits, Blueprint graph editing supports user-friendly node names, and all tools use structured logging.

### ✨ Added

<details>
<summary><b>🔌 WebSocket Message Size Limits</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/36">#36</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Max Message Size** | 5MB limit for WebSocket frames and accumulated messages |
| **Close Code 1009** | Connections close with standard "Message Too Big" code when exceeded |
| **Fragment Accumulation** | Size checks applied during fragmented message assembly |

**C++ Changes:**
- Added `MaxWebSocketMessageBytes` (5MB) and `MaxWebSocketFramePayloadBytes` constants
- Implemented size validation at frame receive, fragment accumulation, and initial payload
- Proper teardown with `WebSocketCloseCodeMessageTooBig` (1009)

</details>

<details>
<summary><b>🔷 Blueprint Node Type Aliases</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

User-friendly node names now map to internal K2Node classes:

| Alias | K2Node Class |
|-------|-------------|
| `Branch` | `K2Node_IfThenElse` |
| `Sequence` | `K2Node_ExecutionSequence` |
| `ForLoop` | `K2Node_ForLoop` |
| `ForLoopWithBreak` | `K2Node_ForLoopWithBreak` |
| `WhileLoop` | `K2Node_WhileLoop` |
| `Switch` | `K2Node_SwitchInteger` |
| `Select` | `K2Node_Select` |
| `DoOnce`, `DoN`, `FlipFlop`, `Gate`, `MultiGate` | Flow control nodes |
| `SpawnActorFromClass`, `GetAllActorsOfClass` | Actor manipulation |
| `Timeline`, `MakeArray`, `MakeStruct`, `BreakStruct` | Data/utility nodes |

**C++ & TypeScript Sync:**
- `BLUEPRINT_NODE_ALIASES` map in `graph-handlers.ts`
- `NodeTypeAliases` map in `McpAutomationBridge_BlueprintGraphHandlers.cpp`

</details>

<details>
<summary><b>🌳 Behavior Tree Generic Node Types</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

| Node Type | Default Class | Category |
|-----------|---------------|----------|
| `Task` | `BTTask_Wait` | task |
| `Decorator` / `Blackboard` | `BTDecorator_Blackboard` | decorator |
| `Service` / `DefaultFocus` | `BTService_DefaultFocus` | service |
| `Composite` | `BTComposite_Sequence` | composite |

Aliases for common BT nodes: `Wait`, `MoveTo`, `PlaySound`, `Cooldown`, `Loop`, `TimeLimit`, `Selector`, etc.

</details>

<details>
<summary><b>📊 show_stats Action</b></summary>

New `show_stats` action in `system_control` tool:
- Toggle engine stats display (`stat Unit`, `stat FPS`, etc.)
- Parameters: `category` (string), `enabled` (boolean)

</details>

### 🔧 Changed

<details>
<summary><b>📋 Structured Logging</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/36">#36</a>)</summary>

Replaced `console.error`/`console.warn` with structured `Logger` across all tools:

| File | Change |
|------|--------|
| `actors.ts` | WASM debug logging |
| `debug.ts` | Viewmode stability warnings |
| `dynamic-handler-registry.ts` | Handler overwrite warnings |
| `editor.ts` | Removed commented debug logs |
| `physics.ts` | Improved error handling with fallback mesh resolution |

</details>

<details>
<summary><b>🎯 Handler Response Improvements</b></summary>

| Handler | Change |
|---------|--------|
| `actor-handlers.ts` | Returns clean responses without `ResponseFactory.success()` wrapping |
| `blueprint-handlers.ts` | Includes `blueprintPath` in responses |
| `environment.ts` | Changed default snapshot path to `./tmp/unreal-mcp/` |

</details>

### 🛠️ Fixed

<details>
<summary><b>✅ Input Validation Enhancements</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

| Handler | Validation Added |
|---------|------------------|
| `editor-handlers.ts` | Viewport resolution requires positive numbers |
| `asset-handlers.ts` | Folder paths must start with `/` |
| `lighting-handlers.ts` | Valid light types: `point`, `directional`, `spot`, `rect`, `sky` |
| `lighting-handlers.ts` | Valid GI methods: `lumen`, `screenspace`, `none`, `raytraced`, `ssgi` |
| `performance-handlers.ts` | Valid profiling types with clear error messages |
| `performance-handlers.ts` | Scalability levels clamped to 0-4 range |
| `system-handlers.ts` | Quality level clamped to 0-4 range |

</details>

<details>
<summary><b>🔧 WASM Binding Patching</b> (<code>7cc602a</code>)</summary>

- Fixed TOCTOU (Time-of-Check-Time-of-Use) race condition in `patch-wasm.js`
- Uses atomic file operations with file descriptors (`openSync`, `ftruncateSync`, `writeSync`)
- Proper error handling for missing WASM files

</details>

### 🗑️ Removed

<details>
<summary><b>🧹 Code Cleanup</b></summary>

| Removed | Lines | Reason |
|---------|-------|--------|
| `src/types/responses.ts` content | 355 | Obsolete response type definitions |
| `scripts/validate-server.js` | 46 | Unused validation script |
| `scripts/verify-automation-bridge.js` | 177 | Unused functions and broken code |

</details>

### 📊 Statistics

- **Files Changed:** 28+ source files
- **Lines Removed:** 436 (cleanup)
- **Lines Added:** 283 (validation + features)
- **New Node Aliases:** 30+ Blueprint, 20+ Behavior Tree

---

## 🏷️ [0.5.4] - 2025-12-27

> [!IMPORTANT]
> ### 🛡️ Security Release
> This release focuses on **security hardening** and **defensive improvements** across the entire stack, including command injection prevention, network isolation, and resource management.

### 🛡️ Security & Command Hardening

<details>
<summary><b>UBT Validation & Safe Execution</b></summary>

| Feature | Description |
|---------|-------------|
| **UBT Argument Validation** | Added `validateUbtArgumentsString` and `tokenizeArgs` to block dangerous characters (`;`, `|`, backticks) |
| **Safe Process Spawning** | Updated child process spawning to use `shell: false`, preventing shell injection attacks |
| **Console Command Validation** | Implemented strict input validation for the Unreal Automation Bridge to block chained or multi-line commands |
| **Argument Quoting** | Improved logging and execution logic to correctly quote arguments containing spaces |

</details>

### 🌐 Network & Host Binding

<details>
<summary><b>Localhost Default & Remote Configuration</b></summary>

| Feature | Description |
|---------|-------------|
| **Localhost Default** | WebSocket, Metrics, and GraphQL servers now bind to `127.0.0.1` by default |
| **Remote Exposure Prevention** | Prevents accidental remote exposure of services |
| **GRAPHQL_ALLOW_REMOTE** | Added environment variable check for explicit remote binding configuration |
| **Security Warnings** | Warnings logged for unsafe/permissive network settings |

</details>

### 🚦 Resource Management

<details>
<summary><b>Rate Limiting & Queue Management</b></summary>

| Feature | Description |
|---------|-------------|
| **IP-Based Rate Limiting** | Implemented rate limiting on the metrics server |
| **Queue Limits** | Introduced `maxQueuedRequests` to automation bridge to prevent memory exhaustion |
| **Message Size Enforcement** | Enforced `MAX_WS_MESSAGE_SIZE_BYTES` for WebSocket connections to reject oversized payloads |

</details>

### 🧪 Testing & Cleanup

<details>
<summary><b>Test Updates & File Cleanup</b></summary>

| Change | Description |
|--------|-------------|
| **Path Sanitization Tests** | Modified validation tests to verify path sanitization and expect errors for traversal attempts |
| **Removed Legacy Tests** | Removed outdated test files (`run-unreal-tool-tests.mjs`, `test-asset-errors.mjs`) |
| **Response Logging** | Implemented better response logging in the test runner |

</details>

### 🔄 Dependencies

- **dependencies group**: Bumped 2 updates via @dependabot ([#33](https://github.com/ChiR24/Unreal_mcp/pull/33))

---

## 🏷️ [0.5.3] - 2025-12-21

> [!IMPORTANT]
> ### 🔄 Major Enhancements
> - **Dynamic Type Discovery** - New runtime introspection for lights, debug shapes, and sequencer tracks
> - **Metrics Rate Limiting** - Per-IP rate limiting (60 req/min) on Prometheus endpoint
> - **Centralized Class Configuration** - Unified Unreal Engine class aliases
> - **Enhanced Type Safety** - Comprehensive TypeScript interfaces replacing `any` types

### ✨ Added

<details>
<summary><b>🔍 Dynamic Discovery & Engine Handlers</b></summary>

| Feature | Description |
|---------|-------------|
| **list_light_types** | Discovers all available light class types at runtime |
| **list_debug_shapes** | Enumerates supported debug shape types |
| **list_track_types** | Lists all sequencer track types available in the engine |
| **Heuristic Resolution** | Improved C++ handlers use multiple naming conventions and inheritance validation |
| **Vehicle Type Support** | Expanded vehicle type from union to string for flexibility |

**C++ Changes:**
- `McpAutomationBridge_LightingHandlers.cpp` - Runtime `ResolveUClass` for lights
- `McpAutomationBridge_SequenceHandlers.cpp` - Runtime resolution for tracks
- Added `UObjectIterator.h` for dynamic type scanning
- Unified spawn/track-creation flows
- Removed editor/PIE branching logic

</details>

<details>
<summary><b>⚙️ Tooling & Configuration</b></summary>

| Feature | Description |
|---------|-------------|
| **class-aliases.ts** | Centralized Unreal Engine class name mappings |
| **handler-types.ts** | Comprehensive TypeScript interfaces (ActorArgs, EditorArgs, LightingArgs, etc.) |
| **timeout constants** | Command-specific operation timeouts in constants.ts |
| **listDebugShapes()** | Programmatic access in DebugVisualizationTools |

**Type System:**
- Geometry types: Vector3, Rotator, Transform
- Required-component lookups
- Centralized class-alias mappings

</details>

<details>
<summary><b>📈 Metrics Server Enhancements</b></summary>

| Feature | Description |
|---------|-------------|
| **Rate Limiting** | Per-IP limit of 60 requests/minute |
| **Server Lifecycle** | Returns instance for better management |
| **Error Handling** | Improved internal error handling |

</details>

<details>
<summary><b>📚 Documentation & DX</b></summary>

| Feature | Description |
|---------|-------------|
| **handler-mapping.md** | Updated with new discovery actions |
| **README.md** | Clarified WASM build instructions |
| **Tool Definitions** | Synchronized with new discovery actions |

</details>

### 🔧 Changed

<details>
<summary><b>Handler Type Safety & Logic</b></summary>

**src/tools/handlers/common-handlers.ts:**
- Replaced `any` typings with strict `HandlerArgs`/`LocationInput`/`RotationInput`
- Added automation-bridge connectivity validation
- Enhanced location/rotation normalization with type guards

**Specialized Handlers:**
- `actor-handlers.ts` - Applied typed handler-args
- `asset-handlers.ts` - Improved argument normalization
- `blueprint-handlers.ts` - Added new action cases
- `editor-handlers.ts` - Enhanced default handling
- `effect-handlers.ts` - Added `list_debug_shapes`
- `graph-handlers.ts` - Improved validation
- `level-handlers.ts` - Type-safe operations
- `lighting-handlers.ts` - Added `list_light_types`
- `pipeline-handlers.ts` - Enhanced error handling

</details>

<details>
<summary><b>Infrastructure & Utilities</b></summary>

**Security & Validation:**
- `command-validator.ts` - Blocks semicolons, pipes, backticks
- `error-handler.ts` - Enhanced error logging
- `response-validator.ts` - Improved Ajv typing
- `safe-json.ts` - Generic typing for cleanObject
- `validation.ts` - Expanded path-traversal protection

**Performance:**
- `unreal-command-queue.ts` - Optimized queue processing (250ms interval)
- `unreal-bridge.ts` - Centralized timeout constants

</details>

### 🛠️ Fixed

- **Command Injection Prevention** - Additional dangerous command patterns blocked
- **Path Security** - Enhanced asset-name validation
- **Type Safety** - Eliminated `any` types across handler functions
- **Error Messages** - Clearer error messages for class resolution failures

### 📊 Statistics

- **Files Changed:** 20+
- **New Interfaces:** 15+ handler type definitions
- **Discovery Actions:** 3 new runtime introspection methods
- **Security Enhancements:** 5+ new validation patterns

### 🔄 Dependencies

- **graphql-yoga**: Bumped from 5.17.1 to 5.18.0 (#31)

---

## 🏷️ [0.5.2] - 2025-12-18

> [!IMPORTANT]
> ### 🔄 Breaking Changes
> - **Standardized Tools & Type Safety** - All tool handlers now use consistent interfaces with improved type safety. Some internal API signatures have changed. (`079e3c2`)

### ✨ Added

<details>
<summary><b>🛠️ Blueprint Enhancements</b> (<code>e710751</code>)</summary>

| Feature | Description |
|---------|-------------|
| **Dynamic Node Creation** | Support for creating nodes dynamically in Blueprint graphs |
| **Struct Property Support** | Added ability to set and get struct properties on Blueprint components |

</details>

### 🔄 Changed

<details>
<summary><b>🎯 Standardized Tool Interfaces</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/28">#28</a>)</summary>

| Component | Change |
|-----------|--------|
| Tool Handlers | Optimized bridge communication and standardized response handling |
| Type Safety | Hardened type definitions across all tool interfaces |
| Bridge Optimization | Improved performance and reliability of automation bridge |

</details>

### 🔧 CI/CD

- 🔗 **MCP Publisher** - Fixed download URL format in workflow steps (`0d452e7`)
- 🧹 **Workflow Cleanup** - Removed unnecessary success conditions from MCP workflow steps (`82bd575`)

---

## 🏷️ [0.5.1] - 2025-12-17

> [!WARNING]
> ### ⚠️ Breaking Changes
> - **Standardized Return Types** - All tool methods now return `StandardActionResponse` type instead of generic objects. Consumers must update their code to handle the new response structure with `success`, `data`, `warnings`, and `error` fields. (`5e615c5`)
> - **Test Suite Structure** - New test files added and existing tests enhanced with comprehensive coverage.

### 🔄 Changed

<details>
<summary><b>🎯 Standardized Tool Interfaces</b> (<code>5e615c5</code>)</summary>

| Component | Change |
|-----------|--------|
| Tool Methods | Updated all tool methods to return `StandardActionResponse` type for consistency |
| Tool Interfaces | Modified interfaces (assets, blueprint, editor, environment, foliage, landscape, level, sequence) to use standardized response format |
| Type System | Added proper type imports and exports for `StandardActionResponse` |
| Handler Files | Updated to work with new standardized response types |
| Response Structure | All implementations return correct structure with `success`/`error` fields |

</details>

### ✨ Added

<details>
<summary><b>🧪 Comprehensive Test Suite</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/25">#25</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Test Coverage** | Added comprehensive test files with success, error, and edge cases |
| **GraphQL DataLoader** | Implemented N+1 query optimization with batching and caching |
| **Type-Safe Interfaces** | Added type-safe automation response interfaces for better error handling |
| **Utility Tests** | Added tests for core utilities (normalize, safe-json, validation) |
| **Real-World Scenarios** | Enhanced coverage with real-world scenarios and cleanup procedures |
| **New Test Suites** | Audio, lighting, performance, input, and asset graph management |
| **Enhanced Logging** | Improved diagnostic logging throughout tools |
| **Documentation** | Updated supported Unreal Engine versions (5.0-5.7) in testing documentation |

</details>

### 🧹 Maintenance

- 🗑️ **Prompts Module Cleanup** - Removed prompts module and related GraphQL prompt functionality ([#26](https://github.com/ChiR24/Unreal_mcp/pull/26))
- 🔒 **Security Updates** - Removed unused dependencies (axios, json5, yargs) from package.json for security (`5e615c5`)
- 📐 **Tool Interfaces** - Enhanced asset and level tools with security validation and timeout handling (`5e615c5`)

### 📦 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR | Commit |
|---------|--------|-----|--------|
| `actions/checkout` | v4 → v6 | [#23](https://github.com/ChiR24/Unreal_mcp/pull/23) | `4c6b3b5` |
| `actions/setup-node` | v4 → v6 | [#22](https://github.com/ChiR24/Unreal_mcp/pull/22) | `71aa35c` |
| `softprops/action-gh-release` | 2.0.8 → 2.5.0 | [#21](https://github.com/ChiR24/Unreal_mcp/pull/21) | `b6c8a46` |

</details>

<details>
<summary><b>NPM Package Updates</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/24">#24</a>, <code>5e615c5</code>)</summary>

| Package | Update |
|---------|--------|
| `@modelcontextprotocol/sdk` | 1.25.0 → 1.25.1 |
| `@types/node` | 25.0.2 → 25.0.3 |

</details>

---

## 🏷️ [0.5.0] - 2025-12-16

> [!IMPORTANT]
> ### 🔄 Major Architecture Migration
> This release marks the **complete migration** from Unreal's built-in Remote Plugin to a native C++ **McpAutomationBridge** plugin. This provides:
> - ⚡ Better performance
> - 🔗 Tighter editor integration  
> - 🚫 No dependency on Unreal's Remote API
>
> **BREAKING CHANGE:** Response format has been standardized across all automation tools. Clients should expect responses to follow the new `StandardActionResponse` format with `success`, `data`, `warnings`, and `error` fields.

### 🏗️ Architecture

| Change | Description |
|--------|-------------|
| 🆕 **Native C++ Plugin** | Introduced `McpAutomationBridge` - a native UE5 editor plugin replacing the Remote API |
| 🔌 **Direct Editor Integration** | Commands execute directly in the editor context via automation bridge subsystem |
| 🌐 **WebSocket Communication** | Implemented `McpBridgeWebSocket` for real-time bidirectional communication |
| 🎯 **Bridge-First Architecture** | All operations route through the native C++ bridge (`fe65968`) |
| 📐 **Standardized Responses** | All tools now return `StandardActionResponse` format (`0a8999b`) |

### ✨ Added

<details>
<summary><b>🎮 Engine Compatibility</b></summary>

- **UE 5.7 Support** - Updated McpAutomationBridge with ControlRig dynamic loading and improved sequence handling (`ec5409b`)

</details>

<details>
<summary><b>🔧 New APIs & Integrations</b></summary>

- **GraphQL API** - Broadened automation bridge with GraphQL support, WASM integration, UI/editor integrations (`ffdd814`)
- **WebAssembly Integration** - High-performance JSON parsing with 5-8x performance gains (`23f63c7`)

</details>

<details>
<summary><b>🌉 Automation Bridge Features</b></summary>

| Feature | Commit |
|---------|--------|
| Server mode on port `8091` | `267aa42` |
| Client mode with enhanced connection handling | `bf0fa56` |
| Heartbeat tracking and output capturing | `28242e1` |
| Event handling and asset management | `d10e1e2` |

</details>

<details>
<summary><b>🎛️ New Tool Systems (0a8999b, 0ac82ac)</b></summary>

| Tool | Description |
|------|-------------|
| 🎮 **Input Management** | New `manage_input` tool with EnhancedInput support for Input Actions and Mapping Contexts |
| 💡 **Lighting Manager** | Full lighting configuration via `manage_lighting` including spawn, GI setup, shadow config, build lighting |
| 📊 **Performance Manager** | `manage_performance` with profiling (CPU/GPU/Memory), optimization, scalability, Nanite/Lumen config |
| 🌳 **Behavior Tree Editing** | Full behavior tree creation and node editing via `manage_behavior_tree` |
| 🎬 **Enhanced Sequencer** | Track operations (add/remove tracks, set muted/solo/locked), display rate, tick resolution |
| 🌍 **World Partition** | Cell management, data layer toggling via `manage_level` |
| 🖼️ **Widget Management** | UI widget creation, visibility controls, child widget adding |

</details>

<details>
<summary><b>📊 Graph Editing Capabilities (0a8999b)</b></summary>

- **Blueprint Graph** - Direct node manipulation with `manage_blueprint_graph` (create_node, delete_node, connect_pins, etc.)
- **Material Graph** - Node operations via `manage_asset` (add_material_node, connect_material_pins, etc.)
- **Niagara Graph** - Module and parameter editing (add_niagara_module, set_niagara_parameter, etc.)

</details>

<details>
<summary><b>🛠️ New Handlers & Actions</b></summary>

- Blueprint graph management and Niagara functionalities (`aff4d55`)
- Physics simulation setup in AnimationTools (`83a6f5d`)
- **New Asset Actions:**
  - `generate_lods`, `add_material_parameter`, `list_instances`
  - `reset_instance_parameters`, `get_material_stats`, `exists`
  - `nanite_rebuild_mesh`
- World partition and rendering tool handlers (`83a6f5d`)
- Screenshot with base64 image encoding (`bb4f6a8`)

</details>

<details>
<summary><b>🧪 Test Suites</b></summary>

**50+ new test cases** covering:
- Animation, Assets, Materials
- Sequences, World Partition
- Blueprints, Niagara, Behavior Trees
- Audio, Input Actions
- And more! (`31c6db9`, `85817c9`, `fc47839`, `02fd2af`)

</details>

### 🔄 Changed

#### Core Refactors
| Component | Change | Commit |
|-----------|--------|--------|
| `SequenceTools` | Migrated to Automation Bridge | `c2fb15a` |
| `UnrealBridge` | Refactored for bridge connection | `7bd48d8` |
| Automation Dispatch | Editor-native handlers modernization | `c9db1a4` |
| Test Runner | Timeout expectations & content extraction | `c9766b0` |
| UI Handlers | Improved readability and organization | `bb4f6a8` |
| Connection Manager | Streamlined connection handling | `0ac82ac` |

#### Tool Improvements
- 🚗 **PhysicsTools** - Vehicle config logic updated, deprecated checks removed (`6dba9f7`)
- 🎬 **AnimationTools** - Logging and response normalization (`7666c31`)
- ⚠️ **Error Handling** - Utilities refactored, INI file reader added (`f5444e4`)
- 📐 **Blueprint Actions** - Timeout handling enhancements (`65d2738`)
- 🎨 **Materials** - Enhanced material graph editing capabilities (`0a8999b`)
- 🔊 **Audio** - Improved sound component management (`0a8999b`)

#### Other Changes
- 📡 **Connection & Logging** - Improved error messages for clarity (`41350b3`)
- 📚 **Documentation** - README updated with UE 5.7, WASM docs, architecture overview, 17 tools (`8d72f28`, `4d77b7e`)
- 🔄 **Dependencies** - Updated to latest versions (`08eede5`)
- 📝 **Type Definitions** - Enhanced tool interfaces and type coverage (`0a8999b`)

### 🐛 Fixed

- `McpAutomationBridgeSubsystem` - Header removal, logging category, heartbeat methods (`498f644`)
- `McpBridgeWebSocket` - Reliable WebSocket communication (`861ad91`)
- **AutomationBridge** - Heartbeat handling and server metadata retrieval (`0da54f7`)
- **UI Handlers** - Missing payload and invalid widget path error handling (`bb4f6a8`)
- **Screenshot** - Clearer error messages and flow (`bb4f6a8`)

### 🗑️ Removed

| Removed | Reason |
|---------|--------|
| 🔌 Remote API Dependency | Replaced by native C++ plugin |
| 🐍 Python Fallbacks | Native C++ automation preferred (`fe65968`) |
| 📦 Unused HTTP Client | Cleanup from error-handler (`f5444e4`) |

---

## 🏷️ [0.4.7] - 2025-11-16

### ✨ Added
- Output Log reading via `system_control` tool with `read_log` action. filtering by category, level, line count.
- New `src/tools/logs.ts` implementing robust log tailing.
- 🆕 Initial `McpAutomationBridge` plugin with foundational implementation (`30e62f9`)
- 🧪 Comprehensive test suites for various Unreal Engine tools (`31c6db9`)

### 🔄 Changed
- `system_control` tool schema: Added `read_log` action.
- Updated tool handlers to route `read_log` to LogTools.
- Version bumped to 0.4.7.

### 📚 Documentation
- Updated README.md with initial bridge documentation (`a24dafd`)

---

## 🏷️ [0.4.6] - 2025-10-04

### 🐛 Fixed
- Fixed duplicate response output issue where tool responses were displayed twice in MCP content
- Response validator now emits concise summaries instead of duplicating full JSON payloads
- Structured content preserved for validation while user-facing output is streamlined

---

## 🏷️ [0.4.5] - 2025-10-03

### ✨ Added
- 🔧 Expose `UE_PROJECT_PATH` environment variable across runtime config, Smithery manifest, and client configs
- 📁 Added `projectPath` to runtime `configSchema` for Smithery's session UI

### 🔄 Changed
- ⚡ Made `createServer` synchronous factory (removed `async`)
- 🏠 Default for `ueHost` in exported `configSchema`

### 📚 Documentation
- Updated `README.md`, config examples to include `UE_PROJECT_PATH`
- Updated `smithery.yaml` and `server.json` manifests

### 🔨 Build
- Rebuilt Smithery bundle and TypeScript output

### 🐛 Fixed
- Smithery UI blank `ueHost` field by defining default in runtime schema

---

## 🏷️ [0.4.4] - 2025-09-28

### ✨ Improvements

- 🤝 **Client Elicitation Helper** - Added support for Cursor, VS Code, Claude Desktop, and other MCP clients
- 📊 **Consistent RESULT Parsing** - Handles JSON5 and legacy Python literals across all tools
- 🔒 **Safe Output Stringification** - Robust handling of circular references and complex objects
- 🔍 **Enhanced Logging** - Improved validation messages for easier debugging

---

## 🏷️ [0.4.0] - 2025-09-20

> **Major Release** - Consolidated Tools Mode

### ✨ Improvements

- 🎯 **Consolidated Tools Mode Exclusively** - Removed legacy mode, all tools now use unified handler system
- 🧹 **Simplified Tool Handlers** - Removed deprecated code paths and inline plugin validation
- 📝 **Enhanced Error Handling** - Better error messages and recovery mechanisms

### 🔧 Quality & Maintenance

- ⚡ Reduced resource usage by optimizing tool handlers
- 🧹 Cleanup of deprecated environment variables

---

## 🏷️ [0.3.1] - 2025-09-19

> **BREAKING:** Connection behavior is now on-demand

### 🏗️ Architecture

- 🔄 **On-Demand Connection** - Shifted to intelligent on-demand connection model
- 🚫 **No Background Processes** - Eliminated persistent background connections

### ⚡ Performance

- Reduced resource usage and eliminated background processes
- Optimized connection state management

### 🛡️ Reliability

- Improved error handling and connection state management
- Better recovery from connection failures

---

## 🏷️ [0.3.0] - 2025-09-17

> 🎉 **Initial Public Release**

### ✨ Features

- 🎮 **13 Consolidated Tools** - Full suite of Unreal Engine automation tools
- 📁 **Normalized Asset Listing** - Auto-map `/Content` and `/Game` paths
- 🏔️ **Landscape Creation** - Returns real UE/Python response data
- 📝 **Action-Oriented Descriptions** - Enhanced tool documentation with usage examples

### 🔧 Quality & Maintenance

- Server version 0.3.0 with clarified 13-tool mode
- Comprehensive documentation and examples
- Lint error fixes and code style cleanup

---

<div align="center">

### 🔗 Links

[![GitHub](https://img.shields.io/badge/GitHub-Repository-181717?style=for-the-badge&logo=github)](https://github.com/ChiR24/Unreal_mcp)
[![npm](https://img.shields.io/badge/npm-Package-CB3837?style=for-the-badge&logo=npm)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![UE5](https://img.shields.io/badge/Unreal-5.6%20|%205.7-0E1128?style=for-the-badge&logo=unrealengine)](https://www.unrealengine.com/)

</div>
