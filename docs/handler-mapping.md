# Handler mapping: MCP tool → C++ handler

Lookup from an MCP tool name (and action) to the C++ handler function and file that
serves it. **Source of truth is the code, not this doc:**

- `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/McpConsolidatedActionRouting.h`
  — the shared action lists, family predicates, and `GetToolRoutingTable()`.
- `UMcpAutomationBridgeSubsystem::InitializeHandlers()` in
  `Private/McpAutomationBridgeSubsystem.cpp` — which handler serves each registered
  name, and what each consolidated lambda re-dispatches to.
- `McpStartupValidation::ValidateActionRouting()` (`Private/MCP/McpStartupValidation.cpp`)
  cross-checks the routing table against the published schemas at editor boot and
  logs errors on drift.

Request flow: `tools/call` → handler-registry lookup by tool name → consolidated
lambda tests the routed families in order on the payload's `subAction`/`action` →
matching family handler, else the tool's core fallthrough handler. A handler that
declines (returns `false`) yields `UNKNOWN_ACTION` — there is no secondary dispatch chain.
The 22 canonical tool names live in `FMcpToolRegistry::GetCanonicalToolNames`
(`Private/MCP/McpToolRegistry.cpp`).

## Tools (22 canonical)

File names omit the `McpAutomationBridge_` prefix; all files live in
`plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/` unless a
subdirectory is shown.

| MCP tool | Routed families (in test order) → handler | Core / fallthrough handler |
| :-- | :-- | :-- |
| `manage_asset` | MaterialAuthoring → `HandleManageMaterialAuthoringAction`; Texture → `HandleManageTextureAction` | `HandleAssetAction` @ AssetWorkflowHandlers.cpp |
| `manage_blueprint` | CommonUi → `HandleCommonUiAction`; WidgetAuthoring → `HandleManageWidgetAuthoringAction`; BlueprintGraph → `HandleBlueprintGraphAction` | `HandleBlueprintAction` @ BlueprintHandlers.cpp |
| `build_environment` | Lighting → `HandleLightingAction`; Splines → `HandleManageSplinesAction` | `HandleBuildEnvironmentAction` @ EnvironmentHandlers.cpp |
| `animation_physics` | AnimationAuthoring → `HandleManageAnimationAuthoringAction`; Skeleton → `HandleManageSkeleton` | `HandleAnimationPhysicsAction` @ AnimationHandlers.cpp |
| `system_control` | Performance → `HandlePerformanceAction`; plus per-action re-dispatches (see note) | `HandleSystemControlAction` @ SystemControlHandlers.cpp |
| `manage_networking` | Input → `HandleInputAction`; GameFramework → `HandleManageGameFrameworkAction`; Sessions → `HandleManageSessionsAction` | `HandleManageNetworkingAction` @ NetworkingHandlers.cpp |
| `manage_level_structure` | Volumes → `HandleManageVolumesAction` | `HandleManageLevelStructureAction` @ LevelStructureHandlers.cpp |
| `manage_audio` | AudioAuthoring → `HandleManageAudioAuthoringAction` | `HandleAudioAction` @ AudioHandlers.cpp |
| `manage_ai` | — (BehaviorTree/Navigation membership is tested *inside* the handler, not the lambda) | `HandleManageAIAction` @ AIHandlers.cpp |
| `control_actor` | — | CLASSED: `FMcpCall` instances @ MCP/Calls/McpCalls_ControlActor.cpp (dispatched from the call registry before the handler map; implementations remain the `HandleControlActor*` functions in ControlHandlers.cpp) |
| `control_editor` | — | `HandleControlEditorAction` @ ControlHandlers.cpp |
| `inspect` | — | `HandleInspectAction` @ EnvironmentHandlers.cpp |
| `manage_level` | — | `HandleLevelAction` @ LevelHandlers.cpp |
| `manage_sequence` | — | `HandleSequenceAction` @ SequenceHandlers.cpp |
| `manage_geometry` | — | `HandleGeometryAction` @ GeometryHandlers.cpp |
| `manage_effect` | — | `HandleEffectAction` @ EffectHandlers.cpp |
| `manage_gas` | — | `HandleManageGASAction` @ GASHandlers.cpp |
| `manage_character` | — | `HandleManageCharacterAction` @ CharacterHandlers.cpp |
| `manage_combat` | — | `HandleManageCombatAction` @ CombatHandlers.cpp |
| `manage_inventory` | — | `HandleManageInventoryAction` @ InventoryHandlers.cpp |
| `manage_interaction` | — | `HandleManageInteractionAction` @ InteractionHandlers.cpp |

`system_control` note — its lambda also re-dispatches specific core actions before
the fallthrough: `execute_command`/`set_cvar` →
`HandleConsoleCommandAction` @ ConsoleCommandHandlers.cpp (the handler's internal
canonical name is `console_command`);
`subscribe`/`unsubscribe`/`get_log`/`tail_log`/`clear_log` → `HandleLogAction` @
LogHandlers.cpp; `spawn_category` → `HandleDebugAction` @ DebugHandlers.cpp;
`lumen_update_scene` → `HandleRenderAction` @ RenderHandlers.cpp;
`screenshot`/`create_widget`/`add_widget_child`/`get_project_settings`/`set_project_setting`
→ `HandleUiAction` @ UiHandlers.cpp (schema-advertised on `system_control`, implemented
in the otherwise-unreachable UI handler). These are per-action tests in the lambda,
not routed families, so `GetToolRoutingTable()` does not list them.

## Shared action lists → owning handler

