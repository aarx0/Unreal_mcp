// LINT-TOOL: manage_effect
// manage_effect as FMcpCall classes — seventh classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers until the module split de-members those bodies: the 16
// effect actions live in EffectHandlers.cpp (HandleEffect*), the 36 Niagara
// authoring actions delegate to HandleManageNiagaraAuthoringAction
// (NiagaraAuthoringHandlers.cpp) under its manage_niagara_authoring gate
// literal, and the three graph actions delegate to HandleNiagaraGraphAction
// (NiagaraGraphHandlers.cpp) after rewriting subAction to that handler's
// internal add_module/connect_pins/remove_node spellings.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageEffect
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations, re-verified against the
// live NiagaraAuthoringHandlers/NiagaraGraphHandlers branches and the extracted
// EffectHandlers bodies, except: create_niagara_system's shim row was
// mis-authored with a set_niagara_parameter-shaped contract (required
// parameterName); the live branch requires name and reads the same common
// param block as create_niagara_emitter, so both share P_CreateNiagaraAsset.
// activate/activate_effect share activate_niagara's contract and handler;
// deactivate shares deactivate_niagara's (the extracted members ignore the
// action spelling). One-of pairs the decl vocabulary cannot express stay
// handler-enforced with each member optional: create_particle_trail's
// systemPath/emitter and get_niagara_info's assetPath/systemPath.

