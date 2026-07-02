// McpTool_ManageEffect.cpp — manage_effect tool definition 

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageEffect : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_effect"); }

	FString GetDescription() const override
	{
		return TEXT("Niagara particle systems, VFX, debug shapes, and GPU simulations. "
			"Create systems, emitters, modules, and control particle effects.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }


	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ManageEffect(), TEXT("Effect/Niagara action to perform."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
			.Number(TEXT("timeoutMs"), TEXT("Client-side request timeout in milliseconds."))
			.Bool(TEXT("save"), TEXT("Whether to save modified assets."))
			.String(TEXT("system"), TEXT("Niagara system asset path alias."))
			.String(TEXT("systemPath"), TEXT("Niagara system asset path."))
			.String(TEXT("systemName"), TEXT("Niagara actor/system label."))
			.String(TEXT("emitter"), TEXT("Emitter name alias."))
			.String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
			.String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
			.FreeformObject(TEXT("emitterProperties"), TEXT("Emitter properties to update."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("attachToActor"), TEXT("Actor label to attach spawned Niagara actor to."))
			.Bool(TEXT("reset"), TEXT(""))
			.String(TEXT("filter"), TEXT("Cleanup actor-label prefix filter."))
			.Number(TEXT("deltaTime"), TEXT("Simulation delta time."))
			.Integer(TEXT("steps"), TEXT("Simulation step count."))
			.String(TEXT("preset"), TEXT("Particle preset or asset path."))
			.String(TEXT("shape"), TEXT(""))
			.String(TEXT("shapeType"), TEXT(""))
			.Number(TEXT("radius"), TEXT(""))
			.Array(TEXT("color"), TEXT(""), TEXT("number"))
			.Number(TEXT("duration"), TEXT(""))
			.String(TEXT("lightType"), TEXT(""))
			.Number(TEXT("intensity"), TEXT(""))
			.Number(TEXT("density"), TEXT(""))
			.Number(TEXT("scattering"), TEXT(""))
			.Number(TEXT("extinction"), TEXT(""))
			.String(TEXT("modulePath"), TEXT("Niagara module script asset path."))
			.String(TEXT("scriptType"), TEXT("Niagara script target, e.g. Spawn or Update."))
			.Bool(TEXT("autoConnect"), TEXT("Automatically connect a compatible Niagara graph pin pair."))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.String(TEXT("parameterName"), TEXT("Name of the parameter."))
			.String(TEXT("parameterType"), TEXT(""))
			.FreeformObject(TEXT("parameterValue"), TEXT("Generic parameter value (any type)."))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.String(TEXT("sourceBinding"), TEXT("Niagara source binding, e.g. Emitter.Age."))
			.Number(TEXT("spawnRate"), TEXT(""))
			.Number(TEXT("burstCount"), TEXT(""))
			.Number(TEXT("burstTime"), TEXT(""))
			.Number(TEXT("spawnPerUnit"), TEXT(""))
			.Number(TEXT("lifetime"), TEXT(""))
			.Number(TEXT("mass"), TEXT(""))
			.String(TEXT("forceType"), TEXT(""))
			.Number(TEXT("forceStrength"), TEXT(""))
			.Object(TEXT("forceVector"), TEXT("3D force vector (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("velocity"), TEXT("3D velocity vector (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("velocityMode"), TEXT(""))
			.Object(TEXT("acceleration"), TEXT("3D acceleration vector (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("sizeMode"), TEXT(""))
			.Number(TEXT("uniformSize"), TEXT(""))
			.String(TEXT("colorMode"), TEXT(""))
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.String(TEXT("alignment"), TEXT("Sprite alignment mode."))
			.String(TEXT("facingMode"), TEXT("Sprite facing mode."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.Number(TEXT("lightRadius"), TEXT(""))
			.String(TEXT("collisionMode"), TEXT(""))
			.Number(TEXT("restitution"), TEXT(""))
			.Number(TEXT("friction"), TEXT(""))
			.Bool(TEXT("dieOnCollision"), TEXT(""))
			.String(TEXT("killCondition"), TEXT(""))
			.Number(TEXT("cameraOffset"), TEXT(""))
			.String(TEXT("eventName"), TEXT("Name of the event."))
			.String(TEXT("eventType"), TEXT(""))
			.Bool(TEXT("spawnOnEvent"), TEXT(""))
			.Number(TEXT("eventSpawnCount"), TEXT(""))
			.ArrayOfObjects(TEXT("eventPayload"), TEXT("Niagara event payload attributes."),
				[](FMcpSchemaBuilder& S) {
				S.String(TEXT("name"), TEXT("Attribute name."))
				 .String(TEXT("type"), TEXT("Attribute Niagara type."));
			})
			.Bool(TEXT("fixedBoundsEnabled"), TEXT(""))
			.Bool(TEXT("deterministicEnabled"), TEXT(""))
			.String(TEXT("stageName"), TEXT(""))
			.String(TEXT("stageIterationSource"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageEffect);