Routed family lists:

| List (McpConsolidatedActionRouting.h) | Owner |
| :-- | :-- |
| `MaterialAuthoring` | `HandleManageMaterialAuthoringAction` @ MaterialAuthoringHandlers.cpp |
| `Texture` | `HandleManageTextureAction` @ TextureHandlers.cpp |
| `BlueprintGraph` | `HandleBlueprintGraphAction` @ BlueprintGraphHandlers.cpp |
| `WidgetAuthoring` | `HandleManageWidgetAuthoringAction` @ WidgetAuthoringHandlers.cpp |
| `CommonUi` | `HandleCommonUiAction` @ CommonUIHandlers.cpp |
| `Lighting` | `HandleLightingAction` @ LightingHandlers.cpp |
| `Splines` | `HandleManageSplinesAction` @ SplineHandlers.cpp |
| `AnimationAuthoring` | `HandleManageAnimationAuthoringAction` @ AnimationAuthoringHandlers.cpp |
| `Skeleton` | `HandleManageSkeleton` @ SkeletonHandlers.cpp |
| `AudioAuthoring` | `HandleManageAudioAuthoringAction` @ AudioAuthoringHandlers.cpp |
| `Performance` | `HandlePerformanceAction` @ PerformanceHandlers.cpp |
| `Input` | `HandleInputAction` @ InputHandlers.cpp |
| `GameFramework` | `HandleManageGameFrameworkAction` @ GameFrameworkHandlers.cpp |
| `Sessions` | `HandleManageSessionsAction` @ SessionsHandlers.cpp |
| `Volumes` | `HandleManageVolumesAction` @ VolumeHandlers.cpp |
| `BehaviorTree` | `HandleBehaviorTreeAction` @ BehaviorTreeHandlers.cpp (reached via `HandleManageAIAction`'s internal dispatch) |
| `Navigation` | `HandleManageNavigationAction` @ NavigationHandlers.cpp (same) |

Core (fallthrough) lists:

| List | Owner |
| :-- | :-- |
| `ManageAssetCore` | `HandleAssetAction` @ AssetWorkflowHandlers.cpp |
| `ManageBlueprintCore` | `HandleBlueprintAction` @ BlueprintHandlers.cpp |
| `BuildEnvironmentCore` | `HandleBuildEnvironmentAction` @ EnvironmentHandlers.cpp |
| `AnimationPhysicsCore` | `HandleAnimationPhysicsAction` @ AnimationHandlers.cpp |
| `SystemControlCore` | `HandleSystemControlAction` @ SystemControlHandlers.cpp, plus the lambda re-dispatches noted above |
| `ManageAudioCore` | `HandleAudioAction` @ AudioHandlers.cpp |
| `ManageNetworkingCore` | `HandleManageNetworkingAction` @ NetworkingHandlers.cpp |
| `ManageLevelStructureCore` | `HandleManageLevelStructureAction` @ LevelStructureHandlers.cpp |
| `ManageAICore` | `HandleManageAIAction` @ AIHandlers.cpp |
| `ControlActor` | CLASSED — `FMcpCall` instances @ MCP/Calls/McpCalls_ControlActor.cpp |
| `ControlEditor` | `HandleControlEditorAction` @ ControlHandlers.cpp |
| `Inspect` | `HandleInspectAction` @ EnvironmentHandlers.cpp |
| `ManageLevel` | `HandleLevelAction` @ LevelHandlers.cpp |
| `ManageSequence` | `HandleSequenceAction` @ SequenceHandlers.cpp |
| `ManageGeometry` | `HandleGeometryAction` @ GeometryHandlers.cpp |
| `ManageEffect` | `HandleEffectAction` @ EffectHandlers.cpp |
| `ManageGAS` | `HandleManageGASAction` @ GASHandlers.cpp |
| `ManageCharacter` | `HandleManageCharacterAction` @ CharacterHandlers.cpp |
| `ManageCombat` | `HandleManageCombatAction` @ CombatHandlers.cpp |
| `ManageInventory` | `HandleManageInventoryAction` @ InventoryHandlers.cpp |
| `ManageInteraction` | `HandleManageInteractionAction` @ InteractionHandlers.cpp |

The union builders (`ManageAsset()`, `ManageBlueprint()`, …, `*Union()`) are schema
enums only — they own no actions.

## Finding an action

1. Grep the routing header for the action to find its list:
   `grep -n 'TEXT("set_blend_mode")' plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/McpConsolidatedActionRouting.h`
2. The list's row above names the owning handler and file.
3. Grep that file for the sub-action string to land on the implementing branch:
   `grep -n '"set_blend_mode"' plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_MaterialAuthoringHandlers.cpp`
4. Not in the routing header at all? Then it is not reachable from a canonical
   tool. `InitializeHandlers` registers only the 22 canonical tool names — the
   legacy Node-server-era per-action registrations (`manage_ui`, `manage_misc`,
   `asset_query`, ~100 others) were deleted 2026-07-02 as unreachable. Their
   handler functions survive where a consolidated lambda calls them directly
   (e.g. `HandleUiAction`, `HandleLightingAction`); `HandleMiscAction` had no
   caller and was deleted with `McpAutomationBridge_MiscHandlers.cpp`.

Within one tool an action belongs to exactly one routed family (boot validation
enforces disjointness, including against the core list). The same action name may
appear under *different* tools with different owners (e.g. `delete_node` is
MaterialAuthoring under `manage_asset` but BlueprintGraph under
`manage_blueprint`) — always start from the tool.
