# Handler mapping: MCP tool → C++ handler

Lookup from an MCP tool name (and action) to the C++ handler function and file that
serves it. **Source of truth is the code, not this doc:**

- `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/McpConsolidatedActionRouting.h`
  — the shared action lists, family predicates, and `GetToolRoutingTable()`.
- `MCP/Calls/McpCalls_<Tool>.cpp` (registered via `McpRegister<Tool>Calls()` in
  `Private/McpAutomationBridgeSubsystem.cpp`) — the `FMcpCall` class per
  `(tool, action)` that serves each name, and the handler member it calls.
- `McpStartupValidation::ValidateActionRouting()` (`Private/MCP/McpStartupValidation.cpp`)
  cross-checks the routing table against the published schemas at editor boot and
  logs errors on drift.

Request flow: `tools/call` → `FMcpCallRegistry::FindCall(tool, payload action)` →
`FMcpCall::Execute` (envelope / `RequiresEditor` checks → the class's `Run`). A miss —
or a call that declines by returning `false` — yields `UNKNOWN_ACTION`; there is no
secondary dispatch chain. The 22 canonical tool names live in
`FMcpToolRegistry::GetCanonicalToolNames` (`Private/MCP/McpToolRegistry.cpp`).

## Tools (22 canonical)

File names omit the `McpAutomationBridge_` prefix; all files live in
`plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/` unless a
subdirectory is shown.

| MCP tool | Routed families (in test order) → handler | Core / fallthrough handler |
| :-- | :-- | :-- |
| `manage_asset` | — (MaterialAuthoring/Texture stay listed in `GetToolRoutingTable()` for schema-union validation only) | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageAsset.cpp (dispatched from the call registry; implementations are the existing AssetWorkflow/AssetQuery members for the 43 core actions, the `HandleMaterial*` members @ MaterialAuthoringHandlers.cpp for the 58 material-authoring actions, and the `HandleTexture*` members @ TextureHandlers.cpp for the 21 texture actions) |
| `manage_blueprint` | — (CommonUi/WidgetAuthoring/BlueprintGraph stay listed in `GetToolRoutingTable()` for schema-union validation only) | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageBlueprint.cpp (dispatched from the call registry; each class calls a per-action member directly — Core → `HandleBlueprint*` @ BlueprintHandlers.cpp, BlueprintGraph → `HandleBlueprintGraph*` @ BlueprintGraphHandlers.cpp, WidgetAuthoring → `HandleWidgetAuthoring*` @ WidgetAuthoringHandlers.cpp, CommonUi → `HandleCommonUi*` @ CommonUIHandlers.cpp. The five route dispatchers (`HandleBlueprintAction`/`HandleBlueprintGraphAction`/`HandleSCSAction`/`HandleManageWidgetAuthoringAction`/`HandleCommonUiAction`) were extracted and deleted; `HandleBlueprintModifyScs` is additionally called directly by EditorFunctionHandlers.cpp) |
| `build_environment` | — (Lighting/Splines stay listed in `GetToolRoutingTable()` for schema-union validation only) | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_BuildEnvironment.cpp (dispatched from the call registry; implementations are the `HandleEnvironment*` members @ EnvironmentHandlers.cpp for the 21 core actions, `HandleLighting*` @ LightingHandlers.cpp for the 12 lighting actions, and the `HandleSpline*` wrappers @ SplineHandlers.cpp for the 22 spline actions) |
| `animation_physics` | — (AnimationAuthoring/Skeleton stay listed in `GetToolRoutingTable()` for schema-union validation only) | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_AnimationPhysics.cpp (dispatched from the call registry; implementations are the `HandleAnimPhys*` members @ AnimationHandlers.cpp for the 14 core actions, `HandleAnimAuthoring*` @ AnimationAuthoringHandlers.cpp for the 42 authoring actions, and the `HandleSkeleton*` members @ SkeletonHandlers.cpp for the 29 skeleton actions) |
| `system_control` | — (Performance stays listed in `GetToolRoutingTable()` for schema-union validation only) | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_SystemControl.cpp (dispatched from the call registry; implementations spread across PerformanceHandlers.cpp (`HandlePerf*`), SystemControlHandlers.cpp (`HandleSys*`), UiHandlers.cpp (`HandleUi*`), LogHandlers.cpp (`HandleLog*`), DebugHandlers.cpp, RenderHandlers.cpp — see note) |
| `manage_networking` | — (GameFramework/Sessions stay listed in `GetToolRoutingTable()` for schema-union validation only) | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageNetworking.cpp (dispatched from the call registry; implementations are the `HandleNetworking*` members @ NetworkingHandlers.cpp for the 27 core actions, `HandleGameFramework*` @ GameFrameworkHandlers.cpp for the 20 game-framework actions, and the `HandleSessions*` wrappers @ SessionsHandlers.cpp for the 16 session actions) |
| `manage_input` | — (single-handler: `Input()` is both the core list and the schema union) | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageInput.cpp (dispatched from the call registry; implementations are the `HandleInput*` members @ InputHandlers.cpp for the 9 Enhanced Input actions, split out of manage_networking) |
| `manage_level_structure` | — (Volumes stays listed in `GetToolRoutingTable()` for schema-union validation only) | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageLevelStructure.cpp (dispatched from the call registry; implementations are the `HandleLevelStructure*` members @ LevelStructureHandlers.cpp for the 17 core actions and the `HandleVolume*` members @ VolumeHandlers.cpp for the 28 volume actions) |
| `manage_audio` | — (AudioAuthoring stays listed in `GetToolRoutingTable()` for schema-union validation only) | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageAudio.cpp (dispatched from the call registry; implementations are the `HandleAudio*` members @ AudioHandlers.cpp for the 23 core actions and the `HandleAudioAuthoring*` members @ AudioAuthoringHandlers.cpp for the 27 authoring actions) |
| `manage_ai` | — (BehaviorTree/Navigation survive as `ManageAI()` union inputs for schema-union validation only) | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageAi.cpp (dispatched from the call registry; implementations are the `HandleAi*` members @ AIHandlers.cpp for the 42 core actions, `HandleBehaviorTree*` @ BehaviorTreeHandlers.cpp for the 7 Behavior Tree graph actions, and the `HandleNavigation*` wrappers @ NavigationHandlers.cpp for the 12 navigation actions) |
| `control_actor` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ControlActor.cpp (dispatched from the call registry; implementations remain the `HandleControlActor*` functions in ControlHandlers.cpp) |
| `control_editor` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ControlEditor.cpp (dispatched from the call registry; implementations remain the `HandleControlEditor*` functions in ControlHandlers.cpp + FocusInputHandlers.cpp) |
| `inspect` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_Inspect.cpp (dispatched from the call registry; implementations are the `HandleInspect*` members @ EnvironmentHandlers.cpp, the shared `HandleControlActor*` members @ ControlHandlers.cpp for the twelve actor actions, `HandleSetObjectProperty`/`HandleGetObjectProperty`/`HandleInspectCdoAction`/`HandleDiffAssetAction` @ PropertyHandlers.cpp, and `HandleInspectUiFocus` @ FocusInputHandlers.cpp) |
| `manage_level` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageLevel.cpp (dispatched from the call registry; implementations remain the `HandleLevel*` functions in LevelHandlers.cpp, plus `HandleLightingCreateLight` @ LightingHandlers.cpp for `create_light`) |
| `manage_sequence` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageSequence.cpp (dispatched from the call registry; implementations remain the `HandleSequence*` functions in SequenceHandlers.cpp) |
| `manage_geometry` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageGeometry.cpp (dispatched from the call registry; implementations are the `HandleGeometry*` members @ GeometryHandlers.cpp, thin wrappers over that TU's static free functions) |
| `manage_effect` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageEffect.cpp (dispatched from the call registry; implementations are the `HandleEffect*` members + `CreateNiagaraEffect` @ EffectHandlers.cpp, the `HandleNiagara*` members @ NiagaraAuthoringHandlers.cpp for the 37 authoring actions, and `HandleNiagaraGraphAction` @ NiagaraGraphHandlers.cpp for the three graph actions) |
| `manage_gas` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageGas.cpp (dispatched from the call registry; implementations are the `HandleGas*` members @ GASHandlers.cpp) |
| `manage_character` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageCharacter.cpp (dispatched from the call registry; implementations are the `HandleCharacter*` members @ CharacterHandlers.cpp) |
| `manage_combat` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageCombat.cpp (dispatched from the call registry; implementations are the `HandleCombat*` members @ CombatHandlers.cpp) |
| `manage_inventory` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageInventory.cpp (dispatched from the call registry; implementations are the `HandleInventory*` members @ InventoryHandlers.cpp) |
| `manage_interaction` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ManageInteraction.cpp (dispatched from the call registry; implementations are the `HandleInteraction*` members @ InteractionHandlers.cpp) |

`system_control` note — the family is classed; the registration lambda that used
to re-dispatch per action is gone. Per-class delegation: the 21 Performance
actions → `HandlePerf*` @ PerformanceHandlers.cpp; build/test/python actions →
`HandleSys*` @ SystemControlHandlers.cpp (`start_session` delegates on to
`HandleInsightsAction` @ InsightsHandlers.cpp); `execute_command`/`set_cvar` →
`HandleSysExecuteCommand`/`HandleSysSetCvar`, which call
`HandleConsoleCommandAction` @ ConsoleCommandHandlers.cpp directly (the console
handler's internal canonical name stays `console_command`; it has other owners —
ControlHandlers.cpp and EditorFunctionHandlers.cpp — and survives);
`get_log`/`tail_log`/`clear_log`/`subscribe`/`unsubscribe` → `HandleLogQuery`/
`HandleLogClear`/`HandleLogSubscribe`/`HandleLogUnsubscribe` @ LogHandlers.cpp
(not editor-gated); `spawn_category` → `HandleDebugSpawnCategory` @
DebugHandlers.cpp; `lumen_update_scene` → `HandleRenderLumenUpdateScene` @
RenderHandlers.cpp; `screenshot`/`create_widget`/`add_widget_child`/
`get_project_settings`/`set_project_setting` → `HandleUi*` @ UiHandlers.cpp.
`GetToolRoutingTable()` keeps the Performance routed-family row so boot
validation can prove the schema union matches the published enum — the first
classed family with a non-empty routed list.

## Shared action lists → owning handler

Routed family lists:

| List (McpConsolidatedActionRouting.h) | Owner |
| :-- | :-- |
| `MaterialAuthoring` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageAsset.cpp call `HandleMaterial*` @ MaterialAuthoringHandlers.cpp (list retained for schema-union validation) |
| `Texture` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageAsset.cpp call `HandleTexture*` @ TextureHandlers.cpp (list retained for schema-union validation) |
| `BlueprintGraph` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageBlueprint.cpp call `HandleBlueprintGraph*` @ BlueprintGraphHandlers.cpp (list retained for schema-union validation) |
| `WidgetAuthoring` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageBlueprint.cpp call `HandleWidgetAuthoring*` @ WidgetAuthoringHandlers.cpp (list retained for schema-union validation) |
| `CommonUi` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageBlueprint.cpp call `HandleCommonUi*` @ CommonUIHandlers.cpp (list retained for schema-union validation) |
| `Lighting` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_BuildEnvironment.cpp delegate to `HandleLighting*` @ LightingHandlers.cpp (list retained for schema-union validation) |
| `Splines` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_BuildEnvironment.cpp delegate to `HandleSpline*` wrappers @ SplineHandlers.cpp (list retained for schema-union validation) |
| `AnimationAuthoring` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_AnimationPhysics.cpp delegate to `HandleAnimAuthoring*` @ AnimationAuthoringHandlers.cpp (list retained for schema-union validation) |
| `Skeleton` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_AnimationPhysics.cpp delegate to `HandleSkeleton*` @ SkeletonHandlers.cpp (list retained for schema-union validation) |
| `AudioAuthoring` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageAudio.cpp delegate to `HandleAudioAuthoring*` @ AudioAuthoringHandlers.cpp (list retained for schema-union validation) |
| `Performance` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_SystemControl.cpp delegate to `HandlePerf*` @ PerformanceHandlers.cpp (list retained for schema-union validation) |
| `Input` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageNetworking.cpp delegate to `HandleInput*` @ InputHandlers.cpp (list retained for schema-union validation) |
| `GameFramework` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageNetworking.cpp delegate to `HandleGameFramework*` @ GameFrameworkHandlers.cpp (list retained for schema-union validation) |
| `Sessions` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageNetworking.cpp delegate to `HandleSessions*` wrappers @ SessionsHandlers.cpp (list retained for schema-union validation) |
| `Volumes` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageLevelStructure.cpp delegate to `HandleVolume*` @ VolumeHandlers.cpp (list retained for schema-union validation) |
| `BehaviorTree` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageAi.cpp delegate to `HandleBehaviorTree*` @ BehaviorTreeHandlers.cpp (list retained for schema-union validation) |
| `Navigation` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageAi.cpp delegate to `HandleNavigation*` wrappers @ NavigationHandlers.cpp (list retained for schema-union validation) |

Core (fallthrough) lists:

| List | Owner |
| :-- | :-- |
| `ManageAssetCore` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageAsset.cpp (implementations are the existing AssetWorkflow/AssetQuery members) |
| `ManageBlueprintCore` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageBlueprint.cpp call `HandleBlueprint*` @ BlueprintHandlers.cpp |
| `BuildEnvironmentCore` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_BuildEnvironment.cpp (implementations are the `HandleEnvironment*` members @ EnvironmentHandlers.cpp) |
| `AnimationPhysicsCore` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_AnimationPhysics.cpp (implementations are the `HandleAnimPhys*` members @ AnimationHandlers.cpp) |
| `SystemControlCore` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_SystemControl.cpp (per-class delegation split in the note above) |
| `ManageAudioCore` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageAudio.cpp (implementations are the `HandleAudio*` members @ AudioHandlers.cpp) |
| `ManageNetworkingCore` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageNetworking.cpp (implementations are the `HandleNetworking*` members @ NetworkingHandlers.cpp) |
| `ManageLevelStructureCore` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageLevelStructure.cpp (implementations are the `HandleLevelStructure*` members @ LevelStructureHandlers.cpp) |
| `ManageAICore` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageAi.cpp (implementations are the `HandleAi*` members @ AIHandlers.cpp) |
| `ControlActor` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ControlActor.cpp |
| `ControlEditor` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ControlEditor.cpp |
| `Inspect` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_Inspect.cpp (implementations spread across EnvironmentHandlers.cpp, ControlHandlers.cpp, PropertyHandlers.cpp, FocusInputHandlers.cpp) |
| `ManageLevel` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageLevel.cpp |
| `ManageSequence` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageSequence.cpp |
| `ManageGeometry` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageGeometry.cpp (implementations are the `HandleGeometry*` members @ GeometryHandlers.cpp) |
| `ManageEffect` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageEffect.cpp (implementations spread across EffectHandlers.cpp, NiagaraAuthoringHandlers.cpp, NiagaraGraphHandlers.cpp) |
| `ManageGAS` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageGas.cpp (implementations are the `HandleGas*` members @ GASHandlers.cpp) |
| `ManageCharacter` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageCharacter.cpp (implementations are the `HandleCharacter*` members @ CharacterHandlers.cpp) |
| `ManageCombat` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageCombat.cpp (implementations are the `HandleCombat*` members @ CombatHandlers.cpp) |
| `ManageInventory` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageInventory.cpp (implementations are the `HandleInventory*` members @ InventoryHandlers.cpp) |
| `ManageInteraction` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ManageInteraction.cpp (implementations are the `HandleInteraction*` members @ InteractionHandlers.cpp) |

The union builders (`ManageAsset()`, `ManageBlueprint()`, …, `*Union()`) are schema
enums only — they own no actions.

## Finding an action

1. Grep the routing header for the action to find its list:
   `grep -n 'TEXT("set_blend_mode")' plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/McpConsolidatedActionRouting.h`
2. The list's row above names the owning handler and file.
3. Grep that file for the sub-action string to land on the implementing branch:
   `grep -n '"set_blend_mode"' plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_MaterialAuthoringHandlers.cpp`
4. Not in the routing header at all? Then it is not reachable from a canonical
   tool. Only the 22 canonical tool names are published — the legacy
   Node-server-era per-action registrations (`manage_ui`, `manage_misc`,
   `asset_query`, ~100 others) were deleted 2026-07-02 as unreachable. Their
   handler functions survive where an `FMcpCall` class calls them directly
   (e.g. `HandleConsoleCommandAction`, shared by several classes);
   `HandleMiscAction` had no caller and was deleted with
   `McpAutomationBridge_MiscHandlers.cpp`, and
   `HandleUiAction` died at the system_control classing (its five advertised
   bodies extracted to `HandleUi*` members, its hidden bodies deleted).

Within one tool an action belongs to exactly one routed family (boot validation
enforces disjointness, including against the core list). The same action name may
appear under *different* tools with different owners (e.g. `delete_node` is
MaterialAuthoring under `manage_asset` but BlueprintGraph under
`manage_blueprint`) — always start from the tool.
