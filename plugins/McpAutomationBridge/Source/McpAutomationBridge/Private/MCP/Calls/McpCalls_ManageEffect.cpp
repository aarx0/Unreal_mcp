// LINT-TOOL: manage_effect
// LINT-SCHEMA-DERIVED
// manage_effect as FMcpCall classes — seventh classed family
// (docs/action-declarations.md). Adopts schema-from-decls: each class AUTHORS
// its schema fragment in a S_<Suffix>() function; the published facade schema
// folds those fragments and GetDecl() derives the validation decl from the same
// fragment via McpDeriveDecl(), so schema and decl are one source and cannot
// drift. Run() delegates to the subsystem member handlers until the module
// split de-members those bodies: the effect actions live in EffectHandlers.cpp
// (HandleEffect*), the Niagara authoring actions delegate to
// HandleManageNiagaraAuthoringAction (NiagaraAuthoringHandlers.cpp) under its
// manage_niagara_authoring gate literal, and the three graph actions delegate
// to HandleNiagaraGraphAction (NiagaraGraphHandlers.cpp) after rewriting
// subAction to that handler's internal add_module/connect_pins/remove_node
// spellings.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageEffect
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action's
// handler reads (the fold dedups shared params to one entry). Descriptions are
// the tool's authored help text; McpDeriveDecl() reads the param kinds +
// required-set back out of these to build the transport validation decl.
// One-of pairs the decl vocabulary cannot express stay handler-enforced with
// each member optional: create_particle_trail's systemPath/emitter and
// get_niagara_info's assetPath/systemPath. The retired shims also declared
// params no branch reads (a moduleName on every authoring action — an OUTPUT
// field only; start/end/width on create_niagara_ribbon; burstInterval,
// killOnLifetime, force, sizeScale, startColor/endColor, ribbonWidth,
// lightIntensity, offset, numBands, traceChannel, maxEventsPerFrame,
// sourceEventName/sourceEmitterName, payloadVariables, enabled, numIterations);
// those are dropped. rotation (particle/spawn_niagara) and sourceNodeId/
// targetNodeId (connect_niagara_pins) are read by their branch but the shim
// list omitted a schema description, so they are authored here.

// Effect actions (EffectHandlers.cpp)

static void S_ListDebugShapes(FMcpSchemaBuilder&) {}
static void S_ClearDebugShapes(FMcpSchemaBuilder&) {}

