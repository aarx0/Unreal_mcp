# Changelog

All notable changes to the MCP Automation Bridge plugin will be documented in this file.

---

## [Unreleased]

### Security
- **Capability token enforcement** on native MCP transport — validates `X-MCP-Capability-Token` header when `bRequireCapabilityToken` is enabled (mirrors WebSocket bridge logic)
- **Symlink escape prevention** in `execute_python` file path validation — resolves symlinks and re-validates against project directory
- **Code size limit** in `execute_python` — enforces 1 MB maximum for inline code payloads
- **Explicit request origin tracking** (`ERequestOrigin`) — routes HTTP vs WebSocket responses by explicit origin instead of inferring from `TargetSocket==nullptr`
- **Tool registry thread safety** — `Register()` now holds `CacheMutex` for entire body, `GetAllTools()` returns copy to prevent external mutation
- **Dynamic tool manager protection** — `EnableCategory("all")` now respects protected categories and initial state instead of blindly enabling everything

### Added — Native MCP Streamable HTTP Transport
- **Native MCP endpoint** (`POST /mcp`) directly inside the C++ plugin — AI clients connect without the TypeScript bridge
- **SSE streaming** for `tools/call` — progress notifications arrive in real-time, followed by final JSON-RPC result
- **Raw socket HTTP server** (`FRunnable` + `FSocket`) replacing `FHttpServerModule` — no external dependencies
- **JSON-RPC 2.0** protocol (MCP 2025-03-26) with `initialize`, `tools/list`, `tools/call` methods
- **Multiple concurrent sessions** — Cursor, Claude Code, and other clients can connect simultaneously
- **Session management** with `Mcp-Session-Id` header, 1-hour inactivity timeout, `DELETE /mcp` termination
- **Dynamic tool manager** — enable/disable tools and categories at runtime via `manage_tools`
- **Native tool schemas** generated from self-describing C++ tool classes with full `inputSchema` and categories (core, world, authoring, gameplay, utility); the TypeScript bridge exposes 22 canonical parent MCP tools.
- **`listChanged` notifications** — broadcast `notifications/tools/list_changed` to all active SSE connections when tool state changes
- **Load All Tools on Start** project setting — toggle between the core set and all available native tool schemas at startup
- **Status bar indicator** — `● MCP :3000 (2)` in UE editor status bar, click to open settings
- **Server identity config** — `server-info.json` for name/version/instructions, plus `NativeMCPInstructions` project setting for custom instructions
- **Client info logging** — log connecting client name and version from `initialize` request
- **`execute_python` action** in `system_control` — execute Python code with stdout/stderr capture, supports inline `code` and `file` path, execution time tracking
- **Shared `ListenHost` setting** — native MCP respects `AllowNonLoopback` for network access control
- **Plugin-packaging scripts** for Win/Mac/Linux — build and package the plugin via RunUAT BuildPlugin, with smart arg parsing
- **`manage_asset` actions** `get_referencers`, `get_asset_properties`, `set_asset_property` — read referencers / bulk-read / write a single reflected property on any asset
- **`McpDirectPackageSave`** in `McpSafeOperations.h` — raw `UPackage::Save` that bypasses the editor's validate-on-save and source-control checkout pipelines (both have crashed the editor on unattended saves). Detects read-only targets (e.g. Perforce-locked files) and reports an actionable error instead of failing silently. See `docs/safe-asset-saving.md`
- **Deep struct & object-reference array reflection** (`McpPropertyReflection` + the `McpAutomationBridgeHelpers` twin) — `TArray<FStruct>` and object-reference arrays now export as structured JSON (a nested object per element, object refs as path strings, `FKey` as `{KeyName}`, enums by name) and import back via `FJsonObjectConverter::JsonObjectToUStruct`, instead of collapsing to one opaque `ExportText` blob. Recurses nested structs (depth-capped at 6) and skips transient/deprecated child fields to match `ExportObjectToJson`. The single-struct property branch and the previously primitive-only `ApplyJsonValueToProperty`/`ImportJsonToArray` import path were extended the same way. Verified live on `IMC_GameCommands.DefaultKeyMappings.Mappings`.
- **Deep-export of *Instanced* subobjects** (`McpPropertyReflection`) — `CPF_InstancedReference` object values (Enhanced Input `Triggers`/`Modifiers`, montage `AnimNotify`s, instanced components) now export as a nested JSON object carrying the concrete class under `__class` plus the subobject's own properties, instead of an opaque subobject path that dropped all of its config. Plain asset references still export as a path; depth-capped, transient/deprecated children skipped. Verified live on `AM_AttackMontage1.Notifies` (struct-nested `AnimNotify_PlaySound` → `{__class, Sound, …}`) and `IA_Mute.Triggers` (top-level array → `InputTriggerPressed` with `ActuationThreshold`). Mirrored into the `McpAutomationBridgeHelpers.h` twin (direct top-level instanced properties; array/struct-nested cases already deep-export via the `McpPropertyReflection` delegation).
- **Round-trip (import) of *Instanced* subobject arrays** (`McpPropertyReflection`) — `set_asset_property` can now WRITE an instanced `TArray<TObjectPtr<…>>` from the `{ "__class", … }` form export emits. An owning-`UObject` is threaded through `ApplyJsonValueToProperty`/`ImportJsonToArray` (defaulted, so other callers are unaffected) and used as the `NewObject` Outer, so re-instanced subobjects (input triggers/modifiers, …) serialize into the asset; the new subobject is validated `IsChildOf` the property class and its own fields applied best-effort. Array import also tolerates a JSON string that encodes the array (the typeless `value` param leads some clients to stringify it). Verified live: on a duplicate of `IA_Mute`, writing `Triggers = [{__class: InputTriggerHold, HoldTimeThreshold: 0.75, …}]` re-instanced Pressed→Hold and a fresh read confirmed the persisted `InputTriggerHold`.
- **Round-trip of *struct-nested* Instanced subobjects** (`McpPropertyReflection`) — instanced subobjects inside a struct (canonically a montage `FAnimNotifyEvent.Notify`) now round-trip too. The struct import branch strips `CPF_InstancedReference` fields before `FJsonObjectConverter::JsonObjectToUStruct` (which can't convert the `{"__class", …}` form and would fail the whole struct), lets the engine convert the rest, then re-instances the stripped fields through the owner-aware importer. Verified live: on a duplicate of `AM_AttackMontage1`, writing `Notifies = [{ NotifyName, Notify: {__class: AnimNotify_PlaySound, Sound, VolumeMultiplier: 0.42} }]` re-instanced the `Notify` and a fresh read confirmed it persisted.
- **`TMap` / `TSet` property import** (`McpPropertyReflection`) — `ApplyJsonValueToProperty` could export maps/sets but not import them; writing a map/set property fell through to the string fallback and failed. Maps now import from a JSON object `{ "<key>": <value> }` (keys from field names, keys+values routed through the importer so struct/object/instanced values work), sets from a JSON array — both also tolerate a stringified form. Verified live on `IMC_GameCommands.MappingProfileOverrides` (`{"ProfileA":{}, "ProfileB":{}}` round-tripped). `TSet` mirrors the same path (no set-property fixture in this project to runtime-test — compile-verified).
- **`FText` property export** (`McpPropertyReflection`) — `ExportPropertyToJsonValue` had no `FTextProperty` branch, so text properties were silently dropped from reads. Now emits the display string (`.ToString()`); import already restores `FText` via the generic `ImportText` string fallback. Verified live by round-tripping an `FText` Blueprint variable through its CDO path (`<bp>.Default__<Class>_C`) — which is itself a handy way to fabricate reflection fixtures: a created Blueprint's variables are fully reachable by `get`/`set_asset_property` via that path.
- **`add_variable` container variables** (`McpAutomationBridge_BlueprintHandlers`) — `add_variable` now accepts `Set<Inner>` / `Array<Inner>` / `Map<Key,Value>` (T-prefixes ok), not just scalar types; sets `PinType.ContainerType` (+ `PinValueType` for maps). Verified by creating `Set<Name>`/`Map<Name,Int>`/`Array<Int>` variables and round-tripping their values via the CDO path. (Float/double terminals initially produced int — fixed below, together with a pre-existing scalar float/double bug.)
- **Fix: `TMap` value import** (`McpPropertyReflection`) — the `TMap` import (`da29c05`) double-offset the value via the property's `*_InContainer` accessors (it passed `GetValuePtr`, already pair+valueOffset), so every primitive value was dropped to default (the original empty-struct IMC fixture masked it). Now passes `GetPairPtr` as the container base for both key and value. Verified: `Map<Name,Int>` round-trips `{"apple":5,"banana":7}`. (`TSet` import is now runtime-verified too, via a fabricated `Set<Name>` variable.)
- **Fix: float/double Blueprint variables created as int** (`McpAutomationBridge_BlueprintHandlers`, `c0bff6a`) — `add_variable` mapped `float`/`double` to the legacy `PC_Float` *category*, which UE5 does not honor as a real type, so the BP compiler produced an **int** property and float/double variables truncated fractional values (`1.5 -> 1`) in scalar AND container positions (pre-existing for scalars; surfaced by the container work). Now uses `PC_Real` + a float/double `PinSubCategory` at every position. Verified: scalar `float`/`double` and `Array<Float>`/`Set<Float>`/`Map<Name,Float>`/`Map<Name,Double>` round-trip fractional values.
- **Fix: map/set export dropped non-trivial value/element types** (`McpPropertyReflection` + the `McpAutomationBridgeHelpers` twin, `40c3bf9`) — map-value and set-element export only handled str/int/float/bool and fell back to an `ExportText` **string** for everything else, so double/int64/byte/enum/object/struct values in maps/sets exported as strings; non-str/name/int map keys became a positional `"key_%d"` (losing the key). Both twins now delegate the value/element to `ExportPropertyToJsonValue` (full type coverage, like the array export), keys preserved via `ExportText`. Verified across both read paths: `Map<Name,Double>`/`Set<Double>`/`Map<Name,Int64>` export as proper JSON numbers.

### Changed
- Tool categories now use four groups: `core`, `world`, `gameplay`, and `utility`. The singleton `authoring` category was removed, and `manage_blueprint` moved into `core`.
- `manage_blueprint` schema: `location`, `rotation`, `scale` changed from flat number arrays to structured objects with named sub-fields (`x`/`y`/`z` or `pitch`/`yaw`/`roll`) — matches TypeScript schema
- `system_control` schema: removed `export_asset` action (not in TypeScript schema) and `additionalArgs` parameter (C++-only, never used by TS clients)
- `control_editor` schema: added `set_editor_mode` action (was missing from C++, present in TS)
- Screenshot handler: now returns `async: true` with `expectedDelay` field and timing guidance for polling
- `ScanPathsSynchronous` removed from asset query/workflow handlers to prevent GameThread blocking — documented limitation: newly-added assets may not appear until editor rescan
- Temp file cleanup in `execute_python` uses RAII scope guard for guaranteed cleanup on all exit paths

### Fixed
- **`set_asset_property` wrote to the wrong memory address (corruption / crash)** — the handler passed `ContainerPtrToValuePtr(Asset)` (an already-offset value pointer) to `ApplyJsonValueToProperty`/`ExportPropertyToJsonValue`, which themselves use the `*_InContainer` accessors and add the offset again. Every write landed at `Asset + 2×offset`: silent no-op corruption for POD properties (the matching export read the same wrong address and falsely echoed success) and an `FString::operator=` access-violation crash for string/name properties. Now passes the object base. Was masking the write as "working" in earlier testing
- **`set_asset_property` editor crash on save** — the save routed through editor pipelines that faulted in two different subsystems (`DataValidation` via validate-on-save, `GitSourceControl` via checkout-on-save). Now uses `McpDirectPackageSave` (raw package write), avoiding both. Read-only/locked targets now return `ASSET_SAVE_FAILED` with a checkout hint instead of crashing or silently no-op'ing
- **`ResolveObjectFromPath`** returned the `UPackage` instead of the asset for asset paths — broke `get_property`/`export`/`set_asset_property` on assets; now normalizes to `Package.Object` and `StaticLoadObject`s the asset
- **`execute_python` failures** now fold the captured Python traceback into the error message instead of a generic failure string
- `reset` action now restores initial state from `Initialize()` instead of enabling all tools unconditionally
- UE 5.6 compatibility: `TSharedPtr` for incomplete types, `Headers.Add` instead of `SetHeader`, `TryGetField` return value
- Package script arg parsing — flags no longer eaten as output directory, extra args correctly forwarded to RunUAT

### Technical Details
- Response routing via explicit `ERequestOrigin` enum (`NativeHTTP` vs `WebSocket`) — no more `TargetSocket==nullptr` inference
- Thread-safe SSE writes: per-connection `WriteMutex`, snapshot pattern for broadcast
- Thread-safe tool registry: `CacheMutex` protects `Tools`, `ToolsByName`, `CachedToolSchemas`, `bCacheValid`
- Opt-in via `bEnableNativeMCP` project setting (default: off)
- Capability token validation mirrors WebSocket bridge (`McpConnectionManager.cpp`)

### New Files

| File | Purpose |
|------|---------|
| `Private/MCP/McpNativeTransport.h/cpp` | Raw-socket HTTP+SSE server, session management, JSON-RPC dispatch |
| `Private/MCP/McpJsonRpc.h/cpp` | JSON-RPC 2.0 helpers (parse, response, error, notification, progress) |
| `Private/MCP/McpToolRegistry.h/cpp` | Singleton registry for self-describing C++ tool definitions |
| `Private/MCP/McpSchemaBuilder.h/cpp` | Fluent builder for MCP tool inputSchema JSON |
| `Private/MCP/McpDynamicToolManager.h/cpp` | Runtime tool enable/disable, protected tools, initial state reset |
| `Private/MCP/Tools/McpTool_*.cpp` | Native self-describing tool definition classes with schema + dispatch |
| `Private/UI/SMcpStatusBarWidget.h/cpp` | Editor status bar MCP indicator |
| `Resources/MCP/server-info.json` | Server name, version, default instructions |

---

## [0.1.4] - 2026-04-03

### Security
- Command injection fixes in bump-version action and editor tools with mixed-context sanitization (#327, #322)
- Path traversal fixes in `export_level` action and screenshot filenames (#305)
- Replaced synchronous file operations with async to prevent blocking (#318)

### Added
- Custom content mount points via `MCP_ADDITIONAL_PATH_PREFIXES` environment variable (#326)
- New `manage_project_settings` tool for runtime project configuration
- Audio authoring capabilities: sound wave creation, sound cues, MetaSounds, attenuation settings
- Success flags in audio asset creation responses
- Optional plugin dependencies: IKRig, ChaosVehiclesPlugin, AnimationData

### Fixed
- UE 5.0 API incompatibilities in IK Rig and widget authoring
- Crash when deleting animation/rig assets on UE 5.7+ (9ea2db4)
- Folder deletion crashes with safe deletion implementation (f0f4e44, ed56353)
- Widget creation crash (#306)
- Asset loading reliability for newly created AI assets (bb5e3bb)
- Asset query parameter bugs and expanded classNames support (#311)
- Replaced custom asset directory checks with `UEditorAssetLibrary` to avoid stale cache
- Fixed searchText filtering in `search_assets` action (4b1cb0e)
- Unified pin serialization across blueprint graph handlers (#309, 10f8f2b)
- Actor lookup to match subsystem behavior (checks both label and name)
- Console command settings delegated to C++ handler for performance
- Delay-load for optional plugin modules to prevent missing dependency errors (#317)
- IK retargeter initialization using controller API (UE 5.7+) with backward compatibility
- Rate limiting defaults and missing GraphQL heading in docs (d023284)
- `get_ai_info` schema alignment (#310)

### Dependencies
- `github/codeql-action` 4.33.0 → 4.34.1
- `picomatch` 4.0.3 → 4.0.4

---

## [0.1.3] - 2026-03-21

### Security
- Path traversal fix in `export_asset` action to prevent directory traversal attacks

### Added
- External actors support for World Partition in level structure handlers
- Streaming reference creation for external actor packages

### Fixed
- UE 5.0 compatibility using `bIsWorldInitialized` direct access
- Tick task manager crashes during world operations with proper cleanup
- World cleanup issues with `FlushRenderingCommands` safety
- Sublevel creation process with enhanced path handling
- Missing includes for UE 5.7 build (contributed by @a2448825647)

### Changed
- Enhanced `McpAutomationBridgeHelpers.h` with additional safety helpers
- Improved `McpSafeOperations.h` for safer world operations

---

## [0.1.2] - 2026-03-18

### Security
- Command injection prevention via semicolon sanitization in all user inputs
- Path traversal fixes in validateSnapshotPath and asset handlers
- Blueprint creation savePath sanitization to prevent traversal attacks

### Added
- `McpAutomationBridge_ConsoleCommandHandlers.cpp` - Batch and single command execution (302 lines)
- `McpHandlerUtils.h/cpp` - Standardized JSON response builders (1,900 lines)
- `McpPropertyReflection.h/cpp` - Property reflection utilities (1,356 lines)
- `McpSafeOperations.h` - Safe asset/level save for UE 5.7 (659 lines)
- `McpVersionCompatibility.h` - UE 5.0-5.7 API compatibility macros (225 lines)
- `McpHandlerDeclarations.h` - Forward declarations (844 lines)
- Debug visualization shapes for better testing feedback
- `list_objects`, `set_property`, `get_property` actions to control handlers

### Fixed
- EditorFunctionHandlers: use-after-free bug
- EffectHandlers: truncated condition + missing braces
- InventoryHandlers: duplicate TArray with undefined variables
- MaterialAuthoringHandlers: duplicate include + missing UE 5.0 fallback
- NavigationHandlers: case-sensitivity error
- SkeletonHandlers: duplicate verification + redundant code + duplicate parsing
- WidgetAuthoringHandlers: unreachable code block
- Volume attachment to movable actors by checking mobility
- World memory leaks in UE 5.7 by properly cleaning up created worlds
- Texture property modification errors using PreEditChange/PostEditChange lifecycle
- Blueprint loading to properly find in-memory blueprints first
- Level save/load operations for correct package name matching
- GeometryScript AppendCapsule segment steps for UE 5.5+ compatibility

### Changed
- Complete deep-level refactoring of 57 handler files with line-by-line review
- Centralized utility infrastructure for consistent error handling
- UE 5.0-5.7 cross-version compatibility with API abstraction macros
- All handlers now use standardized response builders

### Compatibility
- Unreal Engine 5.0 - 5.7
- Platforms: Win64, Mac, Linux

---

## [0.1.1] - 2026-02-16

### Added
- 200+ automation action handlers across all domains (AI, Combat, Character, Inventory, GAS, Audio, Materials, Textures, Levels, Volumes, Performance, Input)
- Progress heartbeat protocol for long-running operations
- Dynamic tool management via `manage_tools` MCP tool
- IPv6 support with hostname resolution and zone ID handling
- TLS/SSL support for secure WebSocket connections
- Per-connection rate limiting (600 messages/min, 120 automation requests/min)
- Handler verification metadata in responses (actor/asset/component identity)

### Security
- Path validation helpers: `SanitizeProjectRelativePath`, `SanitizeProjectFilePath`, `ValidateAssetCreationPath`
- Input sanitization for asset names and paths
- Loopback-only binding by default
- Handshake required before automation requests
- Command validation blocks dangerous console commands

### Fixed
- Landscape handler silent fallback bug (now returns `LANDSCAPE_NOT_FOUND` error)
- Rotation yaw bug in lighting handlers
- Integer overflow in heightmap operations (int16 → int32)
- Intel GPU crash prevention with `McpSafeLevelSave` helper
- UE 5.7 compatibility (GetProtocolType API, SCS save, Niagara graph init)

### Compatibility
- Unreal Engine 5.0 - 5.7
- Platforms: Win64, Mac, Linux

---

## [0.1.0] - 2025-12-01

### Added
- Initial release
- WebSocket-based automation bridge
- Core automation handlers for assets, actors, levels
- Blueprint graph editing support
- Niagara authoring support
- Animation and physics handlers

---

For full MCP server changelog, see: https://github.com/ChiR24/Unreal_mcp/blob/main/CHANGELOG.md
