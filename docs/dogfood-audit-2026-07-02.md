# McpAutomationBridge Dogfood/UX Audit — 2026-07-02

**Method:** 16 agents acting as schema-only first-time users exercised all 22 tools live against the running editor — 471 tool calls total. Every claim below was reproduced against the live bridge; error text is quoted verbatim.

**Raw findings:** 112 (17 high / 51 medium / 44 low). After cross-tool dedup (golden-schema artifact x3, response-sanitizer redaction x2, TOOL_DISABLED flake x3): **~107 unique findings** — 20 schema_lie, 33 bug, 9 missing_functionality, 32 confusing_interface, 18 docs.

---

## 1. Headline verdict

**The engine-side machinery is real and mostly correct; the contract layer around it lies.** Trustworthy for real work *today*, per tool family:

| Trust level | Tools |
|---|---|
| Trust for real work | manage_tools, manage_asset (CRUD core), manage_blueprint (BP core), manage_effect (Niagara), manage_networking (supervised), manage_character (base setup), level-editor-suite core loop (actors/sequencer/viewport/PIE), inspect (read side) |
| Trust only with independent verification | manage_gas, manage_combat, manage_ai, animation_physics, manage_audio (MetaSounds yes, cues no), manage_geometry, manage_inventory, manage_character (parkour/footsteps), manage_level_structure |
| Do not trust the success response | manage_interaction configure_*, manage_blueprint set_anchor, inspect set_property `saved:true`, GAS tag actions |

Three systemic failure modes account for the majority of high-severity findings, and all three are the **worst possible failure mode for an agent-facing API — silent success**:

1. **Success responses that echo inputs the handler never consumed.** Dozens of actions accept a param, echo it back in the response, and persist nothing. An agent (or a schema-validating client) has no signal anything went wrong.
2. **Schema-to-handler param drift.** The published inputSchema and the handlers disagree on param names in at least 20 places (`rowStruct`, `packagePath`, `bounds`, `targetActor`/`toolActor`, `blueprintPath` vs `effectPath`, `stateMachineName`, `blendTime`, `pawnClass`, `name` vs `actorName`...). Every one guarantees a wasted call; the silent ones guarantee wrong output.
3. **Ignored destination paths with hardcoded fallback folders.** Four separate tools sprayed audit assets into `/Game/Audio/Cues`, `/Game/Animations`, `/Game/GeneratedMeshes`, and `/Game` root while reporting success — content pollution in a shared project.

The mitigating pattern that saved most workflows: **error text is usually excellent** (names the actual required params), and **readback actions plus the `inspect` tool close the loop** when the tool's own responses can't be trusted. Fix the silent-success class and the schema drift and this becomes a genuinely production-grade surface.

---

## 2. Findings

### 2.1 schema_lie — the published schema and the handlers disagree