static void S_DebugShape(FMcpSchemaBuilder& B)
{
	B.String(TEXT("shapeType"), TEXT(""))
	 .String(TEXT("shape"), TEXT(""))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Number(TEXT("duration"), TEXT(""))
	 .Number(TEXT("radius"), TEXT(""))
	 .Number(TEXT("size"), TEXT("debug_shape/particle: shape size (fallback for radius)."))
	 .Number(TEXT("thickness"), TEXT("debug_shape/particle: line/box drawing thickness."))
	 .Array(TEXT("color"), TEXT(""), TEXT("number"))
	 .Object(TEXT("endLocation"), TEXT("debug_shape/particle: end point for line/cylinder/arrow shapes (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Required({TEXT("location")});
}

static void S_Particle(FMcpSchemaBuilder& B)
{
	B.String(TEXT("preset"), TEXT("Particle preset or asset path."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Array(TEXT("rotation"), TEXT("particle/spawn_niagara: spawn rotation as [pitch, yaw, roll]."), TEXT("number"))
	 .Number(TEXT("scale"), TEXT("particle/spawn_niagara: uniform scale (also accepts an array)."))
	 .Bool(TEXT("autoDestroy"), TEXT("particle/spawn_niagara: destroy the spawned actor once its effect finishes."))
	 .Number(TEXT("duration"), TEXT(""))
	 .Number(TEXT("size"), TEXT("debug_shape/particle: shape size (fallback for radius)."))
	 .Number(TEXT("thickness"), TEXT("debug_shape/particle: line/box drawing thickness."))
	 .Array(TEXT("color"), TEXT(""), TEXT("number"))
	 .String(TEXT("shapeType"), TEXT(""))
	 .Array(TEXT("boxSize"), TEXT("particle: box shape dimensions [x, y, z]."), TEXT("number"))
	 .Object(TEXT("endLocation"), TEXT("debug_shape/particle: end point for line/cylinder/arrow shapes (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Object(TEXT("direction"), TEXT("particle: cone shape direction (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Number(TEXT("length"), TEXT("particle: cone shape length."))
	 .Number(TEXT("angle"), TEXT("particle: cone shape angle in degrees."))
	 .Number(TEXT("halfHeight"), TEXT("particle: capsule shape half-height."))
	 .Required({TEXT("preset")});
}

static void S_SetNiagaraParameter(FMcpSchemaBuilder& B)
{
	B.String(TEXT("systemName"), TEXT("Niagara actor/system label."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .String(TEXT("parameterType"), TEXT("Float | Vector | Color | Bool -- selects which typed value field is read."))
	 // parameterType is the discriminant; supply the matching typed field.
	 .Number(TEXT("floatValue"), TEXT("Value when parameterType=Float."))
	 .Object(TEXT("vectorValue"), TEXT("Value when parameterType=Vector (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("colorValue"), TEXT("Value when parameterType=Color (r, g, b, a, 0..1)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Bool(TEXT("boolValue"), TEXT("Value when parameterType=Bool."))
	 .Required({TEXT("parameterName")});
}

static void S_AdvanceSimulation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("systemName"), TEXT("Niagara actor/system label."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Number(TEXT("deltaTime"), TEXT("Simulation delta time."))
	 .Integer(TEXT("steps"), TEXT("Simulation step count."));
}

static void S_CreateDynamicLight(FMcpSchemaBuilder& B)
{
	B.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .String(TEXT("lightName"), TEXT("create_dynamic_light: actor label for the spawned light."))
	 .String(TEXT("lightType"), TEXT(""))
	 .Number(TEXT("intensity"), TEXT(""))
	 .Array(TEXT("color"), TEXT(""), TEXT("number"))
	 .Object(TEXT("pulse"), TEXT("create_dynamic_light: pulsing light tag (enabled, frequency)."),
		[](FMcpSchemaBuilder& S) {
		S.Bool(TEXT("enabled")).Number(TEXT("frequency"));
	})
	 .Required({TEXT("location")});
}

static void S_Cleanup(FMcpSchemaBuilder& B)
{
	B.String(TEXT("filter"), TEXT("Cleanup actor-label prefix filter."));
}

static void S_SpawnNiagara(FMcpSchemaBuilder& B)
{
	B.String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Array(TEXT("rotation"), TEXT("particle/spawn_niagara: spawn rotation as [pitch, yaw, roll]."), TEXT("number"))
	 .Number(TEXT("scale"), TEXT("particle/spawn_niagara: uniform scale (also accepts an array)."))
	 .Bool(TEXT("autoDestroy"), TEXT("particle/spawn_niagara: destroy the spawned actor once its effect finishes."))
	 .String(TEXT("attachToActor"), TEXT("Actor label to attach spawned Niagara actor to."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Required({TEXT("systemPath")});
}

static void S_CreateVolumetricFog(FMcpSchemaBuilder& B)
{
	B.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Number(TEXT("density"), TEXT(""))
	 .Number(TEXT("scattering"), TEXT(""))
	 .Number(TEXT("extinction"), TEXT(""))
	 .String(TEXT("name"), TEXT("Name identifier."));
}

static void S_CreateParticleTrail(FMcpSchemaBuilder& B)
{
	B.String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitter"), TEXT("Emitter name alias."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."));
}

static void S_CreateEnvironmentEffect(FMcpSchemaBuilder& B)
{
	B.String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Required({TEXT("systemPath")});
}

static void S_CreateImpactEffect(FMcpSchemaBuilder& B)
{
	B.String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Required({TEXT("systemPath")});
}

static void S_CreateNiagaraRibbon(FMcpSchemaBuilder& B)
{
	B.String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .Array(TEXT("color"), TEXT(""), TEXT("number"))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .String(TEXT("actorName"), TEXT("Name of the actor."))
	 .Required({TEXT("systemPath")});
}

static void S_Activate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("systemName"), TEXT("Niagara actor/system label."))
	 .Bool(TEXT("reset"), TEXT(""));
}

static void S_Deactivate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("systemName"), TEXT("Niagara actor/system label."))
	 .String(TEXT("actorName"), TEXT("Name of the actor."));
}

// Niagara graph actions (NiagaraGraphHandlers.cpp — requires assetPath itself)

static void S_AddNiagaraModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .String(TEXT("scriptType"), TEXT("Niagara script target, e.g. Spawn or Update."))
	 .String(TEXT("modulePath"), TEXT("Niagara module script asset path."))
	 .Required({TEXT("assetPath")});
}

static void S_ConnectNiagaraPins(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .String(TEXT("scriptType"), TEXT("Niagara script target, e.g. Spawn or Update."))
	 .Bool(TEXT("autoConnect"), TEXT("Automatically connect a compatible Niagara graph pin pair."))
	 .String(TEXT("sourceNodeId"), TEXT("connect_niagara_pins: GUID of the source node (paired with sourcePinName)."))
	 .String(TEXT("sourcePinName"), TEXT("connect_niagara_pins: name of the source (output) pin."))
	 .String(TEXT("targetNodeId"), TEXT("connect_niagara_pins: GUID of the target node (paired with targetPinName)."))
	 .String(TEXT("targetPinName"), TEXT("connect_niagara_pins: name of the target (input) pin."))
	 .Required({TEXT("assetPath")});
}

static void S_RemoveNiagaraNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .String(TEXT("scriptType"), TEXT("Niagara script target, e.g. Spawn or Update."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Required({TEXT("assetPath")});
}

// Niagara authoring actions (NiagaraAuthoringHandlers.cpp). The monolith reads a
// common param block for every branch (name/path/savePath/assetPath/systemPath/
// emitterPath/emitterName/save); required flags follow each branch's own
// enforcement.

static void S_CreateNiagaraSystem(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Required({TEXT("name")});
}

static void S_CreateNiagaraEmitter(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Required({TEXT("name")});
}

static void S_AddEmitterToSystem(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Required({TEXT("systemPath"), TEXT("emitterPath")});
}

static void S_SetEmitterProperties(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Object(TEXT("emitterProperties"), TEXT("Emitter properties to update."))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddSpawnRateModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Number(TEXT("spawnRate"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddSpawnBurstModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Number(TEXT("burstCount"), TEXT(""))
	 .Number(TEXT("burstTime"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddSpawnPerUnitModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Number(TEXT("spawnPerUnit"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddInitializeParticleModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Number(TEXT("lifetime"), TEXT(""))
	 .Number(TEXT("mass"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddParticleStateModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddForceModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("forceType"), TEXT(""))
	 .Number(TEXT("forceStrength"), TEXT(""))
	 .Object(TEXT("forceVector"), TEXT("3D force vector (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddVelocityModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Object(TEXT("velocity"), TEXT("3D velocity vector (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .String(TEXT("velocityMode"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddAccelerationModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Object(TEXT("acceleration"), TEXT("3D acceleration vector (x, y, z)."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
	})
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddSizeModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("sizeMode"), TEXT(""))
	 .Number(TEXT("uniformSize"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddColorModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Array(TEXT("color"), TEXT(""), TEXT("number"))
	 .String(TEXT("colorMode"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddSpriteRendererModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."))
	 .String(TEXT("alignment"), TEXT("Sprite alignment mode."))
	 .String(TEXT("facingMode"), TEXT("Sprite facing mode."))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddMeshRendererModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("meshPath"), TEXT("Mesh asset path."))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddRibbonRendererModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddLightRendererModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Number(TEXT("lightRadius"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddCollisionModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("collisionMode"), TEXT(""))
	 .Number(TEXT("restitution"), TEXT(""))
	 .Number(TEXT("friction"), TEXT(""))
	 .Bool(TEXT("dieOnCollision"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddKillParticlesModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("killCondition"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddCameraOffsetModule(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Number(TEXT("cameraOffset"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddUserParameter(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .String(TEXT("parameterType"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("parameterName")});
}

static void S_SetParameterValue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 // Typed value; the Niagara user-parameter's declared type selects which field is read.
	 .Number(TEXT("floatValue"), TEXT("Value for a Float/Int Niagara parameter."))
	 .Bool(TEXT("boolValue"), TEXT("Value for a Bool Niagara parameter."))
	 .Object(TEXT("vectorValue"), TEXT("Value for a Vector Niagara parameter (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Required({TEXT("systemPath"), TEXT("parameterName")});
}

static void S_BindParameterToSource(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .String(TEXT("sourceBinding"), TEXT("Niagara source binding, e.g. Emitter.Age."))
	 .String(TEXT("parameterType"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("parameterName"), TEXT("sourceBinding")});
}

static void S_AddSkeletalMeshDataInterface(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("skeletalMeshPath"), TEXT("add_skeletal_mesh_data_interface: skeletal mesh asset path."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddStaticMeshDataInterface(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("staticMeshPath"), TEXT("add_static_mesh_data_interface: static mesh asset path."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddSplineDataInterface(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddAudioSpectrumDataInterface(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddCollisionQueryDataInterface(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("parameterName"), TEXT("Name of the parameter."))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddEventGenerator(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("eventName"), TEXT("Name of the event."))
	 .String(TEXT("eventType"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName"), TEXT("eventName")});
}

static void S_AddEventReceiver(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("eventName"), TEXT("Name of the event."))
	 .Bool(TEXT("spawnOnEvent"), TEXT(""))
	 .Number(TEXT("eventSpawnCount"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName"), TEXT("eventName")});
}


static void S_EnableGpuSimulation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Bool(TEXT("fixedBoundsEnabled"), TEXT(""))
	 .Bool(TEXT("deterministicEnabled"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName")});
}

static void S_AddSimulationStage(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .String(TEXT("stageName"), TEXT(""))
	 .String(TEXT("stageIterationSource"), TEXT(""))
	 .Required({TEXT("systemPath"), TEXT("emitterName"), TEXT("stageName")});
}

static void S_GetNiagaraInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."));
}

static void S_ValidateNiagaraSystem(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
	 .String(TEXT("systemPath"), TEXT("Niagara system asset path."))
	 .String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
	 .String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
	 .Bool(TEXT("save"), TEXT("Whether to save modified assets."))
	 .Required({TEXT("systemPath")});
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row (all three implementation TUs are
// editor-gated). Mutating on the 52 writers; the readers are get_niagara_info,
// validate_niagara_system, list_debug_shapes, and the transient debug-draw
// trio (debug_shape, particle, clear_debug_shapes).

#define MCP_ME_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                     \
class FMcpCall_ManageEffect_##ClassSuffix final : public FMcpCall                          \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_effect"),               \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),            \
			&S_##ClassSuffix);                                                            \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return S.HandlerFn(RequestId, Payload, Socket);                                   \
	}                                                                                     \
};

// Niagara authoring delegates: the monolith gates on the
// manage_niagara_authoring literal and dispatches on the payload's subAction,
// which the transport mirrors from action (McpNativeTransport.cpp).
#define MCP_ME_NA_CALL(ClassSuffix, ActionLiteral, ExtraFlags)                             \
class FMcpCall_ManageEffect_##ClassSuffix final : public FMcpCall                          \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_effect"),               \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),            \
			&S_##ClassSuffix);                                                            \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		return S.HandleManageNiagaraAuthoringAction(                                      \
			RequestId, TEXT("manage_niagara_authoring"), Payload, Socket);                \
	}                                                                                     \
};

// Niagara graph delegates: the graph handler gates on the manage_niagara_graph
// literal and dispatches on subAction under its internal spellings, so the
// advertised add_niagara_*/connect_*/remove_* names rewrite subAction first.
#define MCP_ME_GRAPH_CALL(ClassSuffix, ActionLiteral, SubActionLiteral, ExtraFlags)        \
class FMcpCall_ManageEffect_##ClassSuffix final : public FMcpCall                          \
{                                                                                         \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_effect"),               \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),            \
			&S_##ClassSuffix);                                                            \
		return D;                                                                         \
	}                                                                                     \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                  \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override  \
	{                                                                                     \
		Payload->SetStringField(TEXT("subAction"), TEXT(SubActionLiteral));               \
		return S.HandleNiagaraGraphAction(RequestId, TEXT("manage_niagara_graph"),        \
		                                  Payload, Socket);                               \
	}                                                                                     \
};

// Effect members (EffectHandlers.cpp)
MCP_ME_CALL(ListDebugShapes, "list_debug_shapes", HandleEffectListDebugShapes, EMcpCallFlags::None)
MCP_ME_CALL(ClearDebugShapes, "clear_debug_shapes", HandleEffectClearDebugShapes, EMcpCallFlags::None)
MCP_ME_CALL(DebugShape, "debug_shape", HandleEffectDebugShape, EMcpCallFlags::None)
MCP_ME_CALL(Particle, "particle", HandleEffectParticle, EMcpCallFlags::None)
MCP_ME_CALL(SetNiagaraParameter, "set_niagara_parameter", HandleEffectSetNiagaraParameter, EMcpCallFlags::Mutating)
MCP_ME_CALL(AdvanceSimulation, "advance_simulation", HandleEffectAdvanceSimulation, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateDynamicLight, "create_dynamic_light", HandleEffectCreateDynamicLight, EMcpCallFlags::Mutating)
MCP_ME_CALL(Cleanup, "cleanup", HandleEffectCleanup, EMcpCallFlags::Mutating)
MCP_ME_CALL(SpawnNiagara, "spawn_niagara", HandleEffectSpawnNiagara, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateVolumetricFog, "create_volumetric_fog", HandleEffectCreateVolumetricFog, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateParticleTrail, "create_particle_trail", HandleEffectCreateParticleTrail, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateEnvironmentEffect, "create_environment_effect", HandleEffectCreateEnvironmentEffect, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateImpactEffect, "create_impact_effect", HandleEffectCreateImpactEffect, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateNiagaraRibbon, "create_niagara_ribbon", HandleEffectCreateNiagaraRibbon, EMcpCallFlags::Mutating)

MCP_ME_CALL(Activate, "activate", HandleEffectActivateNiagara, EMcpCallFlags::Mutating)
MCP_ME_CALL(Deactivate, "deactivate", HandleEffectDeactivateNiagara, EMcpCallFlags::Mutating)

// Niagara graph actions (NiagaraGraphHandlers.cpp)
MCP_ME_GRAPH_CALL(AddNiagaraModule, "add_niagara_module", "add_module", EMcpCallFlags::Mutating)
MCP_ME_GRAPH_CALL(ConnectNiagaraPins, "connect_niagara_pins", "connect_pins", EMcpCallFlags::Mutating)
MCP_ME_GRAPH_CALL(RemoveNiagaraNode, "remove_niagara_node", "remove_node", EMcpCallFlags::Mutating)

// Niagara authoring actions (NiagaraAuthoringHandlers.cpp)
MCP_ME_NA_CALL(CreateNiagaraSystem, "create_niagara_system", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(CreateNiagaraEmitter, "create_niagara_emitter", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddEmitterToSystem, "add_emitter_to_system", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(SetEmitterProperties, "set_emitter_properties", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSpawnRateModule, "add_spawn_rate_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSpawnBurstModule, "add_spawn_burst_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSpawnPerUnitModule, "add_spawn_per_unit_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddInitializeParticleModule, "add_initialize_particle_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddParticleStateModule, "add_particle_state_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddForceModule, "add_force_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddVelocityModule, "add_velocity_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddAccelerationModule, "add_acceleration_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSizeModule, "add_size_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddColorModule, "add_color_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSpriteRendererModule, "add_sprite_renderer_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddMeshRendererModule, "add_mesh_renderer_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddRibbonRendererModule, "add_ribbon_renderer_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddLightRendererModule, "add_light_renderer_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddCollisionModule, "add_collision_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddKillParticlesModule, "add_kill_particles_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddCameraOffsetModule, "add_camera_offset_module", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddUserParameter, "add_user_parameter", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(SetParameterValue, "set_parameter_value", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(BindParameterToSource, "bind_parameter_to_source", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSkeletalMeshDataInterface, "add_skeletal_mesh_data_interface", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddStaticMeshDataInterface, "add_static_mesh_data_interface", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSplineDataInterface, "add_spline_data_interface", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddAudioSpectrumDataInterface, "add_audio_spectrum_data_interface", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddCollisionQueryDataInterface, "add_collision_query_data_interface", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddEventGenerator, "add_event_generator", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddEventReceiver, "add_event_receiver", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(EnableGpuSimulation, "enable_gpu_simulation", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSimulationStage, "add_simulation_stage", EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(GetNiagaraInfo, "get_niagara_info", EMcpCallFlags::None)
MCP_ME_NA_CALL(ValidateNiagaraSystem, "validate_niagara_system", EMcpCallFlags::None)

#undef MCP_ME_CALL
#undef MCP_ME_NA_CALL
#undef MCP_ME_GRAPH_CALL

} // namespace McpCalls::ManageEffect

void McpRegisterManageEffectCalls()
{
	using namespace McpCalls::ManageEffect;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_ListDebugShapes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_ClearDebugShapes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_DebugShape>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_Particle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_SetNiagaraParameter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AdvanceSimulation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_CreateDynamicLight>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_Cleanup>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_SpawnNiagara>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_CreateVolumetricFog>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_CreateParticleTrail>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_CreateEnvironmentEffect>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_CreateImpactEffect>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_CreateNiagaraRibbon>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_Activate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_Deactivate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddNiagaraModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_ConnectNiagaraPins>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_RemoveNiagaraNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_CreateNiagaraSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_CreateNiagaraEmitter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddEmitterToSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_SetEmitterProperties>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddSpawnRateModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddSpawnBurstModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddSpawnPerUnitModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddInitializeParticleModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddParticleStateModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddForceModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddVelocityModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddAccelerationModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddSizeModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddColorModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddSpriteRendererModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddMeshRendererModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddRibbonRendererModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddLightRendererModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddCollisionModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddKillParticlesModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddCameraOffsetModule>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddUserParameter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_SetParameterValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_BindParameterToSource>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddSkeletalMeshDataInterface>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddStaticMeshDataInterface>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddSplineDataInterface>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddAudioSpectrumDataInterface>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddCollisionQueryDataInterface>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddEventGenerator>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddEventReceiver>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_EnableGpuSimulation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddSimulationStage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_GetNiagaraInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_ValidateNiagaraSystem>());
}
