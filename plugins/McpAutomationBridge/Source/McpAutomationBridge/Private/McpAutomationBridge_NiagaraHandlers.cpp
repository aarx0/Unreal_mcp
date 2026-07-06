// =============================================================================
// McpAutomationBridge_NiagaraHandlers.cpp
// =============================================================================
// Handler implementations for Niagara particle system creation and manipulation.
//
// HANDLERS IMPLEMENTED:
// ---------------------
// create_niagara_system:
//   - Create new Niagara System asset with default emitter
//   - Initialize system spawn/update scripts with graph sources
//   - Create NiagaraGraph for visual editing
//
// create_niagara_emitter:
//   - Create standalone Niagara Emitter asset
//   - Initialize with GraphSource for editor compatibility
//
// spawn_niagara_actor:
//   - Spawn NiagaraActor in world with specified system
//   - Set asset reference on NiagaraComponent
//
// modify_niagara_parameter:
//   - Set Float/Vector/Color/Bool parameters on NiagaraComponent
//   - Find actor by name in editor world
//
// create_niagara_ribbon:
//   - Spawn NiagaraActor configured for ribbon/beam effects
//   - Set ribbon start/end/width/color parameters
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0: GraphSource directly on UNiagaraEmitter
//         AddEmitterHandle takes 2 parameters
// UE 5.1+: GraphSource on FVersionedNiagaraEmitterData via GetLatestEmitterData()
//          AddEmitterHandle takes 3 parameters (with FGuid)
//
// SECURITY:
// ---------
// - Niagara plugin module availability checked before operations
// - Asset paths validated through standard UE loading
// =============================================================================

// =============================================================================
// Version Compatibility Header (MUST BE FIRST)
// =============================================================================
#include "McpVersionCompatibility.h"

// =============================================================================
// Core Headers
// =============================================================================
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"

// =============================================================================
// Editor-Only Headers
// =============================================================================
#if WITH_EDITOR

// Asset Management
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Async/Async.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"

// Niagara Core
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraSystem.h"

// Niagara Graph
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "UObject/Package.h"

// Editor Subsystems (version-dependent header location)
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

// Niagara Stack Utilities (optional)
#if __has_include("ViewModels/Stack/NiagaraStackGraphUtilities.h")
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

#if __has_include("NiagaraEmitterFactoryNew.h") && !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
#include "NiagaraEmitterFactoryNew.h"
#define MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW 1
#else
#define MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW 0
#endif

#if __has_include("NiagaraSystemFactoryNew.h") && !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
#include "NiagaraSystemFactoryNew.h"
#define MCP_HAS_NIAGARA_SYSTEM_FACTORY_NEW 1
#else
#define MCP_HAS_NIAGARA_SYSTEM_FACTORY_NEW 0
#endif

#endif // WITH_EDITOR

// =============================================================================
// Handler Implementations
// =============================================================================

/**
 * @brief Creates a new Niagara System asset.
 *
 * @param RequestId Request identifier.
 * @param Action Action name ("create_niagara_system").
 * @param Payload JSON payload with name, savePath.
 * @param RequestingSocket WebSocket for response.
 * @return true if handled.
 */

/**
 * @brief Creates a new Niagara Emitter asset.
 *
 * @param RequestId Request identifier.
 * @param Action Action name ("create_niagara_emitter").
 * @param Payload JSON payload with name, savePath.
 * @param RequestingSocket WebSocket for response.
 * @return true if handled.
 */

/**
 * @brief Spawns a NiagaraActor with specified system.
 *
 * @param RequestId Request identifier.
 * @param Action Action name ("spawn_niagara_actor").
 * @param Payload JSON payload with systemPath, location, name.
 * @param RequestingSocket WebSocket for response.
 * @return true if handled.
 */

/**
 * @brief Modifies a parameter on a Niagara component.
 *
 * @param RequestId Request identifier.
 * @param Action Action name ("modify_niagara_parameter").
 * @param Payload JSON payload with actorName, parameterName, parameterType, value.
 * @param RequestingSocket WebSocket for response.
 * @return true if handled.
 */

/**
 * @brief Creates a Niagara ribbon/beam effect.
 *
 * @param RequestId Request identifier.
 * @param Action Action name ("create_niagara_ribbon").
 * @param Payload JSON payload with systemPath, name, start, end, width, color.
 * @param RequestingSocket WebSocket for response.
 * @return true if handled.
 */