inline const FMcpParamDecl P_ActivateNiagara[] = { { TEXT("systemName"), EMcpParamKind::String, false }, { TEXT("reset"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_DeactivateNiagara[] = { { TEXT("systemName"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AdvanceSimulation[] = { { TEXT("systemName"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false }, { TEXT("deltaTime"), EMcpParamKind::Number, false }, { TEXT("steps"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_Cleanup[] = { { TEXT("filter"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateDynamicLight[] = { { TEXT("location"), EMcpParamKind::Any, true }, { TEXT("lightName"), EMcpParamKind::String, false }, { TEXT("lightType"), EMcpParamKind::String, false }, { TEXT("intensity"), EMcpParamKind::Number, false }, { TEXT("color"), EMcpParamKind::Any, false }, { TEXT("pulse"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_CreateFromSystem[] = { { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Any, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateNiagaraRibbon[] = { { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("start"), EMcpParamKind::Object, false }, { TEXT("end"), EMcpParamKind::Object, false }, { TEXT("width"), EMcpParamKind::Number, false }, { TEXT("color"), EMcpParamKind::Any, false }, { TEXT("location"), EMcpParamKind::Any, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateParticleTrail[] = { { TEXT("systemPath"), EMcpParamKind::String, false }, { TEXT("emitter"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_CreateVolumetricFog[] = { { TEXT("location"), EMcpParamKind::Array, false }, { TEXT("density"), EMcpParamKind::Number, false }, { TEXT("scattering"), EMcpParamKind::Number, false }, { TEXT("extinction"), EMcpParamKind::Number, false }, { TEXT("name"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_DebugShape[] = { { TEXT("shapeType"), EMcpParamKind::String, false }, { TEXT("shape"), EMcpParamKind::String, false }, { TEXT("location"), EMcpParamKind::Any, true }, { TEXT("duration"), EMcpParamKind::Number, false }, { TEXT("radius"), EMcpParamKind::Number, false }, { TEXT("size"), EMcpParamKind::Number, false }, { TEXT("thickness"), EMcpParamKind::Number, false }, { TEXT("color"), EMcpParamKind::Array, false }, { TEXT("endLocation"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_Particle[] = { { TEXT("preset"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Any, false }, { TEXT("rotation"), EMcpParamKind::Array, false }, { TEXT("scale"), EMcpParamKind::Any, false }, { TEXT("autoDestroy"), EMcpParamKind::Bool, false }, { TEXT("duration"), EMcpParamKind::Number, false }, { TEXT("size"), EMcpParamKind::Number, false }, { TEXT("thickness"), EMcpParamKind::Number, false }, { TEXT("color"), EMcpParamKind::Array, false }, { TEXT("shapeType"), EMcpParamKind::String, false }, { TEXT("boxSize"), EMcpParamKind::Array, false }, { TEXT("endLocation"), EMcpParamKind::Any, false }, { TEXT("direction"), EMcpParamKind::Any, false }, { TEXT("length"), EMcpParamKind::Number, false }, { TEXT("angle"), EMcpParamKind::Number, false }, { TEXT("halfHeight"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetNiagaraParameter[] = { { TEXT("systemName"), EMcpParamKind::String, false }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("parameterType"), EMcpParamKind::String, false }, { TEXT("value"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_SpawnNiagara[] = { { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("location"), EMcpParamKind::Any, false }, { TEXT("rotation"), EMcpParamKind::Array, false }, { TEXT("scale"), EMcpParamKind::Any, false }, { TEXT("autoDestroy"), EMcpParamKind::Bool, false }, { TEXT("attachToActor"), EMcpParamKind::String, false }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("actorName"), EMcpParamKind::String, false } };

// Niagara graph actions (NiagaraGraphHandlers.cpp requires assetPath itself).
inline const FMcpParamDecl P_AddNiagaraModule[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("scriptType"), EMcpParamKind::String, false }, { TEXT("modulePath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConnectNiagaraPins[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("scriptType"), EMcpParamKind::String, false }, { TEXT("autoConnect"), EMcpParamKind::Bool, false }, { TEXT("sourceNodeId"), EMcpParamKind::String, false }, { TEXT("sourcePinName"), EMcpParamKind::String, false }, { TEXT("targetNodeId"), EMcpParamKind::String, false }, { TEXT("targetPinName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_RemoveNiagaraNode[] = { { TEXT("assetPath"), EMcpParamKind::String, true }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("scriptType"), EMcpParamKind::String, false }, { TEXT("nodeId"), EMcpParamKind::String, false } };

// Niagara authoring actions. The monolith reads a common param block for every
// branch (name/path/savePath/assetPath/systemPath/emitterPath/emitterName/
// save); required flags follow each branch's own enforcement.
inline const FMcpParamDecl P_CreateNiagaraAsset[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, false }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddEmitterToSystem[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, true }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetEmitterProperties[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("emitterProperties"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_AddSpawnRateModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("spawnRate"), EMcpParamKind::Number, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddSpawnBurstModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("burstCount"), EMcpParamKind::Number, false }, { TEXT("burstTime"), EMcpParamKind::Number, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("burstInterval"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddSpawnPerUnitModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("spawnPerUnit"), EMcpParamKind::Number, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddInitializeParticleModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("lifetime"), EMcpParamKind::Number, false }, { TEXT("mass"), EMcpParamKind::Number, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddParticleStateModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("killOnLifetime"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddForceModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("forceType"), EMcpParamKind::String, false }, { TEXT("forceStrength"), EMcpParamKind::Number, false }, { TEXT("forceVector"), EMcpParamKind::Object, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("force"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_AddVelocityModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("velocity"), EMcpParamKind::Object, false }, { TEXT("velocityMode"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddAccelerationModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("acceleration"), EMcpParamKind::Any, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddSizeModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("sizeMode"), EMcpParamKind::String, false }, { TEXT("uniformSize"), EMcpParamKind::Number, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("sizeScale"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddColorModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("color"), EMcpParamKind::Object, false }, { TEXT("colorMode"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("startColor"), EMcpParamKind::Array, false }, { TEXT("endColor"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_AddSpriteRendererModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("materialPath"), EMcpParamKind::String, false }, { TEXT("alignment"), EMcpParamKind::String, false }, { TEXT("facingMode"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddMeshRendererModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("meshPath"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddRibbonRendererModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("materialPath"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("ribbonWidth"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddLightRendererModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("lightRadius"), EMcpParamKind::Number, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("lightIntensity"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddCollisionModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("collisionMode"), EMcpParamKind::String, false }, { TEXT("restitution"), EMcpParamKind::Number, false }, { TEXT("friction"), EMcpParamKind::Number, false }, { TEXT("dieOnCollision"), EMcpParamKind::Bool, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddKillParticlesModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("killCondition"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddCameraOffsetModule[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("cameraOffset"), EMcpParamKind::Number, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("offset"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddUserParameter[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("parameterType"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetParameterValue[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("parameterValue"), EMcpParamKind::Any, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_BindParameterToSource[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("parameterName"), EMcpParamKind::String, true }, { TEXT("sourceBinding"), EMcpParamKind::String, true }, { TEXT("parameterType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddSkeletalMeshDataInterface[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("skeletalMeshPath"), EMcpParamKind::String, false }, { TEXT("parameterName"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddStaticMeshDataInterface[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("staticMeshPath"), EMcpParamKind::String, false }, { TEXT("parameterName"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddSplineDataInterface[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("parameterName"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddAudioSpectrumDataInterface[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("parameterName"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("numBands"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddCollisionQueryDataInterface[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("parameterName"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("traceChannel"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_AddEventGenerator[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("eventName"), EMcpParamKind::String, true }, { TEXT("eventType"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("maxEventsPerFrame"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_AddEventReceiver[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("eventName"), EMcpParamKind::String, true }, { TEXT("spawnOnEvent"), EMcpParamKind::Bool, false }, { TEXT("eventSpawnCount"), EMcpParamKind::Number, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("sourceEventName"), EMcpParamKind::String, false }, { TEXT("sourceEmitterName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureEventPayload[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("eventName"), EMcpParamKind::String, true }, { TEXT("eventPayload"), EMcpParamKind::Array, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("payloadVariables"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_EnableGpuSimulation[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("fixedBoundsEnabled"), EMcpParamKind::Bool, false }, { TEXT("deterministicEnabled"), EMcpParamKind::Bool, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("enabled"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_AddSimulationStage[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, true }, { TEXT("save"), EMcpParamKind::Bool, false }, { TEXT("stageName"), EMcpParamKind::String, true }, { TEXT("stageIterationSource"), EMcpParamKind::String, false }, { TEXT("moduleName"), EMcpParamKind::String, false }, { TEXT("numIterations"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_GetNiagaraInfo[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, false }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ValidateNiagaraSystem[] = { { TEXT("name"), EMcpParamKind::String, false }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("savePath"), EMcpParamKind::String, false }, { TEXT("assetPath"), EMcpParamKind::String, false }, { TEXT("systemPath"), EMcpParamKind::String, true }, { TEXT("emitterPath"), EMcpParamKind::String, false }, { TEXT("emitterName"), EMcpParamKind::String, false }, { TEXT("save"), EMcpParamKind::Bool, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row (all three implementation TUs are
// editor-gated). Mutating on the 52 writers; the readers are get_niagara_info,
// validate_niagara_system, list_debug_shapes, and the transient debug-draw
// trio (debug_shape, particle, clear_debug_shapes).

#define MCP_ME_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)        \
class FMcpCall_ManageEffect_##ClassSuffix final : public FMcpCall                          \
{                                                                                         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl Decl{ TEXT("manage_effect"), TEXT(ActionLiteral),       \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                  \
		return Decl;                                                                      \
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
#define MCP_ME_NA_CALL(ClassSuffix, ActionLiteral, ParamsArray, ExtraFlags)                \
class FMcpCall_ManageEffect_##ClassSuffix final : public FMcpCall                          \
{                                                                                         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl Decl{ TEXT("manage_effect"), TEXT(ActionLiteral),       \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                  \
		return Decl;                                                                      \
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
#define MCP_ME_GRAPH_CALL(ClassSuffix, ActionLiteral, ParamsArray, SubActionLiteral, ExtraFlags) \
class FMcpCall_ManageEffect_##ClassSuffix final : public FMcpCall                          \
{                                                                                         \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                     \
		static const FMcpCallDecl Decl{ TEXT("manage_effect"), TEXT(ActionLiteral),       \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                  \
		return Decl;                                                                      \
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
MCP_ME_CALL(ListDebugShapes, "list_debug_shapes", {}, HandleEffectListDebugShapes, EMcpCallFlags::None)
MCP_ME_CALL(ClearDebugShapes, "clear_debug_shapes", {}, HandleEffectClearDebugShapes, EMcpCallFlags::None)
MCP_ME_CALL(DebugShape, "debug_shape", P_DebugShape, HandleEffectDebugShape, EMcpCallFlags::None)
MCP_ME_CALL(Particle, "particle", P_Particle, HandleEffectParticle, EMcpCallFlags::None)
MCP_ME_CALL(SetNiagaraParameter, "set_niagara_parameter", P_SetNiagaraParameter, HandleEffectSetNiagaraParameter, EMcpCallFlags::Mutating)
MCP_ME_CALL(ActivateNiagara, "activate_niagara", P_ActivateNiagara, HandleEffectActivateNiagara, EMcpCallFlags::Mutating)
MCP_ME_CALL(DeactivateNiagara, "deactivate_niagara", P_DeactivateNiagara, HandleEffectDeactivateNiagara, EMcpCallFlags::Mutating)
MCP_ME_CALL(AdvanceSimulation, "advance_simulation", P_AdvanceSimulation, HandleEffectAdvanceSimulation, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateDynamicLight, "create_dynamic_light", P_CreateDynamicLight, HandleEffectCreateDynamicLight, EMcpCallFlags::Mutating)
MCP_ME_CALL(Cleanup, "cleanup", P_Cleanup, HandleEffectCleanup, EMcpCallFlags::Mutating)
MCP_ME_CALL(SpawnNiagara, "spawn_niagara", P_SpawnNiagara, HandleEffectSpawnNiagara, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateVolumetricFog, "create_volumetric_fog", P_CreateVolumetricFog, HandleEffectCreateVolumetricFog, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateParticleTrail, "create_particle_trail", P_CreateParticleTrail, HandleEffectCreateParticleTrail, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateEnvironmentEffect, "create_environment_effect", P_CreateFromSystem, HandleEffectCreateEnvironmentEffect, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateImpactEffect, "create_impact_effect", P_CreateFromSystem, HandleEffectCreateImpactEffect, EMcpCallFlags::Mutating)
MCP_ME_CALL(CreateNiagaraRibbon, "create_niagara_ribbon", P_CreateNiagaraRibbon, HandleEffectCreateNiagaraRibbon, EMcpCallFlags::Mutating)

// Advertised aliases of the activate/deactivate members
MCP_ME_CALL(Activate, "activate", P_ActivateNiagara, HandleEffectActivateNiagara, EMcpCallFlags::Mutating)
MCP_ME_CALL(ActivateEffect, "activate_effect", P_ActivateNiagara, HandleEffectActivateNiagara, EMcpCallFlags::Mutating)
MCP_ME_CALL(Deactivate, "deactivate", P_DeactivateNiagara, HandleEffectDeactivateNiagara, EMcpCallFlags::Mutating)

// Niagara graph actions (NiagaraGraphHandlers.cpp)
MCP_ME_GRAPH_CALL(AddNiagaraModule, "add_niagara_module", P_AddNiagaraModule, "add_module", EMcpCallFlags::Mutating)
MCP_ME_GRAPH_CALL(ConnectNiagaraPins, "connect_niagara_pins", P_ConnectNiagaraPins, "connect_pins", EMcpCallFlags::Mutating)
MCP_ME_GRAPH_CALL(RemoveNiagaraNode, "remove_niagara_node", P_RemoveNiagaraNode, "remove_node", EMcpCallFlags::Mutating)

// Niagara authoring actions (NiagaraAuthoringHandlers.cpp)
MCP_ME_NA_CALL(CreateNiagaraSystem, "create_niagara_system", P_CreateNiagaraAsset, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(CreateNiagaraEmitter, "create_niagara_emitter", P_CreateNiagaraAsset, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddEmitterToSystem, "add_emitter_to_system", P_AddEmitterToSystem, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(SetEmitterProperties, "set_emitter_properties", P_SetEmitterProperties, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSpawnRateModule, "add_spawn_rate_module", P_AddSpawnRateModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSpawnBurstModule, "add_spawn_burst_module", P_AddSpawnBurstModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSpawnPerUnitModule, "add_spawn_per_unit_module", P_AddSpawnPerUnitModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddInitializeParticleModule, "add_initialize_particle_module", P_AddInitializeParticleModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddParticleStateModule, "add_particle_state_module", P_AddParticleStateModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddForceModule, "add_force_module", P_AddForceModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddVelocityModule, "add_velocity_module", P_AddVelocityModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddAccelerationModule, "add_acceleration_module", P_AddAccelerationModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSizeModule, "add_size_module", P_AddSizeModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddColorModule, "add_color_module", P_AddColorModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSpriteRendererModule, "add_sprite_renderer_module", P_AddSpriteRendererModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddMeshRendererModule, "add_mesh_renderer_module", P_AddMeshRendererModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddRibbonRendererModule, "add_ribbon_renderer_module", P_AddRibbonRendererModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddLightRendererModule, "add_light_renderer_module", P_AddLightRendererModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddCollisionModule, "add_collision_module", P_AddCollisionModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddKillParticlesModule, "add_kill_particles_module", P_AddKillParticlesModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddCameraOffsetModule, "add_camera_offset_module", P_AddCameraOffsetModule, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddUserParameter, "add_user_parameter", P_AddUserParameter, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(SetParameterValue, "set_parameter_value", P_SetParameterValue, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(BindParameterToSource, "bind_parameter_to_source", P_BindParameterToSource, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSkeletalMeshDataInterface, "add_skeletal_mesh_data_interface", P_AddSkeletalMeshDataInterface, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddStaticMeshDataInterface, "add_static_mesh_data_interface", P_AddStaticMeshDataInterface, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSplineDataInterface, "add_spline_data_interface", P_AddSplineDataInterface, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddAudioSpectrumDataInterface, "add_audio_spectrum_data_interface", P_AddAudioSpectrumDataInterface, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddCollisionQueryDataInterface, "add_collision_query_data_interface", P_AddCollisionQueryDataInterface, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddEventGenerator, "add_event_generator", P_AddEventGenerator, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddEventReceiver, "add_event_receiver", P_AddEventReceiver, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(ConfigureEventPayload, "configure_event_payload", P_ConfigureEventPayload, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(EnableGpuSimulation, "enable_gpu_simulation", P_EnableGpuSimulation, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(AddSimulationStage, "add_simulation_stage", P_AddSimulationStage, EMcpCallFlags::Mutating)
MCP_ME_NA_CALL(GetNiagaraInfo, "get_niagara_info", P_GetNiagaraInfo, EMcpCallFlags::None)
MCP_ME_NA_CALL(ValidateNiagaraSystem, "validate_niagara_system", P_ValidateNiagaraSystem, EMcpCallFlags::None)

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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_ActivateNiagara>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_DeactivateNiagara>());
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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_ActivateEffect>());
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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_ConfigureEventPayload>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_EnableGpuSimulation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_AddSimulationStage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_GetNiagaraInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageEffect_ValidateNiagaraSystem>());
}