#### SL-1 [HIGH, cross-cutting: manage_audio, animation_physics, manage_geometry, manage_ai] Destination path params ignored; assets land in hardcoded folders with success:true
- **manage_audio create_sound_cue** ignores documented `path`; only undocumented `packagePath` works; default is hardcoded `/Game/Audio/Cues` (handler `McpAutomationBridge_AudioHandlers.cpp:614-626`; a second handler at `AudioAuthoringHandlers.cpp:400` reads `path` but never runs). Repro: `{action:'create_sound_cue', name:'SC_AuditCue', path:'/Game/ZZ_McpAudit/manage_audio'}` → `assetPath:'/Game/Audio/Cues/SC_AuditCue'`.
- **animation_physics create_blend_space_1d** ignores `savePath` (which works for create_animation_sequence/create_montage) and reads only undocumented `path` (`AnimationAuthoringHandlers.cpp:1830`), defaulting to `/Game/Animations`. create_montage was already patched for exactly this (lines 1378-1386); blend spaces were missed.
- **manage_geometry convert_to_static_mesh / convert_to_nanite** ignore declared `outputPath`; real working param `assetPath` is not in the schema at all; default `/Game/GeneratedMeshes/`.
- **manage_ai create** ignores `path` entirely (only undocumented-per-action `savePath` honored) → `assetPath:"/Game/BT_Audit2"` at the content root.
- **Fix:** one sweep making every create/convert handler accept `path` AND `savePath`/`packagePath`/`assetPath` as aliases, and **error** (don't default) when the resolved destination differs from a requested one.
- **Cleanup debt from this audit (tool placed them, agents couldn't delete):** `/Game/Audio/Cues/SC_AuditCue`, `/Game/Animations/BS_AuditLocomotion`, `/Game/GeneratedMeshes/GeneratedBox`, `/Game/GeneratedMeshes/GeoAudit_Cyl_Nanite`.

#### SL-2 [HIGH, animation_physics] set_blend_in/set_blend_out succeed while applying the 0.25 default unless undocumented `blendTime` is passed
Real params are `blendTime`/`blendOption` (`AnimationAuthoringHandlers.cpp:1722-1723, 1758-1759`), neither in the schema. `time`, `value`, `length` each returned success "Blend in settings updated" with blendInTime stuck at 0.25. **Fix:** declare `blendTime`/`blendOption`; error on unrecognized value-bearing params.

#### SL-3 [MEDIUM, manage_geometry] Five declared params are dead; the real controls are undeclared
- `actorName` on create_* ignored (actor named "GeneratedBox"); undeclared `name` works.
- Boolean ops reject `actorName`: `Error [INVALID_ARGUMENT]: targetActor and toolActor required` — neither param is in the schema.
- `simplify_mesh` ignores `targetTriangleCount` — always cuts ~50% (verified twice: 220→110 with target 100; 110→54 with target 20; response even echoes `reductionPercent:50`).
- `create_spiral_stairs` ignores `numTurns` (1.5 turns → `curveAngle:90`, a quarter turn); undeclared `curveAngle` (degrees) is the real knob.
- `project_uv` ignores declared `uvScale` object (echoes `scale:1`).
- **Fix:** reconcile the geometry schema against the handler param reads; simplify_mesh must honor the target count or error.

#### SL-4 [MEDIUM, manage_gas] Documented path/tag params rejected; handlers demand undocumented names
`attributeSetPath`, `effectPath`, `tagName` all documented, all rejected: `Error [INVALID_ARGUMENT]: Missing blueprintPath.` / `Error [INVALID_ARGUMENT]: Missing tag.` (`tag` is not in the schema at all). **Fix:** accept documented names as aliases.

#### SL-5 [MEDIUM, cross-cutting] Advertised actions that always fail
- **manage_gas add_ability**: in the enum, documented in `setPath`, returns `Error [NOT_IMPLEMENTED]: add_ability is not implemented...` (at least honest, with a workaround — but see B-9, the workaround path is redacted).
- **manage_effect set_niagara_parameter**: in the enum, returns `Error [UNKNOWN_ACTION]: Unhandled manage_effect action: set_niagara_parameter` (working alternative `set_parameter_value` never mentioned).
- **manage_ai add_composite_node/add_task_node/add_decorator/add_service** plus four dedicated param enums (15 taskTypes, 16 decoratorTypes): all return `Error [USE_GRAPH_AUTHORING]` redirecting to add_node/connect_nodes/add_subnode.
- **Fix:** remove dead actions and their param enums from tools/list, or implement them.

#### SL-6 [MEDIUM, cross-cutting] Declared params silently accepted-and-dropped at create/configure time
- manage_blueprint **add_variable `defaultValue`**: documented, accepted, never applied (`defaultValue=42.5` → get_default reads 0; reproduced twice).
- manage_gas **add_attribute `baseValue`**: `baseValue=100` → `defaultValue:0` (set_attribute_base_value works).
- manage_inventory **create_item_data_asset `properties`**: documented "Properties to apply to an item data asset", silently dropped (Description reads back `""`); same param works via set_item_properties.
- animation_physics **create_montage `length`**: accepted, duration stays 0.
- **Fix:** either apply these at create or reject them.

#### SL-7 [MEDIUM, animation_physics] add_state_machine rejects schema's `machineName`/`assetPath`; wants undocumented `stateMachineName`+`blueprintPath`
`Error [MISSING_STATE_MACHINE_NAME]: stateMachineName is required`, then `Error [ANIM_BP_NOT_FOUND]: Could not load animation blueprint: ` (blank — assetPath never read, input never echoed). add_state/add_transition follow the same undocumented blueprintPath convention.

#### SL-8 [MEDIUM, level-editor-suite]
- **manage_level_structure set_volume_bounds** rejects the documented `volumeLocation`/`volumeExtent` and requires an undeclared 6-element `bounds` array (`Error [INVALID_ARGUMENT]: bounds must be an array of 6 values [minX, minY, minZ, maxX, maxY, maxZ]`). This undeclared param is the ONLY working way to reposition/resize a volume.
- **control_actor detach** rejects documented `childActor` (`Error [INVALID_ARGUMENT]: actorName required`) while attach accepts it.

#### SL-9 [MEDIUM, manage_audio] configure_spatialization ignores documented `spatialization`/`enabled`; real params `spatializationAlgorithm`/`spatialize` undocumented
`spatialization='HRTF'` → `{success:true, spatializationAlgorithm:'panner'}` — silently wrong config (`AudioAuthoringHandlers.cpp:1850-1854`).

#### SL-10 [MEDIUM, manage_asset] create_data_table requires `rowStruct`, absent from the schema
`Error [INVALID_ARGUMENT]: name, path and rowStruct are required` — good error, but a schema-validating client can never send the param (verified absent in both golden file and live tools/list).

---

### 2.2 bug — the handler does the wrong thing

#### B-1 [HIGH, cross-cutting — the flagship problem] Silent-success no-op writes: success:true + echoed values, nothing persisted
The single most repeated finding, independently hit by five agents:
- **manage_interaction configure_chest_properties / configure_interaction_widget / configure_door_properties (lock params) / configure_interaction_trace (TraceType vars):** e.g. chest response `{"locked":true,"lootTablePath":"...","configured":true}` but after compile, inspect_cdo shows `bIsLocked:false, LootTable:null`. Every lock/key/loot/prompt workflow no-ops green.
- **manage_blueprint set_anchor:** three param shapes all return `{"success":true,"message":"Anchor set"}`; get_widget_slot_info shows anchors unchanged (set_position on the same slot works and reflects immediately, so readback isn't the issue).
- **manage_gas set_effect_tags / set_ability_tags:** return `{"tagsAdded":[]}` with no error when the tag isn't in the project registry (this project has zero registered tags, so every GAS tag silently vanishes — and tags are how GAS cooldowns/gating work).
- **manage_character setup_wall_running / setup_footstep_system:** create variables but drop the passed values while echoing them (`{"wallRunDuration":2,...}`); handler only calls AddMemberVariable, never SetBPVarDefaultValue (`CharacterHandlers.cpp:1477-1513, 1592-1626`) — unlike setup_sliding/mantling/vaulting/climbing which do it right.
- **manage_level_structure set_volume_extent:** requested 250/250/150, response claims success with `"newExtent":{"x":100,...}` — the OLD value reported as new; nothing changed.
- **manage_ai set_node_properties:** `properties={TotallyBogusProperty:42}` returns the same success shape as a real set; typos undetectable.
- **Fix (one policy, applied everywhere):** a mutating handler must (a) return `appliedProperties`/`ignoredParams`, and (b) return an error when zero requested changes were applied. This matches the fork's own fail-fast rule.

#### B-2 [HIGH, inspect] set_property returns saved:true without writing the package to disk
Three consecutive set_property calls (float, Tags array, component RelativeLocation) all returned `saved:true`; a byte-level snapshot of the .uasset contained none of them (grep for FName 'mcp_audit' = 0 hits; diff_asset confirmed old values). One response was self-contradictory: `saved:true` + `unsavedChanges:true`. Values only hit disk when a later manage_blueprint save flushed the package. Edits are lost on editor close and diff_asset verification reads stale state. **Fix:** actually save (or report `saved:false, markedDirty:true`).

#### B-3 [HIGH, manage_asset] set_tags → find_by_tag round-trip broken: find_by_tag never returns results
Write side works (`appliedTags:2`, get_metadata confirms); `{action:'find_by_tag', tag:'AuditPass'}` → `{"tag":"AuditPass","count":0,"results":[]}` in every variation — including `'RowStructure'`, a genuine asset-registry tag present on every DataTable. Tagging is write-only. **Fix:** query the same store set_tags writes (or the asset registry properly).

#### B-4 [HIGH, manage_inventory] create_crafting_recipe silently drops the recipe output
Response echoes `{"outputItemPath":...,"outputQuantity":2,"craftTime":3.5}` but the saved asset contains only ingredients/requirements; `OutputItemPath` → PROPERTY_NOT_FOUND; no other action persists it. A recipe with no output is unusable. **Fix:** persist output fields into the property bag like Ingredient_N.

#### B-5 [HIGH, manage_character] configure_sprint, setup_wall_running, add_custom_movement_mode all write CMC MaxCustomMovementSpeed — last writer wins
SprintSpeed BP var created but its default never set (reads 0); sprintSpeed=700 goes into the shared field that wall-run (600) and custom-mode (600) also write. After the natural sequence, 700 exists nowhere. Source: `CharacterHandlers.cpp:2082-2098, 1503, 1148-1149`. **Fix:** set the scaffolded variable's default; stop multiplexing one CMC field.

#### B-6 [HIGH, manage_ai] 'create' silently binds the new BT to an unrelated **production** blackboard
Passed `blackboardPath='/Game/ZZ_McpAudit/manage_ai/BB_Audit'`; readback shows `assignedBlackboard: "BB_Memory"` — the game's real enemy-AI blackboard. Param ignored; auto-pick (likely first BlackboardData found) creates a hidden scratch→production dependency with no mention in the response. Combined with the /Game-root placement (SL-1) this is the audit's biggest data-integrity hazard. **Fix:** honor blackboardPath; never auto-pick silently.

#### B-7 [HIGH, level-editor-suite] manage_level_structure volume creates ignore volumeLocation
create_trigger_box at (500,0,100) and create_post_process_volume at (0,500,100) both spawned at world origin (verified via get_transform/get_actor_bounds = [0,0,0]) while echoing the requested extent back. **Fix:** apply the location at spawn.

#### B-8 [HIGH, cross-cutting: manage_networking, manage_audio, manage_inventory] Intermittent spurious TOOL_DISABLED
Three consecutive manage_networking calls across fresh sessions returned `Error [TOOL_DISABLED]: Tool 'manage_networking' is not enabled` while manage_tools get_status reported 22/22 enabled; the identical call then succeeded unchanged. manage_audio hit the same mid-session (two back-to-back failures, retry OK). Looks like a race in tool-enable state (possibly interacting with parallel sessions, but get_status contradicted the error in the same window). Breaks unattended chains; error text misdescribes a transient as configuration and offers no remediation. **Fix:** find the race; add "retry or check manage_tools" to the message.

#### B-9 [MEDIUM, cross-cutting: manage_gas, manage_ai] Response sanitizer redacts the guidance out of error messages
- add_ability's workaround: `...write the array via set_default [path redacted]'s CDO` — the path the user needs is stripped.
- USE_GRAPH_AUTHORING redirect: `For decorators[path redacted]{parentNodeId, subnodeType:'Decorator'|'Service', nodeClass}` — the slash in "decorators/services use add_subnode" triggered the redactor and deleted the replacement action's name.
- **Fix:** exempt error/guidance strings from the path redactor (known issue class — "response sanitizer redacts slashes").

#### B-10 [MEDIUM, inspect] diff_asset: three defects
1. Cannot load old files outside the project dir — schema explicitly suggests "a git revision extracted to a temp file", but any %TEMP% path fails `Error [DIFF_LOAD_FAILED]: Failed to load 'X' for diff (old=null new=ok)`; the same file under `<Project>/Saved/` works. Error names neither the constraint nor the fix.
2. Misses component-template property changes: a verified on-disk DefaultSceneRoot RelativeLocation change produced `components:{added:[],removed:[],changed:[]}` — a component-only edit reads as anyChange:false.
3. Leaks `/Temp/McpAuditDiff/...` packages into the loaded-object space: find_objects afterwards returns the stale OLD copy alongside the real template.

#### B-11 [MEDIUM, manage_interaction] Create-time property params accepted, echoed, dropped; partial configures stomp defaults
create_door_actor openAngle=110/openTime=1.5 echoed with `existsAfter:true` but the BP has no such variables until configure_door_properties runs; create_chest_actor `locked=true` dropped. Then configure_door_properties with only openAngle=90 reset OpenTime to the hardcoded 0.5 default — partial updates are not read-modify-write. Same pattern: configure_switch_properties echoed `switchType:'button'` for a lever switch.

#### B-12 [MEDIUM, animation_physics] create_montage produces two identical 'DefaultSlot' slot tracks
Factory already creates one; handler unconditionally appends another (`AnimationAuthoringHandlers.cpp:1430-1435`). `numSlots:2`, both 'DefaultSlot' — invalid montage structure.

#### B-13 [MEDIUM, manage_networking] set_replicated_using never creates the OnRep stub; compile stays silent
"ReplicatedUsing set to OnRep_Ammo" but no OnRep_Ammo function exists and compile returns `compiled:true` with no warnings — variable references a nonexistent notify handler.

#### B-14 [MEDIUM, manage_geometry] auto_uv fails on any post-boolean mesh; suggested fix not exposed
`Error [ENGINE_ERROR]: ... TargetMesh is non-Compact, XAtlas cannot be run. Try calling CompactMesh...` — no compact_mesh action among the 76; weld_vertices doesn't compact. **Fix:** CompactMesh in the handler before XAtlas.

#### B-15 [MEDIUM, cross-cutting] Readback actions contradict verified writes
- manage_inventory get_inventory_info(itemPath) returns `properties:{}` for native UPROPERTYs a just-confirmed set_item_properties wrote (only reports the string bag) — cost three calls chasing a phantom failure.
- manage_audio get_audio_info on a MetaSoundSource falls into the SoundWave branch (`type:'SoundWave', duration:-1`) — zero graph readback.
- level-editor get_volumes_info: brush-based PPV reports location/extent 0,0,0; TriggerBox reports 128 where the created extent was 100.

#### B-16 [LOW] Assorted response-integrity bugs
- inspect_class: BP generated classes → CLASS_NOT_FOUND even via the exact classPath inspect_cdo itself emits; engine classes return only {className, classPath, parentClass} (`detailed` is a no-op).
- manage_blueprint: add_variable reports top-level `"public":false` and embedded `"public":true` for the same var; function outputs listed under `inputs`; create returns `saved:true` + `existsOnDisk:false` + `unsavedChanges:true`.
- manage_combat create_shield: response says `shieldAmount:50` but the scaffolded variable is `CurrentShield`.
- manage_audio: `save` param ignored by creates (save=true → existsOnDisk:false) while edit actions auto-save to disk unasked; manage_blueprint compile ignores generic `save:true` (silently `saved:false`) and only `saveAfterCompile` works.

---

### 2.3 missing_functionality

- **[HIGH, manage_audio] No way to wire a SoundCue root (FirstNode) when authoring node-by-node.** connect_cue_nodes only matches Cue->AllNodes; 'Root'/'root'/omitted all fail SOURCE_NODE_NOT_FOUND. Graph-authored cues (Random-of-WavePlayers — the bread-and-butter variation cue) read back duration:0 and are silently inaudible. Only the single-wave create_sound_cue+wavePath path auto-roots. Fix: accept a root pseudo-node or add set_cue_root.
- **[MEDIUM, cross-cutting] get_*_info readbacks too thin to close the create→configure→verify loop.** get_gas_info: no attributes/modifier details/cost-cooldown classes, enums as raw ints. get_interaction_info: literally only `{assetType, path, name}` — its no-information-ness is how B-1/B-11 stayed hidden. manage_ai: no way to enumerate BT node IDs after authoring (nodeIds unrecoverable; remove_node/set_node_properties need them). manage_effect: no module-stack readback (emitterName silently ignored). manage_networking get_networking_info: replication condition/RepNotify/RPC flags write-only. get_combat_stats: names only, no values. get_animation_info: BlendSpace = numSamples only, AnimBP = parentClass only. get_character_info omits mesh/ABP/crouch/sprint/rotationRate/nav. get_mesh_info: actor-only, no static-mesh asset readback (LODs/collision/Nanite). manage_blueprint: no readback of widget property values (TextBlock text unverifiable).
- **[MEDIUM, manage_character] map_surface_to_sound cannot map a surface to a sound.** No sound param exists anywhere in the schema; surfaceType is ignored too; handler only ensures an empty FootstepSoundMap TMap var (`CharacterHandlers.cpp:1638-1660`). Response "Surface mapping configured" + echoed `surfaceType:'Grass'` implies an entry was added; nothing was.
- **[MEDIUM, manage_inventory] No way to author custom gameplay stats on an item.** set_item_properties only writes native props (DisplayName/Value → "Property not found"); the string bag is only written by fixed actions. Effective item model = Description + stacking flags + icon.
- **[MEDIUM, manage_tools] Server declares `tools.listChanged:false`.** The whole point of manage_tools is runtime tool-set mutation, yet connected clients are never sent list_changed — dead tools stay offered, newly enabled tools invisible until reconnect.

---

### 2.4 confusing_interface / docs

- **[MEDIUM, systemic] Flat per-tool param pools with no per-action mapping.** manage_combat (~90 params), manage_geometry (~50), manage_interaction, manage_character: params accepted by the wrong action vanish silently (set_weapon_stats swallowed criticalMultiplier/headshotMultiplier — they belong to configure_damage_execution; setup_reload_system ignored maxAmmo; setup_hitbox_component ignored `name` and `hitboxBoneName`). Only undocumented hint: responses echo only consumed params. Fix: per-action param docs + `ignoredParams` warning.
- **[MEDIUM, manage_ai] create_behavior_tree makes a BT the graph API can't extend.** add_node fails `Error [GRAPH_NOT_FOUND]: Behavior Tree has no graph.` with no hint that the generically-named 'create' action is the only graph-authorable creator. Two creators, one trap.
- **[MEDIUM, manage_combat] Runtime-verb actions (apply_damage, heal, create_shield) are variable scaffolds and their responses never say so.** apply_damage added AppliedDamageAmount/Type vars; heal left CurrentHealth untouched. Per-response there's no scaffold-vs-system signal (only 2 of ~14 mutating actions return `variablesAdded` — setup_parry_block_system's response is the model everything else should copy).
- **[MEDIUM, manage_character] jumpHeight/set_jump_height write raw JumpZVelocity with no conversion** — jumpHeight=300 is a ~4.6cm hop, not 300cm. Convert via sqrt(2gh) or rename. Related: configure_movement_speeds silently drops runSpeed when walkSpeed present (`runSpeedApplied:false`, no explanation) and never consumes sprintSpeed.
- **[MEDIUM, tool descriptions undersell/mislead]** manage_networking's description covers replication only — Enhanced Input authoring, game-framework classes, split-screen, voice, LAN sessions are undiscoverable. manage_asset's description omits DataTables and the ~70% of its 129-action enum that is material/texture authoring. manage_character advertises "animation assets" but has no anim-asset action. manage_effect doesn't distinguish level-mutating actions (spawn_niagara, debug_shape...) from asset authoring.
- **[MEDIUM, param addressing inconsistency]** inspect get/set_property: schema-form `objectPath` resolves to the Blueprint asset (PROPERTY_NOT_FOUND); working `blueprintPath` (silently redirects to CDO) is undocumented; BP component writes need the internal `...BP_X_C:Comp_GEN_VARIABLE` form. manage_asset rename/duplicate reject `assetPath`/`newName` for `sourcePath`+`destinationPath` (newName is dead in the schema). manage_ai get_ai_info with the tool's own generic `assetPath` returns empty `{}` with no error. `name` vs `actorName` honored inconsistently across control_actor/build_environment (spawn ignored `name`, spawn_light honored it, create_fog_volume ignored it).
- **[LOW, error-message quality]** MISSING_PARAMETER errors list ALL required params, not the missing one (manage_blueprint get_widget_slot_info blamed a param that was provided); add_emitter_to_system blames `systemPath` when `emitterPath` is missing; add_keyframe `UNSUPPORTED_PROPERTY` names no supported properties ('Transform' works, 'Location' doesn't); get_audio_info with no assetPath → `ASSET_NOT_FOUND: Could not load asset: ` (empty echo, wrong code); add_subnode INVALID_CLASS gives no naming hint (wants exact `BTDecorator_Blackboard` while the dead decoratorType enum teaches short names); create_interaction_component → `BLUEPRINT_NOT_FOUND: Empty request`.
- **[LOW, response-shape inconsistency]** level-editor mixes structured envelopes, bare objects, and plain strings ('Keyframe added', 'FPS stat toggled' — set or flip?). manage_networking echoes valueType as raw enum int (`"valueType":"0"`) that can't round-trip into the schema's string names; manage_gas readbacks likewise (instancingPolicy:2). manage_effect list_debug_shapes returns the shape-type catalog, not active shapes. connect_cue_nodes 'source' means parent-consuming-child (MAX_CHILDREN_EXCEEDED on the intuitive direction); undocumented `childIndex`.
- **[LOW, docs]** Golden schema file (`tests/schema/tools-schema.golden.json`) renders `"required": "action"` as a string — invalid JSON Schema; cause is `Canonicalize` in `tests/schema/schema-snapshot-test.ps1` unrolling single-element arrays via pipeline return (would also corrupt any single-element enum). Reported independently by three agents told to treat the golden as spec. Also: `Encumberance` (sic) misspelling propagates from schema into generated BP variable names permanently; protected tools not discoverable in list_tools (only via a failed disable probe); enable/disable_tools with a missing `tools` param is a silent success:true no-op; documented `pwsh -File mcp-call.ps1 -Arguments @{...}` invocation can't pass hashtables across a process boundary; array variableType syntax (`Array<String>`, not `String[]`) undocumented and the CLASS_NOT_FOUND error gives no hint; create_sky_sphere on 5.7 never creates a sky sphere (honest fallbackUsed:true, but the name over-promises); manage_inventory data shape is semicolon-packed strings in a TMap<FString,FString> (game code needs a parser; raw path strings break on rename); get_metadata carries a stray `debug_has_meta:true` field; find_by_tag omits the `success` field every other action returns.

---

## 3. What worked well

- **Chainable responses.** Nearly every create returns the exact path/nodeId/entryIndex the next call needs; manage_ai/manage_audio/manage_effect graph authoring, manage_sequence bindings, and DataTable row CRUD all chained first-try.
- **Verification fields where they exist are excellent:** `existsAfter`, `rowCount`, `scsVerification` transform readback (manage_blueprint add_scs_component), set_item_properties' per-key `modifiedProperties`/`failedProperties`, set_input_trigger's `alreadyPresent`/`replaced`/`triggerCount`, manage_tools' `disabled`/`notFound`/`protected` arrays. These are the patterns to generalize.
- **Error text usually names the real fix** (`sourcePath and destinationPath required`, `bounds must be an array of 6 values [...]`, `Missing 'pawnClass' or 'defaultPawnClass'`) — most schema lies were recoverable in one retry because of this.
- **Whole pipelines completed end-to-end first-try:** MetaSound create→input→node→output→connect→default; Niagara system→emitter→6 modules→user params→GPU→validate; montage+sections+notifies; blackboard→BT graph→EQS→perception; Enhanced Input asset authoring with honest trigger readback; attenuation workflow; the full level-editor core loop (spawn/transform/attach/list/delete, sequencer, synchronous screenshots with maxWidth honored, PIE start/stop with isPieWorld verification).
- **Engine-side geometry machinery is genuinely solid** — booleans, deformers, collision, LOD, Nanite conversion all verified via triangle-count readbacks.
- **manage_tools is the one tool with zero schema lies** — everything worked exactly as advertised, verified at the raw tools/list wire level.
- **Honest self-disclosure where it exists:** set_attribute_clamping says PreAttributeChange must be wired manually; create_sky_sphere reports fallbackUsed + missingAsset; add_ability fails loudly with a workaround.

---

## 4. Prioritized top-10 fix list

1. **Kill the silent-success class (B-1).** One policy sweep: every mutating handler returns `appliedProperties` + `ignoredParams` and errors when zero requested changes applied. Directly fixes manage_interaction configure_*, set_anchor, GAS set_*_tags, set_blend_in/out, set_volume_extent, set_node_properties, wall-run/footstep scaffolds — 6 of 17 high-severity findings with one convention.
2. **Honor destination paths; abolish hardcoded fallback folders (SL-1).** create_sound_cue, create_blend_space_1d, convert_to_static_mesh/nanite, manage_ai create. Accept path/savePath aliases; error rather than default. Also clean up the four stray assets listed in SL-1.
3. **manage_ai 'create': honor `path` and `blackboardPath` (SL-1/B-6).** The silent bind of a scratch BT to production BB_Memory is the audit's worst data-integrity hazard.
4. **Fix inspect set_property `saved:true` lie + unify save semantics (B-2, B-16).** saved must mean on-disk; reconcile save vs saveAfterCompile vs ignored-save-on-create vs auto-save-on-edit.
5. **Schema-to-handler param reconciliation pass (SL-2..SL-10).** Generate the schema from the handlers' actual param reads (or add a test that diffs them). ~20 known drift points; geometry, gas, animation_physics, audio worst.
6. **Fix the TOOL_DISABLED race (B-8)** and flip `tools.listChanged` to true with real notifications — together these make unattended chains viable.
7. **Persist what the response echoes:** create_crafting_recipe output (B-4), interaction create-time params + read-modify-write configures (B-11), MaxCustomMovementSpeed clobber (B-5), add_variable defaultValue / add_attribute baseValue / create-item properties (SL-6).
8. **Fix find_by_tag (B-3)** — tagging is currently write-only — and add SoundCue root wiring (missing_functionality, HIGH).
9. **Remove dead actions from the published enums (SL-5):** add_ability, set_niagara_parameter, the add_composite_node family + its four param enums; and exempt error-guidance text from the response sanitizer (B-9) so the redirects that remain are readable.
10. **Readback depth pass (B-15 + missing_functionality):** get_interaction_info, get_gas_info, get_ai_info node listing, get_niagara_info module stack, get_audio_info MetaSound branch, per-property replication state, get_combat_stats values. Every silent-success bug above stayed hidden exactly as long as the tool's own readback couldn't see it; the `inspect` tool caught them all — the domain readbacks should be able to.

Also worth 15 minutes each: fix the golden-schema Canonicalize array-unroll (misleads anyone auditing against the golden), the `Encumberance` typo before more BPs inherit it, and CompactMesh-before-XAtlas in auto_uv (B-14).
